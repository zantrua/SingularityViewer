/** 
 * @file llagent.cpp
 * @brief LLAgent class implementation
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

#include "llagent.h" 

#include "pipeline.h"

#include "llagentaccess.h"
#include "llagentcamera.h"
#include "llagentwearables.h"
#include "llagentui.h"
#include "llanimationstates.h"
#include "llcallingcard.h"
#include "llconsole.h"
#include "llfirstuse.h"
#include "llfloatercamera.h"
#include "llfloatertools.h"

#include "llgroupmgr.h"
#include "llhomelocationresponder.h"
#include "llhudmanager.h"
#include "lljoystickbutton.h"
#include "llmorphview.h"
#include "llmoveview.h"
#include "llchatbar.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llrendersphere.h"
#include "llsdutil.h"
#include "llsky.h"
#include "llsmoothstep.h"
#include "llstartup.h"
#include "llstatusbar.h"
#include "lltool.h"
#include "lltoolpie.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerjoystick.h"
#include "llviewerkeyboard.h" //For crouch toggle
#include "llviewermediafocus.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llworld.h"
#include "llworldmap.h"

//Misc non-standard includes
#include "llviewerregion.h"
#include "llurldispatcher.h"
#include "llimview.h" //For gIMMgr
//Floaters
#include "llfloatermute.h"
#include "llfloatermap.h"
#include "llfloateractivespeakers.h"
#include "llfloaterdirectory.h"
#include "llfloatergroupinfo.h"
#include "llfloatergroups.h"
#include "llfloateravatarinfo.h"
#include "llfloaterworldmap.h"
#include "llfloaterland.h"
#include "llfloatersnapshot.h"
#include "llfloaterchat.h"

#include "lluictrlfactory.h" //For LLUICtrlFactory::getLayeredXMLNode

// [RLVa:KB] - Checked: 2010-09-27 (RLVa-1.1.3b)
#include "rlvhandler.h"
#include "rlvinventory.h"
#include "llattachmentsmgr.h"
// [/RLVa:KB]

using namespace LLVOAvatarDefines;

const BOOL ANIMATE = TRUE;
const U8 AGENT_STATE_TYPING =	0x04;
const U8 AGENT_STATE_EDITING =  0x10;

// Autopilot constants
const F32 AUTOPILOT_HEIGHT_ADJUST_DISTANCE = 8.f;			// meters
const F32 AUTOPILOT_MIN_TARGET_HEIGHT_OFF_GROUND = 1.f;	// meters
const F32 AUTOPILOT_MAX_TIME_NO_PROGRESS = 1.5f;		// seconds

const F32 MAX_VELOCITY_AUTO_LAND_SQUARED = 4.f * 4.f;
const F64 CHAT_AGE_FAST_RATE = 3.0;

// fidget constants
const F32 MIN_FIDGET_TIME = 8.f; // seconds
const F32 MAX_FIDGET_TIME = 20.f; // seconds


// The agent instance.
LLAgent gAgent;
std::string gAuthString;

// <edit>
LLUUID gReSitTargetID;
LLVector3 gReSitOffset;
// </edit>
//
// Statics
//

BOOL LLAgent::exlPhantom = 0;
BOOL LLAgent::mForceTPose = 0;
LLVector3 LLAgent::exlStartMeasurePoint = LLVector3::zero;
LLVector3 LLAgent::exlEndMeasurePoint = LLVector3::zero;


const F32 LLAgent::TYPING_TIMEOUT_SECS = 5.f;

std::map<std::string, std::string> LLAgent::sTeleportErrorMessages;
std::map<std::string, std::string> LLAgent::sTeleportProgressMessages;

class LLAgentFriendObserver : public LLFriendObserver
{
public:
	LLAgentFriendObserver() {}
	virtual ~LLAgentFriendObserver() {}
	virtual void changed(U32 mask);
};

void LLAgentFriendObserver::changed(U32 mask)
{
	// if there's a change we're interested in.
	if((mask & (LLFriendObserver::POWERS)) != 0)
	{
		gAgent.friendsChanged();
	}
}

// ************************************************************
// Enabled this definition to compile a 'hacked' viewer that
// locally believes the end user has godlike powers.
// #define HACKED_GODLIKE_VIEWER
// For a toggled version, see viewer.h for the
// TOGGLE_HACKED_GODLIKE_VIEWER define, instead.
// ************************************************************

// Constructors and Destructors

// JC - Please try to make this order match the order in the header
// file.  Otherwise it's hard to find variables that aren't initialized.
//-----------------------------------------------------------------------------
// LLAgent()
//-----------------------------------------------------------------------------
LLAgent::LLAgent() :
	mGroupPowers(0),
	mHideGroupTitle(FALSE),
	mGroupID(),

	mInitialized(FALSE),

	mDoubleTapRunTimer(),
	mDoubleTapRunMode(DOUBLETAP_NONE),

	mbAlwaysRun(false),
	mbRunning(false),
	mbTeleportKeepsLookAt(false),

	mAgentAccess(new LLAgentAccess(gSavedSettings)),
	mTeleportState( TELEPORT_NONE ),
	mRegionp(NULL),

	mAgentOriginGlobal(),
	mPositionGlobal(),

	mDistanceTraveled(0.F),
	mLastPositionGlobal(LLVector3d::zero),

	mRenderState(0),
	mTypingTimer(),

	mViewsPushed(FALSE),

	mCustomAnim(FALSE),
	mShowAvatar(TRUE),
	mFrameAgent(),

	mIsBusy(FALSE),

	mControlFlags(0x00000000),
	mbFlagsDirty(FALSE),
	mbFlagsNeedReset(FALSE),

	mAutoPilot(FALSE),
	mAutoPilotFlyOnStop(FALSE),
	mAutoPilotTargetGlobal(),
	mAutoPilotStopDistance(1.f),
	mAutoPilotUseRotation(FALSE),
	mAutoPilotTargetFacing(LLVector3::zero),
	mAutoPilotTargetDist(0.f),
	mAutoPilotNoProgressFrameCount(0),
	mAutoPilotRotationThreshold(0.f),
	mAutoPilotFinishedCallback(NULL),
	mAutoPilotCallbackData(NULL),

	mEffectColor(new LLColor4(0.f, 1.f, 1.f, 1.f)),

	mHaveHomePosition(FALSE),
	mHomeRegionHandle( 0 ),
	mNearChatRadius(CHAT_NORMAL_RADIUS / 2.f),

	mNextFidgetTime(0.f),
	mCurrentFidget(0),
	mFirstLogin(FALSE),
	mGenderChosen(FALSE),
	mAppearanceSerialNum(0),
	mMouselookModeInSignal(NULL),
	mMouselookModeOutSignal(NULL),
	mPendingLure(NULL)
{
	for (U32 i = 0; i < TOTAL_CONTROLS; i++)
	{
		mControlsTakenCount[i] = 0;
		mControlsTakenPassedOnCount[i] = 0;
	}
}

// Requires gSavedSettings to be initialized.
//-----------------------------------------------------------------------------
// init()
//-----------------------------------------------------------------------------
void LLAgent::init()
{

	setFlying( gSavedSettings.getBOOL("FlyingAtExit") );



//	LLDebugVarMessageBox::show("Camera Lag", &CAMERA_FOCUS_HALF_LIFE, 0.5f, 0.01f);

	*mEffectColor = gSavedSettings.getColor4("EffectColor");

	gSavedSettings.getControl("PreferredMaturity")->getValidateSignal()->connect(boost::bind(&LLAgent::validateMaturity, this, _2));
	gSavedSettings.getControl("PreferredMaturity")->getSignal()->connect(boost::bind(&LLAgent::handleMaturity, this, _2));
	
	mInitialized = TRUE;
}

//-----------------------------------------------------------------------------
// cleanup()
//-----------------------------------------------------------------------------
void LLAgent::cleanup()
{
	mRegionp = NULL;
	if(mPendingLure)
		delete mPendingLure;
	mPendingLure = NULL;
}

//-----------------------------------------------------------------------------
// LLAgent()
//-----------------------------------------------------------------------------
LLAgent::~LLAgent()
{
	cleanup();

	delete mMouselookModeInSignal;
	mMouselookModeInSignal = NULL;
	delete mMouselookModeOutSignal;
	mMouselookModeOutSignal = NULL;

	delete mAgentAccess;
	mAgentAccess = NULL;
	delete mEffectColor;
	mEffectColor = NULL;
}


// Handle any actions that need to be performed when the main app gains focus
// (such as through alt-tab).
//-----------------------------------------------------------------------------
// onAppFocusGained()
//-----------------------------------------------------------------------------
void LLAgent::onAppFocusGained()
{
	if (CAMERA_MODE_MOUSELOOK == gAgentCamera.getCameraMode())
	{
		gAgentCamera.changeCameraToDefault();
		LLToolMgr::getInstance()->clearSavedTool();
	}
}


void LLAgent::ageChat()
{
	if (isAgentAvatarValid())
	{
		// get amount of time since I last chatted
		F64 elapsed_time = (F64)gAgentAvatarp->mChatTimer.getElapsedTimeF32();
		// add in frame time * 3 (so it ages 4x)
		gAgentAvatarp->mChatTimer.setAge(elapsed_time + (F64)gFrameDTClamped * (CHAT_AGE_FAST_RATE - 1.0));
	}
}

//-----------------------------------------------------------------------------
// moveAt()
//-----------------------------------------------------------------------------
void LLAgent::moveAt(S32 direction, bool reset)
{
	// age chat timer so it fades more quickly when you are intentionally moving
	ageChat();

	gAgentCamera.setAtKey(LLAgentCamera::directionToKey(direction));

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_AT_POS | AGENT_CONTROL_FAST_AT);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_AT_NEG | AGENT_CONTROL_FAST_AT);
	}

	if (reset)
	{
		gAgentCamera.resetView();
	}
}

//-----------------------------------------------------------------------------
// moveAtNudge()
//-----------------------------------------------------------------------------
void LLAgent::moveAtNudge(S32 direction)
{
	// age chat timer so it fades more quickly when you are intentionally moving
	ageChat();

	gAgentCamera.setWalkKey(LLAgentCamera::directionToKey(direction));

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_NUDGE_AT_POS);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_NUDGE_AT_NEG);
	}

	gAgentCamera.resetView();
}

//-----------------------------------------------------------------------------
// moveLeft()
//-----------------------------------------------------------------------------
void LLAgent::moveLeft(S32 direction)
{
	// age chat timer so it fades more quickly when you are intentionally moving
	ageChat();

	gAgentCamera.setLeftKey(LLAgentCamera::directionToKey(direction));

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_LEFT_POS | AGENT_CONTROL_FAST_LEFT);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_LEFT_NEG | AGENT_CONTROL_FAST_LEFT);
	}

	gAgentCamera.resetView();
}

//-----------------------------------------------------------------------------
// moveLeftNudge()
//-----------------------------------------------------------------------------
void LLAgent::moveLeftNudge(S32 direction)
{
	// age chat timer so it fades more quickly when you are intentionally moving
	ageChat();

	gAgentCamera.setLeftKey(LLAgentCamera::directionToKey(direction));

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_NUDGE_LEFT_POS);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_NUDGE_LEFT_NEG);
	}

	gAgentCamera.resetView();
}

//-----------------------------------------------------------------------------
// moveUp()
//-----------------------------------------------------------------------------
void LLAgent::moveUp(S32 direction)
{
	// age chat timer so it fades more quickly when you are intentionally moving
	ageChat();

	gAgentCamera.setUpKey(LLAgentCamera::directionToKey(direction));

	if (direction > 0)
	{
		setControlFlags(AGENT_CONTROL_UP_POS | AGENT_CONTROL_FAST_UP);
	}
	else if (direction < 0)
	{
		setControlFlags(AGENT_CONTROL_UP_NEG | AGENT_CONTROL_FAST_UP);
	}

	if (!isCrouch) gAgentCamera.resetView();
}

//-----------------------------------------------------------------------------
// moveYaw()
//-----------------------------------------------------------------------------
void LLAgent::moveYaw(F32 mag, bool reset_view)
{
	gAgentCamera.setYawKey(mag);

	if (mag > 0)
	{
		setControlFlags(AGENT_CONTROL_YAW_POS);
	}
	else if (mag < 0)
	{
		setControlFlags(AGENT_CONTROL_YAW_NEG);
	}

    if (reset_view)
	{
        gAgentCamera.resetView();
	}
}

//-----------------------------------------------------------------------------
// movePitch()
//-----------------------------------------------------------------------------
void LLAgent::movePitch(F32 mag)
{
	gAgentCamera.setPitchKey(mag);

	if (mag > 0)
	{
		setControlFlags(AGENT_CONTROL_PITCH_POS );
	}
	else if (mag < 0)
	{
		setControlFlags(AGENT_CONTROL_PITCH_NEG);
	}
}


// Does this parcel allow you to fly?
BOOL LLAgent::canFly()
{
// [RLVa:KB] - Checked: 2009-07-05 (RLVa-1.0.0c)
	if (gRlvHandler.hasBehaviour(RLV_BHVR_FLY)) return FALSE;
// [/RLVa:KB]
	if (isGodlike()) return TRUE;

	// <edit>
	static const LLCachedControl<bool> ascent_fly_always_enabled("AscentFlyAlwaysEnabled",false);
	if(ascent_fly_always_enabled) 
		return TRUE;
	// </edit>

	LLViewerRegion* regionp = getRegion();
	if (regionp && regionp->getBlockFly()) return FALSE;
	
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!parcel) return FALSE;

	// Allow owners to fly on their own land.
	if (LLViewerParcelMgr::isParcelOwnedByAgent(parcel, GP_LAND_ALLOW_FLY))
	{
		return TRUE;
	}

	return parcel->getAllowFly();
}

BOOL LLAgent::getFlying() const
{ 
	return mControlFlags & AGENT_CONTROL_FLY; 
}

// Better Set Phantom options ~Charbl
void LLAgent::setPhantom(BOOL phantom)
{
	exlPhantom = phantom;
}

BOOL LLAgent::getPhantom()
{
	return exlPhantom;
}

void LLAgent::resetClientTag()
{
	if (gAgentAvatarp)
	{
		llinfos << "Resetting mClientTag." << llendl;
		gAgentAvatarp->mClientTag = "";
	}
}
//

//-----------------------------------------------------------------------------
// setFlying()
//-----------------------------------------------------------------------------
void LLAgent::setFlying(BOOL fly)
{
	if (isAgentAvatarValid())
	{
		// *HACK: Don't allow to start the flying mode if we got ANIM_AGENT_STANDUP signal
		// because in this case we won't get a signal to start avatar flying animation and
		// it will be walking with flying mode "ON" indication. However we allow to switch
		// the flying mode off if we get ANIM_AGENT_STANDUP signal. See process_avatar_animation().
		// See EXT-2781.
		if(fly && gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_STANDUP) != gAgentAvatarp->mSignaledAnimations.end())
		{
			return;
		}

		// don't allow taking off while sitting
		if (fly && gAgentAvatarp->isSitting())
		{
			return;
		}
	}

	if (fly)
	{
// [RLVa:KB] - Checked: 2009-07-05 (RLVa-1.0.0c)
		if (gRlvHandler.hasBehaviour(RLV_BHVR_FLY))
		{
			return;
		}
// [/RLVa:KB]

		BOOL was_flying = getFlying();
		if (!canFly() && !was_flying)
		{
			// parcel doesn't let you start fly
			// gods can always fly
			// and it's OK if you're already flying
			make_ui_sound("UISndBadKeystroke");
			return;
		}
		if( !was_flying )
		{
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_FLY_COUNT);
		}
		setControlFlags(AGENT_CONTROL_FLY);
	}
	else
	{
		clearControlFlags(AGENT_CONTROL_FLY);
	}


	gSavedSettings.setBOOL("FlyBtnState",fly);

	mbFlagsDirty = TRUE;
}


// UI based mechanism of setting fly state
//-----------------------------------------------------------------------------
// toggleFlying()
//-----------------------------------------------------------------------------
void LLAgent::toggleFlying()
{
	BOOL fly = !gAgent.getFlying();

	gAgent.setFlying( fly );
	gAgentCamera.resetView();
}

// static
bool LLAgent::enableFlying()
{
	BOOL sitting = FALSE;
	if (isAgentAvatarValid())
	{
		sitting = gAgentAvatarp->isSitting();
	}
	return !sitting;
}

void LLAgent::standUp()
{
	setControlFlags(AGENT_CONTROL_STAND_UP);
}

void LLAgent::togglePhantom()
{
	BOOL phan = !(exlPhantom);

	setPhantom( phan );
}

void LLAgent::toggleTPosed()
{
	BOOL posed = !(mForceTPose);

	setTPosed(posed);
}


//-----------------------------------------------------------------------------
// setRegion()
//-----------------------------------------------------------------------------
void LLAgent::setRegion(LLViewerRegion *regionp)
{
	llassert(regionp);
	if (mRegionp != regionp)
	{
		// std::string host_name;
		// host_name = regionp->getHost().getHostName();

		std::string ip = regionp->getHost().getString();
		llinfos << "Moving agent into region: " << regionp->getName()
				<< " located at " << ip << llendl;
		if (mRegionp)
		{
			// We've changed regions, we're now going to change our agent coordinate frame.
			mAgentOriginGlobal = regionp->getOriginGlobal();
			LLVector3d agent_offset_global = mRegionp->getOriginGlobal();

			LLVector3 delta;
			delta.setVec(regionp->getOriginGlobal() - mRegionp->getOriginGlobal());

			setPositionAgent(getPositionAgent() - delta);

			LLVector3 camera_position_agent = LLViewerCamera::getInstance()->getOrigin();
			LLViewerCamera::getInstance()->setOrigin(camera_position_agent - delta);

			// Update all of the regions.
			LLWorld::getInstance()->updateAgentOffset(agent_offset_global);

			// Hack to keep sky in the agent's region, otherwise it may get deleted - DJS 08/02/02
			// *TODO: possibly refactor into gSky->setAgentRegion(regionp)? -Brad
			if (gSky.mVOSkyp)
			{
				gSky.mVOSkyp->setRegion(regionp);
			}
			if (gSky.mVOGroundp)
			{
				gSky.mVOGroundp->setRegion(regionp);
			}

		}
		else
		{
			// First time initialization.
			// We've changed regions, we're now going to change our agent coordinate frame.
			mAgentOriginGlobal = regionp->getOriginGlobal();

			LLVector3 delta;
			delta.setVec(regionp->getOriginGlobal());

			setPositionAgent(getPositionAgent() - delta);
			LLVector3 camera_position_agent = LLViewerCamera::getInstance()->getOrigin();
			LLViewerCamera::getInstance()->setOrigin(camera_position_agent - delta);

			// Update all of the regions.
			LLWorld::getInstance()->updateAgentOffset(mAgentOriginGlobal);
		}
	}
	mRegionp = regionp;

	// Must shift hole-covering water object locations because local
	// coordinate frame changed.
	LLWorld::getInstance()->updateWaterObjects();

	// keep a list of regions we've been too
	// this is just an interesting stat, logged at the dataserver
	// we could trake this at the dataserver side, but that's harder
	U64 handle = regionp->getHandle();
	mRegionsVisited.insert(handle);

	LLSelectMgr::getInstance()->updateSelectionCenter();
}


//-----------------------------------------------------------------------------
// getRegion()
//-----------------------------------------------------------------------------
LLViewerRegion *LLAgent::getRegion() const
{
	return mRegionp;
}


const LLHost& LLAgent::getRegionHost() const
{
	if (mRegionp)
	{
		return mRegionp->getHost();
	}
	else
	{
		return LLHost::invalid;
	}
}

//-----------------------------------------------------------------------------
// getSLURL()
// returns empty() if getRegion() == NULL
//-----------------------------------------------------------------------------
std::string LLAgent::getSLURL() const
{
	std::string slurl;
	LLViewerRegion *regionp = getRegion();
	if (regionp)
	{
		LLVector3d agentPos = getPositionGlobal();
		S32 x = llround( (F32)fmod( agentPos.mdV[VX], (F64)REGION_WIDTH_METERS ) );
		S32 y = llround( (F32)fmod( agentPos.mdV[VY], (F64)REGION_WIDTH_METERS ) );
		S32 z = llround( (F32)agentPos.mdV[VZ] );
		slurl = LLURLDispatcher::buildSLURL(regionp->getName(), x, y, z);
	}
	return slurl;
}

//-----------------------------------------------------------------------------
// inPrelude()
//-----------------------------------------------------------------------------
BOOL LLAgent::inPrelude()
{
	return mRegionp && mRegionp->isPrelude();
}


//-----------------------------------------------------------------------------
// canManageEstate()
//-----------------------------------------------------------------------------

BOOL LLAgent::canManageEstate() const
{
	return mRegionp && mRegionp->canManageEstate();
}

//-----------------------------------------------------------------------------
// sendMessage()
//-----------------------------------------------------------------------------
void LLAgent::sendMessage()
{
	if (gDisconnected)
	{
		llwarns << "Trying to send message when disconnected!" << llendl;
		return;
	}
	if (!mRegionp)
	{
		llerrs << "No region for agent yet!" << llendl;
		return;
	}
	gMessageSystem->sendMessage(mRegionp->getHost());
}


//-----------------------------------------------------------------------------
// sendReliableMessage()
//-----------------------------------------------------------------------------
void LLAgent::sendReliableMessage()
{
	if (gDisconnected)
	{
		lldebugs << "Trying to send message when disconnected!" << llendl;
		return;
	}
	if (!mRegionp)
	{
		lldebugs << "LLAgent::sendReliableMessage No region for agent yet, not sending message!" << llendl;
		return;
	}
	gMessageSystem->sendReliable(mRegionp->getHost());
}

//-----------------------------------------------------------------------------
// getVelocity()
//-----------------------------------------------------------------------------
LLVector3 LLAgent::getVelocity() const
{
	if (isAgentAvatarValid())
	{
		return gAgentAvatarp->getVelocity();
	}
	else
	{
		return LLVector3::zero;
	}
}


//-----------------------------------------------------------------------------
// setPositionAgent()
//-----------------------------------------------------------------------------
void LLAgent::setPositionAgent(const LLVector3 &pos_agent)
{
	if (!pos_agent.isFinite())
	{
		llerrs << "setPositionAgent is not a number" << llendl;
	}

	if (isAgentAvatarValid() && gAgentAvatarp->getParent())
	{
		LLVector3 pos_agent_sitting;
		LLVector3d pos_agent_d;
		LLViewerObject *parent = (LLViewerObject*)gAgentAvatarp->getParent();

		pos_agent_sitting = gAgentAvatarp->getPosition() * parent->getRotation() + parent->getPositionAgent();
		pos_agent_d.setVec(pos_agent_sitting);

		mFrameAgent.setOrigin(pos_agent_sitting);
		mPositionGlobal = pos_agent_d + mAgentOriginGlobal;
	}
	else
	{
		mFrameAgent.setOrigin(pos_agent);

		LLVector3d pos_agent_d;
		pos_agent_d.setVec(pos_agent);
		mPositionGlobal = pos_agent_d + mAgentOriginGlobal;
	}
}

//-----------------------------------------------------------------------------
// getPositionGlobal()
//-----------------------------------------------------------------------------
const LLVector3d &LLAgent::getPositionGlobal() const
{
	if (isAgentAvatarValid() && !gAgentAvatarp->mDrawable.isNull())
	{
		mPositionGlobal = getPosGlobalFromAgent(gAgentAvatarp->getRenderPosition());
	}
	else
	{
		mPositionGlobal = getPosGlobalFromAgent(mFrameAgent.getOrigin());
	}

	return mPositionGlobal;
}

//-----------------------------------------------------------------------------
// getPositionAgent()
//-----------------------------------------------------------------------------
const LLVector3 &LLAgent::getPositionAgent()
{
	if (isAgentAvatarValid() && !gAgentAvatarp->mDrawable.isNull())
	{
		mFrameAgent.setOrigin(gAgentAvatarp->getRenderPosition());	
	}

	return mFrameAgent.getOrigin();
}

//-----------------------------------------------------------------------------
// getRegionsVisited()
//-----------------------------------------------------------------------------
S32 LLAgent::getRegionsVisited() const
{
	return mRegionsVisited.size();
}

//-----------------------------------------------------------------------------
// getDistanceTraveled()
//-----------------------------------------------------------------------------
F64 LLAgent::getDistanceTraveled() const
{
	return mDistanceTraveled;
}


//-----------------------------------------------------------------------------
// getPosAgentFromGlobal()
//-----------------------------------------------------------------------------
LLVector3 LLAgent::getPosAgentFromGlobal(const LLVector3d &pos_global) const
{
	LLVector3 pos_agent;
	pos_agent.setVec(pos_global - mAgentOriginGlobal);
	return pos_agent;
}


//-----------------------------------------------------------------------------
// getPosGlobalFromAgent()
//-----------------------------------------------------------------------------
LLVector3d LLAgent::getPosGlobalFromAgent(const LLVector3 &pos_agent) const
{
	LLVector3d pos_agent_d;
	pos_agent_d.setVec(pos_agent);
	return pos_agent_d + mAgentOriginGlobal;
}

void LLAgent::sitDown()
{
	setControlFlags(AGENT_CONTROL_SIT_ON_GROUND);
}


//-----------------------------------------------------------------------------
// resetAxes()
//-----------------------------------------------------------------------------
void LLAgent::resetAxes()
{
	mFrameAgent.resetAxes();
}


// Copied from LLCamera::setOriginAndLookAt
// Look_at must be unit vector
//-----------------------------------------------------------------------------
// resetAxes()
//-----------------------------------------------------------------------------
void LLAgent::resetAxes(const LLVector3 &look_at)
{
	LLVector3	skyward = getReferenceUpVector();

	// if look_at has zero length, fail
	// if look_at and skyward are parallel, fail
	//
	// Test both of these conditions with a cross product.
	LLVector3 cross(look_at % skyward);
	if (cross.isNull())
	{
		llinfos << "LLAgent::resetAxes cross-product is zero" << llendl;
		return;
	}

	// Make sure look_at and skyward are not parallel
	// and neither are zero length
	LLVector3 left(skyward % look_at);
	LLVector3 up(look_at % left);

	mFrameAgent.setAxes(look_at, left, up);
}


//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLAgent::rotate(F32 angle, const LLVector3 &axis) 
{ 
	mFrameAgent.rotate(angle, axis); 
}


//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLAgent::rotate(F32 angle, F32 x, F32 y, F32 z) 
{ 
	mFrameAgent.rotate(angle, x, y, z); 
}


//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLAgent::rotate(const LLMatrix3 &matrix) 
{ 
	mFrameAgent.rotate(matrix); 
}


//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLAgent::rotate(const LLQuaternion &quaternion) 
{ 
	mFrameAgent.rotate(quaternion); 
}


//-----------------------------------------------------------------------------
// getReferenceUpVector()
//-----------------------------------------------------------------------------
LLVector3 LLAgent::getReferenceUpVector()
{
	// this vector is in the coordinate frame of the avatar's parent object, or the world if none
	LLVector3 up_vector = LLVector3::z_axis;
	if (isAgentAvatarValid() && 
		gAgentAvatarp->getParent() &&
		gAgentAvatarp->mDrawable.notNull())
	{
		U32 camera_mode = gAgentCamera.getCameraAnimating() ? gAgentCamera.getLastCameraMode() : gAgentCamera.getCameraMode();
		// and in third person...
		if (camera_mode == CAMERA_MODE_THIRD_PERSON)
		{
			// make the up vector point to the absolute +z axis
			up_vector = up_vector * ~((LLViewerObject*)gAgentAvatarp->getParent())->getRenderRotation();
		}
		else if (camera_mode == CAMERA_MODE_MOUSELOOK)
		{
			// make the up vector point to the avatar's +z axis
			up_vector = up_vector * gAgentAvatarp->mDrawable->getRotation();
		}
	}

	return up_vector;
}


// Radians, positive is forward into ground
//-----------------------------------------------------------------------------
// pitch()
//-----------------------------------------------------------------------------
void LLAgent::pitch(F32 angle)
{
	// don't let user pitch if pointed almost all the way down or up
	mFrameAgent.pitch(clampPitchToLimits(angle));
}


// Radians, positive is forward into ground
//-----------------------------------------------------------------------------
// clampPitchToLimits()
//-----------------------------------------------------------------------------
F32 LLAgent::clampPitchToLimits(F32 angle)
{
	// A dot B = mag(A) * mag(B) * cos(angle between A and B)
	// so... cos(angle between A and B) = A dot B / mag(A) / mag(B)
	//                                  = A dot B for unit vectors

	LLVector3 skyward = getReferenceUpVector();

	F32			look_down_limit;
	F32			look_up_limit = 10.f * DEG_TO_RAD;

	F32 angle_from_skyward = acos( mFrameAgent.getAtAxis() * skyward );

	if (isAgentAvatarValid() && gAgentAvatarp->isSitting())
	{
		look_down_limit = 178.f * DEG_TO_RAD;
	}
	else
	{
		look_down_limit = 178.f * DEG_TO_RAD;
	}

	// clamp pitch to limits
	if ((angle >= 0.f) && (angle_from_skyward + angle > look_down_limit))
	{
		angle = look_down_limit - angle_from_skyward;
	}
	else if ((angle < 0.f) && (angle_from_skyward + angle < look_up_limit))
	{
		angle = look_up_limit - angle_from_skyward;
	}
   
    return angle;
}


//-----------------------------------------------------------------------------
// roll()
//-----------------------------------------------------------------------------
void LLAgent::roll(F32 angle)
{
	mFrameAgent.roll(angle);
}


//-----------------------------------------------------------------------------
// yaw()
//-----------------------------------------------------------------------------
void LLAgent::yaw(F32 angle)
{
	if (!rotateGrabbed())
	{
		mFrameAgent.rotate(angle, getReferenceUpVector());
	}
}


// Returns a quat that represents the rotation of the agent in the absolute frame
//-----------------------------------------------------------------------------
// getQuat()
//-----------------------------------------------------------------------------
LLQuaternion LLAgent::getQuat() const
{
	return mFrameAgent.getQuaternion();
}

//-----------------------------------------------------------------------------
// getControlFlags()
//-----------------------------------------------------------------------------
U32 LLAgent::getControlFlags()
{
/*
	// HACK -- avoids maintenance of control flags when camera mode is turned on or off,
	// only worries about it when the flags are measured
	if (mCameraMode == CAMERA_MODE_MOUSELOOK) 
	{
		if ( !(mControlFlags & AGENT_CONTROL_MOUSELOOK) )
		{
			mControlFlags |= AGENT_CONTROL_MOUSELOOK;
		}
	}
*/
	return mControlFlags;
}

