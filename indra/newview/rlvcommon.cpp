/** 
 *
 * Copyright (c) 2009-2010, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU General Public License, version 2.0, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the GPL can be found in doc/GPL-license.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/gpl-2.0.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#include "llviewerprecompiledheaders.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llavatarnamecache.h"
#include "llnotificationsutil.h"
#include "lluictrlfactory.h"
#include "llversionviewer.h"
#include "llviewermenu.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llvoavatar.h"
#include "llworld.h"

#include "rlvcommon.h"
#include "rlvhandler.h"

// ============================================================================
// RlvNotifications
//

#ifdef RLV_EXTENSION_NOTIFY_BEHAVIOUR
// Checked: 2009-12-05 (RLVa-1.1.0h) | Added: RLVa-1.1.0h
void RlvNotifications::notifyBehaviour(ERlvBehaviour eBhvr, ERlvParamType eType)
{
	const std::string& strMsg = RlvStrings::getBehaviourNotificationString(eBhvr, eType);
	if (!strMsg.empty())
	{
		LLSD argsNotify;
		argsNotify["MESSAGE"] = strMsg;
		LLNotificationsUtil::add("SystemMessageTip", argsNotify);
	}
}
#endif // RLV_EXTENSION_NOTIFY_BEHAVIOUR

// Checked: 2010-10-11 (RLVa-1.1.3c) | Added: RLVa-1.1.3c
void RlvNotifications::notifyBlocked(const std::string& strRlvString)
{
	LLSD argsNotify;
	argsNotify["MESSAGE"] = RlvStrings::getString(strRlvString);
	LLNotificationsUtil::add("SystemMessageTip", argsNotify);
}

// Checked: 2009-11-11 (RLVa-1.1.0a) | Added: RLVa-1.1.0a
void RlvNotifications::notifyBlockedViewXXX(const char* pstrAssetType)
{
	LLStringUtil::format_map_t argsMsg; std::string strMsg = RlvStrings::getString(RLV_STRING_BLOCKED_VIEWXXX);
	argsMsg["[TYPE]"] = pstrAssetType;
	LLStringUtil::format(strMsg, argsMsg);

	LLSD argsNotify;
	argsNotify["MESSAGE"] = strMsg;
	LLNotificationsUtil::add("SystemMessageTip", argsNotify);
}

// Checked: 2009-11-13 (RLVa-1.1.0b) | Modified: RLVa-1.1.0b
void RlvNotifications::warnGiveToRLV()
{
	if ( (gSavedSettings.getWarning(RLV_SETTING_FIRSTUSE_GIVETORLV)) && (RlvSettings::getForbidGiveToRLV()) )
		LLNotifications::instance().add(RLV_SETTING_FIRSTUSE_GIVETORLV, LLSD(), LLSD(), &RlvNotifications::onGiveToRLVConfirmation);
}

// Checked: 2009-11-13 (RLVa-1.1.0b) | Modified: RLVa-1.1.0b
void RlvNotifications::onGiveToRLVConfirmation(const LLSD& notification, const LLSD& response)
{
	gSavedSettings.setWarning(RLV_SETTING_FIRSTUSE_GIVETORLV, FALSE);

	S32 idxOption = LLNotification::getSelectedOption(notification, response);
	if ( (0 == idxOption) || (1 == idxOption) )
		gSavedSettings.setBOOL(RLV_SETTING_FORBIDGIVETORLV, (idxOption == 1));
}

// =========================================================================
// RlvSettings
//

#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
BOOL RlvSettings::fCompositeFolders = FALSE;
#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
BOOL RlvSettings::fLegacyNaming = TRUE;
BOOL RlvSettings::fNoSetEnv = FALSE;
BOOL RlvSettings::fShowNameTags = FALSE;

// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
void RlvSettings::initClass()
{
	static bool fInitialized = false;
	if (!fInitialized)
	{
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		fCompositeFolders = rlvGetSettingBOOL(RLV_SETTING_ENABLECOMPOSITES, FALSE);
		if (gSavedSettings.controlExists(RLV_SETTING_ENABLECOMPOSITES))
			gSavedSettings.getControl(RLV_SETTING_ENABLECOMPOSITES)->getSignal()->connect(boost::bind(&onChangedSettingBOOL, _2, &fCompositeFolders));
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

		fLegacyNaming = rlvGetSettingBOOL(RLV_SETTING_ENABLELEGACYNAMING, TRUE);
		if (gSavedSettings.controlExists(RLV_SETTING_ENABLELEGACYNAMING))
			gSavedSettings.getControl(RLV_SETTING_ENABLELEGACYNAMING)->getSignal()->connect(boost::bind(&onChangedSettingBOOL, _2, &fLegacyNaming));

		fNoSetEnv = rlvGetSettingBOOL(RLV_SETTING_NOSETENV, FALSE);

		fShowNameTags = rlvGetSettingBOOL(RLV_SETTING_SHOWNAMETAGS, FALSE);
		if (gSavedSettings.controlExists(RLV_SETTING_SHOWNAMETAGS))
			gSavedSettings.getControl(RLV_SETTING_SHOWNAMETAGS)->getSignal()->connect(boost::bind(&onChangedSettingBOOL, _2, &fShowNameTags));

		fInitialized = true;
	}
}

#ifdef RLV_EXTENSION_STARTLOCATION
	// Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-0.2.1d
	void RlvSettings::updateLoginLastLocation()
	{
		if (gSavedPerAccountSettings.controlExists(RLV_SETTING_LOGINLASTLOCATION))
		{
			BOOL fValue = (gRlvHandler.hasBehaviour(RLV_BHVR_TPLOC)) || (!gRlvHandler.canStand());
			if (gSavedPerAccountSettings.getBOOL(RLV_SETTING_LOGINLASTLOCATION) != fValue)
			{
				gSavedPerAccountSettings.setBOOL(RLV_SETTING_LOGINLASTLOCATION, fValue);
				gSavedPerAccountSettings.saveToFile(gSavedSettings.getString("PerAccountSettingsFile"), TRUE);
			}
		}
	}
#endif // RLV_EXTENSION_STARTLOCATION

// Checked: 2009-12-18 (RLVa-1.1.0k) | Added: RLVa-1.1.0i
bool RlvSettings::onChangedSettingBOOL(const LLSD& newvalue, BOOL* pfSetting)
{
	if (pfSetting)
		*pfSetting = newvalue.asBoolean();
	return true;
}

// ============================================================================
// RlvStrings
//

std::vector<std::string> RlvStrings::m_Anonyms;
std::map<std::string, std::string> RlvStrings::m_StringMap;
#ifdef RLV_EXTENSION_NOTIFY_BEHAVIOUR
std::map<ERlvBehaviour, std::string> RlvStrings::m_BhvrAddMap;
std::map<ERlvBehaviour, std::string> RlvStrings::m_BhvrRemMap;
#endif // RLV_EXTENSION_NOTIFY_BEHAVIOUR

// Checked: 2010-03-09 (RLVa-1.2.0a) | Added: RLVa-1.1.0h
void RlvStrings::initClass()
{
	static bool fInitialized = false;
	if (!fInitialized)
	{
		LLXMLNodePtr xmlRoot;
		if ( (!LLUICtrlFactory::getLayeredXMLNode("rlva_strings.xml", xmlRoot)) || (xmlRoot.isNull()) || (!xmlRoot->hasName("rlva_strings")) )
		{
			RLV_ERRS << "Problem reading RLVa string XML file" << RLV_ENDL;
			return;
		}

		for (LLXMLNode* pNode = xmlRoot->getFirstChild(); pNode != NULL; pNode = pNode->getNextSibling())
		{
			if (pNode->hasName("strings"))
			{
				std::string strName;
				for (LLXMLNode* pStringNode = pNode->getFirstChild(); pStringNode != NULL; pStringNode = pStringNode->getNextSibling())
				{
					if ( (!pStringNode->hasName("string")) || (!pStringNode->getAttributeString("name", strName)) )
						continue;
					m_StringMap[strName] = pStringNode->getTextContents();
				}
			}
			else if (pNode->hasName("anonyms"))
			{
				for (LLXMLNode* pAnonymNode = pNode->getFirstChild(); pAnonymNode != NULL; pAnonymNode = pAnonymNode->getNextSibling())
				{
					if (!pAnonymNode->hasName("anonym"))
						continue;
					m_Anonyms.push_back(pAnonymNode->getTextContents());
				}
			}
			#ifdef RLV_EXTENSION_NOTIFY_BEHAVIOUR
			else if (pNode->hasName("behaviour-notifications"))
			{
				std::string strBhvr, strType; ERlvBehaviour eBhvr;
				for (LLXMLNode* pNotifyNode = pNode->getFirstChild(); pNotifyNode != NULL; pNotifyNode = pNotifyNode->getNextSibling())
				{
					if ( (!pNotifyNode->hasName("notification")) || (!pNotifyNode->getAttributeString("type", strType)) ||
						 (!pNotifyNode->getAttributeString("behaviour", strBhvr)) || 
						 ((eBhvr = RlvCommand::getBehaviourFromString(strBhvr)) == RLV_BHVR_UNKNOWN) )
					{
						continue;
					}
					if ("add" == strType)
						m_BhvrAddMap.insert(std::pair<ERlvBehaviour, std::string>(eBhvr, pNotifyNode->getTextContents()));
					else if ("rem" == strType)
						m_BhvrRemMap.insert(std::pair<ERlvBehaviour, std::string>(eBhvr, pNotifyNode->getTextContents()));
				}
			}
			#endif // RLV_EXTENSION_NOTIFY_BEHAVIOUR
		}

		if ( (m_StringMap.empty()) || (m_Anonyms.empty()) )
		{
			RLV_ERRS << "Problem parsing RLVa string XML file" << RLV_ENDL;
			return;
		}

		fInitialized = true;
	}
}

// Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
const std::string& RlvStrings::getAnonym(const std::string& strName)
{
	const char* pszName = strName.c_str(); U32 nHash = 0;
	
	// Test with 11,264 SL names showed a 3.33% - 3.82% occurance for each so we *should* get a very even spread
	for (int idx = 0, cnt = strName.length(); idx < cnt; idx++)
		nHash += pszName[idx];

	return m_Anonyms[nHash % m_Anonyms.size()];
}

#ifdef RLV_EXTENSION_NOTIFY_BEHAVIOUR
// Checked: 2009-12-05 (RLVa-1.1.0h) | Added: RLVa-1.1.0h
const std::string& RlvStrings::getBehaviourNotificationString(ERlvBehaviour eBhvr, ERlvParamType eType)
{
	if (RLV_TYPE_ADD == eType)
	{
		std::map<ERlvBehaviour, std::string>::const_iterator itString = m_BhvrAddMap.find(eBhvr);
		return (itString != m_BhvrAddMap.end()) ? itString->second : LLStringUtil::null;
	}
	else if (RLV_TYPE_REMOVE == eType)
	{
		std::map<ERlvBehaviour, std::string>::const_iterator itString = m_BhvrRemMap.find(eBhvr);
		return (itString != m_BhvrRemMap.end()) ? itString->second : LLStringUtil::null;
	}
	return LLStringUtil::null;
}
#endif // RLV_EXTENSION_NOTIFY_BEHAVIOUR

// Checked: 2009-11-11 (RLVa-1.1.0a) | Added: RLVa-1.1.0a
const std::string& RlvStrings::getString(const std::string& strStringName)
{
	static const std::string strMissing = "(Missing RLVa string)";
	std::map<std::string, std::string>::const_iterator itString = m_StringMap.find(strStringName);
	return (itString != m_StringMap.end()) ? itString->second : strMissing;
}

// Checked: 2009-11-25 (RLVa-1.1.0f) | Added: RLVa-1.1.0f
const char* RlvStrings::getStringFromReturnCode(ERlvCmdRet eRet)
{
	// TODO-RLVa: [2009-11-25] clean this up along with the calling code in process_chat_from_simulator() once we're happy with the output
	switch (eRet)
	{
		case RLV_RET_SUCCESS_UNSET:
			return "unset";
		case RLV_RET_SUCCESS_DUPLICATE:
			return "duplicate";
		case RLV_RET_FAILED_SYNTAX:
			return "syntax error";
		case RLV_RET_FAILED_OPTION:
			return "invalid option";
		case RLV_RET_FAILED_PARAM:
			return "invalid param";
		case RLV_RET_FAILED_LOCK:
			return "locked command";
		case RLV_RET_FAILED_DISABLED:
			return "disabled command";
		case RLV_RET_FAILED_UNKNOWN:
			return "unknown command";
		case RLV_RET_FAILED_NOSHAREDROOT:
			return "missing #RLV";
		// The following are identified by the chat verb
		case RLV_RET_RETAINED:
		case RLV_RET_SUCCESS:
		case RLV_RET_FAILED:
			break;
		// The following shouldn't occur
		case RLV_RET_UNKNOWN:
		default:
			RLV_ASSERT(false);
			break;
	};
	return NULL;
}

// Checked: 2010-03-27 (RLVa-1.2.0b) | Modified: RLVa-1.2.0b
std::string RlvStrings::getVersion(bool fLegacy /*=false*/) 
{
	return llformat("%s viewer v%d.%d.%d (%s %d.%d.%d.%d - RLVa %d.%d.%d)",
		( (!fLegacy) ? "RestrainedLove" : "RestrainedLife" ),
		RLV_VERSION_MAJOR, RLV_VERSION_MINOR, RLV_VERSION_PATCH,
		LLAppViewer::instance()->getSecondLifeTitle().c_str(), LL_VERSION_MAJOR, LL_VERSION_MINOR, LL_VERSION_PATCH, LL_VERSION_BUILD,
		RLVa_VERSION_MAJOR, RLVa_VERSION_MINOR, RLVa_VERSION_PATCH);
}

