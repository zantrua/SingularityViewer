/** 
 * @file llvertexbuffer.cpp
 * @brief LLVertexBuffer implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <boost/static_assert.hpp>
#include "llsys.h"
#include "llvertexbuffer.h"
// #include "llrender.h"
#include "llglheaders.h"
#include "llmemtype.h"
#include "llrender.h"
#include "llvector4a.h"
#include "llcontrol.h"
#include "llshadermgr.h"
#include "llglslshader.h"
#include "llmemory.h"

//Next Highest Power Of Two
//helper function, returns first number > v that is a power of 2, or v if v is already a power of 2
U32 nhpo2(U32 v)
{
	U32 r = 1;
	while (r < v) {
		r *= 2;
	}
	return r;
}


//============================================================================

//static
LLVBOPool LLVertexBuffer::sStreamVBOPool;
LLVBOPool LLVertexBuffer::sDynamicVBOPool;
LLVBOPool LLVertexBuffer::sStreamIBOPool;
LLVBOPool LLVertexBuffer::sDynamicIBOPool;
U32 LLVBOPool::sBytesPooled = 0;

LLPrivateMemoryPool* LLVertexBuffer::sPrivatePoolp = NULL ;
U32 LLVertexBuffer::sBindCount = 0;
U32 LLVertexBuffer::sSetCount = 0;
S32 LLVertexBuffer::sCount = 0;
S32 LLVertexBuffer::sGLCount = 0;
S32 LLVertexBuffer::sMappedCount = 0;
BOOL LLVertexBuffer::sDisableVBOMapping = FALSE ;
BOOL LLVertexBuffer::sEnableVBOs = TRUE;
U32 LLVertexBuffer::sGLRenderBuffer = 0;
U32 LLVertexBuffer::sGLRenderArray = 0;
U32 LLVertexBuffer::sGLRenderIndices = 0;
U32 LLVertexBuffer::sLastMask = 0;
BOOL LLVertexBuffer::sVBOActive = FALSE;
BOOL LLVertexBuffer::sIBOActive = FALSE;
U32 LLVertexBuffer::sAllocatedBytes = 0;
BOOL LLVertexBuffer::sMapped = FALSE;
BOOL LLVertexBuffer::sUseStreamDraw = TRUE;
BOOL LLVertexBuffer::sUseVAO = FALSE;
BOOL LLVertexBuffer::sPreferStreamDraw = FALSE;

const U32 FENCE_WAIT_TIME_NANOSECONDS = 10000;  //1 ms

class LLGLSyncFence : public LLGLFence
{
public:
#ifdef GL_ARB_sync
	GLsync mSync;
#endif
	
	LLGLSyncFence()
	{
#ifdef GL_ARB_sync
		mSync = 0;
#endif
	}

	virtual ~LLGLSyncFence()
	{
#ifdef GL_ARB_sync
		if (mSync)
		{
			glDeleteSync(mSync);
		}
#endif
	}

	void placeFence()
	{
#ifdef GL_ARB_sync
		if (mSync)
		{
			glDeleteSync(mSync);
		}
		mSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
	}

	void wait()
	{
#ifdef GL_ARB_sync
		if (mSync)
		{
			while (glClientWaitSync(mSync, 0, FENCE_WAIT_TIME_NANOSECONDS) == GL_TIMEOUT_EXPIRED)
			{ //track the number of times we've waited here
				static S32 waits = 0;
				waits++;
			}
		}
#endif
	}


};


//which power of 2 is i?
//assumes i is a power of 2 > 0
U32 wpo2(U32 i)
{
	llassert(i > 0);
	llassert(nhpo2(i) == i);

	U32 r = 0;

	while (i >>= 1) ++r;

	return r;
}

volatile U8* LLVBOPool::allocate(U32& name, U32 size)
{
	llassert(nhpo2(size) == size);

	U32 i = wpo2(size);

	if (mFreeList.size() <= i)
	{
		mFreeList.resize(i+1);
	}

	volatile U8* ret = NULL;

	if (mFreeList[i].empty())
	{
		//make a new buffer
		glGenBuffersARB(1, &name);
		glBindBufferARB(mType, name);
		LLVertexBuffer::sAllocatedBytes += size;

		if (LLVertexBuffer::sDisableVBOMapping || mUsage != GL_DYNAMIC_DRAW_ARB)
		{
			glBufferDataARB(mType, size, 0, mUsage);
			ret = (U8*) ll_aligned_malloc_16(size);
		}
		else
		{ //always use a true hint of static draw when allocating non-client-backed buffers
			glBufferDataARB(mType, size, 0, GL_STATIC_DRAW_ARB);
		}
		glBindBufferARB(mType, 0);
	}
	else
	{
		name = mFreeList[i].front().mGLName;
		ret = mFreeList[i].front().mClientData;

		sBytesPooled -= size;

		mFreeList[i].pop_front();
	}

	return ret;
}

void LLVBOPool::release(U32 name, volatile U8* buffer, U32 size)
{
	llassert(nhpo2(size) == size);

	U32 i = wpo2(size);

	llassert(mFreeList.size() > i);

	Record rec;
	rec.mGLName = name;
	rec.mClientData = buffer;

	sBytesPooled += size;
	
	if (!LLVertexBuffer::sDisableVBOMapping && mUsage == GL_DYNAMIC_DRAW_ARB)
	{
		glDeleteBuffersARB(1, &rec.mGLName);
	}
	else
	{
		mFreeList[i].push_back(rec);
	}
}

void LLVBOPool::cleanup()
{
	U32 size = 1;

	for (U32 i = 0; i < mFreeList.size(); ++i)
	{
		record_list_t& l = mFreeList[i];

		while (!l.empty())
		{
			Record& r = l.front();

			glDeleteBuffersARB(1, &r.mGLName);

			if (r.mClientData)
			{
				ll_aligned_free_16((void*) r.mClientData);
			}

			l.pop_front();

			LLVertexBuffer::sAllocatedBytes -= size;
			sBytesPooled -= size;
		}

		size *= 2;
	}
}


//NOTE: each component must be AT LEAST 4 bytes in size to avoid a performance penalty on AMD hardware
S32 LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_MAX] =
{
	sizeof(LLVector4), // TYPE_VERTEX,
	sizeof(LLVector4), // TYPE_NORMAL,
	sizeof(LLVector2), // TYPE_TEXCOORD0,
	sizeof(LLVector2), // TYPE_TEXCOORD1,
	sizeof(LLVector2), // TYPE_TEXCOORD2,
	sizeof(LLVector2), // TYPE_TEXCOORD3,
	sizeof(LLColor4U), // TYPE_COLOR,
	sizeof(LLColor4U), // TYPE_EMISSIVE, only alpha is used currently
	sizeof(LLVector4), // TYPE_BINORMAL,
	sizeof(F32),	   // TYPE_WEIGHT,
	sizeof(LLVector4), // TYPE_WEIGHT4,
	sizeof(LLVector4), // TYPE_CLOTHWEIGHT,
	sizeof(LLVector4), // TYPE_TEXTURE_INDEX (actually exists as position.w), no extra data, but stride is 16 bytes
};

U32 LLVertexBuffer::sGLMode[LLRender::NUM_MODES] = 
{
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN,
	GL_POINTS,
	GL_LINES,
	GL_LINE_STRIP,
	GL_QUADS,
	GL_LINE_LOOP,
};


//static
void LLVertexBuffer::setupClientArrays(U32 data_mask)
{
	if (sLastMask != data_mask)
	{
		BOOL error = FALSE;

		if (LLGLSLShader::sNoFixedFunction)
		{
			for (U32 i = 0; i < TYPE_MAX; ++i)
			{
				S32 loc = i;
										
				U32 mask = 1 << i;

				if (sLastMask & (1 << i))
				{ //was enabled
					if (!(data_mask & mask))
					{ //needs to be disabled
						glDisableVertexAttribArrayARB(loc);
					}
				}
				else 
				{	//was disabled
					if (data_mask & mask)
					{ //needs to be enabled
						glEnableVertexAttribArrayARB(loc);
					}
				}
			}
		}
		else
		{

			GLenum array[] =
			{
				GL_VERTEX_ARRAY,
				GL_NORMAL_ARRAY,
				GL_TEXTURE_COORD_ARRAY,
				GL_COLOR_ARRAY,
			};

			GLenum mask[] = 
			{
				MAP_VERTEX,
				MAP_NORMAL,
				MAP_TEXCOORD0,
				MAP_COLOR
			};



			for (U32 i = 0; i < 4; ++i)
			{
				if (sLastMask & mask[i])
				{ //was enabled
					if (!(data_mask & mask[i]))
					{ //needs to be disabled
						glDisableClientState(array[i]);
					}
					else if (gDebugGL)
					{ //needs to be enabled, make sure it was (DEBUG)
						if (!glIsEnabled(array[i]))
						{
							if (gDebugSession)
							{
								error = TRUE;
								gFailLog << "Bad client state! " << array[i] << " disabled." << std::endl;
							}
							else
							{
								llerrs << "Bad client state! " << array[i] << " disabled." << llendl;
							}
						}
					}
				}
				else 
				{	//was disabled
					if (data_mask & mask[i])
					{ //needs to be enabled
						glEnableClientState(array[i]);
					}
					else if (gDebugGL && glIsEnabled(array[i]))
					{ //needs to be disabled, make sure it was (DEBUG TEMPORARY)
						if (gDebugSession)
						{
							error = TRUE;
							gFailLog << "Bad client state! " << array[i] << " enabled." << std::endl;
						}
						else
						{
							llerrs << "Bad client state! " << array[i] << " enabled." << llendl;
						}
					}
				}
			}
		
			U32 map_tc[] = 
			{
				MAP_TEXCOORD1,
				MAP_TEXCOORD2,
				MAP_TEXCOORD3
			};

			for (U32 i = 0; i < 3; i++)
			{
				if (sLastMask & map_tc[i])
				{
					if (!(data_mask & map_tc[i]))
					{ //disable
						glClientActiveTextureARB(GL_TEXTURE1_ARB+i);
						glDisableClientState(GL_TEXTURE_COORD_ARRAY);
						glClientActiveTextureARB(GL_TEXTURE0_ARB);
					}
				}
				else if (data_mask & map_tc[i])
				{
					glClientActiveTextureARB(GL_TEXTURE1_ARB+i);
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glClientActiveTextureARB(GL_TEXTURE0_ARB);
				}
			}

			if (sLastMask & MAP_BINORMAL)
			{
				if (!(data_mask & MAP_BINORMAL))
				{
					glClientActiveTextureARB(GL_TEXTURE2_ARB);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					glClientActiveTextureARB(GL_TEXTURE0_ARB);
				}
			}
			else if (data_mask & MAP_BINORMAL)
			{
				glClientActiveTextureARB(GL_TEXTURE2_ARB);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glClientActiveTextureARB(GL_TEXTURE0_ARB);
			}
		}
				
		sLastMask = data_mask;
	}
}

//static
void LLVertexBuffer::drawArrays(U32 mode, const std::vector<LLVector3>& pos, const std::vector<LLVector3>& norm)
{
	llassert(!LLGLSLShader::sNoFixedFunction || LLGLSLShader::sCurBoundShaderPtr != NULL);
	gGL.syncMatrices();

	U32 count = pos.size();
	llassert_always(norm.size() >= pos.size());
	llassert_always(count > 0) ;

	unbind();
	
	setupClientArrays(MAP_VERTEX | MAP_NORMAL);

	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

	if (shader)
	{
		S32 loc = LLVertexBuffer::TYPE_VERTEX;
		if (loc > -1)
		{
			glVertexAttribPointerARB(loc, 3, GL_FLOAT, GL_FALSE, 0, pos[0].mV);
		}
		loc = LLVertexBuffer::TYPE_NORMAL;
		if (loc > -1)
		{
			glVertexAttribPointerARB(loc, 3, GL_FLOAT, GL_FALSE, 0, norm[0].mV);
		}
	}
	else
	{
		glVertexPointer(3, GL_FLOAT, 0, pos[0].mV);
		glNormalPointer(GL_FLOAT, 0, norm[0].mV);
	}

	glDrawArrays(sGLMode[mode], 0, count);
}

//static
void LLVertexBuffer::drawElements(U32 mode, const LLVector4a* pos, const LLVector2* tc, S32 num_indices, const U16* indicesp)
{
	llassert(!LLGLSLShader::sNoFixedFunction || LLGLSLShader::sCurBoundShaderPtr != NULL);

	gGL.syncMatrices();

	U32 mask = LLVertexBuffer::MAP_VERTEX;
	if (tc)
	{
		mask = mask | LLVertexBuffer::MAP_TEXCOORD0;
	}

	unbind();
	
	setupClientArrays(mask);

	if (LLGLSLShader::sNoFixedFunction)
	{
		S32 loc = LLVertexBuffer::TYPE_VERTEX;
		glVertexAttribPointerARB(loc, 3, GL_FLOAT, GL_FALSE, 16, pos);

		if (tc)
		{
			loc = LLVertexBuffer::TYPE_TEXCOORD0;
			glVertexAttribPointerARB(loc, 2, GL_FLOAT, GL_FALSE, 0, tc);
		}
	}
	else
	{
		glTexCoordPointer(2, GL_FLOAT, 0, tc);
		glVertexPointer(3, GL_FLOAT, 16, pos);
	}

	glDrawElements(sGLMode[mode], num_indices, GL_UNSIGNED_SHORT, indicesp);
}

void LLVertexBuffer::validateRange(U32 start, U32 end, U32 count, U32 indices_offset) const
{
	if (start >= (U32) mNumVerts ||
	    end >= (U32) mNumVerts)
	{
		llerrs << "Bad vertex buffer draw range: [" << start << ", " << end << "] vs " << mNumVerts << llendl;
	}

	llassert(mNumIndices >= 0);

	if (indices_offset >= (U32) mNumIndices ||
	    indices_offset + count > (U32) mNumIndices)
	{
		llerrs << "Bad index buffer draw range: [" << indices_offset << ", " << indices_offset+count << "]" << llendl;
	}

	if (gDebugGL && !useVBOs())
	{
		U16* idx = ((U16*) getIndicesPointer())+indices_offset;
		for (U32 i = 0; i < count; ++i)
		{
			if (idx[i] < start || idx[i] > end)
			{
				llerrs << "Index out of range: " << idx[i] << " not in [" << start << ", " << end << "]" << llendl;
			}
		}

		LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

		if (shader && shader->mFeatures.mIndexedTextureChannels > 1)
		{
			LLStrider<LLVector4a> v;
			//hack to get non-const reference
			LLVertexBuffer* vb = (LLVertexBuffer*) this;
			vb->getVertexStrider(v);

			for (U32 i = start; i < end; i++)
			{
				S32 idx = (S32) (v[i][3]+0.25f);
				if (idx < 0 || idx >= shader->mFeatures.mIndexedTextureChannels)
				{
					llerrs << "Bad texture index found in vertex data stream." << llendl;
				}
			}
		}
	}
}

void LLVertexBuffer::drawRange(U32 mode, U32 start, U32 end, U32 count, U32 indices_offset) const
{
	validateRange(start, end, count, indices_offset);
	mMappable = FALSE;
	gGL.syncMatrices();

	llassert(mNumVerts >= 0);
	llassert(!LLGLSLShader::sNoFixedFunction || LLGLSLShader::sCurBoundShaderPtr != NULL);

	if (mGLArray)
	{
		if (mGLArray != sGLRenderArray)
		{
			llerrs << "Wrong vertex array bound." << llendl;
		}
	}
	else
	{
		if (mGLIndices != sGLRenderIndices)
		{
			llerrs << "Wrong index buffer bound." << llendl;
		}

		if (mGLBuffer != sGLRenderBuffer)
		{
			llerrs << "Wrong vertex buffer bound." << llendl;
		}
	}

	if (gDebugGL && !mGLArray && useVBOs())
	{
		GLint elem = 0;
		glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &elem);

		if (elem != mGLIndices)
		{
			llerrs << "Wrong index buffer bound!" << llendl;
		}
	}

	if (mode >= LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
		return;
	}

	U16* idx = ((U16*) getIndicesPointer())+indices_offset;

	stop_glerror();
	glDrawRangeElements(sGLMode[mode], start, end, count, GL_UNSIGNED_SHORT, 
		idx);
	stop_glerror();
	placeFence();
}

void LLVertexBuffer::draw(U32 mode, U32 count, U32 indices_offset) const
{
	llassert(!LLGLSLShader::sNoFixedFunction || LLGLSLShader::sCurBoundShaderPtr != NULL);
	mMappable = FALSE;
	gGL.syncMatrices();

	llassert(mNumIndices >= 0);
	if (indices_offset >= (U32) mNumIndices ||
	    indices_offset + count > (U32) mNumIndices)
	{
		llerrs << "Bad index buffer draw range: [" << indices_offset << ", " << indices_offset+count << "]" << llendl;
	}

	if (mGLArray)
	{
		if (mGLArray != sGLRenderArray)
		{
			llerrs << "Wrong vertex array bound." << llendl;
		}
	}
	else
	{
		if (mGLIndices != sGLRenderIndices)
		{
			llerrs << "Wrong index buffer bound." << llendl;
		}

		if (mGLBuffer != sGLRenderBuffer)
		{
			llerrs << "Wrong vertex buffer bound." << llendl;
		}
	}

	if (mode >= LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
		return;
	}

	stop_glerror();
	glDrawElements(sGLMode[mode], count, GL_UNSIGNED_SHORT,
		((U16*) getIndicesPointer()) + indices_offset);
	stop_glerror();
	placeFence();
}

void LLVertexBuffer::drawArrays(U32 mode, U32 first, U32 count) const
{
	llassert(!LLGLSLShader::sNoFixedFunction || LLGLSLShader::sCurBoundShaderPtr != NULL);
	mMappable = FALSE;
	gGL.syncMatrices();
	
	llassert(mNumVerts >= 0);
	if (first >= (U32) mNumVerts ||
	    first + count > (U32) mNumVerts)
	{
		llerrs << "Bad vertex buffer draw range: [" << first << ", " << first+count << "]" << llendl;
	}

	if (mGLArray)
	{
		if (mGLArray != sGLRenderArray)
		{
			llerrs << "Wrong vertex array bound." << llendl;
		}
	}
	else
	{
		if (mGLBuffer != sGLRenderBuffer || useVBOs() != sVBOActive)
		{
			llerrs << "Wrong vertex buffer bound." << llendl;
		}
	}

	if (mode >= LLRender::NUM_MODES)
	{
		llerrs << "Invalid draw mode: " << mode << llendl;
		return;
	}

	stop_glerror();
	glDrawArrays(sGLMode[mode], first, count);
	stop_glerror();
	placeFence();
}

//static
void LLVertexBuffer::initClass(bool use_vbo, bool no_vbo_mapping)
{
	sEnableVBOs = use_vbo && gGLManager.mHasVertexBufferObject ;
	sDisableVBOMapping = sEnableVBOs && no_vbo_mapping ;

	if(!sPrivatePoolp)
	{ 
		sPrivatePoolp = LLPrivateMemoryPoolManager::getInstance()->newPool(LLPrivateMemoryPool::STATIC) ;
	}

	sStreamVBOPool.mType = GL_ARRAY_BUFFER_ARB;
	sStreamVBOPool.mUsage= GL_STREAM_DRAW_ARB;
	sStreamIBOPool.mType = GL_ELEMENT_ARRAY_BUFFER_ARB;
	sStreamIBOPool.mUsage= GL_STREAM_DRAW_ARB;

	sDynamicVBOPool.mType = GL_ARRAY_BUFFER_ARB;
	sDynamicVBOPool.mUsage= GL_DYNAMIC_DRAW_ARB;
	sDynamicIBOPool.mType = GL_ELEMENT_ARRAY_BUFFER_ARB;
	sDynamicIBOPool.mUsage= GL_DYNAMIC_DRAW_ARB;
}

//static 
void LLVertexBuffer::unbind()
{
	if (sGLRenderArray)
	{
#if GL_ARB_vertex_array_object
		glBindVertexArray(0);
#endif
		sGLRenderArray = 0;
		sGLRenderIndices = 0;
		sIBOActive = FALSE;
	}

	if (sVBOActive)
	{
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		sVBOActive = FALSE;
	}
	if (sIBOActive)
	{
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		sIBOActive = FALSE;
	}

	sGLRenderBuffer = 0;
	sGLRenderIndices = 0;

	setupClientArrays(0);
}

//static
void LLVertexBuffer::cleanupClass()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	unbind();
	
	sStreamIBOPool.cleanup();
	sDynamicIBOPool.cleanup();
	sStreamVBOPool.cleanup();
	sDynamicVBOPool.cleanup();

	if(sPrivatePoolp)
	{
		LLPrivateMemoryPoolManager::getInstance()->deletePool(sPrivatePoolp) ;
		sPrivatePoolp = NULL ;
	}
}

//----------------------------------------------------------------------------

LLVertexBuffer::LLVertexBuffer(U32 typemask, S32 usage) :
	LLRefCount(),

	mNumVerts(0),
	mNumIndices(0),
	mUsage(usage),
	mGLBuffer(0),
	mGLArray(0),
	mGLIndices(0), 
	mMappedData(NULL),
	mMappedIndexData(NULL), 
	mVertexLocked(FALSE),
	mIndexLocked(FALSE),
	mFinal(FALSE),
	mEmpty(TRUE),
	mFence(NULL)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	mFence = NULL;
	if (!sEnableVBOs)
	{
		mUsage = 0 ; 
	}

	if (mUsage == GL_STREAM_DRAW_ARB && !sUseStreamDraw)
	{
		mUsage = 0;
	}
	
	if (mUsage == GL_DYNAMIC_DRAW_ARB && sPreferStreamDraw)
	{
		mUsage = GL_STREAM_DRAW_ARB;
	}

	if (mUsage == 0 && LLRender::sGLCoreProfile)
	{ //MUST use VBOs for all rendering
		mUsage = GL_STREAM_DRAW_ARB;
	}

	if (mUsage && mUsage != GL_STREAM_DRAW_ARB)
	{ //only stream_draw and dynamic_draw are supported when using VBOs, dynamic draw is the default
		if (sDisableVBOMapping)
		{ //always use stream draw if VBO mapping is disabled
			mUsage = GL_STREAM_DRAW_ARB;
		}
		else
		{
			mUsage = GL_DYNAMIC_DRAW_ARB;
		}
	}
		
	if (mUsage == GL_DYNAMIC_DRAW_ARB && !sDisableVBOMapping)
	{
		mMappable = TRUE;
	}
	else
	{
		mMappable = FALSE;
	}

	//zero out offsets
	for (U32 i = 0; i < TYPE_MAX; i++)
	{
		mOffsets[i] = 0;
	}

	mTypeMask = typemask;
	mSize = 0;
	mIndicesSize = 0;
	mAlignedOffset = 0;
	mAlignedIndexOffset = 0;

	sCount++;
}

//static
S32 LLVertexBuffer::calcOffsets(const U32& typemask, S32* offsets, S32 num_vertices)
{
	S32 offset = 0;
	for (S32 i=0; i<TYPE_TEXTURE_INDEX; i++)
	{
		U32 mask = 1<<i;
		if (typemask & mask)
		{
			if (offsets && LLVertexBuffer::sTypeSize[i])
			{
				offsets[i] = offset;
				offset += LLVertexBuffer::sTypeSize[i]*num_vertices;
				offset = (offset + 0xF) & ~0xF;
			}
		}
	}

	offsets[TYPE_TEXTURE_INDEX] = offsets[TYPE_VERTEX] + 12;
	
	return offset+16;
}

//static 
S32 LLVertexBuffer::calcVertexSize(const U32& typemask)
{
	S32 size = 0;
	for (S32 i = 0; i < TYPE_TEXTURE_INDEX; i++)
	{
		U32 mask = 1<<i;
		if (typemask & mask)
		{
			size += LLVertexBuffer::sTypeSize[i];
		}
	}

	return size;
}

S32 LLVertexBuffer::getSize() const
{
	return mSize;
}

// protected, use unref()
//virtual
LLVertexBuffer::~LLVertexBuffer()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	destroyGLBuffer();
	destroyGLIndices();

	if (mGLArray)
	{
#if GL_ARB_vertex_array_object
		glDeleteVertexArrays(1, &mGLArray);
#endif
	}

	sCount--;

	if (mFence)
	{
		delete mFence;
	}
	
	mFence = NULL;

	llassert_always(!mMappedData && !mMappedIndexData) ;
};

void LLVertexBuffer::placeFence() const
{
	/*if (!mFence && useVBOs())
	{
		if (gGLManager.mHasSync)
		{
			mFence = new LLGLSyncFence();
		}
	}

	if (mFence)
	{
		mFence->placeFence();
	}*/
}