//-----------------------------------------------------------------------------
// setControlFlags()
//-----------------------------------------------------------------------------
void LLAgent::setControlFlags(U32 mask)
{
	U32 old_flags = mControlFlags;
	mControlFlags |= mask;
	mbFlagsDirty = mControlFlags ^ old_flags;
}


//-----------------------------------------------------------------------------
// clearControlFlags()
//-----------------------------------------------------------------------------
void LLAgent::clearControlFlags(U32 mask)
{
	U32 old_flags = mControlFlags;
	mControlFlags &= ~mask;
	if (old_flags != mControlFlags)
	{
		mbFlagsDirty = TRUE;
	}
}

//-----------------------------------------------------------------------------
// controlFlagsDirty()
//-----------------------------------------------------------------------------
BOOL LLAgent::controlFlagsDirty() const
{
	return mbFlagsDirty;
}

//-----------------------------------------------------------------------------
// enableControlFlagReset()
//-----------------------------------------------------------------------------
void LLAgent::enableControlFlagReset()
{
	mbFlagsNeedReset = TRUE;
}

//-----------------------------------------------------------------------------
// resetControlFlags()
//-----------------------------------------------------------------------------
void LLAgent::resetControlFlags()
{
	if (mbFlagsNeedReset)
	{
		mbFlagsNeedReset = FALSE;
		mbFlagsDirty = FALSE;
		// reset all of the ephemeral flags
		// some flags are managed elsewhere
		mControlFlags &= AGENT_CONTROL_AWAY | AGENT_CONTROL_FLY | AGENT_CONTROL_MOUSELOOK;
	}
}