// Checked: 2010-04-18 (RLVa-1.2.0e) | Added: RLVa-1.2.0e
std::string RlvStrings::getVersionAbout()
{
	return llformat("RLV v%d.%d.%d / RLVa v%d.%d.%d%c" , 
		RLV_VERSION_MAJOR, RLV_VERSION_MINOR, RLV_VERSION_PATCH,
		RLVa_VERSION_MAJOR, RLVa_VERSION_MINOR, RLVa_VERSION_PATCH, 'a' + RLVa_VERSION_BUILD);
}

// Checked: 2010-03-27 (RLVa-1.2.0b) | Modified: RLVa-1.1.0a
std::string RlvStrings::getVersionNum() 
{
	return llformat("%d%02d%02d%02d", RLV_VERSION_MAJOR, RLV_VERSION_MINOR, RLV_VERSION_PATCH, RLV_VERSION_BUILD);
}

// Checked: 2010-05-26 (RLVa-1.2.0h) | Added: RLVa-1.2.0g
bool RlvStrings::hasString(const std::string& strStringName)
{
	return m_StringMap.find(strStringName) != m_StringMap.end();
}

// ============================================================================
// RlvUtil
//

bool RlvUtil::m_fForceTp = false;

// Checked: 2009-07-04 (RLVa-1.0.0a) | Modified: RLVa-1.0.0a
void RlvUtil::filterLocation(std::string& strUTF8Text)
{
	// TODO-RLVa: if either the region or parcel name is a simple word such as "a" or "the" then confusion will ensue?
	//            -> not sure how you would go about preventing this though :|...

	// Filter any mention of the surrounding region names
	LLWorld::region_list_t regions = LLWorld::getInstance()->getRegionList();
	const std::string& strHiddenRegion = RlvStrings::getString(RLV_STRING_HIDDEN_REGION);
	for (LLWorld::region_list_t::const_iterator itRegion = regions.begin(); itRegion != regions.end(); ++itRegion)
		rlvStringReplace(strUTF8Text, (*itRegion)->getName(), strHiddenRegion);

	// Filter any mention of the parcel name
	LLViewerParcelMgr* pParcelMgr = LLViewerParcelMgr::getInstance();
	if (pParcelMgr)
		rlvStringReplace(strUTF8Text, pParcelMgr->getAgentParcelName(), RlvStrings::getString(RLV_STRING_HIDDEN_PARCEL));
}

