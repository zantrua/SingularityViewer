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
#include "llattachmentsmgr.h"
#include "llviewerobjectlist.h"
#include "pipeline.h"
#include "llagentwearables.h"

#include "rlvhelper.h"
#include "rlvlocks.h"

// ============================================================================
// RlvAttachPtLookup member functions
//

std::map<std::string, S32> RlvAttachPtLookup::m_AttachPtLookupMap;

// Checked: 2010-03-02 (RLVa-1.1.3a) | Modified: RLVa-1.2.0a
void RlvAttachPtLookup::initLookupTable()
{
	static bool fInitialized = false;
	if (!fInitialized)
	{
		LLVOAvatar* pAvatar = gAgentAvatarp;
		if (pAvatar)
		{
			std::string strAttachPtName;
			for (LLVOAvatar::attachment_map_t::const_iterator itAttach = pAvatar->mAttachmentPoints.begin(); 
					 itAttach != pAvatar->mAttachmentPoints.end(); ++itAttach)
			{
				const LLViewerJointAttachment* pAttachPt = itAttach->second;
				if (pAttachPt)
				{
					strAttachPtName = pAttachPt->getName();
					LLStringUtil::toLower(strAttachPtName);
					m_AttachPtLookupMap.insert(std::pair<std::string, S32>(strAttachPtName, itAttach->first));
				}
			}
			fInitialized = true;
		}
	}
}