//-----------------------------------------------------------------------------
// setAFK()
//-----------------------------------------------------------------------------
void LLAgent::setAFK()
{
	// Drones can't go AFK
	if (gNoRender)
	{
		return;
	}

	if (!gAgent.getRegion())
	{
		// Don't set AFK if we're not talking to a region yet.
		return;
	}

	if (!(mControlFlags & AGENT_CONTROL_AWAY))
	{
		sendAnimationRequest(ANIM_AGENT_AWAY, ANIM_REQUEST_START);
		setControlFlags(AGENT_CONTROL_AWAY | AGENT_CONTROL_STOP);
		LL_INFOS("AFK") << "Setting Away" << LL_ENDL;
		gAwayTimer.start();
		if (gAFKMenu)
		{
			// *TODO:Translate
			gAFKMenu->setLabel(std::string("Set Not Away"));
		}
	}
}

//-----------------------------------------------------------------------------
// clearAFK()
//-----------------------------------------------------------------------------
void LLAgent::clearAFK()
{
	gAwayTriggerTimer.reset();
	if (!gSavedSettings.controlExists("FakeAway")) gSavedSettings.declareBOOL("FakeAway", FALSE, "", NO_PERSIST);
	if (gSavedSettings.getBOOL("FakeAway") == TRUE) return;

	// Gods can sometimes get into away state (via gestures)
	// without setting the appropriate control flag. JC
	if (mControlFlags & AGENT_CONTROL_AWAY
		|| (isAgentAvatarValid()
			&& (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_AWAY) != gAgentAvatarp->mSignaledAnimations.end())))
	{
		sendAnimationRequest(ANIM_AGENT_AWAY, ANIM_REQUEST_STOP);
		clearControlFlags(AGENT_CONTROL_AWAY);
		LL_INFOS("AFK") << "Clearing Away" << LL_ENDL;
		if (gAFKMenu)
		{
			// *TODO:Translate
			gAFKMenu->setLabel(std::string("Set Away"));
		}
	}
}

//-----------------------------------------------------------------------------
// getAFK()
//-----------------------------------------------------------------------------
BOOL LLAgent::getAFK() const
{
	return (mControlFlags & AGENT_CONTROL_AWAY) != 0;
}

//-----------------------------------------------------------------------------
// setBusy()
//-----------------------------------------------------------------------------
void LLAgent::setBusy()
{
	sendAnimationRequest(ANIM_AGENT_BUSY, ANIM_REQUEST_START);
	mIsBusy = TRUE;
	if (gBusyMenu)
	{
		// *TODO:Translate
		gBusyMenu->setLabel(std::string("Set Not Busy"));
	}
	LLFloaterMute::getInstance()->updateButtons();
}

//-----------------------------------------------------------------------------
// clearBusy()
//-----------------------------------------------------------------------------
void LLAgent::clearBusy()
{
	mIsBusy = FALSE;
	sendAnimationRequest(ANIM_AGENT_BUSY, ANIM_REQUEST_STOP);
	if (gBusyMenu)
	{
		// *TODO:Translate
		gBusyMenu->setLabel(std::string("Set Busy"));
	}
	LLFloaterMute::getInstance()->updateButtons();
}

//-----------------------------------------------------------------------------
// getBusy()
//-----------------------------------------------------------------------------
BOOL LLAgent::getBusy() const
{
	return mIsBusy;
}


//-----------------------------------------------------------------------------
// startAutoPilotGlobal()
//-----------------------------------------------------------------------------
void LLAgent::startAutoPilotGlobal(
	const LLVector3d &target_global,
	const std::string& behavior_name,
	const LLQuaternion *target_rotation,
	void (*finish_callback)(BOOL, void *),
	void *callback_data,
	F32 stop_distance,
	F32 rot_threshold)
{
	if (!isAgentAvatarValid())
	{
		return;
	}
	
	mAutoPilotFinishedCallback = finish_callback;
	mAutoPilotCallbackData = callback_data;
	mAutoPilotRotationThreshold = rot_threshold;
	mAutoPilotBehaviorName = behavior_name;

	LLVector3d delta_pos( target_global );
	delta_pos -= getPositionGlobal();
	F64 distance = delta_pos.magVec();
	LLVector3d trace_target = target_global;

	trace_target.mdV[VZ] -= 10.f;

	LLVector3d intersection;
	LLVector3 normal;
	LLViewerObject *hit_obj;
	F32 heightDelta = LLWorld::getInstance()->resolveStepHeightGlobal(NULL, target_global, trace_target, intersection, normal, &hit_obj);

	if (stop_distance > 0.f)
	{
		mAutoPilotStopDistance = stop_distance;
	}
	else
	{
		// Guess at a reasonable stop distance.
		mAutoPilotStopDistance = (F32) sqrt( distance );
		if (mAutoPilotStopDistance < 0.5f) 
		{
			mAutoPilotStopDistance = 0.5f;
		}
	}

	mAutoPilotFlyOnStop = getFlying();

	if (distance > 30.0)
	{
		setFlying(TRUE);
	}

	if ( distance > 1.f && heightDelta > (sqrtf(mAutoPilotStopDistance) + 1.f))
	{
		setFlying(TRUE);
		// Do not force flying for "Sit" behavior to prevent flying after pressing "Stand"
		// from an object. See EXT-1655.
		if ("Sit" != mAutoPilotBehaviorName)
			mAutoPilotFlyOnStop = TRUE;
	}

	mAutoPilot = TRUE;
	setAutoPilotTargetGlobal(target_global);

	if (target_rotation)
	{
		mAutoPilotUseRotation = TRUE;
		mAutoPilotTargetFacing = LLVector3::x_axis * *target_rotation;
		mAutoPilotTargetFacing.mV[VZ] = 0.f;
		mAutoPilotTargetFacing.normalize();
	}
	else
	{
		mAutoPilotUseRotation = FALSE;
	}

	mAutoPilotNoProgressFrameCount = 0;
}


//-----------------------------------------------------------------------------
// setAutoPilotTargetGlobal
//-----------------------------------------------------------------------------
void LLAgent::setAutoPilotTargetGlobal(const LLVector3d &target_global)
{
	if (mAutoPilot)
	{
		mAutoPilotTargetGlobal = target_global;

		// trace ray down to find height of destination from ground
		LLVector3d traceEndPt = target_global;
		traceEndPt.mdV[VZ] -= 20.f;

		LLVector3d targetOnGround;
		LLVector3 groundNorm;
		LLViewerObject *obj;

		LLWorld::getInstance()->resolveStepHeightGlobal(NULL, target_global, traceEndPt, targetOnGround, groundNorm, &obj);
		F64 target_height = llmax((F64)gAgentAvatarp->getPelvisToFoot(), target_global.mdV[VZ] - targetOnGround.mdV[VZ]);

		// clamp z value of target to minimum height above ground
		mAutoPilotTargetGlobal.mdV[VZ] = targetOnGround.mdV[VZ] + target_height;
		mAutoPilotTargetDist = (F32)dist_vec(gAgent.getPositionGlobal(), mAutoPilotTargetGlobal);
	}
}

//-----------------------------------------------------------------------------
// startFollowPilot()
//-----------------------------------------------------------------------------
void LLAgent::startFollowPilot(const LLUUID &leader_id)
{
	if (!mAutoPilot) return;

	mLeaderID = leader_id;
	if ( mLeaderID.isNull() ) return;

	LLViewerObject* object = gObjectList.findObject(mLeaderID);
	if (!object) 
	{
		mLeaderID = LLUUID::null;
		return;
	}

	startAutoPilotGlobal(object->getPositionGlobal());
}


//-----------------------------------------------------------------------------
// stopAutoPilot()
//-----------------------------------------------------------------------------
void LLAgent::stopAutoPilot(BOOL user_cancel)
{
	if (mAutoPilot)
	{
		mAutoPilot = FALSE;
		if (mAutoPilotUseRotation && !user_cancel)
		{
			resetAxes(mAutoPilotTargetFacing);
		}
		// Restore previous flying state before invoking mAutoPilotFinishedCallback to allow
		// callback function to change the flying state (like in near_sit_down_point()).
		// If the user cancelled, don't change the fly state
		if (!user_cancel)
		{
			setFlying(mAutoPilotFlyOnStop);
		}
		//NB: auto pilot can terminate for a reason other than reaching the destination
		if (mAutoPilotFinishedCallback)
		{
			mAutoPilotFinishedCallback(!user_cancel && dist_vec_squared(gAgent.getPositionGlobal(), mAutoPilotTargetGlobal) < (mAutoPilotStopDistance * mAutoPilotStopDistance), mAutoPilotCallbackData);
			mAutoPilotFinishedCallback = NULL;
		}
		mLeaderID = LLUUID::null;

		setControlFlags(AGENT_CONTROL_STOP);

		if (user_cancel && !mAutoPilotBehaviorName.empty())
		{
			if (mAutoPilotBehaviorName == "Sit")
				LLNotificationsUtil::add("CancelledSit");
			else if (mAutoPilotBehaviorName == "Attach")
				LLNotificationsUtil::add("CancelledAttach");
			else
				LLNotificationsUtil::add("Cancelled");
		}
	}
}


