/**
 * @file llgiveinventory.cpp
 * @brief LLGiveInventory class implementation
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"
#include "llgiveinventory.h"

// library includes
#include "llnotificationsutil.h"
#include "lltrans.h"

// newview includes
#include "llagent.h"
#include "llagentdata.h"
#include "llagentui.h"
#include "llagentwearables.h"
#include "llfloatertools.h" // for gFloaterTool
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llimview.h"
#include "llinventory.h"
#include "llinventoryfunctions.h"
#include "llmutelist.h"
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"

// MAX ITEMS is based on (sizeof(uuid)+2) * count must be < MTUBYTES
// or 18 * count < 1200 => count < 1200/18 => 66. I've cut it down a
// bit from there to give some pad.
const S32 MAX_ITEMS = 42;

class LLGiveable : public LLInventoryCollectFunctor
{
public:
	LLGiveable() : mCountLosing(0) {}
	virtual ~LLGiveable() {}
	virtual bool operator()(LLInventoryCategory* cat, LLInventoryItem* item);

	S32 countNoCopy() const { return mCountLosing; }
protected:
	S32 mCountLosing;
};

bool LLGiveable::operator()(LLInventoryCategory* cat, LLInventoryItem* item)
{
	// All categories can be given.
	if (cat)
		return true;

	bool allowed = false;
	if(item)
	{
		allowed = itemTransferCommonlyAllowed(item);
		if(allowed &&
		   !item->getPermissions().allowOperationBy(PERM_TRANSFER,
							    gAgent.getID()))
		{
			allowed = FALSE;
		}
		if(allowed &&
		   !item->getPermissions().allowCopyBy(gAgent.getID()))
		{
			++mCountLosing;
		}
	}
	return allowed;
}

class LLUncopyableItems : public LLInventoryCollectFunctor
{
public:
	LLUncopyableItems() {}
	virtual ~LLUncopyableItems() {}
	virtual bool operator()(LLInventoryCategory* cat, LLInventoryItem* item);
};

bool LLUncopyableItems::operator()(LLInventoryCategory* cat,
				   LLInventoryItem* item)
{
	bool uncopyable = false;
	if(item)
	{
		if (itemTransferCommonlyAllowed(item) &&
		   !item->getPermissions().allowCopyBy(gAgent.getID()))
		{
			uncopyable = true;
		}
	}
	return uncopyable;
}

// static
bool LLGiveInventory::isInventoryGiveAcceptable(const LLInventoryItem* item)
{
	if (!item) return false;
	
	if (!isAgentAvatarValid()) return false;

	if (!item->getPermissions().allowOperationBy(PERM_TRANSFER, gAgentID))
	{
		return false;
	}
	
	
	bool acceptable = true;
	switch(item->getType())
	{
	case LLAssetType::AT_CALLINGCARD:
		acceptable = false;
		break;
	case LLAssetType::AT_OBJECT:
		// <edit>		
		/*if(my_avatar->isWearingAttachment(item->getUUID()))
		{
			acceptable = false;
		}*/
		// </edit>
		break;
	case LLAssetType::AT_BODYPART:
	case LLAssetType::AT_CLOTHING:
		{
			// <edit>
			/*bool copyable = false;
			if(item->getPermissions().allowCopyBy(gAgent.getID())) copyable = true;
		
			if(!copyable && gAgentWearables.isWearingItem(item->getUUID()))
			{
				acceptable = false;
			}*/
			// </edit>
		}
		
		break;
	default:
		break;
	}
	return acceptable;
}

// Static
bool LLGiveInventory::isInventoryGroupGiveAcceptable(const LLInventoryItem* item)
{
	if(!item) return false;

	if (!isAgentAvatarValid()) return false;

	// These permissions are double checked in the simulator in
	// LLGroupNoticeInventoryItemFetch::result().
	if (!item->getPermissions().allowOperationBy(PERM_TRANSFER, gAgentID))
	{
		return false;
	}
	if (!item->getPermissions().allowCopyBy(gAgent.getID()))
	{
		return false;
	}


	bool acceptable = true;
	
	switch(item->getType())
	{
	case LLAssetType::AT_CALLINGCARD:
		acceptable = false;
		break;
		// <edit>
	/*case LLAssetType::AT_OBJECT:
		if(gAgentAvatarp->isWearingAttachment(item->getUUID()))
		{
			acceptable = false;
		}*
		break;*/
		// </edit>
	default:
		break;
	}
	return acceptable;
}

// static
bool LLGiveInventory::doGiveInventoryItem(const LLUUID& to_agent,
									  const LLInventoryItem* item,
									  const LLUUID& im_session_id/* = LLUUID::null*/)