void LLVertexBuffer::waitFence() const
{
	/*if (mFence)
	{
		mFence->wait();
	}*/
}

//----------------------------------------------------------------------------

void LLVertexBuffer::genBuffer(U32 size)
{
	mSize = nhpo2(size);

	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		mMappedData = sStreamVBOPool.allocate(mGLBuffer, mSize);
	}
	else
	{
		mMappedData = sDynamicVBOPool.allocate(mGLBuffer, mSize);
	}
	
	sGLCount++;
}

void LLVertexBuffer::genIndices(U32 size)
{
	mIndicesSize = nhpo2(size);

	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		mMappedIndexData = sStreamIBOPool.allocate(mGLIndices, mIndicesSize);
	}
	else
	{
		mMappedIndexData = sDynamicIBOPool.allocate(mGLIndices, mIndicesSize);
	}
	
	sGLCount++;
}

void LLVertexBuffer::releaseBuffer()
{
	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		sStreamVBOPool.release(mGLBuffer, mMappedData, mSize);
	}
	else
	{
		sDynamicVBOPool.release(mGLBuffer, mMappedData, mSize);
	}
	
	mGLBuffer = 0;
	mMappedData = NULL;

	sGLCount--;
}

void LLVertexBuffer::releaseIndices()
{
	if (mUsage == GL_STREAM_DRAW_ARB)
	{
		sStreamIBOPool.release(mGLIndices, mMappedIndexData, mIndicesSize);
	}
	else
	{
		sDynamicIBOPool.release(mGLIndices, mMappedIndexData, mIndicesSize);
	}

	mGLIndices = 0;
	mMappedIndexData = NULL;
	
	sGLCount--;
}