// Returns necessary agent pitch and yaw changes, radians.
//-----------------------------------------------------------------------------
// autoPilot()
//-----------------------------------------------------------------------------
void LLAgent::autoPilot(F32 *delta_yaw)
{
	if (mAutoPilot)
	{
		if (!mLeaderID.isNull())
		{
			LLViewerObject* object = gObjectList.findObject(mLeaderID);
			if (!object) 
			{
				stopAutoPilot();
				return;
			}
			mAutoPilotTargetGlobal = object->getPositionGlobal();
		}
		
		if (!isAgentAvatarValid()) return;

		if (gAgentAvatarp->mInAir)
		{
			setFlying(TRUE);
		}
	
		LLVector3 at;
		at.setVec(mFrameAgent.getAtAxis());
		LLVector3 target_agent = getPosAgentFromGlobal(mAutoPilotTargetGlobal);
		LLVector3 direction = target_agent - getPositionAgent();

		F32 target_dist = direction.magVec();

		if (target_dist >= mAutoPilotTargetDist)
		{
			mAutoPilotNoProgressFrameCount++;
			if (mAutoPilotNoProgressFrameCount > AUTOPILOT_MAX_TIME_NO_PROGRESS * gFPSClamped)
			{
				stopAutoPilot();
				return;
			}
		}

		mAutoPilotTargetDist = target_dist;

		// Make this a two-dimensional solution
		at.mV[VZ] = 0.f;
		direction.mV[VZ] = 0.f;

		at.normalize();
		F32 xy_distance = direction.normalize();

		F32 yaw = 0.f;
		if (mAutoPilotTargetDist > mAutoPilotStopDistance)
		{
			yaw = angle_between(mFrameAgent.getAtAxis(), direction);
		}
		else if (mAutoPilotUseRotation)
		{
			// we're close now just aim at target facing
			yaw = angle_between(at, mAutoPilotTargetFacing);
			direction = mAutoPilotTargetFacing;
		}

		yaw = 4.f * yaw / gFPSClamped;

		// figure out which direction to turn
		LLVector3 scratch(at % direction);

		if (scratch.mV[VZ] > 0.f)
		{
			setControlFlags(AGENT_CONTROL_YAW_POS);
		}
		else
		{
			yaw = -yaw;
			setControlFlags(AGENT_CONTROL_YAW_NEG);
		}

		*delta_yaw = yaw;

		// Compute when to start slowing down and when to stop
		F32 stop_distance = mAutoPilotStopDistance;
		F32 slow_distance;
		if (getFlying())
		{
			slow_distance = llmax(6.f, mAutoPilotStopDistance + 5.f);
			stop_distance = llmax(2.f, mAutoPilotStopDistance);
		}
		else
		{
			slow_distance = llmax(3.f, mAutoPilotStopDistance + 2.f);
		}

		// If we're flying, handle autopilot points above or below you.
		if (getFlying() && xy_distance < AUTOPILOT_HEIGHT_ADJUST_DISTANCE)
		{
			if (isAgentAvatarValid())
			{
				F64 current_height = gAgentAvatarp->getPositionGlobal().mdV[VZ];
				F32 delta_z = (F32)(mAutoPilotTargetGlobal.mdV[VZ] - current_height);
				F32 slope = delta_z / xy_distance;
				if (slope > 0.45f && delta_z > 6.f)
				{
					setControlFlags(AGENT_CONTROL_FAST_UP | AGENT_CONTROL_UP_POS);
				}
				else if (slope > 0.002f && delta_z > 0.5f)
				{
					setControlFlags(AGENT_CONTROL_UP_POS);
				}
				else if (slope < -0.45f && delta_z < -6.f && current_height > AUTOPILOT_MIN_TARGET_HEIGHT_OFF_GROUND)
				{
					setControlFlags(AGENT_CONTROL_FAST_UP | AGENT_CONTROL_UP_NEG);
				}
				else if (slope < -0.002f && delta_z < -0.5f && current_height > AUTOPILOT_MIN_TARGET_HEIGHT_OFF_GROUND)
				{
					setControlFlags(AGENT_CONTROL_UP_NEG);
				}
			}
		}

		//  calculate delta rotation to target heading
		F32 delta_target_heading = angle_between(mFrameAgent.getAtAxis(), mAutoPilotTargetFacing);

		if (xy_distance > slow_distance && yaw < (F_PI / 10.f))
		{
			// walking/flying fast
			setControlFlags(AGENT_CONTROL_FAST_AT | AGENT_CONTROL_AT_POS);
		}
		else if (mAutoPilotTargetDist > mAutoPilotStopDistance)
		{
			// walking/flying slow
			if (at * direction > 0.9f)
			{
				setControlFlags(AGENT_CONTROL_AT_POS);
			}
			else if (at * direction < -0.9f)
			{
				setControlFlags(AGENT_CONTROL_AT_NEG);
			}
		}

		// check to see if we need to keep rotating to target orientation
		if (mAutoPilotTargetDist < mAutoPilotStopDistance)
		{
			setControlFlags(AGENT_CONTROL_STOP);
			if(!mAutoPilotUseRotation || (delta_target_heading < mAutoPilotRotationThreshold))
			{
				stopAutoPilot();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// propagate()
//-----------------------------------------------------------------------------
void LLAgent::propagate(const F32 dt)
{
	// Update UI based on agent motion
	LLFloaterMove *floater_move = LLFloaterMove::getInstance();
	if (floater_move)
	{
		floater_move->mForwardButton   ->setToggleState( gAgentCamera.getAtKey() > 0 || gAgentCamera.getWalkKey() > 0 );
		floater_move->mBackwardButton  ->setToggleState( gAgentCamera.getAtKey() < 0 || gAgentCamera.getWalkKey() < 0 );
		floater_move->mTurnLeftButton  ->setToggleState( gAgentCamera.getYawKey() > 0.f );
		floater_move->mTurnRightButton ->setToggleState( gAgentCamera.getYawKey() < 0.f );
		floater_move->mSlideLeftButton  ->setToggleState( gAgentCamera.getLeftKey() > 0.f );
		floater_move->mSlideRightButton ->setToggleState( gAgentCamera.getLeftKey() < 0.f );
		floater_move->mMoveUpButton    ->setToggleState( gAgentCamera.getUpKey() > 0 );
		floater_move->mMoveDownButton  ->setToggleState( gAgentCamera.getUpKey() < 0 );
	}

	// handle rotation based on keyboard levels
	const F32 YAW_RATE = 90.f * DEG_TO_RAD;				// radians per second
	yaw(YAW_RATE * gAgentCamera.getYawKey() * dt);

	const F32 PITCH_RATE = 90.f * DEG_TO_RAD;			// radians per second
	pitch(PITCH_RATE * gAgentCamera.getPitchKey() * dt);
	
	// handle auto-land behavior
	if (isAgentAvatarValid())
	{
		BOOL in_air = gAgentAvatarp->mInAir;
		LLVector3 land_vel = getVelocity();
		land_vel.mV[VZ] = 0.f;

		if (!in_air 
			&& gAgentCamera.getUpKey() < 0 
			&& land_vel.magVecSquared() < MAX_VELOCITY_AUTO_LAND_SQUARED
			&& gSavedSettings.getBOOL("AutomaticFly"))
		{
			// land automatically
			setFlying(FALSE);
		}
	}

	gAgentCamera.clearGeneralKeys();
}

//-----------------------------------------------------------------------------
// updateAgentPosition()
//-----------------------------------------------------------------------------
void LLAgent::updateAgentPosition(const F32 dt, const F32 yaw_radians, const S32 mouse_x, const S32 mouse_y)
{
	propagate(dt);

	// static S32 cameraUpdateCount = 0;

	rotate(yaw_radians, 0, 0, 1);
	
	//
	// Check for water and land collision, set underwater flag
	//

	gAgentCamera.updateLookAt(mouse_x, mouse_y);
}


// friends and operators

std::ostream& operator<<(std::ostream &s, const LLAgent &agent)
{
	// This is unfinished, but might never be used. 
	// We'll just leave it for now; we can always delete it.
	s << " { "
	  << "  Frame = " << agent.mFrameAgent << "\n"
	  << " }";
	return s;
}


// ------------------- Beginning of legacy LLCamera hack ----------------------
// This section is included for legacy LLCamera support until
// it is no longer needed.  Some legacy code must exist in 
// non-legacy functions, and is labeled with "// legacy" comments.

// TRUE if your own avatar needs to be rendered.  Usually only
// in third person and build.
//-----------------------------------------------------------------------------
// needsRenderAvatar()
//-----------------------------------------------------------------------------
BOOL LLAgent::needsRenderAvatar()
{
	if (gAgentCamera.cameraMouselook() && !LLVOAvatar::sVisibleInFirstPerson)
	{
		return FALSE;
	}

	return mShowAvatar && mGenderChosen;
}

// TRUE if we need to render your own avatar's head.
BOOL LLAgent::needsRenderHead()
{
	return (LLVOAvatar::sVisibleInFirstPerson && LLPipeline::sReflectionRender) || (mShowAvatar && !gAgentCamera.cameraMouselook());
}

//-----------------------------------------------------------------------------
// startTyping()
//-----------------------------------------------------------------------------
void LLAgent::startTyping()
{
	if (gSavedSettings.getBOOL("FakeAway")) 
		return;
	mTypingTimer.reset();

	if (getRenderState() & AGENT_STATE_TYPING)
	{
		// already typing, don't trigger a different animation
		return;
	}
	setRenderState(AGENT_STATE_TYPING);

	if (mChatTimer.getElapsedTimeF32() < 2.f)
	{
		LLVOAvatar* chatter = gObjectList.findAvatar(mLastChatterID);
		if (chatter)
		{
			gAgentCamera.setLookAt(LOOKAT_TARGET_RESPOND, chatter, LLVector3::zero);
		}
	}

	if (gSavedSettings.getBOOL("PlayTypingAnim"))
	{
		sendAnimationRequest(ANIM_AGENT_TYPE, ANIM_REQUEST_START);
	}
	gChatBar->sendChatFromViewer("", CHAT_TYPE_START, FALSE);
}

//-----------------------------------------------------------------------------
// stopTyping()
//-----------------------------------------------------------------------------
void LLAgent::stopTyping()
{
	if (mRenderState & AGENT_STATE_TYPING)
	{
		clearRenderState(AGENT_STATE_TYPING);
		sendAnimationRequest(ANIM_AGENT_TYPE, ANIM_REQUEST_STOP);
		gChatBar->sendChatFromViewer("", CHAT_TYPE_STOP, FALSE);
	}
}

//-----------------------------------------------------------------------------
// setRenderState()
//-----------------------------------------------------------------------------
void LLAgent::setRenderState(U8 newstate)
{
	mRenderState |= newstate;
}

//-----------------------------------------------------------------------------
// clearRenderState()
//-----------------------------------------------------------------------------
void LLAgent::clearRenderState(U8 clearstate)
{
	mRenderState &= ~clearstate;
}


//-----------------------------------------------------------------------------
// getRenderState()
//-----------------------------------------------------------------------------
U8 LLAgent::getRenderState()
{
	if (gNoRender || gKeyboard == NULL)
	{
		return 0;
	}

	// *FIX: don't do stuff in a getter!  This is infinite loop city!
	if ((mTypingTimer.getElapsedTimeF32() > TYPING_TIMEOUT_SECS) 
		&& (mRenderState & AGENT_STATE_TYPING))
	{
		stopTyping();
	}
	
	if ((!LLSelectMgr::getInstance()->getSelection()->isEmpty() && LLSelectMgr::getInstance()->shouldShowSelection())
		|| LLToolMgr::getInstance()->getCurrentTool()->isEditing() )
	{
		setRenderState(AGENT_STATE_EDITING);
	}
	else
	{
		clearRenderState(AGENT_STATE_EDITING);
	}

	return mRenderState;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

static const LLFloaterView::skip_list_t& get_skip_list()
{
	static LLFloaterView::skip_list_t skip_list;
	skip_list.insert(LLFloaterMap::getInstance());
	return skip_list;
}

//-----------------------------------------------------------------------------
// endAnimationUpdateUI()
//-----------------------------------------------------------------------------
void LLAgent::endAnimationUpdateUI()
{
	if (gAgentCamera.getCameraMode() == gAgentCamera.getLastCameraMode())
	{
		// We're already done endAnimationUpdateUI for this transition.
		return;
	}

	// clean up UI from mode we're leaving
	if (gAgentCamera.getLastCameraMode() == CAMERA_MODE_MOUSELOOK )
	{
		// show mouse cursor
		gViewerWindow->showCursor();
		// show menus
		gMenuBarView->setVisible(TRUE);
		gStatusBar->setVisibleForMouselook(true);

		LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);


		if (mMouselookModeOutSignal)
		{
			(*mMouselookModeOutSignal)();
		}
		// Only pop if we have pushed...
		if (TRUE == mViewsPushed)
		{
			mViewsPushed = FALSE;
			gFloaterView->popVisibleAll(get_skip_list());
		}

		gAgentCamera.setLookAt(LOOKAT_TARGET_CLEAR);
		if( gMorphView )
		{
			gMorphView->setVisible( FALSE );
		}

		// Disable mouselook-specific animations
		if (isAgentAvatarValid())
		{
			if( gAgentAvatarp->isAnyAnimationSignaled(AGENT_GUN_AIM_ANIMS, NUM_AGENT_GUN_AIM_ANIMS) )
			{
				if (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_AIM_RIFLE_R) != gAgentAvatarp->mSignaledAnimations.end())
				{
					sendAnimationRequest(ANIM_AGENT_AIM_RIFLE_R, ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_HOLD_RIFLE_R, ANIM_REQUEST_START);
				}
				if (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_AIM_HANDGUN_R) != gAgentAvatarp->mSignaledAnimations.end())
				{
					sendAnimationRequest(ANIM_AGENT_AIM_HANDGUN_R, ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_HOLD_HANDGUN_R, ANIM_REQUEST_START);
				}
				if (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_AIM_BAZOOKA_R) != gAgentAvatarp->mSignaledAnimations.end())
				{
					sendAnimationRequest(ANIM_AGENT_AIM_BAZOOKA_R, ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_HOLD_BAZOOKA_R, ANIM_REQUEST_START);
				}
				if (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_AIM_BOW_L) != gAgentAvatarp->mSignaledAnimations.end())
				{
					sendAnimationRequest(ANIM_AGENT_AIM_BOW_L, ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_HOLD_BOW_L, ANIM_REQUEST_START);
				}
			}
		}
	}
	else if (gAgentCamera.getLastCameraMode() == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		// make sure we ask to save changes

		LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);

		// HACK: If we're quitting, and we were in customize avatar, don't
		// let the mini-map go visible again. JC
        if (!LLAppViewer::instance()->quitRequested())
		{
			LLFloaterMap::getInstance()->popVisible();
		}

		if( gMorphView )
		{
			gMorphView->setVisible( FALSE );
		}

		if (isAgentAvatarValid())
		{
			if(mCustomAnim)
			{
				sendAnimationRequest(ANIM_AGENT_CUSTOMIZE, ANIM_REQUEST_STOP);
				sendAnimationRequest(ANIM_AGENT_CUSTOMIZE_DONE, ANIM_REQUEST_START);

				mCustomAnim = FALSE ;
			}
			
		}
		gAgentCamera.setLookAt(LOOKAT_TARGET_CLEAR);
	}

	//---------------------------------------------------------------------
	// Set up UI for mode we're entering
	//---------------------------------------------------------------------
	if (gAgentCamera.getCameraMode() == CAMERA_MODE_MOUSELOOK)
	{
		// hide menus
		gMenuBarView->setVisible(FALSE);
		gStatusBar->setVisibleForMouselook(false);

		// clear out camera lag effect
		gAgentCamera.clearCameraLag();

		// JC - Added for always chat in third person option
		gFocusMgr.setKeyboardFocus(NULL);

		LLToolMgr::getInstance()->setCurrentToolset(gMouselookToolset);

		mViewsPushed = TRUE;

		if (mMouselookModeInSignal)
		{
			(*mMouselookModeInSignal)();
		}
		gFloaterView->pushVisibleAll(FALSE, get_skip_list());

		if( gMorphView )
		{
			gMorphView->setVisible(FALSE);
		}

		gConsole->setVisible( TRUE );

		if (isAgentAvatarValid())
		{
			// Trigger mouselook-specific animations
			if( gAgentAvatarp->isAnyAnimationSignaled(AGENT_GUN_HOLD_ANIMS, NUM_AGENT_GUN_HOLD_ANIMS) )
			{
				if (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_HOLD_RIFLE_R) != gAgentAvatarp->mSignaledAnimations.end())
				{
					sendAnimationRequest(ANIM_AGENT_HOLD_RIFLE_R, ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_AIM_RIFLE_R, ANIM_REQUEST_START);
				}
				if (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_HOLD_HANDGUN_R) != gAgentAvatarp->mSignaledAnimations.end())
				{
					sendAnimationRequest(ANIM_AGENT_HOLD_HANDGUN_R, ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_AIM_HANDGUN_R, ANIM_REQUEST_START);
				}
				if (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_HOLD_BAZOOKA_R) != gAgentAvatarp->mSignaledAnimations.end())
				{
					sendAnimationRequest(ANIM_AGENT_HOLD_BAZOOKA_R, ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_AIM_BAZOOKA_R, ANIM_REQUEST_START);
				}
				if (gAgentAvatarp->mSignaledAnimations.find(ANIM_AGENT_HOLD_BOW_L) != gAgentAvatarp->mSignaledAnimations.end())
				{
					sendAnimationRequest(ANIM_AGENT_HOLD_BOW_L, ANIM_REQUEST_STOP);
					sendAnimationRequest(ANIM_AGENT_AIM_BOW_L, ANIM_REQUEST_START);
				}
			}
			if (gAgentAvatarp->getParent())
			{
				LLVector3 at_axis = LLViewerCamera::getInstance()->getAtAxis();
				LLViewerObject* root_object = (LLViewerObject*)gAgentAvatarp->getRoot();
				if (root_object->flagCameraDecoupled())
				{
					resetAxes(at_axis);
				}
				else
				{
					resetAxes(at_axis * ~((LLViewerObject*)gAgentAvatarp->getParent())->getRenderRotation());
				}
			}
		}
	}
	else if (gAgentCamera.getCameraMode() == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		LLToolMgr::getInstance()->setCurrentToolset(gFaceEditToolset);

		LLFloaterMap::getInstance()->pushVisible(FALSE);
		/*
		LLView *view;
		for (view = gFloaterView->getFirstChild(); view; view = gFloaterView->getNextChild())
		{
			view->pushVisible(FALSE);
		}
		*/

		if( gMorphView )
		{
			gMorphView->setVisible( TRUE );
		}

		// freeze avatar
		if (isAgentAvatarValid())
		{
			// <edit>
			//mPauseRequest = gAgentAvatarp->requestPause();
			// </edit>
		}
	}

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->updateAttachmentVisibility(gAgentCamera.getCameraMode());
	}

	gFloaterTools->dirty();

	// Don't let this be called more than once if the camera
	// mode hasn't changed.  --JC
	gAgentCamera.updateLastCamera();
}

boost::signals2::connection LLAgent::setMouselookModeInCallback( const camera_signal_t::slot_type& cb )
{
	if (!mMouselookModeInSignal) mMouselookModeInSignal = new camera_signal_t();
	return mMouselookModeInSignal->connect(cb);
}

boost::signals2::connection LLAgent::setMouselookModeOutCallback( const camera_signal_t::slot_type& cb )
{
	if (!mMouselookModeOutSignal) mMouselookModeOutSignal = new camera_signal_t();
	return mMouselookModeOutSignal->connect(cb);
}

//-----------------------------------------------------------------------------
// heardChat()
//-----------------------------------------------------------------------------
void LLAgent::heardChat(const LLUUID& id)
{
	// log text and voice chat to speaker mgr
	// for keeping track of active speakers, etc.
	LLLocalSpeakerMgr::getInstance()->speakerChatted(id);

	// don't respond to your own voice
	if (id == getID()) return;
	
	if (ll_rand(2) == 0) 
	{
		LLViewerObject *chatter = gObjectList.findObject(mLastChatterID);
		gAgentCamera.setLookAt(LOOKAT_TARGET_AUTO_LISTEN, chatter, LLVector3::zero);
	}			

	mLastChatterID = id;
	mChatTimer.reset();
}

const F32 SIT_POINT_EXTENTS = 0.2f;

LLSD ll_sdmap_from_vector3(const LLVector3& vec)
{
    LLSD ret;
    ret["X"] = vec.mV[VX];
    ret["Y"] = vec.mV[VY];
    ret["Z"] = vec.mV[VZ];
    return ret;
}

LLVector3 ll_vector3_from_sdmap(const LLSD& sd)
{
    LLVector3 ret;
    ret.mV[VX] = F32(sd["X"].asReal());
    ret.mV[VY] = F32(sd["Y"].asReal());
    ret.mV[VZ] = F32(sd["Z"].asReal());
    return ret;
}

void LLAgent::setStartPosition( U32 location_id )
{
    if (gAgentID == LLUUID::null)
    {
        return;
    }
    if (gObjectList.findAvatar(gAgentID) == NULL)
    {
        llinfos << "setStartPosition - Can't find agent viewerobject id " << gAgentID << llendl;
        return;
    }
    // we've got the viewer object
    // Sometimes the agent can be velocity interpolated off of
    // this simulator.  Clamp it to the region the agent is
    // in, a little bit in on each side.
    const F32 INSET = 0.5f; //meters
    const F32 REGION_WIDTH = LLWorld::getInstance()->getRegionWidthInMeters();

    LLVector3 agent_pos = getPositionAgent();

    if (isAgentAvatarValid())
    {
        // the z height is at the agent's feet
          agent_pos.mV[VZ] -= 0.5f * gAgentAvatarp->mBodySize.mV[VZ];
    }

    agent_pos.mV[VX] = llclamp( agent_pos.mV[VX], INSET, REGION_WIDTH - INSET );
    agent_pos.mV[VY] = llclamp( agent_pos.mV[VY], INSET, REGION_WIDTH - INSET );

    // Don't let them go below ground, or too high.
    agent_pos.mV[VZ] = llclamp( agent_pos.mV[VZ],
                                mRegionp->getLandHeightRegion( agent_pos ),
                                LLWorld::getInstance()->getRegionMaxHeight() );
	std::string url = gAgent.getRegion()->getCapability("HomeLocation");
	if( !url.empty() )
	{
		// Send the CapReq
	    LLSD request;
	    LLSD body;
	    LLSD homeLocation;

	    homeLocation["LocationId"] = LLSD::Integer(location_id);
	    homeLocation["LocationPos"] = ll_sdmap_from_vector3(agent_pos);
	    homeLocation["LocationLookAt"] = ll_sdmap_from_vector3(mFrameAgent.getAtAxis());

	    body["HomeLocation"] = homeLocation;

		LLHTTPClient::post( url, body, new LLHomeLocationResponder() );
	}
	else
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_SetStartLocationRequest);
		msg->nextBlockFast( _PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, getID());
		msg->addUUIDFast(_PREHASH_SessionID, getSessionID());
		msg->nextBlockFast( _PREHASH_StartLocationData);
		// corrected by sim
		msg->addStringFast(_PREHASH_SimName, "");
		msg->addU32Fast(_PREHASH_LocationID, location_id);
		msg->addVector3Fast(_PREHASH_LocationPos, agent_pos);
		msg->addVector3Fast(_PREHASH_LocationLookAt,mFrameAgent.getAtAxis());
	
		// Reliable only helps when setting home location.  Last
		// location is sent on quit, and we don't have time to ack
		// the packets.
		msg->sendReliable(mRegionp->getHost());
	}
	const U32 HOME_INDEX = 1;
	if( HOME_INDEX == location_id )
	{
		setHomePosRegion( mRegionp->getHandle(), getPositionAgent() );
	}
}