// Checked: 2010-03-03 (RLVa-1.1.3a) | Added: RLVa-0.2.2a
S32 RlvAttachPtLookup::getAttachPointIndex(const LLViewerJointAttachment* pAttachPt)
{
	LLVOAvatar* pAvatar = gAgentAvatarp;
	if (pAvatar)
	{
		for (LLVOAvatar::attachment_map_t::const_iterator itAttach = pAvatar->mAttachmentPoints.begin(); 
				itAttach != pAvatar->mAttachmentPoints.end(); ++itAttach)
		{
			if (itAttach->second == pAttachPt)
				return itAttach->first;
		}
	}
	return 0;
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.0.1b
S32 RlvAttachPtLookup::getAttachPointIndex(const LLInventoryCategory* pFolder)
{
	if (!pFolder)
		return 0;

	// RLVa-1.0.1 added support for legacy matching (See http://rlva.catznip.com/blog/2009/07/attachment-point-naming-convention/)
	if (RlvSettings::getEnableLegacyNaming())
		return getAttachPointIndexLegacy(pFolder);

	// Otherwise the only valid way to specify an attachment point in a folder name is: ^\.\(\s+attachpt\s+\)
	std::string::size_type idxMatch;
	std::string strAttachPt = rlvGetFirstParenthesisedText(pFolder->getName(), &idxMatch);
	LLStringUtil::trim(strAttachPt);

	return ( (1 == idxMatch) && (RLV_FOLDER_PREFIX_HIDDEN == pFolder->getName().at(0)) ) ? getAttachPointIndex(strAttachPt) : 0;
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
S32 RlvAttachPtLookup::getAttachPointIndex(const LLInventoryItem* pItem, bool fFollowLinks /*=true*/)
{
	// Sanity check - if it's not an object then it can't have an attachment point
	if ( (!pItem) || (LLAssetType::AT_OBJECT != pItem->getType()) )
		return 0;

	// If the item is an inventory link then we first examine its target before examining the link itself
	if ( (LLAssetType::AT_LINK == pItem->getActualType()) && (fFollowLinks) )
	{
		S32 idxAttachPt = getAttachPointIndex(gInventory.getItem(pItem->getLinkedUUID()), false);
		if (idxAttachPt)
			return idxAttachPt;
	}

	// The attachment point should be placed at the end of the item's name, surrounded by parenthesis
	// (if there is no such text then strAttachPt will be an empty string which is fine since it means we'll look at the item's parent)
	// (if the item is an inventory link then we only look at its containing folder and never examine its name)
	std::string strAttachPt = 
		(LLAssetType::AT_LINK != pItem->getActualType()) ? rlvGetLastParenthesisedText(pItem->getName()) : LLStringUtil::null;
	LLStringUtil::trim(strAttachPt);

	// If the item is modify   : we look at the item's name first and only then at the containing folder
	// If the item is no modify: we look at the containing folder's name first and only then at the item itself
	S32 idxAttachPt = 0;
	if (pItem->getPermissions().allowModifyBy(gAgent.getID()))
	{
		idxAttachPt = (!strAttachPt.empty()) ? getAttachPointIndex(strAttachPt) : 0;
		if (!idxAttachPt)
			idxAttachPt = getAttachPointIndex(gInventory.getCategory(pItem->getParentUUID()));
	}
	else
	{
		idxAttachPt = getAttachPointIndex(gInventory.getCategory(pItem->getParentUUID()));
		if ( (!idxAttachPt) && (!strAttachPt.empty()) )
			idxAttachPt = getAttachPointIndex(strAttachPt);
	}
	return idxAttachPt;
}

// Checked: 2010-03-03 (RLVa-1.2.0a) | Added: RLVa-1.0.1b
S32 RlvAttachPtLookup::getAttachPointIndexLegacy(const LLInventoryCategory* pFolder)
{
	// Hopefully some day this can just be deprecated (see http://rlva.catznip.com/blog/2009/07/attachment-point-naming-convention/)
	if ( (!pFolder) || (pFolder->getName().empty()) )
		return 0;

	// Check for a (...) block *somewhere* in the name
	std::string::size_type idxMatch;
	std::string strAttachPt = rlvGetFirstParenthesisedText(pFolder->getName(), &idxMatch);
	if (!strAttachPt.empty())
	{
		// Could be "(attachpt)", ".(attachpt)" or "Folder name (attachpt)"
		if ( (0 != idxMatch) && ((1 != idxMatch) || (RLV_FOLDER_PREFIX_HIDDEN == pFolder->getName().at(0)) ) &&	// No '(' or '.(' start
			 (idxMatch + strAttachPt.length() + 1 != pFolder->getName().length()) )								// or there's extra text
		{
			// It's definitely not one of the first two so assume it's the last form (in which case we need the last paranthesised block)
			strAttachPt = rlvGetLastParenthesisedText(pFolder->getName());
		}
	}
	else
	{
		// There's no paranthesised block, but it could still be "attachpt" or ".attachpt" (just strip away the '.' from the last one)
		strAttachPt = pFolder->getName();
		if (RLV_FOLDER_PREFIX_HIDDEN == strAttachPt[0])
			strAttachPt.erase(0, 1);
	}
	return getAttachPointIndex(strAttachPt);
}

// ============================================================================
// RlvAttachmentLocks member functions
//

RlvAttachmentLocks gRlvAttachmentLocks;

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvAttachmentLocks::addAttachmentLock(const LLUUID& idAttachObj, const LLUUID& idRlvObj)
{
/*
	// Sanity check - make sure it's an object we know about
	if ( (m_Objects.find(idRlvObj) == m_Objects.end()) || (!idxAttachPt) )
		return;	// If (idxAttachPt) == 0 then: (pObj == NULL) || (pObj->isAttachment() == FALSE)
*/

#ifndef RLV_RELEASE
	LLViewerObject* pDbgObj = gObjectList.findObject(idAttachObj);
	// Assertion: the object specified by idAttachObj exists/is rezzed, is an attachment and always specifies the root
	RLV_VERIFY( (pDbgObj) && (pDbgObj->isAttachment()) && (pDbgObj == pDbgObj->getRootEdit()) );
#endif // RLV_RELEASE

	m_AttachObjRem.insert(std::pair<LLUUID, LLUUID>(idAttachObj, idRlvObj));
	if(LLViewerObject *pObj = gObjectList.findObject(idAttachObj))	//OK
	{
		gInventory.addChangedMask(LLInventoryObserver::LABEL, pObj->getAttachmentItemID());
		gInventory.notifyObservers();
	}
	updateLockedHUD();
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvAttachmentLocks::addAttachmentPointLock(S32 idxAttachPt, const LLUUID& idRlvObj, ERlvLockMask eLock)
{
/*
	// Sanity check - make sure it's an object we know about
	if ( (m_Objects.find(idRlvObj) == m_Objects.end()) || (!idxAttachPt) )
		return;	// If (idxAttachPt) == 0 then: (pObj == NULL) || (pObj->isAttachment() == FALSE)
*/

	// NOTE: m_AttachPtXXX can contain duplicate <idxAttachPt, idRlvObj> pairs (ie @detach:spine=n,detach=n from an attachment on spine)
	if (eLock & RLV_LOCK_REMOVE)
	{
		m_AttachPtRem.insert(std::pair<S32, LLUUID>(idxAttachPt, idRlvObj));
		LLVOAvatar* pAvatar = gAgentAvatarp;
		if (pAvatar)
		{
			bool need_update = false;
			LLVOAvatar::attachment_map_t::iterator iter = pAvatar->mAttachmentPoints.find(idxAttachPt);
			if (iter != pAvatar->mAttachmentPoints.end())
			{
				for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = iter->second->mAttachedObjects.begin();
						attachment_iter != iter->second->mAttachedObjects.end();++attachment_iter)
				{
					LLViewerObject* attached_object = (*attachment_iter);
					if(attached_object)
					{
						gInventory.addChangedMask(LLInventoryObserver::LABEL, attached_object->getAttachmentItemID());
						need_update = true;
					}
				}
			}
			if(need_update)
				gInventory.notifyObservers();
		}
		updateLockedHUD();
	}
	if (eLock & RLV_LOCK_ADD)
		m_AttachPtAdd.insert(std::pair<S32, LLUUID>(idxAttachPt, idRlvObj));
}

// Checked: 2010-08-07 (RLVa-1.2.0i) | Modified: RLVa-1.2.0i
bool RlvAttachmentLocks::canDetach(const LLViewerJointAttachment* pAttachPt, bool fDetachAll /*=false*/) const
{
	//   (fDetachAll) | (isLockedAttachment)
	//   ===================================
	//        F       |         F             => unlocked attachment => return true
	//        F       |         T             => locked attachment   => keep going
	//        T       |         F             => unlocked attachment => keep going
	//        T       |         T             => locked attachment   => return false
	//  -> inside the loop : (A xor (not B)) condition and return !fDetachAll
	//  -> outside the loop: return fDetachAll
	//       fDetachAll == false : return false => all attachments are locked
	//       fDetachAll == true  : return true  => all attachments are unlocked
	if (pAttachPt)
	{
		for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
				itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
		{
			if ( (fDetachAll) ^ (!isLockedAttachment(*itAttachObj)) )
				return !fDetachAll;
		}
	}
	return (pAttachPt) && (fDetachAll);
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
bool RlvAttachmentLocks::hasLockedAttachment(const LLViewerJointAttachment* pAttachPt) const
{
	if (pAttachPt)
	{
		for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
				itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
		{
			if (isLockedAttachment(*itAttachObj))
				return true;
		}
	}
	return false;
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
bool RlvAttachmentLocks::isLockedAttachmentExcept(const LLViewerObject* pObj, const LLUUID& idRlvObj) const
{
	if (idRlvObj.isNull())
		return isLockedAttachment(pObj);

	// If pObj is valid then it should always specify a root since we store root UUIDs in m_AttachObjRem
	RLV_ASSERT( (!pObj) || (pObj == pObj->getRootEdit()) );

	// Loop over every object that has the specified attachment locked (but ignore any locks owned by idRlvObj)
	for (rlv_attachobjlock_map_t::const_iterator itAttachObj = m_AttachObjRem.lower_bound(pObj->getID()), 
			endAttachObj = m_AttachObjRem.upper_bound(pObj->getID()); itAttachObj != endAttachObj; ++itAttachObj)
	{
		if (itAttachObj->second != idRlvObj)
			return true;
	}
	return isLockedAttachmentPointExcept(RlvAttachPtLookup::getAttachPointIndex(pObj), RLV_LOCK_REMOVE, idRlvObj);
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.0.5b
bool RlvAttachmentLocks::isLockedAttachmentPointExcept(S32 idxAttachPt, ERlvLockMask eLock, const LLUUID& idRlvObj) const
{
	if (idRlvObj.isNull())
		return isLockedAttachmentPoint(idxAttachPt, eLock);

	// Loop over every object that has the specified attachment point locked (but ignore any locks owned by idRlvObj)
	if (eLock & RLV_LOCK_REMOVE)
	{
		for (rlv_attachptlock_map_t::const_iterator itAttachPt = m_AttachPtRem.lower_bound(idxAttachPt), 
				endAttachPt = m_AttachPtRem.upper_bound(idxAttachPt); itAttachPt != endAttachPt; ++itAttachPt)
		{
			if (itAttachPt->second != idRlvObj)
				return true;
		}
	}
	if (eLock & RLV_LOCK_ADD)
	{
		for (rlv_attachptlock_map_t::const_iterator itAttachPt = m_AttachPtAdd.lower_bound(idxAttachPt), 
				endAttachPt = m_AttachPtAdd.upper_bound(idxAttachPt); itAttachPt != endAttachPt; ++itAttachPt)
		{
			if (itAttachPt->second != idRlvObj)
				return true;
		}
	}
	return false;
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvAttachmentLocks::removeAttachmentLock(const LLUUID& idAttachObj, const LLUUID& idRlvObj)
{
/*
	// Sanity check - make sure it's an object we know about
	if ( (m_Objects.find(idRlvObj) == m_Objects.end()) || (!idxAttachPt) )
		return;	// If (idxAttachPt) == 0 then: (pObj == NULL) || (pObj->isAttachment() == FALSE)
*/

#ifndef RLV_RELEASE
	// NOTE: pObj *can* be NULL [see comments for @detach=n in RlvHandler::onAddRemDetach()]
	const LLViewerObject* pDbgObj = gObjectList.findObject(idAttachObj);
	// Assertion: if the object exists then it's an attachment and always specifies the root
	RLV_VERIFY( (!pDbgObj) || ((pDbgObj->isAttachment()) && (pDbgObj == pDbgObj->getRootEdit())) );
#endif // RLV_RELEASE

	// NOTE: try to remove the lock even if pObj isn't an attachment (ie in case the user was able to "Drop" it)
	RLV_ASSERT( m_AttachObjRem.lower_bound(idAttachObj) != m_AttachObjRem.upper_bound(idAttachObj) ); // The lock should always exist
	for (rlv_attachobjlock_map_t::iterator itAttachObj = m_AttachObjRem.lower_bound(idAttachObj), 
			endAttachObj = m_AttachObjRem.upper_bound(idAttachObj); itAttachObj != endAttachObj; ++itAttachObj)
	{
		if (idRlvObj == itAttachObj->second)
		{
			m_AttachObjRem.erase(itAttachObj);
			if(LLViewerObject *pObj = gObjectList.findObject(idAttachObj))	//OK
			{
				gInventory.addChangedMask(LLInventoryObserver::LABEL, pObj->getAttachmentItemID());
				gInventory.notifyObservers();
			}
			updateLockedHUD();
			break;
		}
	}
}

// Checked: 2010-02-28 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvAttachmentLocks::removeAttachmentPointLock(S32 idxAttachPt, const LLUUID& idRlvObj, ERlvLockMask eLock)
{
/*
	// Sanity check - make sure it's an object we know about
	if ( (m_Objects.find(idRlvObj) == m_Objects.end()) || (!idxAttachPt) )
		return;	// If (idxAttachPt) == 0 then: (pObj == NULL) || (pObj->isAttachment() == FALSE)
*/

	if (eLock & RLV_LOCK_REMOVE)
	{
		bool removed_entry = false;
		RLV_ASSERT( m_AttachPtRem.lower_bound(idxAttachPt) != m_AttachPtRem.upper_bound(idxAttachPt) ); // The lock should always exist
		for (rlv_attachptlock_map_t::iterator itAttachPt = m_AttachPtRem.lower_bound(idxAttachPt), 
				endAttachPt = m_AttachPtRem.upper_bound(idxAttachPt); itAttachPt != endAttachPt; ++itAttachPt)
		{
			if (idRlvObj == itAttachPt->second)
			{
				m_AttachPtRem.erase(itAttachPt);
				removed_entry = true;
				updateLockedHUD();
				break;
			}
		}
		if(removed_entry)
		{
			if(m_AttachPtRem.find(idxAttachPt) == m_AttachPtRem.end())
			{
				LLVOAvatar* pAvatar = gAgentAvatarp;
				if (pAvatar)
				{
					bool need_update = false;
					LLVOAvatar::attachment_map_t::iterator iter = pAvatar->mAttachmentPoints.find(idxAttachPt);
					if (iter != pAvatar->mAttachmentPoints.end())
					{
						for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = iter->second->mAttachedObjects.begin();
								attachment_iter != iter->second->mAttachedObjects.end();++attachment_iter)
						{
							LLViewerObject* attached_object = (*attachment_iter);
							if(attached_object)
							{
								gInventory.addChangedMask(LLInventoryObserver::LABEL, attached_object->getAttachmentItemID());
								need_update = true;
							}
						}
						if(need_update)
							gInventory.notifyObservers();
					}
				}
			}
		}
	}
	if (eLock & RLV_LOCK_ADD)
	{
		RLV_ASSERT( m_AttachPtAdd.lower_bound(idxAttachPt) != m_AttachPtAdd.upper_bound(idxAttachPt) ); // The lock should always exist
		for (rlv_attachptlock_map_t::iterator itAttachPt = m_AttachPtAdd.lower_bound(idxAttachPt), 
				endAttachPt = m_AttachPtAdd.upper_bound(idxAttachPt); itAttachPt != endAttachPt; ++itAttachPt)
		{
			if (idRlvObj == itAttachPt->second)
			{
				m_AttachPtAdd.erase(itAttachPt);
				break;
			}
		}
	}
}

// Checked: 2010-08-22 (RLVa-1.1.3a) | Modified: RLVa-1.2.1a
void RlvAttachmentLocks::updateLockedHUD()
{
	LLVOAvatar* pAvatar = gAgentAvatarp;
	if (!pAvatar || pAvatar->isDead())
		return;

	m_fHasLockedHUD = false;
	for (LLVOAvatar::attachment_map_t::const_iterator itAttachPt = pAvatar->mAttachmentPoints.begin(); 
			itAttachPt != pAvatar->mAttachmentPoints.end(); ++itAttachPt)
	{
		const LLViewerJointAttachment* pAttachPt = itAttachPt->second;
		if ( (pAttachPt) && (pAttachPt->getIsHUDAttachment()) && (hasLockedAttachment(pAttachPt)) )
		{
			m_fHasLockedHUD = true;
			break;
		}
	}

	// Reset HUD visibility and wireframe options if at least one HUD attachment is locked
	if (m_fHasLockedHUD)
	{
		LLPipeline::sShowHUDAttachments = TRUE;
		gUseWireframe = FALSE;
	}
}

// Checked: 2010-03-11 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvAttachmentLocks::verifyAttachmentLocks()
{
	bool fSuccess = true;

	// Verify attachment locks
	rlv_attachobjlock_map_t::iterator itAttachObj = m_AttachObjRem.begin(), itCurrentObj;
	while (itAttachObj != m_AttachObjRem.end())
	{
		itCurrentObj = itAttachObj++;

		// If the attachment no longer exists (or is no longer attached) then we shouldn't be holding a lock to it
		const LLViewerObject* pAttachObj = gObjectList.findObject(itCurrentObj->first);
		if ( (!pAttachObj) || (!pAttachObj->isAttachment()) )
		{
			m_AttachObjRem.erase(itCurrentObj);
			fSuccess = false;
		}
	}

	return fSuccess;
}

// ============================================================================
// RlvAttachmentLockWatchdog member functions
//

// Checked: 2010-09-23 (RLVa-1.2.1d) | Added: RLVa-1.2.1d
bool RlvAttachmentLockWatchdog::RlvWearInfo::isAddLockedAttachPt(S32 idxAttachPt) const
{
	// If idxAttachPt has no entry in attachPts then the attachment point wasn't RLV_LOCK_ADD restricted at the time we were instantiated
	// [See RlvAttachmentLockWatchdog::onWearAttachment()]
	return (attachPts.find(idxAttachPt) != attachPts.end());
}

// Checked: 2010-09-23 (RLVa-1.1.3b) | Added: RLVa-1.2.1d
void RlvAttachmentLockWatchdog::RlvWearInfo::dumpInstance() const
{
	const LLViewerInventoryItem* pItem = gInventory.getItem(idItem);
	std::string strItemId = idItem.asString();

	std::string strTemp = llformat("Wear %s '%s' (%s)", 
		(RLV_WEAR_ADD == eWearAction) ? "add" : "replace", (pItem) ? pItem->getName().c_str() : "missing", strItemId.c_str());
	RLV_INFOS << strTemp.c_str() << RLV_ENDL;

	if (!attachPts.empty())
	{
		std::string strEmptyAttachPt;
		for (std::map<S32, uuid_vec_t>::const_iterator itAttachPt = attachPts.begin(); itAttachPt != attachPts.end(); ++itAttachPt)
		{
			const LLViewerJointAttachment* pAttachPt =
				get_if_there(gAgentAvatarp->mAttachmentPoints, itAttachPt->first, (LLViewerJointAttachment*)NULL);
			if (!itAttachPt->second.empty())
			{
				for (uuid_vec_t::const_iterator itAttach = itAttachPt->second.begin(); itAttach != itAttachPt->second.end(); ++itAttach)
				{
					pItem = gInventory.getItem(*itAttach);
					strItemId = (*itAttach).asString();

					strTemp = llformat("  -> %s : %s (%s)",
						pAttachPt->getName().c_str(), (pItem) ? pItem->getName().c_str() : "missing", strItemId.c_str());
					RLV_INFOS << strTemp.c_str() << RLV_ENDL;
				}
			}
			else
			{
				if (!strEmptyAttachPt.empty())
					strEmptyAttachPt += ", ";
				strEmptyAttachPt += pAttachPt->getName();
			}
		}
		if (!strEmptyAttachPt.empty())
			RLV_INFOS << "  -> " << strEmptyAttachPt << " : empty" << RLV_ENDL;
	}
	else
	{
		RLV_INFOS << "  -> no attachment point information" << RLV_ENDL;
	}
}

// Checked: 2010-07-28 (RLVa-1.2.0i) | Modified: RLVa-1.2.0i
void RlvAttachmentLockWatchdog::detach(const LLViewerObject* pAttachObj)
{
	if (pAttachObj)
	{
		gMessageSystem->newMessage("ObjectDetach");
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, pAttachObj->getLocalID());
		if (std::find(m_PendingDetach.begin(), m_PendingDetach.end(), pAttachObj->getAttachmentItemID()) == m_PendingDetach.end())
			m_PendingDetach.push_back(pAttachObj->getAttachmentItemID());

		gMessageSystem->sendReliable(gAgent.getRegionHost() );

		// HACK-RLVa: force the region to send out an ObjectUpdate for the old attachment so obsolete viewers will remember it exists
		S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pAttachObj);
		const LLViewerJointAttachment* pAttachPt = 
			(gAgentAvatarp) ? get_if_there(gAgentAvatarp->mAttachmentPoints, (S32)idxAttachPt, (LLViewerJointAttachment*)NULL) : NULL;
		if ( (pAttachPt) && (!pAttachPt->getIsHUDAttachment()) && (pAttachPt->mAttachedObjects.size() > 1) )
		{
			for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
					itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
			{
				if (pAttachObj != *itAttachObj)
				{
					LLSelectMgr::instance().deselectAll();
					LLSelectMgr::instance().selectObjectAndFamily(*itAttachObj);
					LLSelectMgr::instance().deselectAll();
					break;
				}
			}
		}
	}
}

// Checked: 2010-07-28 (RLVa-1.1.3b) | Added: RLVa-1.2.0i
void RlvAttachmentLockWatchdog::detach(S32 idxAttachPt, const LLViewerObject* pAttachObjExcept /*=NULL*/)
{
	const LLViewerJointAttachment* pAttachPt = 
		(gAgentAvatarp) ? get_if_there(gAgentAvatarp->mAttachmentPoints, (S32)idxAttachPt, (LLViewerJointAttachment*)NULL) : NULL;
	if (!pAttachPt)
		return;

	c_llvo_vec_t attachObjs;
	for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
			itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
	{
		const LLViewerObject* pAttachObj = *itAttachObj;
		if (pAttachObj != pAttachObjExcept)
			attachObjs.push_back(pAttachObj);
	}

	if (!attachObjs.empty())
	{
		gMessageSystem->newMessage("ObjectDetach");
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		
		for (c_llvo_vec_t::const_iterator itAttachObj = attachObjs.begin(); itAttachObj != attachObjs.end(); ++itAttachObj)
		{
			const LLViewerObject* pAttachObj = *itAttachObj;
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, pAttachObj->getLocalID());
			if (std::find(m_PendingDetach.begin(), m_PendingDetach.end(), pAttachObj->getAttachmentItemID()) == m_PendingDetach.end())
				m_PendingDetach.push_back(pAttachObj->getAttachmentItemID());
		}

		gMessageSystem->sendReliable(gAgent.getRegionHost());

		// HACK-RLVa: force the region to send out an ObjectUpdate for the old attachment so obsolete viewers will remember it exists
		if ( (!pAttachPt->getIsHUDAttachment()) && (pAttachPt->mAttachedObjects.size() > attachObjs.size()) )
		{
			for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
					itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
			{
				if (std::find(attachObjs.begin(), attachObjs.end(), *itAttachObj) == attachObjs.end())
				{
					LLSelectMgr::instance().deselectAll();
					LLSelectMgr::instance().selectObjectAndFamily(*itAttachObj);
					LLSelectMgr::instance().deselectAll();
					break;
				}
			}
		}
	}
}

// Checked: 2010-09-23 (RLVa-1.1.3a) | Modified: RLVa-1.2.1d
void RlvAttachmentLockWatchdog::onAttach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt)
{
	S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pAttachObj);
	const LLUUID& idAttachItem = (pAttachObj) ? pAttachObj->getAttachmentItemID() : LLUUID::null;
	RLV_ASSERT( (!gAgentAvatarp) || ((idxAttachPt) && (idAttachItem.notNull())) );
	if ( (!idxAttachPt) || (idAttachItem.isNull()) )
		return;

	// Check if the attachment point has a pending "reattach"
	rlv_attach_map_t::iterator itAttach = m_PendingAttach.lower_bound(idxAttachPt), itAttachEnd = m_PendingAttach.upper_bound(idxAttachPt);
	if (itAttach != itAttachEnd)
	{
		bool fPendingReattach = false;
		for (; itAttach != itAttachEnd; ++itAttach)
		{
			if (idAttachItem == itAttach->second.idItem)
			{
				fPendingReattach = true;
				m_PendingAttach.erase(itAttach);
				break;
			}
		}
		if (!fPendingReattach)
			detach(pAttachObj);
		return;
	}

	// Check if the attach was allowed at the time it was requested
	rlv_wear_map_t::iterator itWear = m_PendingWear.find(idAttachItem);
	if (itWear != m_PendingWear.end())
	{
		// We'll need to return the attachment point to its previous state if it was non-attachable
		if (itWear->second.isAddLockedAttachPt(idxAttachPt))
		{
			// Get the saved state of the attachment point (but do nothing if the item itself was already worn then)
			std::map<S32, uuid_vec_t>::iterator itAttachPrev = itWear->second.attachPts.find(idxAttachPt);
			RLV_ASSERT(itAttachPrev != itWear->second.attachPts.end());
			if (std::find(itAttachPrev->second.begin(), itAttachPrev->second.end(), idAttachItem) == itAttachPrev->second.end())
			{
				// If it was empty we need to detach everything on the attachment point; if it wasn't we need to restore it to what it was
				if (itAttachPrev->second.empty())
				{
					detach(idxAttachPt);
				}
				else
				{
					// Iterate over all the current attachments and force detach any that shouldn't be there
					c_llvo_vec_t attachObjs;
					for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
							itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
					{
						const LLViewerObject* pAttachObj = *itAttachObj;

						uuid_vec_t::iterator itAttach = 
							std::find(itAttachPrev->second.begin(), itAttachPrev->second.end(), pAttachObj->getAttachmentItemID());
						if (itAttach == itAttachPrev->second.end())
							detach(pAttachObj);
						else
							itAttachPrev->second.erase(itAttach);
					}

					// Whatever is left is something that needs to be reattached
					for (uuid_vec_t::const_iterator itAttach = itAttachPrev->second.begin(); 
							itAttach != itAttachPrev->second.end(); ++itAttach)
					{
						m_PendingAttach.insert(std::pair<S32, RlvReattachInfo>(idxAttachPt, RlvReattachInfo(*itAttach)));
					}
				}
			}
		}
		else if (RLV_WEAR_REPLACE == itWear->second.eWearAction)
		{
			// Now that we know where this attaches to check if we can actually perform a "replace"
			bool fCanReplace = true;
			for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
					((itAttachObj != pAttachPt->mAttachedObjects.end()) && (fCanReplace)); ++itAttachObj)
			{
				if (pAttachObj != *itAttachObj)
					fCanReplace &= !gRlvAttachmentLocks.isLockedAttachment(*itAttachObj);
			}

			if (fCanReplace)
				detach(idxAttachPt, pAttachObj);	// Replace == allowed: detach everything except the new attachment
			else
				detach(pAttachObj);					// Replace != allowed: detach the new attachment
		}
		m_PendingWear.erase(itWear); // No need to start the timer since it should be running already if '!m_PendingWear.empty()'
	}
}