// Checked: 2010-12-08 (RLVa-1.2.2c) | Modified: RLVa-1.2.2c
void RlvUtil::filterNames(std::string& strUTF8Text, bool fFilterLegacy)
{
	std::vector<LLUUID> idAgents;
	LLWorld::getInstance()->getAvatars(&idAgents, NULL);

	for (int idxAgent = 0, cntAgent = idAgents.size(); idxAgent < cntAgent; idxAgent++)
	{
		LLAvatarName avName;
		if (LLAvatarNameCache::get(idAgents[idxAgent], &avName))
		{
			const std::string& strAnonym = RlvStrings::getAnonym(avName.mDisplayName);

			// NOTE: if the legacy first and last name are empty we get a legacy name of " " which would replace all spaces in the string
			std::string strLegacyName;
			if ( (fFilterLegacy) && (!avName.mIsDisplayNameDefault) && (!avName.mLegacyFirstName.empty()) )
				strLegacyName = avName.getLegacyName();

			// If the display name is a subset of the legacy name we need to filter that first, otherwise it's the other way around
			if (std::string::npos != strLegacyName.find(avName.mDisplayName))
			{
				if (!strLegacyName.empty())
					rlvStringReplace(strUTF8Text, strLegacyName, strAnonym);
				rlvStringReplace(strUTF8Text, avName.mDisplayName, strAnonym);
			}
			else
			{
				rlvStringReplace(strUTF8Text, avName.mDisplayName, strAnonym);
				if (!strLegacyName.empty())
					rlvStringReplace(strUTF8Text, strLegacyName, strAnonym);
			}
		}
	}
}