void LLAgent::requestStopMotion( LLMotion* motion )
{
	// Notify all avatars that a motion has stopped.
	// This is needed to clear the animation state bits
	LLUUID anim_state = motion->getID();
	onAnimStop(motion->getID());

	// if motion is not looping, it could have stopped by running out of time
	// so we need to tell the server this
//	llinfos << "Sending stop for motion " << motion->getName() << llendl;
	sendAnimationRequest( anim_state, ANIM_REQUEST_STOP );
}

void LLAgent::onAnimStop(const LLUUID& id)
{
	// handle automatic state transitions (based on completion of animation playback)
	if (id == ANIM_AGENT_STAND)
	{
		stopFidget();
	}
	else if (id == ANIM_AGENT_AWAY)
	{
		//clearAFK();
// [RLVa:KB] - Checked: 2009-10-19 (RLVa-1.1.0g) | Added: RLVa-1.1.0g
#ifdef RLV_EXTENSION_CMD_ALLOWIDLE
		if (!gRlvHandler.hasBehaviour(RLV_BHVR_ALLOWIDLE))
			clearAFK();
#else
		clearAFK();
#endif // RLV_EXTENSION_CMD_ALLOWIDLE
// [/RLVa:KB]
	}
	else if (id == ANIM_AGENT_STANDUP)
	{
		// send stand up command
		setControlFlags(AGENT_CONTROL_FINISH_ANIM);

		// now trigger dusting self off animation
		if (isAgentAvatarValid() && !gAgentAvatarp->mBelowWater && rand() % 3 == 0)
			sendAnimationRequest( ANIM_AGENT_BRUSH, ANIM_REQUEST_START );
	}
	else if (id == ANIM_AGENT_PRE_JUMP || id == ANIM_AGENT_LAND || id == ANIM_AGENT_MEDIUM_LAND)
	{
		setControlFlags(AGENT_CONTROL_FINISH_ANIM);
	}
}

bool LLAgent::isGodlike() const
{
	return mAgentAccess->isGodlike();
}

bool LLAgent::isGodlikeWithoutAdminMenuFakery() const
{
	return mAgentAccess->isGodlikeWithoutAdminMenuFakery();
}

U8 LLAgent::getGodLevel() const
{
	return mAgentAccess->getGodLevel();
}

bool LLAgent::wantsPGOnly() const
{
	return mAgentAccess->wantsPGOnly();
}

bool LLAgent::canAccessMature() const
{
	return mAgentAccess->canAccessMature();
}

bool LLAgent::canAccessAdult() const
{
	return mAgentAccess->canAccessAdult();
}

bool LLAgent::canAccessMaturityInRegion( U64 region_handle ) const
{
	LLViewerRegion *regionp = LLWorld::getInstance()->getRegionFromHandle( region_handle );
	if( regionp )
	{
		switch( regionp->getSimAccess() )
		{
			case SIM_ACCESS_MATURE:
				if( !canAccessMature() )
					return false;
				break;
			case SIM_ACCESS_ADULT:
				if( !canAccessAdult() )
					return false;
				break;
			default:
				// Oh, go on and hear the silly noises.
				break;
		}
	}
	
	return true;
}

bool LLAgent::canAccessMaturityAtGlobal( LLVector3d pos_global ) const
{
	U64 region_handle = to_region_handle_global( pos_global.mdV[0], pos_global.mdV[1] );
	return canAccessMaturityInRegion( region_handle );
}

bool LLAgent::prefersPG() const
{
	return mAgentAccess->prefersPG();
}

bool LLAgent::prefersMature() const
{
	return mAgentAccess->prefersMature();
}
	
bool LLAgent::prefersAdult() const
{
	return mAgentAccess->prefersAdult();
}

bool LLAgent::isTeen() const
{
	return mAgentAccess->isTeen();
}

bool LLAgent::isMature() const
{
	return mAgentAccess->isMature();
}

bool LLAgent::isAdult() const
{
	return mAgentAccess->isAdult();
}

void LLAgent::setTeen(bool teen)
{
	mAgentAccess->setTeen(teen);
}

//static 
int LLAgent::convertTextToMaturity(char text)
{
	return LLAgentAccess::convertTextToMaturity(text);
}

bool LLAgent::sendMaturityPreferenceToServer(int preferredMaturity)
{
	if (!getRegion())
		return false;
	
	// Update agent access preference on the server
	std::string url = getRegion()->getCapability("UpdateAgentInformation");
	if (!url.empty())
	{
		// Set new access preference
		LLSD access_prefs = LLSD::emptyMap();
		if (preferredMaturity == SIM_ACCESS_PG)
		{
			access_prefs["max"] = "PG";
		}
		else if (preferredMaturity == SIM_ACCESS_MATURE)
		{
			access_prefs["max"] = "M";
		}
		if (preferredMaturity == SIM_ACCESS_ADULT)
		{
			access_prefs["max"] = "A";
		}
		
		LLSD body = LLSD::emptyMap();
		body["access_prefs"] = access_prefs;
		llinfos << "Sending access prefs update to " << (access_prefs["max"].asString()) << " via capability to: "
		<< url << llendl;
		LLHTTPClient::post(url, body, new LLHTTPClient::Responder());    // Ignore response
		return true;
	}
	return false;
}

BOOL LLAgent::getAdminOverride() const	
{ 
	return mAgentAccess->getAdminOverride(); 
}

void LLAgent::setMaturity(char text)
{
	mAgentAccess->setMaturity(text);
}

void LLAgent::setAdminOverride(BOOL b)	
{ 
	mAgentAccess->setAdminOverride(b);
}

void LLAgent::setGodLevel(U8 god_level)	
{ 
	mAgentAccess->setGodLevel(god_level);
}

void LLAgent::setAOTransition()
{
	mAgentAccess->setTransition();
}

const LLAgentAccess& LLAgent::getAgentAccess()
{
	return *mAgentAccess;
}

bool LLAgent::validateMaturity(const LLSD& newvalue)
{
	return mAgentAccess->canSetMaturity(newvalue.asInteger());
}

void LLAgent::handleMaturity(const LLSD& newvalue)
{
	sendMaturityPreferenceToServer(newvalue.asInteger());
}

void LLAgent::buildFullname(std::string& name) const
{
	if (isAgentAvatarValid())
	{
		name = gAgentAvatarp->getFullname();
	}
}

void LLAgent::buildFullnameAndTitle(std::string& name) const
{
	if (isGroupMember())
	{
		name = mGroupTitle;
		name += ' ';
	}
	else
	{
		name.erase(0, name.length());
	}

	if (isAgentAvatarValid())
	{
		name += gAgentAvatarp->getFullname();
	}
}

BOOL LLAgent::isInGroup(const LLUUID& group_id) const
{
	if (isGodlike())
		return true;

	S32 count = mGroups.count();
	for(S32 i = 0; i < count; ++i)
	{
		if(mGroups.get(i).mID == group_id)
		{
			return TRUE;
		}
	}
	return FALSE;
}

// This implementation should mirror LLAgentInfo::hasPowerInGroup
BOOL LLAgent::hasPowerInGroup(const LLUUID& group_id, U64 power) const
{
	if (isGodlike())
		return true;

	// GP_NO_POWERS can also mean no power is enough to grant an ability.
	if (GP_NO_POWERS == power) return FALSE;

	S32 count = mGroups.count();
	for(S32 i = 0; i < count; ++i)
	{
		if(mGroups.get(i).mID == group_id)
		{
			return (BOOL)((mGroups.get(i).mPowers & power) > 0);
		}
	}
	return FALSE;
}

BOOL LLAgent::hasPowerInActiveGroup(U64 power) const
{
	return (mGroupID.notNull() && (hasPowerInGroup(mGroupID, power)));
}

U64 LLAgent::getPowerInGroup(const LLUUID& group_id) const
{
	if (isGodlike())
		return GP_ALL_POWERS;
	
	S32 count = mGroups.count();
	for(S32 i = 0; i < count; ++i)
	{
		if(mGroups.get(i).mID == group_id)
		{
			return (mGroups.get(i).mPowers);
		}
	}

	return GP_NO_POWERS;
}

BOOL LLAgent::getGroupData(const LLUUID& group_id, LLGroupData& data) const
{
	S32 count = mGroups.count();
	for(S32 i = 0; i < count; ++i)
	{
		if(mGroups.get(i).mID == group_id)
		{
			data = mGroups.get(i);
			return TRUE;
		}
	}
	return FALSE;
}

S32 LLAgent::getGroupContribution(const LLUUID& group_id) const
{
	S32 count = mGroups.count();
	for(S32 i = 0; i < count; ++i)
	{
		if(mGroups.get(i).mID == group_id)
		{
			S32 contribution = mGroups.get(i).mContribution;
			return contribution;
		}
	}
	return 0;
}

BOOL LLAgent::setGroupContribution(const LLUUID& group_id, S32 contribution)
{
	S32 count = mGroups.count();
	for(S32 i = 0; i < count; ++i)
	{
		if(mGroups.get(i).mID == group_id)
		{
			mGroups.get(i).mContribution = contribution;
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessage("SetGroupContribution");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgentID);
			msg->addUUID("SessionID", gAgentSessionID);
			msg->nextBlock("Data");
			msg->addUUID("GroupID", group_id);
			msg->addS32("Contribution", contribution);
			sendReliableMessage();
			return TRUE;
		}
	}
	return FALSE;
}

BOOL LLAgent::setUserGroupFlags(const LLUUID& group_id, BOOL accept_notices, BOOL list_in_profile)
{
	S32 count = mGroups.count();
	for(S32 i = 0; i < count; ++i)
	{
		LLGroupData &group = mGroups.get(i);
		if(group.mID == group_id)
		{
			group.mAcceptNotices = accept_notices;
			group.mListInProfile = list_in_profile;
			LLMessageSystem* msg = gMessageSystem;
			msg->newMessage("SetGroupAcceptNotices");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgentID);
			msg->addUUID("SessionID", gAgentSessionID);
			msg->nextBlock("Data");
			msg->addUUID("GroupID", group_id);
			msg->addBOOL("AcceptNotices", accept_notices);
			msg->nextBlock("NewData");
			msg->addBOOL("ListInProfile", list_in_profile);
			sendReliableMessage();

			update_group_floaters(group.mID);

			return TRUE;
		}
	}
	return FALSE;
}


LLQuaternion LLAgent::getHeadRotation()
{
	if (!isAgentAvatarValid() || !gAgentAvatarp->mPelvisp || !gAgentAvatarp->mHeadp)
	{
		return LLQuaternion::DEFAULT;
	}

	if (!gAgentCamera.cameraMouselook())
	{
		return gAgentAvatarp->getRotation();
	}

	// We must be in mouselook
	LLVector3 look_dir( LLViewerCamera::getInstance()->getAtAxis() );
	LLVector3 up = look_dir % mFrameAgent.getLeftAxis();
	LLVector3 left = up % look_dir;

	LLQuaternion rot(look_dir, left, up);
	if (gAgentAvatarp->getParent())
	{
		rot = rot * ~gAgentAvatarp->getParent()->getRotation();
	}

	return rot;
}

void LLAgent::sendAnimationRequests(LLDynamicArray<LLUUID> &anim_ids, EAnimRequest request)
{
	if (gAgentID.isNull())
	{
		return;
	}

	S32 num_valid_anims = 0;

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_AgentAnimation);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, getID());
	msg->addUUIDFast(_PREHASH_SessionID, getSessionID());

	for (S32 i = 0; i < anim_ids.count(); i++)
	{
		if (anim_ids[i].isNull())
		{
			continue;
		}
		msg->nextBlockFast(_PREHASH_AnimationList);
		msg->addUUIDFast(_PREHASH_AnimID, (anim_ids[i]) );
		msg->addBOOLFast(_PREHASH_StartAnim, (request == ANIM_REQUEST_START) ? TRUE : FALSE);
		num_valid_anims++;
	}

	msg->nextBlockFast(_PREHASH_PhysicalAvatarEventList);
	msg->addBinaryDataFast(_PREHASH_TypeData, NULL, 0);
	if (num_valid_anims)
	{
		sendReliableMessage();
	}
}

void LLAgent::sendAnimationRequest(const LLUUID &anim_id, EAnimRequest request)
{
	if (gAgentID.isNull() || anim_id.isNull() || !mRegionp)
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_AgentAnimation);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, getID());
	msg->addUUIDFast(_PREHASH_SessionID, getSessionID());

	msg->nextBlockFast(_PREHASH_AnimationList);
	msg->addUUIDFast(_PREHASH_AnimID, (anim_id) );
	msg->addBOOLFast(_PREHASH_StartAnim, (request == ANIM_REQUEST_START) ? TRUE : FALSE);

	msg->nextBlockFast(_PREHASH_PhysicalAvatarEventList);
	msg->addBinaryDataFast(_PREHASH_TypeData, NULL, 0);
	sendReliableMessage();
}

void LLAgent::sendWalkRun(bool running)
{
	LLMessageSystem* msgsys = gMessageSystem;
	if (msgsys)
	{
		msgsys->newMessageFast(_PREHASH_SetAlwaysRun);
		msgsys->nextBlockFast(_PREHASH_AgentData);
		msgsys->addUUIDFast(_PREHASH_AgentID, getID());
		msgsys->addUUIDFast(_PREHASH_SessionID, getSessionID());
		msgsys->addBOOLFast(_PREHASH_AlwaysRun, BOOL(running) );
		sendReliableMessage();
	}
}

void LLAgent::friendsChanged()
{
	LLCollectProxyBuddies collector;
	LLAvatarTracker::instance().applyFunctor(collector);
	mProxyForAgents = collector.mProxy;
}

BOOL LLAgent::isGrantedProxy(const LLPermissions& perm)
{
	return (mProxyForAgents.count(perm.getOwner()) > 0);
}

BOOL LLAgent::allowOperation(PermissionBit op,
							 const LLPermissions& perm,
							 U64 group_proxy_power,
							 U8 god_minimum)
{
	// Check god level.
	if (getGodLevel() >= god_minimum) return TRUE;

	if (!perm.isOwned()) return FALSE;

	// A group member with group_proxy_power can act as owner.
	BOOL is_group_owned;
	LLUUID owner_id;
	perm.getOwnership(owner_id, is_group_owned);
	LLUUID group_id(perm.getGroup());
	LLUUID agent_proxy(getID());

	if (is_group_owned)
	{
		if (hasPowerInGroup(group_id, group_proxy_power))
		{
			// Let the member assume the group's id for permission requests.
			agent_proxy = owner_id;
		}
	}
	else
	{
		// Check for granted mod permissions.
		if ((PERM_OWNER != op) && isGrantedProxy(perm))
		{
			agent_proxy = owner_id;
		}
	}

	// This is the group id to use for permission requests.
	// Only group members may use this field.
	LLUUID group_proxy = LLUUID::null;
	if (group_id.notNull() && isInGroup(group_id))
	{
		group_proxy = group_id;
	}

	// We now have max ownership information.
	if (PERM_OWNER == op)
	{
		// This this was just a check for ownership, we can now return the answer.
		return (agent_proxy == owner_id);
	}

	return perm.allowOperationBy(op, agent_proxy, group_proxy);
}