// Checked: 2010-07-28 (RLVa-1.1.3a) | Modified: RLVa-1.2.0i
void RlvAttachmentLockWatchdog::onDetach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt)
{
	S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pAttachPt);
	const LLUUID& idAttachItem = (pAttachObj) ? pAttachObj->getAttachmentItemID() : LLUUID::null;
	RLV_ASSERT( (!gAgentAvatarp) || ((idxAttachPt) && (idAttachItem.notNull())) );
	if ( (!idxAttachPt) || (idAttachItem.isNull()) )
		return;

	// If it's an attachment that's pending force-detach then we don't want to do anything (even if it's currently "remove locked")
	rlv_detach_map_t::iterator itDetach = std::find(m_PendingDetach.begin(), m_PendingDetach.end(), idAttachItem);
	if (itDetach != m_PendingDetach.end())
	{
		m_PendingDetach.erase(itDetach);
		return;
	}

	// If the attachment is currently "remove locked" then we should reattach it (unless it's already pending reattach)
	if (gRlvAttachmentLocks.isLockedAttachment(pAttachObj))
	{
		bool fPendingAttach = false;
		for (rlv_attach_map_t::const_iterator itReattach = m_PendingAttach.lower_bound(idxAttachPt), 
				itReattachEnd = m_PendingAttach.upper_bound(idxAttachPt); itReattach != itReattachEnd; ++itReattach)
		{
			if (itReattach->second.idItem == idAttachItem)
			{
				fPendingAttach = true;
				break;
			}
		}

		// TODO-RLVa: [RLVa-1.2.1] we should re-add the item to COF as well to make sure it'll reattach when the user relogs
		//		-> check the call order in LLVOAvatarSelf::detachObject() since COF removal happens *after* we're called
		if (!fPendingAttach)
		{
			m_PendingAttach.insert(std::pair<S32, RlvReattachInfo>(idxAttachPt, RlvReattachInfo(idAttachItem)));
			startTimer();
		}
	}
}