void LLVertexBuffer::createGLBuffer(U32 size)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	
	if (mGLBuffer)
	{
		destroyGLBuffer();
	}

	if (size == 0)
	{
		return;
	}

	mEmpty = TRUE;

	if (useVBOs())
	{
		genBuffer(size);
	}
	else
	{
		static int gl_buffer_idx = 0;
		mGLBuffer = ++gl_buffer_idx;
		mMappedData = (U8*)ALLOCATE_MEM(sPrivatePoolp, size);
		mSize = size;
	}
}

void LLVertexBuffer::createGLIndices(U32 size)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	
	if (mGLIndices)
	{
		destroyGLIndices();
	}
	
	if (size == 0)
	{
		return;
	}

	mEmpty = TRUE;

	//pad by 16 bytes for aligned copies
	size += 16;

	if (useVBOs())
	{
		//pad by another 16 bytes for VBO pointer adjustment
		size += 16;
		genIndices(size);
	}
	else
	{
		mMappedIndexData = (U8*)ALLOCATE_MEM(sPrivatePoolp, size);
		static int gl_buffer_idx = 0;
		mGLIndices = ++gl_buffer_idx;
		mIndicesSize = size;
	}
}

void LLVertexBuffer::destroyGLBuffer()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (mGLBuffer)
	{
		if (useVBOs())
		{
			releaseBuffer();
		}
		else
		{
			FREE_MEM(sPrivatePoolp, (void*) mMappedData) ;
			mMappedData = NULL;
			mEmpty = TRUE;
		}
	}
	
	mGLBuffer = 0;
	//unbind();
}