void LLAgent::getName(std::string& name)
{
	name.clear();

	if (gAgentAvatarp)
	{
		LLNameValue *first_nv = gAgentAvatarp->getNVPair("FirstName");
		LLNameValue *last_nv = gAgentAvatarp->getNVPair("LastName");
		if (first_nv && last_nv)
		{
			name = first_nv->printData() + " " + last_nv->printData();
		}
		else
		{
			llwarns << "Agent is missing FirstName and/or LastName nv pair." << llendl;
		}
	}
	else
	{
		name = gSavedSettings.getString("FirstName") + " " + gSavedSettings.getString("LastName");
	}
}

const LLColor4 &LLAgent::getEffectColor()
{
	return *mEffectColor;
}

void LLAgent::setEffectColor(const LLColor4 &color)
{
	*mEffectColor = color;
}

void LLAgent::initOriginGlobal(const LLVector3d &origin_global)
{
	mAgentOriginGlobal = origin_global;
}

BOOL LLAgent::leftButtonGrabbed() const	
{ 
	const BOOL camera_mouse_look = gAgentCamera.cameraMouselook();
	return (!camera_mouse_look && mControlsTakenCount[CONTROL_LBUTTON_DOWN_INDEX] > 0) 
		|| (camera_mouse_look && mControlsTakenCount[CONTROL_ML_LBUTTON_DOWN_INDEX] > 0)
		|| (!camera_mouse_look && mControlsTakenPassedOnCount[CONTROL_LBUTTON_DOWN_INDEX] > 0)
		|| (camera_mouse_look && mControlsTakenPassedOnCount[CONTROL_ML_LBUTTON_DOWN_INDEX] > 0);
}

BOOL LLAgent::rotateGrabbed() const		
{ 
	return (mControlsTakenCount[CONTROL_YAW_POS_INDEX] > 0)
		|| (mControlsTakenCount[CONTROL_YAW_NEG_INDEX] > 0); 
}

BOOL LLAgent::forwardGrabbed() const
{ 
	return (mControlsTakenCount[CONTROL_AT_POS_INDEX] > 0); 
}

BOOL LLAgent::backwardGrabbed() const
{ 
	return (mControlsTakenCount[CONTROL_AT_NEG_INDEX] > 0); 
}

BOOL LLAgent::upGrabbed() const		
{ 
	return (mControlsTakenCount[CONTROL_UP_POS_INDEX] > 0); 
}

BOOL LLAgent::downGrabbed() const	
{ 
	return (mControlsTakenCount[CONTROL_UP_NEG_INDEX] > 0); 
}

void update_group_floaters(const LLUUID& group_id)
{
	LLFloaterGroupInfo::refreshGroup(group_id);

	// update avatar info
	LLFloaterAvatarInfo* fa = LLFloaterAvatarInfo::getInstance(gAgent.getID());
	if(fa)
	{
		fa->resetGroupList();
	}

	if (gIMMgr)
	{
		// update the talk view
		gIMMgr->refresh();
	}

	gAgent.fireEvent(new LLEvent(&gAgent, "new group"), "");
}

// static
void LLAgent::processAgentDropGroup(LLMessageSystem *msg, void **)
{
	LLUUID	agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );

	if (agent_id != gAgentID)
	{
		llwarns << "processAgentDropGroup for agent other than me" << llendl;
		return;
	}

	LLUUID	group_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_GroupID, group_id );

	// Remove the group if it already exists remove it and add the new data to pick up changes.
	LLGroupData gd;
	gd.mID = group_id;
	S32 index = gAgent.mGroups.find(gd);
	if (index != -1)
	{
		gAgent.mGroups.remove(index);
		if (gAgent.getGroupID() == group_id)
		{
			gAgent.mGroupID.setNull();
			gAgent.mGroupPowers = 0;
			gAgent.mGroupName.clear();
			gAgent.mGroupTitle.clear();
		}
		
		// refresh all group information
		gAgent.sendAgentDataUpdateRequest();

		LLGroupMgr::getInstance()->clearGroupData(group_id);
		// close the floater for this group, if any.
		LLFloaterGroupInfo::closeGroup(group_id);
		// refresh the group panel of the search window, if necessary.
		LLFloaterDirectory::refreshGroup(group_id);
	}
	else
	{
		llwarns << "processAgentDropGroup, agent is not part of group " << group_id << llendl;
	}
}

class LLAgentDropGroupViewerNode : public LLHTTPNode
{
	virtual void post(
		LLHTTPNode::ResponsePtr response,
		const LLSD& context,
		const LLSD& input) const
	{

		if (
			!input.isMap() ||
			!input.has("body") )
		{
			//what to do with badly formed message?
			response->statusUnknownError(400);
			response->result(LLSD("Invalid message parameters"));
		}

		LLSD body = input["body"];
		if ( body.has("body") ) 
		{
			//stupid message system doubles up the "body"s
			body = body["body"];
		}

		if (
			body.has("AgentData") &&
			body["AgentData"].isArray() &&
			body["AgentData"][0].isMap() )
		{
			llinfos << "VALID DROP GROUP" << llendl;

			//there is only one set of data in the AgentData block
			LLSD agent_data = body["AgentData"][0];
			LLUUID agent_id;
			LLUUID group_id;

			agent_id = agent_data["AgentID"].asUUID();
			group_id = agent_data["GroupID"].asUUID();

			if (agent_id != gAgentID)
			{
				llwarns
					<< "AgentDropGroup for agent other than me" << llendl;

				response->notFound();
				return;
			}

			// Remove the group if it already exists remove it
			// and add the new data to pick up changes.
			LLGroupData gd;
			gd.mID = group_id;
			S32 index = gAgent.mGroups.find(gd);
			if (index != -1)
			{
				gAgent.mGroups.remove(index);
				if (gAgent.getGroupID() == group_id)
				{
					gAgent.mGroupID.setNull();
					gAgent.mGroupPowers = 0;
					gAgent.mGroupName.clear();
					gAgent.mGroupTitle.clear();
				}
		
				// refresh all group information
				gAgent.sendAgentDataUpdateRequest();

				LLGroupMgr::getInstance()->clearGroupData(group_id);
				// close the floater for this group, if any.
				LLFloaterGroupInfo::closeGroup(group_id);
				// refresh the group panel of the search window,
				//if necessary.
				LLFloaterDirectory::refreshGroup(group_id);
			}
			else
			{
				llwarns
					<< "AgentDropGroup, agent is not part of group "
					<< group_id << llendl;
			}

			response->result(LLSD());
		}
		else
		{
			//what to do with badly formed message?
			response->statusUnknownError(400);
			response->result(LLSD("Invalid message parameters"));
		}
	}
};

LLHTTPRegistration<LLAgentDropGroupViewerNode>
	gHTTPRegistrationAgentDropGroupViewerNode(
		"/message/AgentDropGroup");

// static
void LLAgent::processAgentGroupDataUpdate(LLMessageSystem *msg, void **)
{
	LLUUID	agent_id;

	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );

	if (agent_id != gAgentID)
	{
		llwarns << "processAgentGroupDataUpdate for agent other than me" << llendl;
		return;
	}	
	
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_GroupData);
	LLGroupData group;
	S32 index = -1;
	bool need_floater_update = false;
	for(S32 i = 0; i < count; ++i)
	{
		msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupID, group.mID, i);
		msg->getUUIDFast(_PREHASH_GroupData, _PREHASH_GroupInsigniaID, group.mInsigniaID, i);
		msg->getU64(_PREHASH_GroupData, "GroupPowers", group.mPowers, i);
		msg->getBOOL(_PREHASH_GroupData, "AcceptNotices", group.mAcceptNotices, i);
		msg->getS32(_PREHASH_GroupData, "Contribution", group.mContribution, i);
		msg->getStringFast(_PREHASH_GroupData, _PREHASH_GroupName, group.mName, i);
		
		if(group.mID.notNull())
		{
			need_floater_update = true;
			// Remove the group if it already exists remove it and add the new data to pick up changes.
			index = gAgent.mGroups.find(group);
			if (index != -1)
			{
				gAgent.mGroups.remove(index);
			}
			gAgent.mGroups.put(group);
		}
		if (need_floater_update)
		{
			update_group_floaters(group.mID);
		}
	}

}

class LLAgentGroupDataUpdateViewerNode : public LLHTTPNode
{
	virtual void post(
		LLHTTPNode::ResponsePtr response,
		const LLSD& context,
		const LLSD& input) const
	{
		LLSD body = input["body"];
		if(body.has("body"))
			body = body["body"];
		LLUUID agent_id = body["AgentData"][0]["AgentID"].asUUID();

		if (agent_id != gAgentID)
		{
			llwarns << "processAgentGroupDataUpdate for agent other than me" << llendl;
			return;
		}	

		LLSD group_data = body["GroupData"];

		LLSD::array_iterator iter_group =
			group_data.beginArray();
		LLSD::array_iterator end_group =
			group_data.endArray();
		int group_index = 0;
		for(; iter_group != end_group; ++iter_group)
		{

			LLGroupData group;
			S32 index = -1;
			bool need_floater_update = false;

			group.mID = (*iter_group)["GroupID"].asUUID();
			group.mPowers = ll_U64_from_sd((*iter_group)["GroupPowers"]);
			group.mAcceptNotices = (*iter_group)["AcceptNotices"].asBoolean();
			group.mListInProfile = body["NewGroupData"][group_index]["ListInProfile"].asBoolean();
			group.mInsigniaID = (*iter_group)["GroupInsigniaID"].asUUID();
			group.mName = (*iter_group)["GroupName"].asString();
			group.mContribution = (*iter_group)["Contribution"].asInteger();

			group_index++;

			if(group.mID.notNull())
			{
				need_floater_update = true;
				// Remove the group if it already exists remove it and add the new data to pick up changes.
				index = gAgent.mGroups.find(group);
				if (index != -1)
				{
					gAgent.mGroups.remove(index);
				}
				gAgent.mGroups.put(group);
			}
			if (need_floater_update)
			{
				update_group_floaters(group.mID);
			}
		}
	}
};

LLHTTPRegistration<LLAgentGroupDataUpdateViewerNode >
	gHTTPRegistrationAgentGroupDataUpdateViewerNode ("/message/AgentGroupDataUpdate"); 

// static
void LLAgent::processAgentDataUpdate(LLMessageSystem *msg, void **)
{
	LLUUID	agent_id;

	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );

	if (agent_id != gAgentID)
	{
		llwarns << "processAgentDataUpdate for agent other than me" << llendl;
		return;
	}

	msg->getStringFast(_PREHASH_AgentData, _PREHASH_GroupTitle, gAgent.mGroupTitle);
	LLUUID active_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_ActiveGroupID, active_id);


	if(active_id.notNull())
	{
		gAgent.mGroupID = active_id;
		msg->getU64(_PREHASH_AgentData, "GroupPowers", gAgent.mGroupPowers);
		msg->getString(_PREHASH_AgentData, _PREHASH_GroupName, gAgent.mGroupName);
	}
	else
	{
		gAgent.mGroupID.setNull();
		gAgent.mGroupPowers = 0;
		gAgent.mGroupName.clear();
	}		

	update_group_floaters(active_id);
}

// static
void LLAgent::processScriptControlChange(LLMessageSystem *msg, void **)
{
	S32 block_count = msg->getNumberOfBlocks("Data");
	for (S32 block_index = 0; block_index < block_count; block_index++)
	{
		BOOL take_controls;
		U32	controls;
		BOOL passon;
		U32 i;
		msg->getBOOL("Data", "TakeControls", take_controls, block_index);
		if (take_controls)
		{
			// take controls
			msg->getU32("Data", "Controls", controls, block_index );
			msg->getBOOL("Data", "PassToAgent", passon, block_index );
			U32 total_count = 0;
			for (i = 0; i < TOTAL_CONTROLS; i++)
			{
				if (controls & ( 1 << i))
				{
					if (passon)
					{
						gAgent.mControlsTakenPassedOnCount[i]++;
					}
					else
					{
						gAgent.mControlsTakenCount[i]++;
					}
					total_count++;
				}
			}
		
			// Any control taken?  If so, might be first time.
			if (total_count > 0)
			{
				LLFirstUse::useOverrideKeys();
			}
		}
		else
		{
			// release controls
			msg->getU32("Data", "Controls", controls, block_index );
			msg->getBOOL("Data", "PassToAgent", passon, block_index );
			for (i = 0; i < TOTAL_CONTROLS; i++)
			{
				if (controls & ( 1 << i))
				{
					if (passon)
					{
						gAgent.mControlsTakenPassedOnCount[i]--;
						if (gAgent.mControlsTakenPassedOnCount[i] < 0)
						{
							gAgent.mControlsTakenPassedOnCount[i] = 0;
						}
					}
					else
					{
						gAgent.mControlsTakenCount[i]--;
						if (gAgent.mControlsTakenCount[i] < 0)
						{
							gAgent.mControlsTakenCount[i] = 0;
						}
					}
				}
			}
		}
	}
}

/*
// static
void LLAgent::processControlTake(LLMessageSystem *msg, void **)
{
	U32	controls;
	msg->getU32("Data", "Controls", controls );
	U32 passon;
	msg->getBOOL("Data", "PassToAgent", passon );

	S32 i;
	S32 total_count = 0;
	for (i = 0; i < TOTAL_CONTROLS; i++)
	{
		if (controls & ( 1 << i))
		{
			if (passon)
			{
				gAgent.mControlsTakenPassedOnCount[i]++;
			}
			else
			{
				gAgent.mControlsTakenCount[i]++;
			}
			total_count++;
		}
	}

	// Any control taken?  If so, might be first time.
	if (total_count > 0)
	{
		LLFirstUse::useOverrideKeys();
	}
}

// static
void LLAgent::processControlRelease(LLMessageSystem *msg, void **)
{
	U32	controls;
	msg->getU32("Data", "Controls", controls );
	U32 passon;
	msg->getBOOL("Data", "PassToAgent", passon );

	S32 i;
	for (i = 0; i < TOTAL_CONTROLS; i++)
	{
		if (controls & ( 1 << i))
		{
			if (passon)
			{
				gAgent.mControlsTakenPassedOnCount[i]--;
				if (gAgent.mControlsTakenPassedOnCount[i] < 0)
				{
					gAgent.mControlsTakenPassedOnCount[i] = 0;
				}
			}
			else
			{
				gAgent.mControlsTakenCount[i]--;
				if (gAgent.mControlsTakenCount[i] < 0)
				{
					gAgent.mControlsTakenCount[i] = 0;
				}
			}
		}
	}
}
*/