// Checked: 2010-08-29 (RLVa-1.2.1c) | Added: RLVa-1.2.1c
void RlvUtil::forceTp(const LLVector3d& posDest)
{
	m_fForceTp = true;
	gAgent.teleportViaLocationLookAt(posDest);
	m_fForceTp = false;
}

// Checked: 2010-04-22 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
bool RlvUtil::isNearbyAgent(const LLUUID& idAgent)
{
	// Sanity check since we call this with notification payloads as well and those strings tend to change from one release to another
	RLV_ASSERT(idAgent.notNull());
	if ( (idAgent.notNull()) && (gAgent.getID() != idAgent) )
	{
		std::vector<LLUUID> idAgents;
		LLWorld::getInstance()->getAvatars(&idAgents, NULL);

		for (int idxAgent = 0, cntAgent = idAgents.size(); idxAgent < cntAgent; idxAgent++)
			if (idAgents[idxAgent] == idAgent)
				return true;
	}
	return false;
}

// Checked: 2010-04-05 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
bool RlvUtil::isNearbyRegion(const std::string& strRegion)
{
	LLWorld::region_list_t regions = LLWorld::getInstance()->getRegionList();
	for (LLWorld::region_list_t::const_iterator itRegion = regions.begin(); itRegion != regions.end(); ++itRegion)
		if ((*itRegion)->getName() == strRegion)
			return true;
	return false;
}