// Checked: 2010-03-05 (RLVa-1.2.0a) | Modified: RLVa-1.0.5b
void RlvAttachmentLockWatchdog::onSavedAssetIntoInventory(const LLUUID& idItem)
{
	for (rlv_attach_map_t::iterator itAttach = m_PendingAttach.begin(); itAttach != m_PendingAttach.end(); ++itAttach)
	{
		if ( (!itAttach->second.fAssetSaved) && (idItem == itAttach->second.idItem) )
		{
			LLAttachmentsMgr::instance().addAttachment(itAttach->second.idItem, itAttach->first, true, true);
			itAttach->second.tsAttach = LLFrameTimer::getElapsedSeconds();
		}
	}
}

// Checked: 2010-03-05 (RLVa-1.2.0a) | Modified: RLVa-1.0.5b
BOOL RlvAttachmentLockWatchdog::onTimer()
{
	// RELEASE-RLVa: [SL-2.0.0] This will need rewriting for "ENABLE_MULTIATTACHMENTS"
	F64 tsCurrent = LLFrameTimer::getElapsedSeconds();

	// Garbage collect (failed) wear requests older than 60 seconds
	rlv_wear_map_t::iterator itWear = m_PendingWear.begin();
	while (itWear != m_PendingWear.end())
	{
		if (itWear->second.tsWear + 60 < tsCurrent)
			m_PendingWear.erase(itWear++);
		else
			++itWear;
	}

	// Walk over the pending reattach list
	rlv_attach_map_t::iterator itAttach = m_PendingAttach.begin();
	while (itAttach != m_PendingAttach.end())
	{
		// Sanity check - make sure the item is still in the user's inventory
		if (gInventory.getItem(itAttach->second.idItem) == NULL)
		{
			m_PendingAttach.erase(itAttach++);
			continue;
		}

		// Force an attach if we haven't gotten a SavedAssetIntoInventory message after 15 seconds
		// (or if it's been 30 seconds since we last tried to reattach the item)
		bool fAttach = false;
		if ( (!itAttach->second.fAssetSaved) && (itAttach->second.tsDetach + 15 < tsCurrent) )
		{
			itAttach->second.fAssetSaved = true;
			fAttach = true;
		}
		else if ( (itAttach->second.fAssetSaved) && (itAttach->second.tsAttach + 30 < tsCurrent) )
		{
			fAttach = true;
		}

		if (fAttach)
		{
			LLAttachmentsMgr::instance().addAttachment(itAttach->second.idItem, itAttach->first, true, true);
			itAttach->second.tsAttach = tsCurrent;
		}

		++itAttach;
	}

	return ( (m_PendingAttach.empty()) && (m_PendingDetach.empty()) && (m_PendingWear.empty()) );
}