//static
void LLAgent::processAgentCachedTextureResponse(LLMessageSystem *mesgsys, void **user_data)
{
	gAgentQueryManager.mNumPendingQueries--;

	if (!isAgentAvatarValid() || gAgentAvatarp->isDead())
	{
		llwarns << "No avatar for user in cached texture update!" << llendl;
		return;
	}

	if (isAgentAvatarValid() && gAgentCamera.cameraCustomizeAvatar())
	{
		// ignore baked textures when in customize mode
		return;
	}

	S32 query_id;
	mesgsys->getS32Fast(_PREHASH_AgentData, _PREHASH_SerialNum, query_id);

	S32 num_texture_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_WearableData);


	S32 num_results = 0;
	for (S32 texture_block = 0; texture_block < num_texture_blocks; texture_block++)
	{
		LLUUID texture_id;
		U8 texture_index;

		mesgsys->getUUIDFast(_PREHASH_WearableData, _PREHASH_TextureID, texture_id, texture_block);
		mesgsys->getU8Fast(_PREHASH_WearableData, _PREHASH_TextureIndex, texture_index, texture_block);


		if ((S32)texture_index < TEX_NUM_INDICES )
		{
			const LLVOAvatarDictionary::TextureEntry *texture_entry = LLVOAvatarDictionary::instance().getTexture((ETextureIndex)texture_index);
			if (texture_entry)
			{
				EBakedTextureIndex baked_index = texture_entry->mBakedTextureIndex;

				if (gAgentQueryManager.mActiveCacheQueries[baked_index] == query_id)
				{
					if (texture_id.notNull())
					{
						//llinfos << "Received cached texture " << (U32)texture_index << ": " << texture_id << llendl;
						gAgentAvatarp->setCachedBakedTexture((ETextureIndex)texture_index, texture_id);
						//gAgentAvatarp->setTETexture( LLVOAvatar::sBakedTextureIndices[texture_index], texture_id );
						gAgentQueryManager.mActiveCacheQueries[baked_index] = 0;
						num_results++;
					}
					else
					{
						// no cache of this bake. request upload.
						gAgentAvatarp->requestLayerSetUpload(baked_index);
					}
				}
			}
		}
	}

	llinfos << "Received cached texture response for " << num_results << " textures." << llendl;

	gAgentAvatarp->updateMeshTextures();

	if (gAgentQueryManager.mNumPendingQueries == 0)
	{
		// RN: not sure why composites are disabled at this point
		gAgentAvatarp->setCompositeUpdatesEnabled(TRUE);
		gAgent.sendAgentSetAppearance();
	}
}

BOOL LLAgent::anyControlGrabbed() const
{
	for (U32 i = 0; i < TOTAL_CONTROLS; i++)
	{
		if (gAgent.mControlsTakenCount[i] > 0)
			return TRUE;
		if (gAgent.mControlsTakenPassedOnCount[i] > 0)
			return TRUE;
	}
	return FALSE;
}

BOOL LLAgent::isControlGrabbed(S32 control_index) const
{
	return mControlsTakenCount[control_index] > 0;
}

void LLAgent::forceReleaseControls()
{
	gMessageSystem->newMessage("ForceScriptControlRelease");
	gMessageSystem->nextBlock("AgentData");
	gMessageSystem->addUUID("AgentID", getID());
	gMessageSystem->addUUID("SessionID", getSessionID());
	sendReliableMessage();
}

void LLAgent::setHomePosRegion( const U64& region_handle, const LLVector3& pos_region)
{
	mHaveHomePosition = TRUE;
	mHomeRegionHandle = region_handle;
	mHomePosRegion = pos_region;
}

BOOL LLAgent::getHomePosGlobal( LLVector3d* pos_global )
{
	if(!mHaveHomePosition)
	{
		return FALSE;
	}
	F32 x = 0;
	F32 y = 0;
	from_region_handle( mHomeRegionHandle, &x, &y);
	pos_global->setVec( x + mHomePosRegion.mV[VX], y + mHomePosRegion.mV[VY], mHomePosRegion.mV[VZ] );
	return TRUE;
}

void LLAgent::clearVisualParams(void *data)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->clearVisualParamWeights();
		gAgentAvatarp->updateVisualParams();
	}
}

//---------------------------------------------------------------------------
// Teleport
//---------------------------------------------------------------------------

// teleportCore() - stuff to do on any teleport
// protected
bool LLAgent::teleportCore(bool is_local)
{
	if(TELEPORT_NONE != mTeleportState)
	{
		llwarns << "Attempt to teleport when already teleporting." << llendl;
		// <edit>
		//return false;
		teleportCancel();
		// </edit>
	}

#if 0
	// This should not exist. It has been added, removed, added, and now removed again.
	// This change needs to come from the simulator. Otherwise, the agent ends up out of
	// sync with other viewers. Discuss in DEV-14145/VWR-6744 before reenabling.

	// Stop all animation before actual teleporting 
        if (isAgentAvatarValid())
	{
		for ( LLVOAvatar::AnimIterator anim_it= gAgentAvatarp->mPlayingAnimations.begin();
		      anim_it != gAgentAvatarp->mPlayingAnimations.end();
		      ++anim_it)
               {
                       gAgentAvatarp->stopMotion(anim_it->first);
               }
               gAgentAvatarp->processAnimationStateChanges();
       }
#endif

	// Don't call LLFirstUse::useTeleport because we don't know
	// yet if the teleport will succeed.  Look in 
	// process_teleport_location_reply

	// close the map and find panels so we can see our destination
	LLFloaterWorldMap::hide(NULL);
	LLFloaterDirectory::hide(NULL);

	// hide land floater too - it'll be out of date
	LLFloaterLand::hideInstance();
	
	LLViewerParcelMgr::getInstance()->deselectLand();
	LLViewerMediaFocus::getInstance()->setFocusFace(false, NULL, 0, NULL);

	// Close all pie menus, deselect land, etc.
	// Don't change the camera until we know teleport succeeded. JC
	// <edit>
	if(gAgentCamera.getFocusOnAvatar())
	// </edit>
	gAgentCamera.resetView(FALSE);

	// local logic
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_TELEPORT_COUNT);
	if (is_local)
	{
		gAgent.setTeleportState( LLAgent::TELEPORT_LOCAL );
	}
	else
	{
		gTeleportDisplay = TRUE;
		gAgent.setTeleportState( LLAgent::TELEPORT_START );

		//release geometry from old location
		gPipeline.resetVertexBuffers();

		if (gSavedSettings.getBOOL("SpeedRez"))
		{
			F32 draw_distance = gSavedSettings.getF32("RenderFarClip");
			if (gSavedDrawDistance < draw_distance)
			{
				gSavedDrawDistance = draw_distance;
			}
			gSavedSettings.setF32("SavedRenderFarClip", gSavedDrawDistance);
			gSavedSettings.setF32("RenderFarClip", 32.0f);
		}
		if(gSavedSettings.getBOOL("OptionPlayTpSound"))
			make_ui_sound("UISndTeleportOut");
	}
	
	// MBW -- Let the voice client know a teleport has begun so it can leave the existing channel.
	// This was breaking the case of teleporting within a single sim.  Backing it out for now.
//	gVoiceClient->leaveChannel();
	
	return true;
}

void LLAgent::teleportRequest(
	const U64& region_handle,
	const LLVector3& pos_local,
	bool look_at_from_camera)
{
	LLViewerRegion* regionp = getRegion();
	bool is_local = (region_handle == to_region_handle(getPositionGlobal()));
	if(regionp && teleportCore(is_local))
	{
		LL_INFOS("") << "TeleportLocationRequest: '" << region_handle << "':"
					 << pos_local << LL_ENDL;
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("TeleportLocationRequest");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, getID());
		msg->addUUIDFast(_PREHASH_SessionID, getSessionID());
		msg->nextBlockFast(_PREHASH_Info);
		msg->addU64("RegionHandle", region_handle);
		msg->addVector3("Position", pos_local);
		// <edit>
		//LLVector3 look_at(0,1,0);
		LLVector3 look_at = LLViewerCamera::getInstance()->getAtAxis();
		/*if (look_at_from_camera)
		{
			look_at = LLViewerCamera::getInstance()->getAtAxis();
		}*/
		// </edit>
		msg->addVector3("LookAt", look_at);
		sendReliableMessage();
	}
}

// Landmark ID = LLUUID::null means teleport home
void LLAgent::teleportViaLandmark(const LLUUID& landmark_asset_id)
{
// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.0.0d)
	if ( (rlv_handler_t::isEnabled()) &&
		 ( (gRlvHandler.hasBehaviour(RLV_BHVR_TPLM)) || 
		   ((gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) && (gAgentAvatarp) && (gAgentAvatarp->isSitting())) ))
	{
		RlvNotifications::notifyBlockedTeleport();
		return;
	}
// [/RLVa:KB]

	LLViewerRegion *regionp = getRegion();
	if(regionp && teleportCore())
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_TeleportLandmarkRequest);
		msg->nextBlockFast(_PREHASH_Info);
		msg->addUUIDFast(_PREHASH_AgentID, getID());
		msg->addUUIDFast(_PREHASH_SessionID, getSessionID());
		msg->addUUIDFast(_PREHASH_LandmarkID, landmark_asset_id);
		sendReliableMessage();
	}
}

void LLAgent::teleportViaLure(const LLUUID& lure_id, BOOL godlike)
{
	LLViewerRegion* regionp = getRegion();
	if(regionp && teleportCore())
	{
		U32 teleport_flags = 0x0;
		if (godlike)
		{
			teleport_flags |= TELEPORT_FLAGS_VIA_GODLIKE_LURE;
			teleport_flags |= TELEPORT_FLAGS_DISABLE_CANCEL;
		}
		else
		{
			teleport_flags |= TELEPORT_FLAGS_VIA_LURE;
		}

		// send the message
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_TeleportLureRequest);
		msg->nextBlockFast(_PREHASH_Info);
		msg->addUUIDFast(_PREHASH_AgentID, getID());
		msg->addUUIDFast(_PREHASH_SessionID, getSessionID());
		msg->addUUIDFast(_PREHASH_LureID, lure_id);
		// teleport_flags is a legacy field, now derived sim-side:
		msg->addU32("TeleportFlags", teleport_flags);
		sendReliableMessage();
	}	
}


// James Cook, July 28, 2005
void LLAgent::teleportCancel()
{
	LLViewerRegion* regionp = getRegion();
	if(regionp)
	{
		// send the message
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("TeleportCancel");
		msg->nextBlockFast(_PREHASH_Info);
		msg->addUUIDFast(_PREHASH_AgentID, getID());
		msg->addUUIDFast(_PREHASH_SessionID, getSessionID());
		sendReliableMessage();
	}	
	gTeleportDisplay = FALSE;
	gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
}


void LLAgent::teleportViaLocation(const LLVector3d& pos_global)
{
// [RLVa:KB] - Alternate: Snowglobe-1.2.4 | Checked: 2010-03-02 (RLVa-1.1.1a) | Modified: RLVa-1.2.0a
	if (rlv_handler_t::isEnabled()) 
	{
		// If we're getting teleported due to @tpto we should disregard any @tploc=n or @unsit=n restrictions from the same object
		if ( (gRlvHandler.hasBehaviourExcept(RLV_BHVR_TPLOC, gRlvHandler.getCurrentObject())) ||
		     ( (gAgentAvatarp) && (gAgentAvatarp->isSitting()) && 
			   (gRlvHandler.hasBehaviourExcept(RLV_BHVR_UNSIT, gRlvHandler.getCurrentObject()))) )
		{
			RlvNotifications::notifyBlockedTeleport();
			return;
		}

		if ( (gRlvHandler.getCurrentCommand()) && (RLV_BHVR_TPTO == gRlvHandler.getCurrentCommand()->getBehaviourType()) )
		{
			gRlvHandler.setCanCancelTp(false);
		}
	}
// [/RLVa:KB]

	LLViewerRegion* regionp = getRegion();
	U64 handle = to_region_handle(pos_global);
	LLSimInfo* info = LLWorldMap::getInstance()->simInfoFromHandle(handle);
	bool calc = gSavedSettings.getBOOL("OptionOffsetTPByAgentHeight");
	LLVector3 offset = LLVector3(0.f,0.f,0.f);
	if(calc)
		offset += LLVector3(0.f,0.f,gAgentAvatarp->getScale().mV[2] / 2.0);
	if(regionp && info)
	{
		LLVector3d region_origin = info->getGlobalOrigin();
		LLVector3 pos_local(
			(F32)(pos_global.mdV[VX] - region_origin.mdV[VX]),
			(F32)(pos_global.mdV[VY] - region_origin.mdV[VY]),
			(F32)(pos_global.mdV[VZ]));
		pos_local += offset;
		teleportRequest(info->getHandle(), pos_local);
	}
	else if(regionp && 
		teleportCore(regionp->getHandle() == to_region_handle_global((F32)pos_global.mdV[VX], (F32)pos_global.mdV[VY])))
	{
		llwarns << "Using deprecated teleportlocationrequest." << llendl; 
		// send the message
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_TeleportLocationRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, getID());
		msg->addUUIDFast(_PREHASH_SessionID, getSessionID());

		msg->nextBlockFast(_PREHASH_Info);
		F32 width = regionp->getWidth();
		LLVector3 pos(fmod((F32)pos_global.mdV[VX], width),
					  fmod((F32)pos_global.mdV[VY], width),
					  (F32)pos_global.mdV[VZ]);
		F32 region_x = (F32)(pos_global.mdV[VX]);
		F32 region_y = (F32)(pos_global.mdV[VY]);
		U64 region_handle = to_region_handle_global(region_x, region_y);
		msg->addU64Fast(_PREHASH_RegionHandle, region_handle);
		msg->addVector3Fast(_PREHASH_Position, pos);
		pos.mV[VX] += 1;
		// <edit>
		LLVector3 lookat = LLViewerCamera::getInstance()->getAtAxis();
		//msg->addVector3Fast(_PREHASH_LookAt, pos);
		msg->addVector3Fast(_PREHASH_LookAt, lookat);
		// </edit>
		sendReliableMessage();
	}
}

// Teleport to global position, but keep facing in the same direction 
void LLAgent::teleportViaLocationLookAt(const LLVector3d& pos_global)
{
// [RLVa:KB] - Checked: 2010-10-07 (RLVa-1.2.1f) | Added: RLVa-1.2.1f
	// RELEASE-RLVa: [SL-2.2.0] Make sure this isn't used for anything except double-click teleporting
	if ( (rlv_handler_t::isEnabled()) && (!RlvUtil::isForceTp()) && 
		 ((gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)) || (!gRlvHandler.canStand())) )
	{
		RlvNotifications::notifyBlockedTeleport();
		return;
	}
// [/RLVa:KB]

	mbTeleportKeepsLookAt = true;
	gAgentCamera.setFocusOnAvatar(FALSE, ANIMATE);	// detach camera form avatar, so it keeps direction
	U64 region_handle = to_region_handle(pos_global);
	LLSimInfo* simInfo = LLWorldMap::instance().simInfoFromHandle(region_handle);
	if(simInfo)
		region_handle = simInfo->getHandle();

	LLVector3 pos_local = (LLVector3)(pos_global - from_region_handle(region_handle));
	teleportRequest(region_handle, pos_local, getTeleportKeepsLookAt());
}

void LLAgent::setTeleportState(ETeleportState state)
{
	mTeleportState = state;
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if (mTeleportState > TELEPORT_NONE && freeze_time)
	{
		LLFloaterSnapshot::hide(0);
	}

	switch (mTeleportState)
	
	{	
		case TELEPORT_NONE:
			mbTeleportKeepsLookAt = false;
			break;

		case TELEPORT_MOVING:
		// We're outa here. Save "back" slurl.
		mTeleportSourceSLURL = getSLURL();
			break;

		case TELEPORT_ARRIVING:
		// First two position updates after a teleport tend to be weird
		LLViewerStats::getInstance()->mAgentPositionSnaps.mCountOfNextUpdatesToIgnore = 2;

		// Let the interested parties know we've teleported.
		LLViewerParcelMgr::getInstance()->onTeleportFinished(false, getPositionGlobal());
			break;

		default:
			break;
	}
// [RLVa:KB] - Alternate: Snowglobe-1.2.4 | Version: 1.23.4 | Checked: 2009-07-07 (RLVa-1.0.0d) | Added: RLVa-0.2.0b
	if ( (rlv_handler_t::isEnabled()) && (TELEPORT_NONE == mTeleportState) )
	{
		gRlvHandler.setCanCancelTp(true);
	}
// [/RLVa:KB]
}