void LLVertexBuffer::destroyGLIndices()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (mGLIndices)
	{
		if (useVBOs())
		{
			releaseIndices();
		}
		else
		{
			FREE_MEM(sPrivatePoolp, (void*) mMappedIndexData) ;
			mMappedIndexData = NULL;
			mEmpty = TRUE;
		}
	}

	mGLIndices = 0;
	//unbind();
}

void LLVertexBuffer::updateNumVerts(S32 nverts)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);

	llassert(nverts >= 0);

	if (nverts >= 65535)
	{
		llwarns << "Vertex buffer overflow!" << llendl;
		nverts = 65535;
	}

	S32 needed_size = calcOffsets(mTypeMask, mOffsets, nverts);

	if (needed_size > mSize || needed_size <= mSize/2)
	{
		createGLBuffer(needed_size);
	}

	mNumVerts = nverts;
}

void LLVertexBuffer::updateNumIndices(S32 nindices)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);

	llassert(nindices >= 0);

	S32 needed_size = sizeof(U16) * nindices;

	if (needed_size > mIndicesSize || needed_size <= mIndicesSize/2)
	{
		createGLIndices(needed_size);
	}

	mNumIndices = nindices;
}

void LLVertexBuffer::allocateBuffer(S32 nverts, S32 nindices, bool create)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	
	stop_glerror();

	if (nverts < 0 || nindices < 0 ||
		nverts > 65536)
	{
		llerrs << "Bad vertex buffer allocation: " << nverts << " : " << nindices << llendl;
	}

	updateNumVerts(nverts);
	updateNumIndices(nindices);
	
	if (create && (nverts || nindices))
	{
		//actually allocate space for the vertex buffer if using VBO mapping
		flush();

		if (gGLManager.mHasVertexArrayObject && useVBOs() && (LLRender::sGLCoreProfile || sUseVAO))
		{
#if GL_ARB_vertex_array_object
			glGenVertexArrays(1, &mGLArray);
#endif
			setupVertexArray();
		}
	}
}

//static LLFastTimer::DeclareTimer FTM_SETUP_VERTEX_ARRAY("Setup VAO");

void LLVertexBuffer::setupVertexArray()
{
	if (!mGLArray)
	{
		return;
	}

	//LLFastTimer t(FTM_SETUP_VERTEX_ARRAY);
#if GL_ARB_vertex_array_object
	glBindVertexArray(mGLArray);
#endif
	sGLRenderArray = mGLArray;

	U32 attrib_size[] = 
	{
		3, //TYPE_VERTEX,
		3, //TYPE_NORMAL,
		2, //TYPE_TEXCOORD0,
		2, //TYPE_TEXCOORD1,
		2, //TYPE_TEXCOORD2,
		2, //TYPE_TEXCOORD3,
		4, //TYPE_COLOR,
		4, //TYPE_EMISSIVE,
		3, //TYPE_BINORMAL,
		1, //TYPE_WEIGHT,
		4, //TYPE_WEIGHT4,
		4, //TYPE_CLOTHWEIGHT,
		1, //TYPE_TEXTURE_INDEX
	};

	U32 attrib_type[] =
	{
		GL_FLOAT, //TYPE_VERTEX,
		GL_FLOAT, //TYPE_NORMAL,
		GL_FLOAT, //TYPE_TEXCOORD0,
		GL_FLOAT, //TYPE_TEXCOORD1,
		GL_FLOAT, //TYPE_TEXCOORD2,
		GL_FLOAT, //TYPE_TEXCOORD3,
		GL_UNSIGNED_BYTE, //TYPE_COLOR,
		GL_UNSIGNED_BYTE, //TYPE_EMISSIVE,
		GL_FLOAT,   //TYPE_BINORMAL,
		GL_FLOAT, //TYPE_WEIGHT,
		GL_FLOAT, //TYPE_WEIGHT4,
		GL_FLOAT, //TYPE_CLOTHWEIGHT,
		GL_FLOAT, //TYPE_TEXTURE_INDEX
	};

	U32 attrib_normalized[] =
	{
		GL_FALSE, //TYPE_VERTEX,
		GL_FALSE, //TYPE_NORMAL,
		GL_FALSE, //TYPE_TEXCOORD0,
		GL_FALSE, //TYPE_TEXCOORD1,
		GL_FALSE, //TYPE_TEXCOORD2,
		GL_FALSE, //TYPE_TEXCOORD3,
		GL_TRUE, //TYPE_COLOR,
		GL_TRUE, //TYPE_EMISSIVE,
		GL_FALSE,   //TYPE_BINORMAL,
		GL_FALSE, //TYPE_WEIGHT,
		GL_FALSE, //TYPE_WEIGHT4,
		GL_FALSE, //TYPE_CLOTHWEIGHT,
		GL_FALSE, //TYPE_TEXTURE_INDEX
	};

	bindGLBuffer(true);
	bindGLIndices(true);

	for (U32 i = 0; i < TYPE_MAX; ++i)
	{
		if (mTypeMask & (1 << i))
		{
			glEnableVertexAttribArrayARB(i);
			glVertexAttribPointerARB(i, attrib_size[i], attrib_type[i], attrib_normalized[i], sTypeSize[i], (void*) mOffsets[i]); 
		}
		else
		{
			glDisableVertexAttribArrayARB(i);
		}
	}

	//draw a dummy triangle to set index array pointer
	//glDrawElements(GL_TRIANGLES, 0, GL_UNSIGNED_SHORT, NULL);

	unbind();
}