// Checked: 2010-07-28 (RLVa-1.1.3a) | Modified: RLVa-1.2.0i
void RlvAttachmentLockWatchdog::onWearAttachment(const LLUUID& idItem, ERlvWearMask eWearAction)
{
	// We only need to keep track of user wears if there's actually anything locked
	RLV_ASSERT(idItem.notNull());
	LLVOAvatar* pAvatar = gAgentAvatarp;
	if ( (idItem.isNull()) || (!pAvatar) || (!gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_ANY)) )
		return;

	// If the attachment point this will end up being attached to is:
	//   - unlocked    : nothing should happen (from RLVa's point of view)
	//   - RLV_LOCK_ADD: the new attachment should get detached and the current one(s) reattached (unless it's currently empty)
	//   - RLV_LOCK_REM:
	//       o eWearAction == RLV_WEAR_ADD     : nothing should happen (from RLVa's point of view)
	//       o eWearAction == RLV_WEAR_REPLACE : examine whether the new attachment can indeed replace/detach the old one
	RlvWearInfo infoWear(idItem, eWearAction);
	RLV_ASSERT( (RLV_WEAR_ADD == eWearAction) || (RLV_WEAR_REPLACE == eWearAction) ); // One of the two, but never both
	for (LLVOAvatar::attachment_map_t::const_iterator itAttachPt = pAvatar->mAttachmentPoints.begin(); 
			itAttachPt != pAvatar->mAttachmentPoints.end(); ++itAttachPt)
	{
		const LLViewerJointAttachment* pAttachPt = itAttachPt->second;
		// We only need to know which attachments were present for RLV_LOCK_ADD locked attachment points (and not RLV_LOCK_REM locked ones)
		if (gRlvAttachmentLocks.isLockedAttachmentPoint(pAttachPt, RLV_LOCK_ADD))
		{
			uuid_vec_t attachObjs;
			for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
					itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
			{
				const LLViewerObject* pAttachObj = *itAttachObj;
				if (std::find(m_PendingDetach.begin(), m_PendingDetach.end(), pAttachObj->getAttachmentItemID()) != m_PendingDetach.end())
					continue;	// Exclude attachments that are pending a force-detach
				attachObjs.push_back(pAttachObj->getAttachmentItemID());
			}
			infoWear.attachPts.insert(std::pair<S32, uuid_vec_t>(itAttachPt->first, attachObjs));
		}
	}

	m_PendingWear.insert(std::pair<LLUUID, RlvWearInfo>(idItem, infoWear));