// Checked: 2010-11-11 (RLVa-1.2.1g) | Added: RLVa-1.2.1g
void RlvUtil::notifyFailedAssertion(const std::string& strAssert, const std::string& strFile, int nLine)
{
	static std::string strAssertPrev, strFilePrev; static int nLinePrev;
	if ( (strAssertPrev == strAssert) && (strFile == strFilePrev) && (nLine == nLinePrev) )
	{
		// Don't show the same assertion over and over
		return;
	}
	strAssertPrev = strAssert;
	strFilePrev = strFile;
	nLinePrev = nLine;

	LLSD argsNotify;
	argsNotify["MESSAGE"] = llformat("RLVa assertion failure: %s (%s - %d)", strAssert.c_str(), strFile.c_str(), nLine);
	LLNotificationsUtil::add("SystemMessageTip", argsNotify);
}

// Checked: 2010-03-27 (RLVa-1.1.3a) | Modified: RLVa-1.2.0b
void RlvUtil::sendBusyMessage(const LLUUID& idTo, const std::string& strMsg, const LLUUID& idSession)
{
	// [See process_improved_im()]
	std::string strFullName;
	gAgent.buildFullname(strFullName);

	pack_instant_message(gMessageSystem, gAgent.getID(), FALSE, gAgent.getSessionID(), idTo, strFullName,
		strMsg, IM_ONLINE, IM_BUSY_AUTO_RESPONSE, idSession);
	gAgent.sendReliableMessage();
}

// Checked: 2010-03-09 (RLVa-1.2.0a) | Modified: RLVa-1.0.1e
bool RlvUtil::sendChatReply(S32 nChannel, const std::string& strUTF8Text)
{
	if (!isValidReplyChannel(nChannel))
		return false;

	// Copy/paste from send_chat_from_viewer()
	gMessageSystem->newMessageFast(_PREHASH_ChatFromViewer);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_ChatData);
	gMessageSystem->addStringFast(_PREHASH_Message, strUTF8Text);
	gMessageSystem->addU8Fast(_PREHASH_Type, CHAT_TYPE_SHOUT);
	gMessageSystem->addS32("Channel", nChannel);
	gAgent.sendReliableMessage();
	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_CHAT_COUNT);

	return true;
}

// ============================================================================
// Generic menu enablers
//

