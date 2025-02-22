/** 
 * @file llfloatermap.h
 * @brief The "mini-map" or radar in the upper right part of the screen.
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

#ifndef LL_LLFLOATERMAP_H
#define LL_LLFLOATERMAP_H

#include "llfloater.h"

class LLNetMap;

class LLFloaterMap :
	public LLFloater,
	public LLFloaterSingleton<LLFloaterMap>
{
	friend class LLUISingleton<LLFloaterMap, VisibilityPolicy<LLFloater> >;
public:
	virtual ~LLFloaterMap();

	static void* createPanelMiniMap(void* data);

	BOOL postBuild();

	/*virtual*/ void	draw();
	/*virtual*/ void	onOpen();
	/*virtual*/ void	onClose(bool app_quitting);
	/*virtual*/ BOOL	canClose();
// [RLVa:KB] - Version: 1.23.4 | Checked: 2009-07-05 (RLVa-1.0.0c)
    /*virtual*/ void    open();
// [/RLVa:KB]

	LLNetMap* getNetMap(){ return mPanelMap; }

private:
	LLFloaterMap(const LLSD& key = LLSD());
	LLNetMap*		mPanelMap;
};

#endif  // LL_LLFLOATERMAP_H