void LLVertexBuffer::resizeBuffer(S32 newnverts, S32 newnindices)
{
	llassert(newnverts >= 0);
	llassert(newnindices >= 0);

	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	
	updateNumVerts(newnverts);		
	updateNumIndices(newnindices);
	
	if (useVBOs())
	{
		flush();

		if (mGLArray)
		{ //if size changed, offsets changed
			setupVertexArray();
		}
	}
}

BOOL LLVertexBuffer::useVBOs() const
{
	//it's generally ineffective to use VBO for things that are streaming on apple
		
	if (!mUsage)
	{
		return FALSE;
	}

	return TRUE;
}

//----------------------------------------------------------------------------

bool expand_region(LLVertexBuffer::MappedRegion& region, S32 index, S32 count)
{
	S32 end = index+count;
	S32 region_end = region.mIndex+region.mCount;
	
	if (end < region.mIndex ||
		index > region_end)
	{ //gap exists, do not merge
		return false;
	}

	S32 new_end = llmax(end, region_end);
	S32 new_index = llmin(index, region.mIndex);
	region.mIndex = new_index;
	region.mCount = new_end-new_index;
	return true;
}

// Map for data access
volatile U8* LLVertexBuffer::mapVertexBuffer(S32 type, S32 index, S32 count, bool map_range)
{
	bindGLBuffer(true);
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (mFinal)
	{
		llerrs << "LLVertexBuffer::mapVeretxBuffer() called on a finalized buffer." << llendl;
	}
	if (!useVBOs() && !mMappedData && !mMappedIndexData)
	{
		llerrs << "LLVertexBuffer::mapVertexBuffer() called on unallocated buffer." << llendl;
	}
		
	if (useVBOs())
	{
		if (!mMappable || gGLManager.mHasMapBufferRange || gGLManager.mHasFlushBufferRange)
		{
			if (count == -1)
			{
				count = mNumVerts-index;
			}

			bool mapped = false;
			//see if range is already mapped
			for (U32 i = 0; i < mMappedVertexRegions.size(); ++i)
			{
				MappedRegion& region = mMappedVertexRegions[i];
				if (region.mType == type)
				{
					if (expand_region(region, index, count))
					{
						mapped = true;
						break;
					}
				}
			}

			if (!mapped)
			{
				//not already mapped, map new region
				MappedRegion region(type, mMappable && map_range ? -1 : index, count);
				mMappedVertexRegions.push_back(region);
			}
		}

		if (mVertexLocked && map_range)
		{
			llerrs << "Attempted to map a specific range of a buffer that was already mapped." << llendl;
		}

		if (!mVertexLocked)
		{
			mVertexLocked = TRUE;
			sMappedCount++;
			stop_glerror();	

			if(!mMappable)
			{
				map_range = false;
			}
			else
			{
				volatile U8* src = NULL;
				waitFence();
				if (gGLManager.mHasMapBufferRange)
				{
					if (map_range)
					{
#ifdef GL_ARB_map_buffer_range
						S32 offset = mOffsets[type] + sTypeSize[type]*index;
						S32 length = (sTypeSize[type]*count+0xF) & ~0xF;
						src = (U8*) glMapBufferRange(GL_ARRAY_BUFFER_ARB, offset, length, 
							GL_MAP_WRITE_BIT | 
							GL_MAP_FLUSH_EXPLICIT_BIT | 
							GL_MAP_INVALIDATE_RANGE_BIT);
#endif
					}
					else
					{
#ifdef GL_ARB_map_buffer_range

						if (gDebugGL)
						{
							GLint size = 0;
							glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_SIZE_ARB, &size);

							if (size < mSize)
							{
								llerrs << "Invalid buffer size." << llendl;
							}
						}

						src = (U8*) glMapBufferRange(GL_ARRAY_BUFFER_ARB, 0, mSize, 
							GL_MAP_WRITE_BIT | 
							GL_MAP_FLUSH_EXPLICIT_BIT);
#endif
					}
				}
				else if (gGLManager.mHasFlushBufferRange)
				{
					if (map_range)
					{
						glBufferParameteriAPPLE(GL_ARRAY_BUFFER_ARB, GL_BUFFER_SERIALIZED_MODIFY_APPLE, GL_FALSE);
						glBufferParameteriAPPLE(GL_ARRAY_BUFFER_ARB, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
						src = (U8*) glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
					}
					else
					{
						src = (U8*) glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
					}
				}
				else
				{
					map_range = false;
					src = (U8*) glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
				}

				llassert(src != NULL);

				mMappedData = LL_NEXT_ALIGNED_ADDRESS<volatile U8>(src);
				mAlignedOffset = mMappedData - src;
			
				stop_glerror();
			}
				
			if (!mMappedData)
			{
				log_glerror();

			//check the availability of memory
			LLMemory::logMemoryInfo(TRUE) ; 
			
				if(mMappable)
				{			
					//--------------------
					//print out more debug info before crash
					llinfos << "vertex buffer size: (num verts : num indices) = " << getNumVerts() << " : " << getNumIndices() << llendl ;
					GLint size ;
					glGetBufferParameterivARB(GL_ARRAY_BUFFER_ARB, GL_BUFFER_SIZE_ARB, &size) ;
					llinfos << "GL_ARRAY_BUFFER_ARB size is " << size << llendl ;
					//--------------------

					GLint buff;
					glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &buff);
					if ((GLuint)buff != mGLBuffer)
					{
						llerrs << "Invalid GL vertex buffer bound: " << buff << llendl;
					}

							
					llerrs << "glMapBuffer returned NULL (no vertex data)" << llendl;
				}
				else
				{
					llerrs << "memory allocation for vertex data failed." << llendl ;
				}
			}
		}
	}
	else
	{
		map_range = false;
	}
	
	if (map_range && gGLManager.mHasMapBufferRange && mMappable)
	{
		return mMappedData;
	}
	else
	{
		return mMappedData+mOffsets[type]+sTypeSize[type]*index;
	}
}