void LLAgent::stopCurrentAnimations()
{
	// This function stops all current overriding animations on this
	// avatar, propagating this change back to the server.
	if (isAgentAvatarValid())
	{
		for ( LLVOAvatar::AnimIterator anim_it =
			      gAgentAvatarp->mPlayingAnimations.begin();
		      anim_it != gAgentAvatarp->mPlayingAnimations.end();
		      anim_it++)
		{
			if (anim_it->first ==
			    ANIM_AGENT_SIT_GROUND_CONSTRAINED)
			{
				// don't cancel a ground-sit anim, as viewers
				// use this animation's status in
				// determining whether we're sitting. ick.
			}
			else
			{
				// stop this animation locally
				gAgentAvatarp->stopMotion(anim_it->first, TRUE);
				// ...and tell the server to tell everyone.
				sendAnimationRequest(anim_it->first, ANIM_REQUEST_STOP);
			}
		}

		// re-assert at least the default standing animation, because
		// viewers get confused by avs with no associated anims.
		sendAnimationRequest(ANIM_AGENT_STAND,
				     ANIM_REQUEST_START);
	}
}

void LLAgent::fidget()
{
	if (!getAFK())
	{
		F32 curTime = mFidgetTimer.getElapsedTimeF32();
		if (curTime > mNextFidgetTime)
		{
			// pick a random fidget anim here
			S32 oldFidget = mCurrentFidget;

			mCurrentFidget = ll_rand(NUM_AGENT_STAND_ANIMS);

			if (mCurrentFidget != oldFidget)
			{
				//LLAgent::stopFidget();
				// <edit>
				// for the sack of smaller packets, make this cancel the last one only
				if(oldFidget != 0)
					sendAnimationRequest(AGENT_STAND_ANIMS[oldFidget],ANIM_REQUEST_STOP);
				// </edit>
				
				switch(mCurrentFidget)
				{
				case 0:
					mCurrentFidget = 0;
					break;
				case 1:
					sendAnimationRequest(ANIM_AGENT_STAND_1, ANIM_REQUEST_START);
					mCurrentFidget = 1;
					break;
				case 2:
					sendAnimationRequest(ANIM_AGENT_STAND_2, ANIM_REQUEST_START);
					mCurrentFidget = 2;
					break;
				case 3:
					sendAnimationRequest(ANIM_AGENT_STAND_3, ANIM_REQUEST_START);
					mCurrentFidget = 3;
					break;
				case 4:
					sendAnimationRequest(ANIM_AGENT_STAND_4, ANIM_REQUEST_START);
					mCurrentFidget = 4;
					break;
				}
			}

			// calculate next fidget time
			mNextFidgetTime = curTime + ll_frand(MAX_FIDGET_TIME - MIN_FIDGET_TIME) + MIN_FIDGET_TIME;
		}
	}
}

void LLAgent::stopFidget()
{
	LLDynamicArray<LLUUID> anims;
	anims.put(ANIM_AGENT_STAND_1);
	anims.put(ANIM_AGENT_STAND_2);
	anims.put(ANIM_AGENT_STAND_3);
	anims.put(ANIM_AGENT_STAND_4);

	gAgent.sendAnimationRequests(anims, ANIM_REQUEST_STOP);
}


void LLAgent::requestEnterGodMode()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_RequestGodlikePowers);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_RequestBlock);
	msg->addBOOLFast(_PREHASH_Godlike, TRUE);
	msg->addUUIDFast(_PREHASH_Token, LLUUID::null);

	// simulators need to know about your request
	sendReliableMessage();
}

void LLAgent::requestLeaveGodMode()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_RequestGodlikePowers);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_RequestBlock);
	msg->addBOOLFast(_PREHASH_Godlike, FALSE);
	msg->addUUIDFast(_PREHASH_Token, LLUUID::null);

	// simulator needs to know about your request
	sendReliableMessage();
}

//-----------------------------------------------------------------------------
// sendAgentSetAppearance()
//-----------------------------------------------------------------------------
void LLAgent::sendAgentSetAppearance()
{
	if (!isAgentAvatarValid()) return;

	if (gAgentQueryManager.mNumPendingQueries > 0 && (isAgentAvatarValid() && !gAgentCamera.cameraCustomizeAvatar())) 
	{
		return;
	}


	llinfos << "TAT: Sent AgentSetAppearance: " << gAgentAvatarp->getBakedStatusForPrintout() << llendl;
	//dumpAvatarTEs( "sendAgentSetAppearance()" );

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_AgentSetAppearance);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, getID());
	msg->addUUIDFast(_PREHASH_SessionID, getSessionID());

	// correct for the collision tolerance (to make it look like the 
	// agent is actually walking on the ground/object)
	// NOTE -- when we start correcting all of the other Havok geometry 
	// to compensate for the COLLISION_TOLERANCE ugliness we will have 
	// to tweak this number again
	LLVector3 body_size = gAgentAvatarp->mBodySize;

	body_size.mV[VX] = body_size.mV[VX] + gSavedSettings.getF32("AscentAvatarXModifier");
	body_size.mV[VY] = body_size.mV[VY] + gSavedSettings.getF32("AscentAvatarYModifier");
	body_size.mV[VZ] = body_size.mV[VZ] + gSavedSettings.getF32("AscentAvatarZModifier");

	msg->addVector3Fast(_PREHASH_Size, body_size);	

	// To guard against out of order packets
	// Note: always start by sending 1.  This resets the server's count. 0 on the server means "uninitialized"
	mAppearanceSerialNum++;
	msg->addU32Fast(_PREHASH_SerialNum, mAppearanceSerialNum );

	// is texture data current relative to wearables?
	// KLW - TAT this will probably need to check the local queue.
	BOOL textures_current = !gAgentAvatarp->hasPendingBakedUploads() && gAgentWearables.areWearablesLoaded();

	for(U8 baked_index = 0; baked_index < BAKED_NUM_INDICES; baked_index++ )
	{
		const ETextureIndex texture_index = LLVOAvatarDictionary::bakedToLocalTextureIndex((EBakedTextureIndex)baked_index);

		// if we're not wearing a skirt, we don't need the texture to be baked
		if (texture_index == TEX_SKIRT_BAKED && !gAgentAvatarp->isWearingWearableType(LLWearableType::WT_SKIRT))
		{
			continue;
		}

		// IMG_DEFAULT_AVATAR means not baked
		if (!gAgentAvatarp->isTextureDefined(texture_index))
		{
			textures_current = FALSE;
			break;
		}
	}

	// only update cache entries if we have all our baked textures
	if (textures_current)
	{
		llinfos << "TAT: Sending cached texture data" << llendl;
		for (U8 baked_index = 0; baked_index < BAKED_NUM_INDICES; baked_index++)
		{

			const LLUUID hash = gAgentWearables.computeBakedTextureHash((EBakedTextureIndex) baked_index, true);		
			if (hash.notNull())
			{
				const ETextureIndex texture_index = LLVOAvatarDictionary::bakedToLocalTextureIndex((EBakedTextureIndex)baked_index);

				msg->nextBlockFast(_PREHASH_WearableData);
				msg->addUUIDFast(_PREHASH_CacheID, hash);
				msg->addU8Fast(_PREHASH_TextureIndex, (U8)texture_index);
			}
		}
		msg->nextBlockFast(_PREHASH_ObjectData);

		/*if (gSavedSettings.getBOOL("AscentUseCustomTag"))
		{
			LLColor4 color;
			if (!gSavedSettings.getBOOL("AscentStoreSettingsPerAccount"))
			{
				color = gSavedSettings.setColor4("AscentCustomTagColor");
			}
			else
			{
				color = gSavedPerAccountSettings.getColor4("AscentCustomTagColor");
			}
			LLUUID old_teid;
			U8 client_buffer[UUID_BYTES];
			memset(&client_buffer, 0, UUID_BYTES);
			LLTextureEntry* entry = (LLTextureEntry*)gAgentAvatarp->getTE(0);
			old_teid = entry->getID();
			//You edit this to change the tag in your client. Yes.
			const char* tag_client = "Ascent";
			strncpy((char*)&client_buffer[0], tag_client, UUID_BYTES);
			LLUUID part_a;
			memcpy(&part_a.mData, &client_buffer[0], UUID_BYTES);
			entry->setColor(color);
			//This glow is used to tell if the tag color and name is set or not.
			entry->setGlow(0.1f);
			entry->setID(part_a);
			gAgentAvatarp->packTEMessage( gMessageSystem, 1, gSavedSettings.getString("AscentReportClientUUID") );
			entry->setID(old_teid);
			
		}
		else
		{*/
			if (gSavedSettings.getBOOL("AscentUseTag"))
				gAgentAvatarp->packTEMessage( gMessageSystem, 1, gSavedSettings.getString("AscentReportClientUUID"));
			else
				gAgentAvatarp->packTEMessage( gMessageSystem, 1, "c228d1cf-4b5d-4ba8-84f4-899a0796aa97");
		//}
		resetClientTag();
		
	}
	else
	{
		// If the textures aren't baked, send NULL for texture IDs
		// This means the baked texture IDs on the server will be untouched.
		// Once all textures are baked, another AvatarAppearance message will be sent to update the TEs
		msg->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addBinaryDataFast(_PREHASH_TextureEntry, NULL, 0);
	}


	static bool send_physics_params = false;
	send_physics_params |= !!gAgentWearables.selfHasWearable(LLWearableType::WT_PHYSICS);
	S32 transmitted_params = 0;
	for (LLViewerVisualParam* param = (LLViewerVisualParam*)gAgentAvatarp->getFirstVisualParam();
		 param;
		 param = (LLViewerVisualParam*)gAgentAvatarp->getNextVisualParam())
	{
		if (param->getGroup() == VISUAL_PARAM_GROUP_TWEAKABLE) // do not transmit params of group VISUAL_PARAM_GROUP_TWEAKABLE_NO_TRANSMIT
		{
			//A hack to prevent ruthing on older viewers when phys wearables aren't being worn.
			if(!send_physics_params && param->getID() >= 10000)
			{
				break;
			}
			msg->nextBlockFast(_PREHASH_VisualParam );
			// We don't send the param ids.  Instead, we assume that the receiver has the same params in the same sequence.
			const F32 param_value = param->getWeight();
			const U8 new_weight = F32_to_U8(param_value, param->getMinWeight(), param->getMaxWeight());

			msg->addU8Fast(_PREHASH_ParamValue, new_weight );
			transmitted_params++;
		}
	}

//	llinfos << "Avatar XML num VisualParams transmitted = " << transmitted_params << llendl;
	sendReliableMessage();
}

void LLAgent::sendAgentDataUpdateRequest()
{
	gMessageSystem->newMessageFast(_PREHASH_AgentDataUpdateRequest);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	sendReliableMessage();
}

void LLAgent::sendAgentUserInfoRequest()
{
	if(getID().isNull())
		return; // not logged in
	gMessageSystem->newMessageFast(_PREHASH_UserInfoRequest);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, getSessionID());
	sendReliableMessage();
}

void LLAgent::observeFriends()
{
	if(!mFriendObserver)
	{
		mFriendObserver = new LLAgentFriendObserver;
		LLAvatarTracker::instance().addObserver(mFriendObserver);
		friendsChanged();
	}
}

void LLAgent::parseTeleportMessages(const std::string& xml_filename)
{
	LLXMLNodePtr root;
	BOOL success = LLUICtrlFactory::getLayeredXMLNode(xml_filename, root);

	if (!success || !root || !root->hasName( "teleport_messages" ))
	{
		llerrs << "Problem reading teleport string XML file: " 
			   << xml_filename << llendl;
		return;
	}

	for (LLXMLNode* message_set = root->getFirstChild();
		 message_set != NULL;
		 message_set = message_set->getNextSibling())
	{
		if ( !message_set->hasName("message_set") ) continue;

		std::map<std::string, std::string> *teleport_msg_map = NULL;
		std::string message_set_name;

		if ( message_set->getAttributeString("name", message_set_name) )
		{
			//now we loop over all the string in the set and add them
			//to the appropriate set
			if ( message_set_name == "errors" )
			{
				teleport_msg_map = &sTeleportErrorMessages;
			}
			else if ( message_set_name == "progress" )
			{
				teleport_msg_map = &sTeleportProgressMessages;
			}
		}

		if ( !teleport_msg_map ) continue;

		std::string message_name;
		for (LLXMLNode* message_node = message_set->getFirstChild();
			 message_node != NULL;
			 message_node = message_node->getNextSibling())
		{
			if ( message_node->hasName("message") && 
				 message_node->getAttributeString("name", message_name) )
			{
				(*teleport_msg_map)[message_name] =
					message_node->getTextContents();
			} //end if ( message exists and has a name)
		} //end for (all message in set)
	}//end for (all message sets in xml file)
}


void LLAgent::sendAgentUpdateUserInfo(bool im_via_email, const std::string& directory_visibility )
{
	gMessageSystem->newMessageFast(_PREHASH_UpdateUserInfo);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_UserData);
	gMessageSystem->addBOOLFast(_PREHASH_IMViaEMail, im_via_email);
	gMessageSystem->addString("DirectoryVisibility", directory_visibility);
	gAgent.sendReliableMessage();
}
// Draw a representation of current autopilot target
void LLAgent::renderAutoPilotTarget()
{
	if (mAutoPilot)
	{
		F32 height_meters;
		LLVector3d target_global;

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();

		// not textured
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		// lovely green
		gGL.color4f(0.f, 1.f, 1.f, 1.f);

		target_global = mAutoPilotTargetGlobal;

		gGL.translatef((F32)(target_global.mdV[VX]), (F32)(target_global.mdV[VY]), (F32)(target_global.mdV[VZ]));

		height_meters = 1.f;

		gGL.scalef(height_meters, height_meters, height_meters);

		gSphere.render();

		gGL.popMatrix();
	}
}

void LLAgent::showLureDestination(const std::string fromname, const int global_x, const int global_y, const int x, const int y, const int z, const std::string maturity)
{
	const LLVector3d posglobal = LLVector3d(F64(global_x), F64(global_y), F64(0));
	LLSimInfo* siminfo = LLWorldMap::getInstance()->simInfoFromPosGlobal(posglobal);

	if(mPendingLure)
		delete mPendingLure;
	mPendingLure = new SHLureRequest(fromname,posglobal,x,y,z);

	if(siminfo) //We already have an entry? Go right on to displaying it.
	{
		onFoundLureDestination(siminfo);
	}
	else
	{
		U16 grid_x = (U16)(global_x / REGION_WIDTH_UNITS);
		U16 grid_y = (U16)(global_y / REGION_WIDTH_UNITS);
		LLWorldMap::getInstance()->sendMapBlockRequest(grid_x, grid_y, grid_x, grid_y, true); //Will call onFoundLureDestination on response
	}
}

void LLAgent::onFoundLureDestination(LLSimInfo *siminfo)
{
	if(!mPendingLure)
		return; 

	if(!siminfo)
		siminfo = LLWorldMap::getInstance()->simInfoFromPosGlobal(mPendingLure->mPosGlobal);
	if(siminfo)
	{
		const std::string sim_name = siminfo->getName();
		const std::string maturity = siminfo->getAccessString();

		llinfos << mPendingLure->mAvatarName << "'s teleport lure is to " << sim_name << " (" << maturity << ")" << llendl;
		LLStringUtil::format_map_t args;
		args["[NAME]"] = mPendingLure->mAvatarName;
		args["[DESTINATION]"] = LLURLDispatcher::buildSLURL(sim_name, (S32)mPendingLure->mPosLocal[0], (S32)mPendingLure->mPosLocal[1], (S32)mPendingLure->mPosLocal[2] );
		std::string msg = LLTrans::getString("TeleportOfferMaturity", args);
		if (!maturity.empty())
		{
			msg.append(llformat(" (%s)", maturity.c_str()));
		}
		LLChat chat(msg);
		LLFloaterChat::addChat(chat);
	}
	else
		llwarns << "Grand scheme failed" << llendl;
	delete mPendingLure;
	mPendingLure = NULL;
}

/********************************************************************************/

LLAgentQueryManager gAgentQueryManager;

LLAgentQueryManager::LLAgentQueryManager() :
	mWearablesCacheQueryID(0),
	mNumPendingQueries(0),
	mUpdateSerialNum(0)
{
	for (U32 i = 0; i < BAKED_NUM_INDICES; i++)
	{
		mActiveCacheQueries[i] = 0;
	}
}

LLAgentQueryManager::~LLAgentQueryManager()
{
}

// EOF