#ifdef RLV_DEBUG
	infoWear.dumpInstance();
#endif // RLV_RELEASE
	startTimer();
}

// ============================================================================
// RlvWearableLocks member functions
//

RlvWearableLocks gRlvWearableLocks;

// Checked: 2010-03-18 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
void RlvWearableLocks::addWearableTypeLock(LLWearableType::EType eType, const LLUUID& idRlvObj, ERlvLockMask eLock)
{
/*
	// Sanity check - make sure it's an object we know about
	if ( (m_Objects.find(idRlvObj) == m_Objects.end()) || (!idxAttachPt) )
		return;	// If (idxAttachPt) == 0 then: (pObj == NULL) || (pObj->isAttachment() == FALSE)
*/

	// NOTE: m_WearableTypeXXX can contain duplicate <eType, idRlvObj> pairs (ie @remoutfit:shirt=n,remoutfit=n from the same object)
	if (eLock & RLV_LOCK_REMOVE)
	{
		m_WearableTypeRem.insert(std::pair<LLWearableType::EType, LLUUID>(eType, idRlvObj));
		LLUUID item_id = gAgentWearables.getWearableItemID(eType, 0);	// TODO: MULTI-WEARABLE
		if(item_id.notNull())
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);
			gInventory.notifyObservers();
		}
	}
	if (eLock & RLV_LOCK_ADD)
		m_WearableTypeAdd.insert(std::pair<LLWearableType::EType, LLUUID>(eType, idRlvObj));
}