volatile U8* LLVertexBuffer::mapIndexBuffer(S32 index, S32 count, bool map_range)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	bindGLIndices(true);
	if (mFinal)
	{
		llerrs << "LLVertexBuffer::mapIndexBuffer() called on a finalized buffer." << llendl;
	}
	if (!useVBOs() && !mMappedData && !mMappedIndexData)
	{
		llerrs << "LLVertexBuffer::mapIndexBuffer() called on unallocated buffer." << llendl;
	}

	if (useVBOs())
	{
		if (!mMappable || gGLManager.mHasMapBufferRange || gGLManager.mHasFlushBufferRange)
		{
			if (count == -1)
			{
				count = mNumIndices-index;
			}

			bool mapped = false;
			//see if range is already mapped
			for (U32 i = 0; i < mMappedIndexRegions.size(); ++i)
			{
				MappedRegion& region = mMappedIndexRegions[i];
				if (expand_region(region, index, count))
				{
					mapped = true;
					break;
				}
			}

			if (!mapped)
			{
				//not already mapped, map new region
				MappedRegion region(TYPE_INDEX, mMappable && map_range ? -1 : index, count);
				mMappedIndexRegions.push_back(region);
			}
		}

		if (mIndexLocked && map_range)
		{
			llerrs << "Attempted to map a specific range of a buffer that was already mapped." << llendl;
		}

		if (!mIndexLocked)
		{
			//LLMemType mt_v(LLMemType::MTYPE_VERTEX_MAP_BUFFER_INDICES);

			mIndexLocked = TRUE;
			sMappedCount++;
			stop_glerror();	

			if (gDebugGL && useVBOs())
			{
				GLint elem = 0;
				glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &elem);

				if (elem != mGLIndices)
				{
					llerrs << "Wrong index buffer bound!" << llendl;
				}
			}

			if(!mMappable)
			{
				map_range = false;
			}
			else
			{
				volatile U8* src = NULL;
				waitFence();
				if (gGLManager.mHasMapBufferRange)
				{
					if (map_range)
					{
#ifdef GL_ARB_map_buffer_range
						S32 offset = sizeof(U16)*index;
						S32 length = sizeof(U16)*count;
						src = (U8*) glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER_ARB, offset, length, 
							GL_MAP_WRITE_BIT | 
							GL_MAP_FLUSH_EXPLICIT_BIT | 
							GL_MAP_INVALIDATE_RANGE_BIT);
#endif
					}
					else
					{
#ifdef GL_ARB_map_buffer_range
						src = (U8*) glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, sizeof(U16)*mNumIndices, 
							GL_MAP_WRITE_BIT | 
							GL_MAP_FLUSH_EXPLICIT_BIT);
#endif
					}
				}
				else if (gGLManager.mHasFlushBufferRange)
				{
					if (map_range)
					{
						glBufferParameteriAPPLE(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_BUFFER_SERIALIZED_MODIFY_APPLE, GL_FALSE);
						glBufferParameteriAPPLE(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
						src = (U8*) glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
					}
					else
					{
						src = (U8*) glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
					}
				}
				else
				{
					map_range = false;
					src = (U8*) glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
				}

				llassert(src != NULL);


				mMappedIndexData = src; //LL_NEXT_ALIGNED_ADDRESS<U8>(src);
				mAlignedIndexOffset = mMappedIndexData - src;
				stop_glerror();
			}
		}

		if (!mMappedIndexData)
		{
			log_glerror();
			LLMemory::logMemoryInfo(TRUE) ;

			if(mMappable)
			{
				GLint buff;
				glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &buff);
				if ((GLuint)buff != mGLIndices)
				{
					llerrs << "Invalid GL index buffer bound: " << buff << llendl;
				}

				llerrs << "glMapBuffer returned NULL (no index data)" << llendl;
			}
			else
			{
				llerrs << "memory allocation for Index data failed. " << llendl ;
			}
		}
	}
	else
	{
		map_range = false;
	}

	if (map_range && gGLManager.mHasMapBufferRange && mMappable)
	{
		return mMappedIndexData;
	}
	else
	{
		return mMappedIndexData + sizeof(U16)*index;
	}
}

void LLVertexBuffer::unmapBuffer()
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	if (!useVBOs())
	{
		return ; //nothing to unmap
	}

	bool updated_all = false ;

	if (mMappedData && mVertexLocked)
	{
		bindGLBuffer(true);
		updated_all = mIndexLocked; //both vertex and index buffers done updating

		if(!mMappable)
		{
			if (!mMappedVertexRegions.empty())
			{
				stop_glerror();
				for (U32 i = 0; i < mMappedVertexRegions.size(); ++i)
				{
					const MappedRegion& region = mMappedVertexRegions[i];
					S32 offset = region.mIndex >= 0 ? mOffsets[region.mType]+sTypeSize[region.mType]*region.mIndex : 0;
					S32 length = sTypeSize[region.mType]*region.mCount;
					glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, offset, length, (U8*) mMappedData+offset);
					stop_glerror();
				}

				mMappedVertexRegions.clear();
			}
			else
			{
				stop_glerror();
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, getSize(), (U8*) mMappedData);
				stop_glerror();
			}
		}
		else
		{
			if (gGLManager.mHasMapBufferRange || gGLManager.mHasFlushBufferRange)
			{
				if (!mMappedVertexRegions.empty())
				{
					stop_glerror();
					for (U32 i = 0; i < mMappedVertexRegions.size(); ++i)
					{
						const MappedRegion& region = mMappedVertexRegions[i];
						S32 offset = region.mIndex >= 0 ? mOffsets[region.mType]+sTypeSize[region.mType]*region.mIndex : 0;
						S32 length = sTypeSize[region.mType]*region.mCount;
						if (gGLManager.mHasMapBufferRange)
						{
#ifdef GL_ARB_map_buffer_range
							glFlushMappedBufferRange(GL_ARRAY_BUFFER_ARB, offset, length);
#endif
						}
						else if (gGLManager.mHasFlushBufferRange)
						{
							glFlushMappedBufferRangeAPPLE(GL_ARRAY_BUFFER_ARB, offset, length);
						}
						stop_glerror();
					}

					mMappedVertexRegions.clear();
				}
			}
			stop_glerror();
			glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
			stop_glerror();

			mMappedData = NULL;
		}

		mVertexLocked = FALSE ;
		sMappedCount--;
	}
	
	if (mMappedIndexData && mIndexLocked)
	{
		bindGLIndices();
		if(!mMappable)
		{
			if (!mMappedIndexRegions.empty())
			{
				for (U32 i = 0; i < mMappedIndexRegions.size(); ++i)
				{
					const MappedRegion& region = mMappedIndexRegions[i];
					S32 offset = region.mIndex >= 0 ? sizeof(U16)*region.mIndex : 0;
					S32 length = sizeof(U16)*region.mCount;
					glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, offset, length, (U8*) mMappedIndexData+offset);
					stop_glerror();
				}

				mMappedIndexRegions.clear();
			}
			else
			{
				stop_glerror();
				glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0, getIndicesSize(), (U8*) mMappedIndexData);
				stop_glerror();
			}
		}
		else
		{
			if (gGLManager.mHasMapBufferRange || gGLManager.mHasFlushBufferRange)
			{
				if (!mMappedIndexRegions.empty())
				{
					for (U32 i = 0; i < mMappedIndexRegions.size(); ++i)
					{
						const MappedRegion& region = mMappedIndexRegions[i];
						S32 offset = region.mIndex >= 0 ? sizeof(U16)*region.mIndex : 0;
						S32 length = sizeof(U16)*region.mCount;
						if (gGLManager.mHasMapBufferRange)
						{
#ifdef GL_ARB_map_buffer_range
							glFlushMappedBufferRange(GL_ELEMENT_ARRAY_BUFFER_ARB, offset, length);
#endif
						}
						else if (gGLManager.mHasFlushBufferRange)
						{
#ifdef GL_APPLE_flush_buffer_range
							glFlushMappedBufferRangeAPPLE(GL_ELEMENT_ARRAY_BUFFER_ARB, offset, length);
#endif
						}
						stop_glerror();
					}

					mMappedIndexRegions.clear();
				}
			}
			stop_glerror();
			glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
			stop_glerror();

			mMappedIndexData = NULL ;
		}

		mIndexLocked = FALSE ;
		sMappedCount--;
	}

	if(updated_all)
	{
		mEmpty = FALSE;
	}
}

//----------------------------------------------------------------------------

template <class T,S32 type> struct VertexBufferStrider
{
	typedef LLStrider<T> strider_t;
	static bool get(LLVertexBuffer& vbo, 
					strider_t& strider, 
					S32 index, S32 count, bool map_range)
	{
		if (type == LLVertexBuffer::TYPE_INDEX)
		{
			volatile U8* ptr = vbo.mapIndexBuffer(index, count, map_range);

			if (ptr == NULL)
			{
				llwarns << "mapIndexBuffer failed!" << llendl;
				return FALSE;
			}

			strider = (T*)ptr;
			strider.setStride(0);
			return TRUE;
		}
		else if (vbo.hasDataType(type))
		{
			S32 stride = LLVertexBuffer::sTypeSize[type];

			volatile U8* ptr = vbo.mapVertexBuffer(type, index, count, map_range);

			if (ptr == NULL)
			{
				llwarns << "mapVertexBuffer failed!" << llendl;
				return FALSE;
			}


			strider = (T*)ptr;
			
			strider.setStride(stride);
			//strider.setTypeSize(LLVertexBuffer::sTypeSize[type]);
			return TRUE;
		}
		else
		{
			llerrs << "VertexBufferStrider could not find valid vertex data." << llendl;
		}
		return FALSE;
	}
};