{
	bool res = true;
	llinfos << "LLGiveInventory::giveInventory()" << llendl;
	if(!isInventoryGiveAcceptable(item))
	{
		return false;
	}
	if (item->getPermissions().allowCopyBy(gAgentID))
	{
		// just give it away.
		LLGiveInventory::commitGiveInventoryItem(to_agent, item, im_session_id);
	}
	else
	{
		// ask if the agent is sure.
		LLSD payload;
		payload["agent_id"] = to_agent;
		payload["item_id"] = item->getUUID();
		LLNotificationsUtil::add("CannotCopyWarning", LLSD(), payload, 
		        &LLGiveInventory::handleCopyProtectedItem);
		res = false;
	}

	return res;
}

void LLGiveInventory::doGiveInventoryCategory(const LLUUID& to_agent,
											  const LLInventoryCategory* cat,
											  const LLUUID& im_session_id)

{
	if (!cat) return;
	llinfos << "LLGiveInventory::giveInventoryCategory() - "
		<< cat->getUUID() << llendl;

	if (!isAgentAvatarValid()) return;

	// Test out how many items are being given.
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLGiveable giveable;
	gInventory.collectDescendentsIf(cat->getUUID(),
									cats,
									items,
									LLInventoryModel::EXCLUDE_TRASH,
									giveable);
	S32 count = cats.count();
	bool complete = true;
	for(S32 i = 0; i < count; ++i)
	{
		if(!gInventory.isCategoryComplete(cats.get(i)->getUUID()))
		{
			complete = false;
			break;
		}
	}
	if(!complete)
	{
		LLNotificationsUtil::add("IncompleteInventory");
		return;
	}
 	count = items.count() + cats.count();
 	if(count > MAX_ITEMS)
  	{
		LLNotificationsUtil::add("TooManyItems");
  		return;
  	}
 	else if(count == 0)
  	{
		LLNotificationsUtil::add("NoItems");
  		return;
  	}
	else
	{
		if(0 == giveable.countNoCopy())
		{
			LLGiveInventory::commitGiveInventoryCategory(to_agent, cat, im_session_id);
		}
		else 
		{
			LLSD args;
			args["COUNT"] = llformat("%d",giveable.countNoCopy());
			LLSD payload;
			payload["agent_id"] = to_agent;
			payload["folder_id"] = cat->getUUID();
			LLNotificationsUtil::add("CannotCopyCountItems", args, payload, &LLGiveInventory::handleCopyProtectedCategory);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//     PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//static
void LLGiveInventory::logInventoryOffer(const LLUUID& to_agent, const LLUUID &im_session_id)
{
	// If this item was given by drag-and-drop into an IM panel, log this action in the IM panel chat.
	if (im_session_id.notNull())
	{
		LLSD args;
		gIMMgr->addSystemMessage(im_session_id, "inventory_item_offered", args);
	}
}

// static
bool LLGiveInventory::handleCopyProtectedItem(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLInventoryItem* item = NULL;
	switch(option)
	{
	case 0:  // "Yes"
		item = gInventory.getItem(notification["payload"]["item_id"].asUUID());
		if(item)
		{
			LLGiveInventory::commitGiveInventoryItem(notification["payload"]["agent_id"].asUUID(),
													   item);
			// delete it for now - it will be deleted on the server
			// quickly enough.
			gInventory.deleteObject(notification["payload"]["item_id"].asUUID());
			gInventory.notifyObservers();
		}
		else
		{
			LLNotificationsUtil::add("CannotGiveItem");		
		}
		break;

	default: // no, cancel, whatever, who cares, not yes.
		LLNotificationsUtil::add("TransactionCancelled");
		break;
	}
	return false;
}

// static
void LLGiveInventory::commitGiveInventoryItem(const LLUUID& to_agent,
												const LLInventoryItem* item,
												const LLUUID& im_session_id)
{
	if (!item) return;
	std::string name;
	LLAgentUI::buildFullname(name);
	LLUUID transaction_id;
	transaction_id.generate();
	const S32 BUCKET_SIZE = sizeof(U8) + UUID_BYTES;
	U8 bucket[BUCKET_SIZE];
	bucket[0] = (U8)item->getType();
	memcpy(&bucket[1], &(item->getUUID().mData), UUID_BYTES);		/* Flawfinder: ignore */
	pack_instant_message(
		gMessageSystem,
		gAgentID,
		FALSE,
		gAgentSessionID,
		to_agent,
		name,
		item->getName(),
		IM_ONLINE,
		IM_INVENTORY_OFFERED,
		transaction_id,
		0,
		LLUUID::null,
		gAgent.getPositionAgent(),
		NO_TIMESTAMP,
		bucket,
		BUCKET_SIZE);
	gAgent.sendReliableMessage(); 
	// <edit>
	if (gSavedSettings.getBOOL("BroadcastViewerEffects"))
	{
		// </edit>
		// VEFFECT: giveInventory
		LLHUDEffectSpiral *effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_BEAM, TRUE);
		effectp->setSourceObject(gAgentAvatarp);
		effectp->setTargetObject(gObjectList.findObject(to_agent));
		effectp->setDuration(LL_HUD_DUR_SHORT);
		effectp->setColor(LLColor4U(gAgent.getEffectColor()));
	// <edit>
	}
	// </edit>
	gFloaterTools->dirty();

	LLMuteList::getInstance()->autoRemove(to_agent, LLMuteList::AR_INVENTORY);

	logInventoryOffer(to_agent, im_session_id);
}

// static
bool LLGiveInventory::handleCopyProtectedCategory(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLInventoryCategory* cat = NULL;
	switch(option)
	{
	case 0:  // "Yes"
		cat = gInventory.getCategory(notification["payload"]["folder_id"].asUUID());
		if(cat)
		{
			LLGiveInventory::commitGiveInventoryCategory(notification["payload"]["agent_id"].asUUID(),
														   cat);
			LLViewerInventoryCategory::cat_array_t cats;
			LLViewerInventoryItem::item_array_t items;
			LLUncopyableItems remove;
			gInventory.collectDescendentsIf(cat->getUUID(),
											cats,
											items,
											LLInventoryModel::EXCLUDE_TRASH,
											remove);
			S32 count = items.count();
			for(S32 i = 0; i < count; ++i)
			{
				gInventory.deleteObject(items.get(i)->getUUID());
			}
			gInventory.notifyObservers();
		}
		else
		{
			LLNotificationsUtil::add("CannotGiveCategory");
		}
		break;

	default: // no, cancel, whatever, who cares, not yes.
		LLNotificationsUtil::add("TransactionCancelled");
		break;
	}
	return false;
}

// static
void LLGiveInventory::commitGiveInventoryCategory(const LLUUID& to_agent,
													const LLInventoryCategory* cat,
													const LLUUID& im_session_id)

{
if(!cat) return;
	llinfos << "LLGiveInventory::commitGiveInventoryCategory() - "
			<< cat->getUUID() << llendl;

	// Test out how many items are being given.
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLGiveable giveable;
	gInventory.collectDescendentsIf(cat->getUUID(),
									cats,
									items,
									LLInventoryModel::EXCLUDE_TRASH,
									giveable);

	// MAX ITEMS is based on (sizeof(uuid)+2) * count must be <
	// MTUBYTES or 18 * count < 1200 => count < 1200/18 =>
	// 66. I've cut it down a bit from there to give some pad.
 	S32 count = items.count() + cats.count();
 	if(count > MAX_ITEMS)
  	{
		LLNotificationsUtil::add("TooManyItems");
  		return;
  	}
 	else if(count == 0)
  	{
		LLNotificationsUtil::add("NoItems");
  		return;
  	}
	else
	{
		std::string name;
		LLAgentUI::buildFullname(name);
		LLUUID transaction_id;
		transaction_id.generate();
		S32 bucket_size = (sizeof(U8) + UUID_BYTES) * (count + 1);
		U8* bucket = new U8[bucket_size];
		U8* pos = bucket;
		U8 type = (U8)cat->getType();
		memcpy(pos, &type, sizeof(U8));		/* Flawfinder: ignore */
		pos += sizeof(U8);
		memcpy(pos, &(cat->getUUID()), UUID_BYTES);		/* Flawfinder: ignore */
		pos += UUID_BYTES;
		S32 i;
		count = cats.count();
		for(i = 0; i < count; ++i)
		{
			memcpy(pos, &type, sizeof(U8));		/* Flawfinder: ignore */
			pos += sizeof(U8);
			memcpy(pos, &(cats.get(i)->getUUID()), UUID_BYTES);		/* Flawfinder: ignore */
			pos += UUID_BYTES;
		}
		count = items.count();
		for(i = 0; i < count; ++i)
		{
			type = (U8)items.get(i)->getType();
			memcpy(pos, &type, sizeof(U8));		/* Flawfinder: ignore */
			pos += sizeof(U8);
			memcpy(pos, &(items.get(i)->getUUID()), UUID_BYTES);		/* Flawfinder: ignore */
			pos += UUID_BYTES;
		}
		pack_instant_message(
			gMessageSystem,
			gAgent.getID(),
			FALSE,
			gAgent.getSessionID(),
			to_agent,
			name,
			cat->getName(),
			IM_ONLINE,
			IM_INVENTORY_OFFERED,
			transaction_id,
			0,
			LLUUID::null,
			gAgent.getPositionAgent(),
			NO_TIMESTAMP,
			bucket,
			bucket_size);
		gAgent.sendReliableMessage();
		delete[] bucket;
		// <edit>
 		if (gSavedSettings.getBOOL("BroadcastViewerEffects"))
		{
 			// </edit>
			// VEFFECT: giveInventoryCategory
			LLHUDEffectSpiral *effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_BEAM, TRUE);
			effectp->setSourceObject(gAgentAvatarp);
			effectp->setTargetObject(gObjectList.findObject(to_agent));
			effectp->setDuration(LL_HUD_DUR_SHORT);
			effectp->setColor(LLColor4U(gAgent.getEffectColor()));
			// <edit>
		}
		// </edit>
		gFloaterTools->dirty();

		LLMuteList::getInstance()->autoRemove(to_agent, LLMuteList::AR_INVENTORY);

		logInventoryOffer(to_agent, im_session_id);
	}
}
// EOF
