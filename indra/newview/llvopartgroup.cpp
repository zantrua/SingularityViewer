/** 
 * @file llvopartgroup.cpp
 * @brief Group of particle systems
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llvopartgroup.h"

#include "lldrawpoolalpha.h"

#include "llfasttimer.h"
#include "message.h"
#include "v2math.h"

#include "llagentcamera.h"
#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewerpartsim.h"
#include "llviewerregion.h"
#include "pipeline.h"
#include "llspatialpartition.h"

const F32 MAX_PART_LIFETIME = 120.f;

extern U64 gFrameTime;

LLVOPartGroup::LLVOPartGroup(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp)
	:	LLAlphaObject(id, pcode, regionp),
		mViewerPartGroupp(NULL)
{
	setNumTEs(1);
	setTETexture(0, LLUUID::null);
	mbCanSelect = FALSE;			// users can't select particle systems
}


LLVOPartGroup::~LLVOPartGroup()
{
}


BOOL LLVOPartGroup::isActive() const
{
	return FALSE;
}

F32 LLVOPartGroup::getBinRadius()
{ 
	return mScale.mV[0]*2.f;
}

void LLVOPartGroup::updateSpatialExtents(LLVector4a& newMin, LLVector4a& newMax)
{		
	const LLVector3& pos_agent = getPositionAgent();
	newMin.load3( (pos_agent - mScale).mV);
	newMax.load3( (pos_agent + mScale).mV);
	LLVector4a pos;
	pos.load3(pos_agent.mV);
	mDrawable->setPositionGroup(pos);
}

BOOL LLVOPartGroup::idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time)
{
	return TRUE;
}

void LLVOPartGroup::setPixelAreaAndAngle(LLAgent &agent)
{
	// mPixelArea is calculated during render
	F32 mid_scale = getMidScale();
	F32 range = (getRenderPosition()-LLViewerCamera::getInstance()->getOrigin()).length();

	if (range < 0.001f || isHUDAttachment())		// range == zero
	{
		mAppAngle = 180.f;
	}
	else
	{
		mAppAngle = (F32) atan2( mid_scale, range) * RAD_TO_DEG;
	}
}

void LLVOPartGroup::updateTextures()
{
	// Texture stats for particles need to be updated in a different way...
}


LLDrawable* LLVOPartGroup::createDrawable(LLPipeline *pipeline)
{
	pipeline->allocDrawable(this);
	mDrawable->setLit(FALSE);
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_PARTICLES);
	return mDrawable;
}

 const F32 MAX_PARTICLE_AREA_SCALE = 0.02f; // some tuned constant, limits on how much particle area to draw

F32 LLVOPartGroup::getPartSize(S32 idx)
{
	if (idx < (S32) mViewerPartGroupp->mParticles.size())
	{
		return mViewerPartGroupp->mParticles[idx]->mScale.mV[0];
	}

	return 0.f;
}

LLVector3 LLVOPartGroup::getCameraPosition() const
{
	return gAgentCamera.getCameraPositionAgent();
}

BOOL LLVOPartGroup::updateGeometry(LLDrawable *drawable)
{
	LLFastTimer ftm(LLFastTimer::FTM_UPDATE_PARTICLES);

	dirtySpatialGroup();
	
	S32 num_parts = mViewerPartGroupp->getCount();
	LLFace *facep;
	LLSpatialGroup* group = drawable->getSpatialGroup();
	if (!group && num_parts)
	{
		drawable->movePartition();
		group = drawable->getSpatialGroup();
	}

	if (group && group->isVisible())
	{
		dirtySpatialGroup(TRUE);
	}

	if (!num_parts)
	{
		if (group && drawable->getNumFaces())
		{
			group->setState(LLSpatialGroup::GEOM_DIRTY);
		}
		drawable->setNumFaces(0, NULL, getTEImage(0));
		LLPipeline::sCompiles++;
		return TRUE;
	}

 	if (!(gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES)))
	{
		return TRUE;
	}

	if (num_parts > drawable->getNumFaces())
	{
		drawable->setNumFacesFast(num_parts+num_parts/4, NULL, getTEImage(0));
	}

	F32 tot_area = 0;

	F32 max_area = LLViewerPartSim::getMaxPartCount() * MAX_PARTICLE_AREA_SCALE; 
	F32 pixel_meter_ratio = LLViewerCamera::getInstance()->getPixelMeterRatio();
	pixel_meter_ratio *= pixel_meter_ratio;

	LLViewerPartSim::checkParticleCount(mViewerPartGroupp->mParticles.size()) ;

	S32 count=0;
	mDepth = 0.f;
	S32 i = 0 ;
	LLVector3 camera_agent = getCameraPosition();
	for (i = 0 ; i < (S32)mViewerPartGroupp->mParticles.size(); i++)
	{
		const LLViewerPart *part = mViewerPartGroupp->mParticles[i];

		LLVector3 part_pos_agent(part->mPosAgent);
		LLVector3 at(part_pos_agent - camera_agent);

		F32 camera_dist_squared = at.lengthSquared();
		F32 inv_camera_dist_squared;
		if (camera_dist_squared > 1.f)
			inv_camera_dist_squared = 1.f / camera_dist_squared;
		else
			inv_camera_dist_squared = 1.f;
		F32 area = part->mScale.mV[0] * part->mScale.mV[1] * inv_camera_dist_squared;
		tot_area = llmax(tot_area, area);
 		
		if (tot_area > max_area)
		{
			break;
		}
	
		count++;

		facep = drawable->getFace(i);
		if (!facep)
		{
			llwarns << "No face found for index " << i << "!" << llendl;
			continue;
		}

		facep->setTEOffset(i);
		const F32 NEAR_PART_DIST_SQ = 5.f*5.f;  // Only discard particles > 5 m from the camera
		const F32 MIN_PART_AREA = .005f*.005f;  // only less than 5 mm x 5 mm at 1 m from camera
		
		if (camera_dist_squared > NEAR_PART_DIST_SQ && area < MIN_PART_AREA)
		{
			facep->setSize(0, 0);
			continue;
		}

		facep->setSize(4, 6);
		
		facep->setViewerObject(this);

		if (part->mFlags & LLPartData::LL_PART_EMISSIVE_MASK)
		{
			facep->setState(LLFace::FULLBRIGHT);
		}
		else
		{
			facep->clearState(LLFace::FULLBRIGHT);
		}

		facep->mCenterLocal = part->mPosAgent;
		facep->setFaceColor(part->mColor);
		facep->setTexture(part->mImagep);

		mPixelArea = tot_area * pixel_meter_ratio;
		const F32 area_scale = 10.f; // scale area to increase priority a bit
		facep->setVirtualSize(mPixelArea*area_scale);
	}
	for (i = count; i < drawable->getNumFaces(); i++)
	{
		LLFace* facep = drawable->getFace(i);
		if (!facep)
		{
			llwarns << "No face found for index " << i << "!" << llendl;
			continue;
		}
		facep->setTEOffset(i);
		facep->setSize(0, 0);
	}

	mDrawable->movePartition();
	LLPipeline::sCompiles++;
	return TRUE;
}

void LLVOPartGroup::getGeometry(S32 idx,
								LLStrider<LLVector4a>& verticesp,
								LLStrider<LLVector3>& normalsp, 
								LLStrider<LLVector2>& texcoordsp,
								LLStrider<LLColor4U>& colorsp, 
								LLStrider<U16>& indicesp)
{
	if (idx >= (S32) mViewerPartGroupp->mParticles.size())
	{
		return;
	}

	const LLViewerPart &part = *((LLViewerPart*) (mViewerPartGroupp->mParticles[idx]));

	U32 vert_offset = mDrawable->getFace(idx)->getGeomIndex();

	
	LLVector4a part_pos_agent;
	part_pos_agent.load3(part.mPosAgent.mV);
	LLVector4a camera_agent;
	camera_agent.load3(getCameraPosition().mV); 
	LLVector4a at;
	at.setSub(part_pos_agent, camera_agent);
	LLVector4a up(0, 0, 1);
	LLVector4a right;

	right.setCross3(at, up);
	right.normalize3fast();
	up.setCross3(right, at);
	up.normalize3fast();

	if (part.mFlags & LLPartData::LL_PART_FOLLOW_VELOCITY_MASK)
	{
		LLVector4a normvel;
		normvel.load3(part.mVelocity.mV);
		normvel.normalize3fast();
		LLVector2 up_fracs;
		up_fracs.mV[0] = normvel.dot3(right).getF32();
		up_fracs.mV[1] = normvel.dot3(up).getF32();
		up_fracs.normalize();
		LLVector4a new_up;
		LLVector4a new_right;

		//new_up = up_fracs.mV[0] * right + up_fracs.mV[1]*up;
		LLVector4a t = right;
		t.mul(up_fracs.mV[0]);
		new_up = up;
		new_up.mul(up_fracs.mV[1]);
		new_up.add(t);

		//new_right = up_fracs.mV[1] * right - up_fracs.mV[0]*up;
		t = right;
		t.mul(up_fracs.mV[1]);
		new_right = up;
		new_right.mul(up_fracs.mV[0]);
		t.sub(new_right);

		up = new_up;
		right = t;
		up.normalize3fast();
		right.normalize3fast();
	}

	right.mul(0.5f*part.mScale.mV[0]);
	up.mul(0.5f*part.mScale.mV[1]);


	LLVector3 normal = -LLViewerCamera::getInstance()->getXAxis();

	//HACK -- the verticesp->mV[3] = 0.f here are to set the texture index to 0 (particles don't use texture batching, maybe they should)
	// this works because there is actually a 4th float stored after the vertex position which is used as a texture index
	// also, somebody please VECTORIZE THIS

	LLVector4a ppapu;
	LLVector4a ppamu;

	ppapu.setAdd(part_pos_agent, up);
	ppamu.setSub(part_pos_agent, up);

	verticesp->setSub(ppapu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setSub(ppamu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setAdd(ppapu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;
	verticesp->setAdd(ppamu, right);
	(*verticesp++).getF32ptr()[3] = 0.f;

	// *verticesp++ = part_pos_agent + up - right;
	// *verticesp++ = part_pos_agent - up - right;
	// *verticesp++ = part_pos_agent + up + right;
	// *verticesp++ = part_pos_agent - up + right;

	*colorsp++ = part.mColor;
	*colorsp++ = part.mColor;
	*colorsp++ = part.mColor;
	*colorsp++ = part.mColor;

	*texcoordsp++ = LLVector2(0.f, 1.f);
	*texcoordsp++ = LLVector2(0.f, 0.f);
	*texcoordsp++ = LLVector2(1.f, 1.f);
	*texcoordsp++ = LLVector2(1.f, 0.f);

	*normalsp++   = normal;
	*normalsp++   = normal;
	*normalsp++   = normal;
	*normalsp++   = normal;

	*indicesp++ = vert_offset + 0;
	*indicesp++ = vert_offset + 1;
	*indicesp++ = vert_offset + 2;

	*indicesp++ = vert_offset + 1;
	*indicesp++ = vert_offset + 3;
	*indicesp++ = vert_offset + 2;
}

U32 LLVOPartGroup::getPartitionType() const
{ 
	return LLViewerRegion::PARTITION_PARTICLE; 
}

LLParticlePartition::LLParticlePartition()
: LLSpatialPartition(LLDrawPoolAlpha::VERTEX_DATA_MASK | LLVertexBuffer::MAP_TEXTURE_INDEX, TRUE, GL_STREAM_DRAW_ARB)
{
	mRenderPass = LLRenderPass::PASS_ALPHA;
	mDrawableType = LLPipeline::RENDER_TYPE_PARTICLES;
	mPartitionType = LLViewerRegion::PARTITION_PARTICLE;
	mSlopRatio = 0.f;
	mLODPeriod = 1;
}

LLHUDParticlePartition::LLHUDParticlePartition() :
	LLParticlePartition()
{
	mDrawableType = LLPipeline::RENDER_TYPE_HUD_PARTICLES;
	mPartitionType = LLViewerRegion::PARTITION_HUD_PARTICLE;
}

void LLParticlePartition::addGeometryCount(LLSpatialGroup* group, U32& vertex_count, U32& index_count)
{
	group->mBufferUsage = mBufferUsage;

	mFaceList.clear();

	LLViewerCamera* camera = LLViewerCamera::getInstance();
	for (LLSpatialGroup::element_iter i = group->getData().begin(); i != group->getData().end(); ++i)
	{
		LLDrawable* drawablep = *i;
		
		if (drawablep->isDead())
		{
			continue;
		}

		LLAlphaObject* obj = (LLAlphaObject*) drawablep->getVObj().get();
		obj->mDepth = 0.f;
		
		if (drawablep->isAnimating())
		{
			group->mBufferUsage = GL_STREAM_DRAW_ARB;
		}

		U32 count = 0;
		for (S32 j = 0; j < drawablep->getNumFaces(); ++j)
		{
			drawablep->updateFaceSize(j);

			LLFace* facep = drawablep->getFace(j);
			if ( !facep || !facep->hasGeometry())
			{
				continue;
			}
			
			count++;
			facep->mDistance = (facep->mCenterLocal - camera->getOrigin()) * camera->getAtAxis();
			obj->mDepth += facep->mDistance;
			
			mFaceList.push_back(facep);
			vertex_count += facep->getGeomCount();
			index_count += facep->getIndicesCount();
			llassert(facep->getIndicesCount() < 65536);
		}
		
		obj->mDepth /= count;
	}
}

void LLParticlePartition::getGeometry(LLSpatialGroup* group)
{
	LLMemType mt(LLMemType::MTYPE_SPACE_PARTITION);
	LLFastTimer ftm(mDrawableType == LLPipeline::RENDER_TYPE_GRASS ?
					LLFastTimer::FTM_REBUILD_GRASS_VB :
					LLFastTimer::FTM_REBUILD_PARTICLE_VB);

	std::sort(mFaceList.begin(), mFaceList.end(), LLFace::CompareDistanceGreater());

	U32 index_count = 0;
	U32 vertex_count = 0;

	group->clearDrawMap();

	LLVertexBuffer* buffer = group->mVertexBuffer;

	LLStrider<U16> indicesp;
	LLStrider<LLVector4a> verticesp;
	LLStrider<LLVector3> normalsp;
	LLStrider<LLVector2> texcoordsp;
	LLStrider<LLColor4U> colorsp;

	buffer->getVertexStrider(verticesp);
	buffer->getNormalStrider(normalsp);
	buffer->getColorStrider(colorsp);
	buffer->getTexCoord0Strider(texcoordsp);
	buffer->getIndexStrider(indicesp);

	LLSpatialGroup::drawmap_elem_t& draw_vec = group->mDrawMap[mRenderPass];	

	for (std::vector<LLFace*>::iterator i = mFaceList.begin(); i != mFaceList.end(); ++i)
	{
		LLFace* facep = *i;
		LLAlphaObject* object = (LLAlphaObject*) facep->getViewerObject();
		facep->setGeomIndex(vertex_count);
		facep->setIndicesIndex(index_count);
		facep->setVertexBuffer(buffer);
		facep->setPoolType(LLDrawPool::POOL_ALPHA);
		object->getGeometry(facep->getTEOffset(), verticesp, normalsp, texcoordsp, colorsp, indicesp);
		
		vertex_count += facep->getGeomCount();
		index_count += facep->getIndicesCount();

		S32 idx = draw_vec.size()-1;

		BOOL fullbright = facep->isState(LLFace::FULLBRIGHT);
		F32 vsize = facep->getVirtualSize();

		if (idx >= 0 && draw_vec[idx]->mEnd == facep->getGeomIndex()-1 &&
			draw_vec[idx]->mTexture == facep->getTexture() &&
			(U16) (draw_vec[idx]->mEnd - draw_vec[idx]->mStart + facep->getGeomCount()) <= (U32) gGLManager.mGLMaxVertexRange &&
			//draw_vec[idx]->mCount + facep->getIndicesCount() <= (U32) gGLManager.mGLMaxIndexRange &&
			draw_vec[idx]->mEnd - draw_vec[idx]->mStart + facep->getGeomCount() < 4096 &&
			draw_vec[idx]->mFullbright == fullbright)
		{
			draw_vec[idx]->mCount += facep->getIndicesCount();
			draw_vec[idx]->mEnd += facep->getGeomCount();
			draw_vec[idx]->mVSize = llmax(draw_vec[idx]->mVSize, vsize);
		}
		else
		{
			U32 start = facep->getGeomIndex();
			U32 end = start + facep->getGeomCount()-1;
			U32 offset = facep->getIndicesStart();
			U32 count = facep->getIndicesCount();
			LLDrawInfo* info = new LLDrawInfo(start,end,count,offset,facep->getTexture(),
				//facep->getTexture(),
				buffer, fullbright);
			info->mExtents[0] = group->mObjectExtents[0];
			info->mExtents[1] = group->mObjectExtents[1];
			info->mVSize = vsize;
			draw_vec.push_back(info);
			//for alpha sorting
			facep->setDrawInfo(info);
		}
	}

	buffer->flush();
	mFaceList.clear();
}

F32 LLParticlePartition::calcPixelArea(LLSpatialGroup* group, LLCamera& camera)
{
	return 1024.f;
}

U32 LLVOHUDPartGroup::getPartitionType() const
{
	return LLViewerRegion::PARTITION_HUD_PARTICLE; 
}

LLDrawable* LLVOHUDPartGroup::createDrawable(LLPipeline *pipeline)
{
	pipeline->allocDrawable(this);
	mDrawable->setLit(FALSE);
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
	return mDrawable;
}

LLVector3 LLVOHUDPartGroup::getCameraPosition() const
{
	return LLVector3(-1,0,0);
}