bool LLVertexBuffer::getVertexStrider(LLStrider<LLVector3>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector3,TYPE_VERTEX>::get(*this, strider, index, count, map_range);
}
bool LLVertexBuffer::getVertexStrider(LLStrider<LLVector4a>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector4a,TYPE_VERTEX>::get(*this, strider, index, count, map_range);
}
bool LLVertexBuffer::getIndexStrider(LLStrider<U16>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<U16,TYPE_INDEX>::get(*this, strider, index, count, map_range);
}
bool LLVertexBuffer::getTexCoord0Strider(LLStrider<LLVector2>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector2,TYPE_TEXCOORD0>::get(*this, strider, index, count, map_range);
}
bool LLVertexBuffer::getTexCoord1Strider(LLStrider<LLVector2>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector2,TYPE_TEXCOORD1>::get(*this, strider, index, count, map_range);
}

bool LLVertexBuffer::getNormalStrider(LLStrider<LLVector3>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector3,TYPE_NORMAL>::get(*this, strider, index, count, map_range);
}
bool LLVertexBuffer::getBinormalStrider(LLStrider<LLVector3>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector3,TYPE_BINORMAL>::get(*this, strider, index, count, map_range);
}
bool LLVertexBuffer::getColorStrider(LLStrider<LLColor4U>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLColor4U,TYPE_COLOR>::get(*this, strider, index, count, map_range);
}
bool LLVertexBuffer::getEmissiveStrider(LLStrider<LLColor4U>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLColor4U,TYPE_EMISSIVE>::get(*this, strider, index, count, map_range);
}
bool LLVertexBuffer::getWeightStrider(LLStrider<F32>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<F32,TYPE_WEIGHT>::get(*this, strider, index, count, map_range);
}

bool LLVertexBuffer::getWeight4Strider(LLStrider<LLVector4>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector4,TYPE_WEIGHT4>::get(*this, strider, index, count, map_range);
}

bool LLVertexBuffer::getClothWeightStrider(LLStrider<LLVector4>& strider, S32 index, S32 count, bool map_range)
{
	return VertexBufferStrider<LLVector4,TYPE_CLOTHWEIGHT>::get(*this, strider, index, count, map_range);
}

//----------------------------------------------------------------------------

//static LLFastTimer::DeclareTimer FTM_BIND_GL_ARRAY("Bind Array");
bool LLVertexBuffer::bindGLArray()
{
	if (mGLArray && sGLRenderArray != mGLArray)
	{
		{
			//LLFastTimer t(FTM_BIND_GL_ARRAY);
#if GL_ARB_vertex_array_object
			glBindVertexArray(mGLArray);
#endif
			sGLRenderArray = mGLArray;
		}

		//really shouldn't be necessary, but some drivers don't properly restore the
		//state of GL_ELEMENT_ARRAY_BUFFER_BINDING
		bindGLIndices();
		
		return true;
	}
		
	return false;
}

//static LLFastTimer::DeclareTimer FTM_BIND_GL_BUFFER("Bind Buffer");

bool LLVertexBuffer::bindGLBuffer(bool force_bind)
{
	bindGLArray();

	bool ret = false;

	if (useVBOs() && (force_bind || (mGLBuffer && (mGLBuffer != sGLRenderBuffer || !sVBOActive))))
	{
		//LLFastTimer t(FTM_BIND_GL_BUFFER);
		/*if (sMapped)
		{
			llerrs << "VBO bound while another VBO mapped!" << llendl;
		}*/
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, mGLBuffer);
		sGLRenderBuffer = mGLBuffer;
		sBindCount++;
		sVBOActive = TRUE;

		if (mGLArray)
		{
			llassert(sGLRenderArray == mGLArray);
			//mCachedRenderBuffer = mGLBuffer;
		}

		ret = true;
	}

	return ret;
}

//static LLFastTimer::DeclareTimer FTM_BIND_GL_INDICES("Bind Indices");

bool LLVertexBuffer::bindGLIndices(bool force_bind)
{
	bindGLArray();

	bool ret = false;
	if (useVBOs() && (force_bind || (mGLIndices && (mGLIndices != sGLRenderIndices || !sIBOActive))))
	{
		//LLFastTimer t(FTM_BIND_GL_INDICES);
		/*if (sMapped)
		{
			llerrs << "VBO bound while another VBO mapped!" << llendl;
		}*/
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, mGLIndices);
		sGLRenderIndices = mGLIndices;
		stop_glerror();
		sBindCount++;
		sIBOActive = TRUE;
		ret = true;
	}

	return ret;
}

void LLVertexBuffer::flush()
{
	if (useVBOs())
	{
		unmapBuffer();
	}
}

// Set for rendering
void LLVertexBuffer::setBuffer(U32 data_mask)
{
	flush();

	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	//set up pointers if the data mask is different ...
	BOOL setup = (sLastMask != data_mask);

	if (gDebugGL && data_mask != 0)
	{ //make sure data requirements are fulfilled
		LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
		if (shader)
		{
			U32 required_mask = 0;
			for (U32 i = 0; i < LLVertexBuffer::TYPE_TEXTURE_INDEX; ++i)
			{
				if (shader->getAttribLocation(i) > -1)
				{
					U32 required = 1 << i;
					if ((data_mask & required) == 0)
					{
						llwarns << "Missing attribute: " << LLShaderMgr::instance()->mReservedAttribs[i] << llendl;
					}

					required_mask |= required;
				}
			}

			if ((data_mask & required_mask) != required_mask)
			{
				llerrs << "Shader consumption mismatches data provision." << llendl;
			}
		}
	}

	if (useVBOs())
	{
		if (mGLArray)
		{
			bindGLArray();
			setup = FALSE; //do NOT perform pointer setup if using VAO
		}
		else
		{
			if (bindGLBuffer())
			{
				setup = TRUE;
			}
			if (bindGLIndices())
			{
				setup = TRUE;
			}
		}

		BOOL error = FALSE;
		if (gDebugGL && !mGLArray)
		{
			GLint buff;
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING_ARB, &buff);
			if ((GLuint)buff != mGLBuffer)
			{
				if (gDebugSession)
				{
					error = TRUE;
					gFailLog << "Invalid GL vertex buffer bound: " << buff << std::endl;
				}
				else
				{
					llerrs << "Invalid GL vertex buffer bound: " << buff << llendl;
				}
			}

			if (mGLIndices)
			{
				glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB, &buff);
				if ((GLuint)buff != mGLIndices)
				{
					if (gDebugSession)
					{
						error = TRUE;
						gFailLog << "Invalid GL index buffer bound: " << buff <<  std::endl;
					}
					else
					{
						llerrs << "Invalid GL index buffer bound: " << buff << llendl;
					}
				}
			}
		}

		
	}
	else
	{	
		if (sGLRenderArray)
		{
#if GL_ARB_vertex_array_object
			glBindVertexArray(0);
#endif
			sGLRenderArray = 0;
			sGLRenderIndices = 0;
			sIBOActive = FALSE;
		}

		if (mGLBuffer)
		{
			if (sVBOActive)
			{
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
				sBindCount++;
				sVBOActive = FALSE;
				setup = TRUE; // ... or a VBO is deactivated
			}
			if (sGLRenderBuffer != mGLBuffer)
			{
				sGLRenderBuffer = mGLBuffer;
				setup = TRUE; // ... or a client memory pointer changed
			}
		}
		if (mGLIndices)
		{
			if (sIBOActive)
			{
				glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
				sBindCount++;
				sIBOActive = FALSE;
			}
			
			sGLRenderIndices = mGLIndices;
		}
	}

	if (!mGLArray)
	{
		setupClientArrays(data_mask);
	}
			
	if (mGLBuffer)
	{
		if (data_mask && setup)
		{
			setupVertexBuffer(data_mask); // subclass specific setup (virtual function)
			sSetCount++;
		}
	}
}