bool RlvEnableIfNot::handleEvent(LLPointer<LLEvent>, const LLSD& userdata)
{
	bool fEnable = true;
	if (rlv_handler_t::isEnabled())
	{
		ERlvBehaviour eBhvr = RlvCommand::getBehaviourFromString(userdata["data"].asString());
		fEnable = (eBhvr != RLV_BHVR_UNKNOWN) ? !gRlvHandler.hasBehaviour(eBhvr) : true;
	}
	gMenuHolder->findControl(userdata["control"].asString())->setValue(fEnable);
	return true;
}

// ============================================================================
// Selection functors
//

// Checked: 2010-04-20 (RLVa-1.2.0f) | Modified: RLVa-0.2.0f
bool RlvSelectHasLockedAttach::apply(LLSelectNode* pNode)
{
	return (pNode->getObject()) ? gRlvAttachmentLocks.isLockedAttachment(pNode->getObject()->getRootEdit()) : false;
}

// Checked: 2009-07-05 (RLVa-1.0.0b) | Modified: RLVa-0.2.0f
bool RlvSelectIsOwnedByOrGroupOwned::apply(LLSelectNode* pNode)
{
	return (pNode->mPermissions->isGroupOwned()) || (pNode->mPermissions->getOwner() == m_idAgent);
}

// Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-0.2.0f
bool RlvSelectIsSittingOn::apply(LLSelectNode* pNode)
{
	return (pNode->getObject()) && (pNode->getObject()->getRootEdit() == m_pObject);
}

// ============================================================================
// Predicates
//

// Checked: 2010-11-11 (RLVa-1.2.1g) | Modified: RLVa-1.2.1g
bool rlvPredCanWearItem(const LLViewerInventoryItem* pItem, ERlvWearMask eWearMask)
{
	if ( (pItem) && (RlvForceWear::isWearableItem(pItem)) )
	{
		if (RlvForceWear::isWearingItem(pItem))
			return true; // Special exception for currently worn items
		switch (pItem->getType())
		{
			case LLAssetType::AT_BODYPART:
				// NOTE: only one body part of each type is allowed so the only way to wear one is if we can replace the current one
				return (RLV_WEAR_LOCKED != (gRlvWearableLocks.canWear(pItem) & RLV_WEAR_REPLACE & eWearMask));
			case LLAssetType::AT_CLOTHING:
				return (RLV_WEAR_LOCKED != (gRlvWearableLocks.canWear(pItem) & eWearMask));
			case LLAssetType::AT_OBJECT:
				return (RLV_WEAR_LOCKED != (gRlvAttachmentLocks.canAttach(pItem) & eWearMask));
			case LLAssetType::AT_GESTURE:
				return true;
			default:
				RLV_ASSERT(false);
		}
	}
	return false;
}

// Checked: 2010-03-22 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
bool rlvPredCanNotWearItem(const LLViewerInventoryItem* pItem, ERlvWearMask eWearMask)
{
	return !rlvPredCanWearItem(pItem, eWearMask);
}

// Checked: 2010-03-22 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
bool rlvPredCanRemoveItem(const LLViewerInventoryItem* pItem)
{
	if ( (pItem) && (RlvForceWear::isWearableItem(pItem)) )
	{
		switch (pItem->getType())
		{
			case LLAssetType::AT_BODYPART:
			case LLAssetType::AT_CLOTHING:
				return gRlvWearableLocks.canRemove(pItem);
			case LLAssetType::AT_OBJECT:
				return gRlvAttachmentLocks.canDetach(pItem);
			case LLAssetType::AT_GESTURE:
				return true;
			default:
				RLV_ASSERT(false);
		}
	}
	return false;
}

// Checked: 2010-03-22 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
bool rlvPredCanNotRemoveItem(const LLViewerInventoryItem* pItem)
{
	return !rlvPredCanRemoveItem(pItem);
}

// ============================================================================
// Various public helper functions
//

// Checked: 2010-09-28 (RLVa-1.1.3b) | Modified: RLVa-1.1.3b
BOOL rlvAttachToEnabler(void* pParam)
{
	// Visually disable an option on the "Attach to (HUD)" submenu if:
	//   - the attachment point is locked non-detachable with an object attached
	//   - the attachment point is locked non-attachable
	return (pParam != NULL) && 
		(!gRlvAttachmentLocks.hasLockedAttachment((LLViewerJointAttachment*)pParam)) &&
		(!gRlvAttachmentLocks.isLockedAttachmentPoint((LLViewerJointAttachment*)pParam, RLV_LOCK_ADD));
}

// ============================================================================
