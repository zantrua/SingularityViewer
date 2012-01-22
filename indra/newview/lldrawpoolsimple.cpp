/** 
 * @file lldrawpoolsimple.cpp
 * @brief LLDrawPoolSimple class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "lldrawpoolsimple.h"

#include "llviewercamera.h"
#include "llagent.h"
#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llviewershadermgr.h"
#include "llrender.h"


static LLGLSLShader* simple_shader = NULL;
static LLGLSLShader* fullbright_shader = NULL;

void LLDrawPoolGlow::beginPostDeferredPass(S32 pass)
{
	gDeferredEmissiveProgram.bind();
}

void LLDrawPoolGlow::renderPostDeferred(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_GLOW);
	LLGLEnable blend(GL_BLEND);
	LLGLDisable test(GL_ALPHA_TEST);
	gGL.flush();
	/// Get rid of z-fighting with non-glow pass.
	LLGLEnable polyOffset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -1.0f);
	gGL.setSceneBlendType(LLRender::BT_ADD);
	
	LLGLDepthTest depth(GL_TRUE, GL_FALSE);
	gGL.setColorMask(false, true);

	{
		//LLFastTimer t(FTM_RENDER_GLOW_PUSH);
		pushBatches(LLRenderPass::PASS_GLOW, getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX, TRUE, TRUE);
	}
	
	gGL.setColorMask(true, false);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);	
}

void LLDrawPoolGlow::endPostDeferredPass(S32 pass)
{
	gDeferredEmissiveProgram.unbind();
	LLRenderPass::endRenderPass(pass);
}

S32 LLDrawPoolGlow::getNumPasses()
{
	if (LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void LLDrawPoolGlow::render(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_GLOW);
	LLGLEnable blend(GL_BLEND);
	LLGLDisable test(GL_ALPHA_TEST);
	gGL.flush();
	/// Get rid of z-fighting with non-glow pass.
	LLGLEnable polyOffset(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -1.0f);
	gGL.setSceneBlendType(LLRender::BT_ADD);
	
	U32 shader_level = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);

	//should never get here without basic shaders enabled
	llassert(shader_level > 0);
	
	LLGLSLShader* shader = LLPipeline::sUnderWaterRender ? &gObjectEmissiveWaterProgram : &gObjectEmissiveProgram;
	shader->bind();

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);
	gGL.setColorMask(false, true);

	pushBatches(LLRenderPass::PASS_GLOW, getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX, TRUE, TRUE);
	
	gGL.setColorMask(true, false);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	
	if (shader_level > 0 && fullbright_shader)
	{
		shader->unbind();
	}
}

void LLDrawPoolGlow::pushBatch(LLDrawInfo& params, U32 mask, BOOL texture, BOOL batch_textures)
{
	//gGL.diffuseColor4ubv(params.mGlowColor.mV);
	LLRenderPass::pushBatch(params, mask, texture, batch_textures);
}


LLDrawPoolSimple::LLDrawPoolSimple() :
	LLRenderPass(POOL_SIMPLE)
{
}

void LLDrawPoolSimple::prerender()
{
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolSimple::beginRenderPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_SIMPLE);

	if (LLPipeline::sUnderWaterRender)
	{
		simple_shader = &gObjectSimpleWaterProgram;
	}
	else
	{
		simple_shader = &gObjectSimpleProgram;
	}

	if (mVertexShaderLevel > 0)
	{
		simple_shader->bind();
	}
	else 
	{
		// don't use shaders!
		if (gGLManager.mHasShaderObjects)
		{
			LLGLSLShader::bindNoShader();
		}		
	}
}

void LLDrawPoolSimple::endRenderPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_SIMPLE);
	stop_glerror();
	LLRenderPass::endRenderPass(pass);
	stop_glerror();
	if (mVertexShaderLevel > 0)
	{
		simple_shader->unbind();
	}
}

void LLDrawPoolSimple::render(S32 pass)
{
	LLGLDisable blend(GL_BLEND);
	
	{ //render simple
		LLFastTimer t(LLFastTimer::FTM_RENDER_SIMPLE);
		gPipeline.enableLightsDynamic();

		if (mVertexShaderLevel > 0)
		{
			U32 mask = getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX;

			pushBatches(LLRenderPass::PASS_SIMPLE, mask, TRUE, TRUE);

			if (LLPipeline::sRenderDeferred)
			{ //if deferred rendering is enabled, bump faces aren't registered as simple
				//render bump faces here as simple so bump faces will appear under water
				pushBatches(LLRenderPass::PASS_BUMP, mask, TRUE, TRUE);
			}
		}
		else
		{
			LLGLDisable alpha_test(GL_ALPHA_TEST);
			renderTexture(LLRenderPass::PASS_SIMPLE, getVertexDataMask());
		}
		
	}
}

//===============================
//DEFERRED IMPLEMENTATION
//===============================

void LLDrawPoolSimple::beginDeferredPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_SIMPLE);
	gDeferredDiffuseProgram.bind();
}

void LLDrawPoolSimple::endDeferredPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_SIMPLE);
	LLRenderPass::endRenderPass(pass);

	gDeferredDiffuseProgram.unbind();
}

void LLDrawPoolSimple::renderDeferred(S32 pass)
{
	LLGLDisable blend(GL_BLEND);
	LLGLDisable alpha_test(GL_ALPHA_TEST);

	{ //render simple
		LLFastTimer t(LLFastTimer::FTM_RENDER_SIMPLE);
		pushBatches(LLRenderPass::PASS_SIMPLE, getVertexDataMask() | LLVertexBuffer::MAP_TEXTURE_INDEX, TRUE, TRUE);
	}
}

// grass drawpool
LLDrawPoolGrass::LLDrawPoolGrass() :
 LLRenderPass(POOL_GRASS)
{

}

void LLDrawPoolGrass::prerender()
{
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}


void LLDrawPoolGrass::beginRenderPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_GRASS);
	stop_glerror();

	if (LLPipeline::sUnderWaterRender)
	{
		simple_shader = &gObjectAlphaMaskNonIndexedWaterProgram;
	}
	else
	{
		simple_shader = &gObjectAlphaMaskNonIndexedProgram;
	}

	if (mVertexShaderLevel > 0)
	{
		simple_shader->bind();
		simple_shader->setMinimumAlpha(0.5f);
	}
	else 
	{
		gGL.setAlphaRejectSettings(LLRender::CF_GREATER, 0.5f);
		// don't use shaders!
		if (gGLManager.mHasShaderObjects)
		{
			LLGLSLShader::bindNoShader();
		}		
	}
}

void LLDrawPoolGrass::endRenderPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_GRASS);
	LLRenderPass::endRenderPass(pass);

	if (mVertexShaderLevel > 0)
	{
		simple_shader->unbind();
	}
	else
	{
		gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
	}
}

void LLDrawPoolGrass::render(S32 pass)
{
	LLGLDisable blend(GL_BLEND);
	
	{
		LLFastTimer t(LLFastTimer::FTM_RENDER_GRASS);
		LLGLEnable test(GL_ALPHA_TEST);
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
		//render grass
		LLRenderPass::renderTexture(LLRenderPass::PASS_GRASS, getVertexDataMask());
	}
}

void LLDrawPoolGrass::beginDeferredPass(S32 pass)
{

}

void LLDrawPoolGrass::endDeferredPass(S32 pass)
{

}

void LLDrawPoolGrass::renderDeferred(S32 pass)
{
	{
		LLFastTimer t(LLFastTimer::FTM_RENDER_GRASS);
		//LLFastTimer t(FTM_RENDER_GRASS_DEFERRED);
		gDeferredNonIndexedDiffuseAlphaMaskProgram.bind();
		gDeferredNonIndexedDiffuseAlphaMaskProgram.setMinimumAlpha(0.5f);
		//render grass
		LLRenderPass::renderTexture(LLRenderPass::PASS_GRASS, getVertexDataMask());
	}			
}


// Fullbright drawpool
LLDrawPoolFullbright::LLDrawPoolFullbright() :
	LLRenderPass(POOL_FULLBRIGHT)
{
}

void LLDrawPoolFullbright::prerender()
{
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolFullbright::beginPostDeferredPass(S32 pass)
{
	gDeferredFullbrightProgram.bind();
}

void LLDrawPoolFullbright::renderPostDeferred(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_FULLBRIGHT);
	
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	U32 fullbright_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXTURE_INDEX;
	pushBatches(LLRenderPass::PASS_FULLBRIGHT, fullbright_mask, TRUE, TRUE);
}

void LLDrawPoolFullbright::endPostDeferredPass(S32 pass)
{
	gDeferredFullbrightProgram.unbind();
	LLRenderPass::endRenderPass(pass);
}

void LLDrawPoolFullbright::beginRenderPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_FULLBRIGHT);
	
	if (LLPipeline::sUnderWaterRender)
	{
		fullbright_shader = &gObjectFullbrightWaterProgram;
	}
	else
	{
		fullbright_shader = &gObjectFullbrightProgram;
	}
}

void LLDrawPoolFullbright::endRenderPass(S32 pass)
{
	LLFastTimer t(LLFastTimer::FTM_RENDER_FULLBRIGHT);
	LLRenderPass::endRenderPass(pass);

	stop_glerror();

	if (mVertexShaderLevel > 0)
	{
		fullbright_shader->unbind();
	}

	stop_glerror();
}

void LLDrawPoolFullbright::render(S32 pass)
{ //render fullbright
	LLFastTimer t(LLFastTimer::FTM_RENDER_FULLBRIGHT);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	stop_glerror();

	if (mVertexShaderLevel > 0)
	{
		fullbright_shader->bind();
		fullbright_shader->uniform1f(LLViewerShaderMgr::FULLBRIGHT, 1.f);
		U32 fullbright_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR | LLVertexBuffer::MAP_TEXTURE_INDEX;
		pushBatches(LLRenderPass::PASS_FULLBRIGHT, fullbright_mask, TRUE, TRUE);
	}
	else
	{
		gPipeline.enableLightsFullbright(LLColor4(1,1,1,1));
		U32 fullbright_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR;
		renderTexture(LLRenderPass::PASS_FULLBRIGHT, fullbright_mask);
	}

	stop_glerror();
}

S32 LLDrawPoolFullbright::getNumPasses()
{ 
	return 1;
}