// virtual (default)
void LLVertexBuffer::setupVertexBuffer(U32 data_mask)
{
	LLMemType mt(LLMemType::MTYPE_VERTEX_DATA);
	stop_glerror();
	volatile U8* base = useVBOs() ? (U8*) mAlignedOffset : mMappedData;

	/*if ((data_mask & mTypeMask) != data_mask)
	{
		llerrs << "LLVertexBuffer::setupVertexBuffer missing required components for supplied data mask." << llendl;
	}*/

	if (LLGLSLShader::sNoFixedFunction)
	{
		if (data_mask & MAP_NORMAL)
		{
			S32 loc = TYPE_NORMAL;
			void* ptr = (void*)(base + mOffsets[TYPE_NORMAL]);
			glVertexAttribPointerARB(loc, 3, GL_FLOAT, GL_FALSE, LLVertexBuffer::sTypeSize[TYPE_NORMAL], ptr);
		}
		if (data_mask & MAP_TEXCOORD3)
		{
			S32 loc = TYPE_TEXCOORD3;
			void* ptr = (void*)(base + mOffsets[TYPE_TEXCOORD3]);
			glVertexAttribPointerARB(loc,2,GL_FLOAT, GL_FALSE, LLVertexBuffer::sTypeSize[TYPE_TEXCOORD3], ptr);
		}
		if (data_mask & MAP_TEXCOORD2)
		{
			S32 loc = TYPE_TEXCOORD2;
			void* ptr = (void*)(base + mOffsets[TYPE_TEXCOORD2]);
			glVertexAttribPointerARB(loc,2,GL_FLOAT, GL_FALSE, LLVertexBuffer::sTypeSize[TYPE_TEXCOORD2], ptr);
		}
		if (data_mask & MAP_TEXCOORD1)
		{
			S32 loc = TYPE_TEXCOORD1;
			void* ptr = (void*)(base + mOffsets[TYPE_TEXCOORD1]);
			glVertexAttribPointerARB(loc,2,GL_FLOAT, GL_FALSE, LLVertexBuffer::sTypeSize[TYPE_TEXCOORD1], ptr);
		}
		if (data_mask & MAP_BINORMAL)
		{
			S32 loc = TYPE_BINORMAL;
			void* ptr = (void*)(base + mOffsets[TYPE_BINORMAL]);
			glVertexAttribPointerARB(loc, 3,GL_FLOAT, GL_FALSE, LLVertexBuffer::sTypeSize[TYPE_BINORMAL], ptr);
		}
		if (data_mask & MAP_TEXCOORD0)
		{
			S32 loc = TYPE_TEXCOORD0;
			void* ptr = (void*)(base + mOffsets[TYPE_TEXCOORD0]);
			glVertexAttribPointerARB(loc,2,GL_FLOAT, GL_FALSE, LLVertexBuffer::sTypeSize[TYPE_TEXCOORD0], ptr);
		}
		if (data_mask & MAP_COLOR)
		{
			S32 loc = TYPE_COLOR;
			void* ptr = (void*)(base + mOffsets[TYPE_COLOR]);
			glVertexAttribPointerARB(loc, 4, GL_UNSIGNED_BYTE, GL_TRUE, LLVertexBuffer::sTypeSize[TYPE_COLOR], ptr);
		}
		if (data_mask & MAP_EMISSIVE)
		{
			S32 loc = TYPE_EMISSIVE;
			void* ptr = (void*)(base + mOffsets[TYPE_EMISSIVE]);
			glVertexAttribPointerARB(loc, 4, GL_UNSIGNED_BYTE, GL_TRUE, LLVertexBuffer::sTypeSize[TYPE_EMISSIVE], ptr);
		}
		if (data_mask & MAP_WEIGHT)
		{
			S32 loc = TYPE_WEIGHT;
			void* ptr = (void*)(base + mOffsets[TYPE_WEIGHT]);
			glVertexAttribPointerARB(loc, 1, GL_FLOAT, FALSE, LLVertexBuffer::sTypeSize[TYPE_WEIGHT], ptr);
		}
		if (data_mask & MAP_WEIGHT4)
		{
			S32 loc = TYPE_WEIGHT4;
			void* ptr = (void*)(base+mOffsets[TYPE_WEIGHT4]);
			glVertexAttribPointerARB(loc, 4, GL_FLOAT, FALSE, LLVertexBuffer::sTypeSize[TYPE_WEIGHT4], ptr);
		}
		if (data_mask & MAP_CLOTHWEIGHT)
		{
			S32 loc = TYPE_CLOTHWEIGHT;
			void* ptr = (void*)(base + mOffsets[TYPE_CLOTHWEIGHT]);
			glVertexAttribPointerARB(loc, 4, GL_FLOAT, TRUE,  LLVertexBuffer::sTypeSize[TYPE_CLOTHWEIGHT], ptr);
		}
		if (data_mask & MAP_TEXTURE_INDEX)
		{
			S32 loc = TYPE_TEXTURE_INDEX;
			void *ptr = (void*) (base + mOffsets[TYPE_VERTEX] + 12);
			glVertexAttribPointerARB(loc, 1, GL_FLOAT, GL_FALSE, LLVertexBuffer::sTypeSize[TYPE_VERTEX], ptr);
		}
		if (data_mask & MAP_VERTEX)
		{
			S32 loc = TYPE_VERTEX;
			void* ptr = (void*)(base + mOffsets[TYPE_VERTEX]);
			glVertexAttribPointerARB(loc, 3,GL_FLOAT, GL_FALSE, LLVertexBuffer::sTypeSize[TYPE_VERTEX], ptr);
		}	
	}	
	else
	{
		if (data_mask & MAP_NORMAL)
		{
			glNormalPointer(GL_FLOAT, LLVertexBuffer::sTypeSize[TYPE_NORMAL], (void*)(base + mOffsets[TYPE_NORMAL]));
		}
		if (data_mask & MAP_TEXCOORD3)
		{
			glClientActiveTextureARB(GL_TEXTURE3_ARB);
			glTexCoordPointer(2,GL_FLOAT, LLVertexBuffer::sTypeSize[TYPE_TEXCOORD3], (void*)(base + mOffsets[TYPE_TEXCOORD3]));
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		if (data_mask & MAP_TEXCOORD2)
		{
			glClientActiveTextureARB(GL_TEXTURE2_ARB);
			glTexCoordPointer(2,GL_FLOAT, LLVertexBuffer::sTypeSize[TYPE_TEXCOORD2], (void*)(base + mOffsets[TYPE_TEXCOORD2]));
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		if (data_mask & MAP_TEXCOORD1)
		{
			glClientActiveTextureARB(GL_TEXTURE1_ARB);
			glTexCoordPointer(2,GL_FLOAT, LLVertexBuffer::sTypeSize[TYPE_TEXCOORD1], (void*)(base + mOffsets[TYPE_TEXCOORD1]));
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		if (data_mask & MAP_BINORMAL)
		{
			glClientActiveTextureARB(GL_TEXTURE2_ARB);
			glTexCoordPointer(3,GL_FLOAT, LLVertexBuffer::sTypeSize[TYPE_BINORMAL], (void*)(base + mOffsets[TYPE_BINORMAL]));
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		if (data_mask & MAP_TEXCOORD0)
		{
			glTexCoordPointer(2,GL_FLOAT, LLVertexBuffer::sTypeSize[TYPE_TEXCOORD0], (void*)(base + mOffsets[TYPE_TEXCOORD0]));
		}
		if (data_mask & MAP_COLOR)
		{
			glColorPointer(4, GL_UNSIGNED_BYTE, LLVertexBuffer::sTypeSize[TYPE_COLOR], (void*)(base + mOffsets[TYPE_COLOR]));
		}
		if (data_mask & MAP_VERTEX)
		{
			glVertexPointer(3,GL_FLOAT, LLVertexBuffer::sTypeSize[TYPE_VERTEX], (void*)(base + 0));
		}	
	}

	llglassertok();
}

LLVertexBuffer::MappedRegion::MappedRegion(S32 type, S32 index, S32 count)
: mType(type), mIndex(index), mCount(count)
{ 
	llassert(mType == LLVertexBuffer::TYPE_INDEX || 
			mType < LLVertexBuffer::TYPE_TEXTURE_INDEX);
}	