// Checked: 2010-03-19 (RLVa-1.1.3b) | Added: RLVa-1.2.0a
bool RlvWearableLocks::canRemove(LLWearableType::EType eType) const
{
	// NOTE: we return TRUE if the wearable type has at least one wearable that can be removed by the user
	LLWearable* pWearable = gAgentWearables.getWearable(eType, 0);	// TODO: MULTI-WEARABLE
	if ( (pWearable) && (!isLockedWearable(pWearable)) )
		return true;
	return false;
}

// Checked: 2010-03-19 (RLVa-1.1.3b) | Added: RLVa-1.2.0a
bool RlvWearableLocks::hasLockedWearable(LLWearableType::EType eType) const
{
	// NOTE: we return TRUE if there is at least 1 non-removable wearable currently worn on this wearable type
	LLWearable* pWearable = gAgentWearables.getWearable(eType, 0);	// TODO: MULTI-WEARABLE
	if ( (pWearable) && (isLockedWearable(pWearable)) )
		return true;
	return false;
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvWearableLocks::isLockedWearableExcept(const LLWearable* pWearable, const LLUUID& idRlvObj) const
{
	if (idRlvObj.isNull())
		return isLockedWearable(pWearable);

	// TODO-RLVa: [RLVa-1.2.1] We don't have the ability to lock a specific wearable yet so rewrite this when we do
	return (pWearable) && (isLockedWearableTypeExcept(pWearable->getType(), RLV_LOCK_REMOVE, idRlvObj));
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvWearableLocks::isLockedWearableTypeExcept(LLWearableType::EType eType, ERlvLockMask eLock, const LLUUID& idRlvObj) const
{
	if (idRlvObj.isNull())
		return isLockedWearableType(eType, eLock);

	// Loop over every object that marked the specified wearable type eLock type locked and skip over anything owned by idRlvObj
	if (eLock & RLV_LOCK_REMOVE)
	{
		for (rlv_wearabletypelock_map_t::const_iterator itWearableType = m_WearableTypeRem.lower_bound(eType), 
				endWearableType = m_WearableTypeRem.upper_bound(eType); itWearableType != endWearableType; ++itWearableType)
		{
			if (itWearableType->second != idRlvObj)
				return true;
		}
	}
	if (eLock & RLV_LOCK_ADD)
	{
		for (rlv_wearabletypelock_map_t::const_iterator itWearableType = m_WearableTypeAdd.lower_bound(eType), 
				endWearableType = m_WearableTypeAdd.upper_bound(eType); itWearableType != endWearableType; ++itWearableType)
		{
			if (itWearableType->second != idRlvObj)
				return true;
		}
	}
	return false;
}

// Checked: 2010-03-18 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
void RlvWearableLocks::removeWearableTypeLock(LLWearableType::EType eType, const LLUUID& idRlvObj, ERlvLockMask eLock)
{
/*
	// Sanity check - make sure it's an object we know about
	if ( (m_Objects.find(idRlvObj) == m_Objects.end()) || (!idxAttachPt) )
		return;	// If (idxAttachPt) == 0 then: (pObj == NULL) || (pObj->isAttachment() == FALSE)
*/

	if (eLock & RLV_LOCK_REMOVE)
	{
		RLV_ASSERT( m_WearableTypeRem.lower_bound(eType) != m_WearableTypeRem.upper_bound(eType) ); // The lock should always exist
		bool removed_entry = false;
		for (rlv_wearabletypelock_map_t::iterator itWearableType = m_WearableTypeRem.lower_bound(eType), 
				endWearableType = m_WearableTypeRem.upper_bound(eType); itWearableType != endWearableType; ++itWearableType)
		{
			if (idRlvObj == itWearableType->second)
			{
				m_WearableTypeRem.erase(itWearableType);
				removed_entry = true;
				break;
			}
		}
		if(removed_entry)
		{
			if(m_WearableTypeRem.find(eType) == m_WearableTypeRem.end())
			{
				LLUUID item_id = gAgentWearables.getWearableItemID(eType, 0);	// TODO: MULTI-WEARABLE
				if(item_id.notNull())
				{
					gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);
					gInventory.notifyObservers();
				}
			}
		}
	}
	if (eLock & RLV_LOCK_ADD)
	{
		RLV_ASSERT( m_WearableTypeAdd.lower_bound(eType) != m_WearableTypeAdd.upper_bound(eType) ); // The lock should always exist
		for (rlv_wearabletypelock_map_t::iterator itWearableType = m_WearableTypeAdd.lower_bound(eType), 
				endWearableType = m_WearableTypeAdd.upper_bound(eType); itWearableType != endWearableType; ++itWearableType)
		{
			if (idRlvObj == itWearableType->second)
			{
				m_WearableTypeAdd.erase(itWearableType);
				break;
			}
		}
	}
}

// ============================================================================
