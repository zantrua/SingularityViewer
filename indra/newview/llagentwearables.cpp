/** 
 * @file llagentwearables.cpp
 * @brief LLAgentWearables class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 * 
 */

#include "llviewerprecompiledheaders.h"
#include "llagentwearables.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llcallbacklist.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "llinventorybridge.h"
#include "llinventoryview.h"
#include "llmd5.h"
#include "llnotificationsutil.h"
#include "lltexlayer.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llwearable.h"
#include "llwearablelist.h"

#include "cofmgr.h"
#include "llfloatercustomize.h"

// [RLVa:KB] - Checked: 2010-09-27 (RLVa-1.1.3b)
#include "rlvhandler.h"
#include "rlvinventory.h"
#include "llattachmentsmgr.h"
// [/RLVa:KB]

#include "hippogridmanager.h"

#include <boost/scoped_ptr.hpp>

LLAgentWearables gAgentWearables;

BOOL LLAgentWearables::mInitialWearablesUpdateReceived = FALSE;

using namespace LLVOAvatarDefines;
LLAgentWearables::LLAgentWearables() :
	mWearablesLoaded(FALSE)
{
}

LLAgentWearables::~LLAgentWearables()
{
	cleanup();
}

void LLAgentWearables::cleanup()
{
}

// static
void LLAgentWearables::initClass()
{
}

void LLAgentWearables::setAvatarObject(LLVOAvatarSelf *avatar)
{ 
	if (avatar)
	{
		sendAgentWearablesRequest();
	}
}
// wearables
LLAgentWearables::createStandardWearablesAllDoneCallback::~createStandardWearablesAllDoneCallback()
{
	llinfos << "destructor - all done?" << llendl;
	gAgentWearables.createStandardWearablesAllDone();
}

LLAgentWearables::sendAgentWearablesUpdateCallback::~sendAgentWearablesUpdateCallback()
{
	gAgentWearables.sendAgentWearablesUpdate();
}

/**
 * @brief Construct a callback for dealing with the wearables.
 *
 * Would like to pass the agent in here, but we can't safely
 * count on it being around later.  Just use gAgent directly.
 * @param cb callback to execute on completion (??? unused ???)
 * @param type Type for the wearable in the agent
 * @param wearable The wearable data.
 * @param todo Bitmask of actions to take on completion.
 */
LLAgentWearables::addWearableToAgentInventoryCallback::addWearableToAgentInventoryCallback(
	LLPointer<LLRefCount> cb, LLWearableType::EType type, U32 index, LLWearable* wearable, U32 todo) :
	mType(type),
	mIndex(index),	
	mWearable(wearable),
	mTodo(todo),
	mCB(cb)
{
	llassert_always(index == 0);
	llinfos << "constructor" << llendl;
}

void LLAgentWearables::addWearableToAgentInventoryCallback::fire(const LLUUID& inv_item)
{
	if (mTodo & CALL_CREATESTANDARDDONE)
	{
		llinfos << "callback fired, inv_item " << inv_item.asString() << llendl;
	}

	if (inv_item.isNull())
		return;

	gAgentWearables.addWearabletoAgentInventoryDone(mType, mIndex, inv_item, mWearable);

	if (mTodo & CALL_UPDATE)
	{
		gAgentWearables.sendAgentWearablesUpdate();
	}
	if (mTodo & CALL_RECOVERDONE)
	{
		gAgentWearables.recoverMissingWearableDone();
	}
	/*
	 * Do this for every one in the loop
	 */
	if (mTodo & CALL_CREATESTANDARDDONE)
	{
		gAgentWearables.createStandardWearablesDone(mType, mIndex);
	}
	if (mTodo & CALL_MAKENEWOUTFITDONE)
	{
		gAgentWearables.makeNewOutfitDone(mType, mIndex);
	}
}

void LLAgentWearables::addWearabletoAgentInventoryDone(const LLWearableType::EType type,
													   const U32 index,
													   const LLUUID& item_id,
													   LLWearable* wearable)
{
	llassert_always(index == 0);
	
	llinfos << "type " << type << " index " << index << " item " << item_id.asString() << llendl;

	if (item_id.isNull())
		return;
	
	LLUUID old_item_id = getWearableItemID(type,index);

	if (wearable)
	{
		wearable->setItemID(item_id);

		if (old_item_id.notNull())
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
			setWearable(type,index,wearable);
		}
		else
		{
			pushWearable(type,wearable);
		}
	}

	gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);

	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if(item && wearable)
	{
		// We're changing the asset id, so we both need to set it
		// locally via setAssetUUID() and via setTransactionID() which
		// will be decoded on the server. JC
		item->setAssetUUID(wearable->getAssetID());
		item->setTransactionID(wearable->getTransactionID());
		gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		item->updateServer(FALSE);
	}
	gInventory.notifyObservers();
}

void LLAgentWearables::sendAgentWearablesUpdate()
{
	// First make sure that we have inventory items for each wearable
	for (S32 type=0; type < LLWearableType::WT_COUNT; ++type)
	{
		for (U32 index=0; index < getWearableCount((LLWearableType::EType)type); ++index)
		{
			LLWearable* wearable = getWearable((LLWearableType::EType)type,index);
			if (wearable)
			{
				if (wearable->getItemID().isNull())
				{
					LLPointer<LLInventoryCallback> cb =
						new addWearableToAgentInventoryCallback(
							LLPointer<LLRefCount>(NULL),
							(LLWearableType::EType)type,
							index,
							wearable,
							addWearableToAgentInventoryCallback::CALL_NONE);
					addWearableToAgentInventory(cb, wearable);
				}
				else
				{
					gInventory.addChangedMask( LLInventoryObserver::LABEL,
											  wearable->getItemID());
				}
			}
		}
	}

	// Then make sure the inventory is in sync with the avatar.
	gInventory.notifyObservers();

	// This isn't the proper place to be doing this, but it's a good "catch-all"
	LLCOFMgr::instance().synchWearables();

	// Send the AgentIsNowWearing 
	gMessageSystem->newMessageFast(_PREHASH_AgentIsNowWearing);

	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	lldebugs << "sendAgentWearablesUpdate()" << llendl;
	// MULTI-WEARABLE: DEPRECATED: HACK: index to 0- server database tables don't support concept of multiwearables.
	for (S32 type=0; type < LLWearableType::WT_COUNT; ++type)
	{
		gMessageSystem->nextBlockFast(_PREHASH_WearableData);

		U8 type_u8 = (U8)type;
		gMessageSystem->addU8Fast(_PREHASH_WearableType, type_u8 );

		LLWearable* wearable = getWearable((LLWearableType::EType)type, 0);
		if( wearable )
		{
			//llinfos << "Sending wearable " << wearable->getName() << llendl;
			LLUUID item_id = wearable->getItemID();
			const LLViewerInventoryItem *item = gInventory.getItem(item_id);
			if (item && item->getIsLinkType())
			{
				// Get the itemID that this item points to.  i.e. make sure
				// we are storing baseitems, not their links, in the database.
				item_id = item->getLinkedUUID();
			}
			gMessageSystem->addUUIDFast(_PREHASH_ItemID, item_id);
		}
		else
		{
			//llinfos << "Not wearing wearable type " << LLWearableType::getTypeName((LLWearableType::EType)i) << llendl;
			gMessageSystem->addUUIDFast(_PREHASH_ItemID, LLUUID::null);
		}

		lldebugs << "       " << LLWearableType::getTypeLabel((LLWearableType::EType)type) << ": " << (wearable ? wearable->getAssetID() : LLUUID::null) << llendl;
	}
	gAgent.sendReliableMessage();
}

void LLAgentWearables::saveWearable(const LLWearableType::EType type, const U32 index, BOOL send_update,
									const std::string new_name)
{
	llassert_always(index == 0);
	LLWearable* old_wearable = getWearable(type, index);
	if(!old_wearable) return;
	bool name_changed = !new_name.empty() && (new_name != old_wearable->getName());
	if (name_changed || old_wearable->isDirty() || old_wearable->isOldVersion())
	{
		LLUUID old_item_id = old_wearable->getItemID();
		LLWearable* new_wearable = LLWearableList::instance().createCopyFromAvatar( old_wearable );
		new_wearable->setItemID(old_item_id); // should this be in LLWearable::copyDataFrom()?
		setWearable(type,index,new_wearable);

		LLInventoryItem* item = gInventory.getItem(old_item_id);
		if( item )
		{
			std::string item_name = item->getName();
			if (name_changed)
			{
				llinfos << "saveWearable changing name from "  << item->getName() << " to " << new_name << llendl;
				item_name = new_name;
			}
			// Update existing inventory item
			LLPointer<LLViewerInventoryItem> template_item =
				new LLViewerInventoryItem(item->getUUID(),
										  item->getParentUUID(),
										  item->getPermissions(),
										  new_wearable->getAssetID(),
										  new_wearable->getAssetType(),
										  item->getInventoryType(),
										  item_name,
										  item->getDescription(),
										  item->getSaleInfo(),
										  item->getFlags(),
										  item->getCreationDate());
			template_item->setTransactionID(new_wearable->getTransactionID());
			template_item->updateServer(FALSE);
			gInventory.updateItem(template_item);
			if (name_changed)
			{
				gInventory.notifyObservers();
			}
		}
		else
		{
			// Add a new inventory item (shouldn't ever happen here)
			U32 todo = addWearableToAgentInventoryCallback::CALL_NONE;
			if (send_update)
			{
				todo |= addWearableToAgentInventoryCallback::CALL_UPDATE;
			}
			LLPointer<LLInventoryCallback> cb =
				new addWearableToAgentInventoryCallback(
					LLPointer<LLRefCount>(NULL),
					type,
					index,
					new_wearable,
					todo);
			addWearableToAgentInventory(cb, new_wearable);
			return;
		}
		
		gAgentAvatarp->wearableUpdated( type, TRUE );

		if( send_update )
		{
			sendAgentWearablesUpdate();
		}
	}
}

void LLAgentWearables::saveWearableAs(const LLWearableType::EType type,
									  const U32 index,
									  const std::string& new_name,
									  BOOL save_in_lost_and_found)
{
	llassert_always(index == 0);
	if (!isWearableCopyable(type, index))
	{
		llwarns << "LLAgent::saveWearableAs() not copyable." << llendl;
		return;
	}
	LLWearable* old_wearable = getWearable(type, index);
	if (!old_wearable)
	{
		llwarns << "LLAgent::saveWearableAs() no old wearable." << llendl;
		return;
	}

	LLInventoryItem* item = gInventory.getItem(getWearableItemID(type,index));
	if (!item)
	{
		llwarns << "LLAgent::saveWearableAs() no inventory item." << llendl;
		return;
	}
	std::string trunc_name(new_name);
	LLStringUtil::truncate(trunc_name, DB_INV_ITEM_NAME_STR_LEN);
	LLWearable* new_wearable = LLWearableList::instance().createCopyFromAvatar(
		old_wearable,
		trunc_name);
	LLPointer<LLInventoryCallback> cb =
		new addWearableToAgentInventoryCallback(
			LLPointer<LLRefCount>(NULL),
			type,
			index,
			new_wearable,
			addWearableToAgentInventoryCallback::CALL_UPDATE);
	LLUUID category_id;
	if (save_in_lost_and_found)
	{
		category_id = gInventory.findCategoryUUIDForType(
			LLFolderType::FT_LOST_AND_FOUND);
	}
	else
	{
		// put in same folder as original
		category_id = item->getParentUUID();
	}

	copy_inventory_item(
		gAgent.getID(),
		item->getPermissions().getOwner(),
		item->getUUID(),
		category_id,
		new_name,
		cb);

/*
	LLWearable* old_wearable = getWearable( type );
	if( old_wearable )
	{
		std::string old_name = old_wearable->getName();
		old_wearable->setName( new_name );
		LLWearable* new_wearable = LLWearableList::instance().createCopyFromAvatar( old_wearable );
		old_wearable->setName( old_name );
			
		LLUUID category_id;
		LLInventoryItem* item = gInventory.getItem( mWearableEntry[ type ].mItemID );
		if( item )
		{
			new_wearable->setPermissions(item->getPermissions());
			if (save_in_lost_and_found)
			{
				category_id = gInventory.findCategoryUUIDForType(LLAssetType::AT_LOST_AND_FOUND);
			}
			else
			{
				// put in same folder as original
				category_id = item->getParentUUID();
			}
			LLInventoryView* view = LLInventoryView::getActiveInventory();
			if(view)
			{
				view->getPanel()->setSelection(item->getUUID(), TAKE_FOCUS_NO);
			}
		}

		mWearableEntry[ type ].mWearable = new_wearable;
		LLPointer<LLInventoryCallback> cb =
			new addWearableToAgentInventoryCallback(
				LLPointer<LLRefCount>(NULL),
				type,
				addWearableToAgentInventoryCallback::CALL_UPDATE);
		addWearableToAgentInventory(cb, new_wearable, category_id);
	}
*/
}

void LLAgentWearables::revertWearable(const LLWearableType::EType type, const U32 index)
{
	llassert_always(index == 0);
	LLWearable* wearable = getWearable(type, index);
	llassert(wearable);
	if( wearable )
	{
		wearable->writeToAvatar( TRUE );
	}

	gAgent.sendAgentSetAppearance();
}

void LLAgentWearables::revertAllWearables()
{
	for( S32 i=0; i < LLWearableType::WT_COUNT; i++ )
	{
		for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
			revertWearable( (LLWearableType::EType)i, j);
	}
}

void LLAgentWearables::saveAllWearables()
{
	//if(!gInventory.isLoaded())
	//{
	//	return;
	//}

	for( S32 i=0; i < LLWearableType::WT_COUNT; i++ )
	{
		for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
			saveWearable((LLWearableType::EType)i, j, FALSE);
	}
	sendAgentWearablesUpdate();
}

// Called when the user changes the name of a wearable inventory item that is currenlty being worn.
void LLAgentWearables::setWearableName( const LLUUID& item_id, const std::string& new_name )
{
	for( S32 i=0; i < LLWearableType::WT_COUNT; i++ )
	{
		for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
		{
			LLUUID curr_item_id = getWearableItemID((LLWearableType::EType)i,j);
			if (curr_item_id == item_id)
			{
				LLWearable* old_wearable = getWearable((LLWearableType::EType)i,j);
				llassert(old_wearable);
				if (!old_wearable) continue;

				std::string old_name = old_wearable->getName();
				old_wearable->setName(new_name);
				LLWearable* new_wearable = LLWearableList::instance().createCopy(old_wearable);
				new_wearable->setItemID(item_id);
				LLInventoryItem* item = gInventory.getItem(item_id);
				if (item)
				{
					new_wearable->setPermissions(item->getPermissions());
				}
				old_wearable->setName(old_name);

				setWearable((LLWearableType::EType)i,j,new_wearable);
				sendAgentWearablesUpdate();
				break;
			}
		}
	}
}


BOOL LLAgentWearables::isWearableModifiable(LLWearableType::EType type, U32 index) const
{
	llassert_always(index == 0);
	LLUUID item_id = getWearableItemID(type, index);
	return item_id.notNull() ? isWearableModifiable(item_id) : FALSE;
}

BOOL LLAgentWearables::isWearableModifiable(const LLUUID& item_id) const
{
	const LLUUID& linked_id = gInventory.getLinkedItemID(item_id);
	if (linked_id.notNull())
	{
		LLInventoryItem* item = gInventory.getItem(linked_id);
		if(item && item->getPermissions().allowModifyBy(gAgent.getID(),
														gAgent.getGroupID()))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL LLAgentWearables::isWearableCopyable(LLWearableType::EType type, U32 index) const
{
	llassert_always(index == 0);
	LLUUID item_id = getWearableItemID(type, index);
	if (!item_id.isNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if(item && item->getPermissions().allowCopyBy(gAgent.getID(),
													  gAgent.getGroupID()))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL LLAgentWearables::areWearablesLoaded() const
{
	if(gSavedSettings.getBOOL("RenderUnloadedAvatar"))
		return TRUE;
	return mWearablesLoaded;
}

/*U32 LLAgentWearables::getWearablePermMask(LLWearableType::EType type) const
{
	LLUUID item_id = getWearableItemID(type);
	if(!item_id.isNull())
	{
		LLInventoryItem* item = gInventory.getItem(item_id);
		if(item)
		{
			return item->getPermissions().getMaskOwner();
		}
	}
	return PERM_NONE;
}
*/

LLInventoryItem* LLAgentWearables::getWearableInventoryItem(LLWearableType::EType type, U32 index)
{
	llassert_always(index == 0);
	LLUUID item_id = getWearableItemID(type,index);
	LLInventoryItem* item = NULL;
	if(item_id.notNull())
	{
		 item = gInventory.getItem(item_id);
	}
	return item;
}

const LLWearable* LLAgentWearables::getWearableFromItemID( const LLUUID& item_id ) const
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
	{
		for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
		{
			const LLWearable * curr_wearable = getWearable((LLWearableType::EType)i, j);
			if (curr_wearable && (curr_wearable->getItemID() == base_item_id))
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

LLWearable* LLAgentWearables::getWearableFromItemID( const LLUUID& item_id )
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
	{
		for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
		{
			LLWearable * curr_wearable = getWearable((LLWearableType::EType)i, j);
			if (curr_wearable && (curr_wearable->getItemID() == base_item_id))
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

LLWearable*	LLAgentWearables::getWearableFromAssetID(const LLUUID& asset_id) 
{
	for (S32 i=0; i < LLWearableType::WT_COUNT; i++)
	{
		for (U32 j=0; j < getWearableCount((LLWearableType::EType)i); j++)
		{
			LLWearable * curr_wearable = getWearable((LLWearableType::EType)i, j);
			if (curr_wearable && (curr_wearable->getAssetID() == asset_id))
			{
				return curr_wearable;
			}
		}
	}
	return NULL;
}

void LLAgentWearables::sendAgentWearablesRequest()
{
	gMessageSystem->newMessageFast(_PREHASH_AgentWearablesRequest);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID() );
	gAgent.sendReliableMessage();
}

// Used to enable/disable menu items.
// static
BOOL LLAgentWearables::selfHasWearable(LLWearableType::EType type)
{
	return (gAgentWearables.getWearableCount(type) > 0);
}

LLWearable* LLAgentWearables::getWearable(const LLWearableType::EType type, U32 index)
{
	llassert_always(index == 0);
	wearableentry_map_t::iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		return NULL;
	}
	wearableentry_vec_t& wearable_vec = wearable_iter->second;
	if (index>=wearable_vec.size())
	{
		return NULL;
	}
	else
	{
		return wearable_vec[index];
	}
}

void LLAgentWearables::setWearable(const LLWearableType::EType type, U32 index, LLWearable *wearable)
{
	llassert_always(index == 0);
	LLWearable *old_wearable = getWearable(type,index);
	if (!old_wearable)
	{
		pushWearable(type,wearable);
		return;
	}
	
	wearableentry_map_t::iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		llwarns << "invalid type, type " << type << " index " << index << llendl; 
		return;
	}
	wearableentry_vec_t& wearable_vec = wearable_iter->second;
	if (index>=wearable_vec.size())
	{
		llwarns << "invalid index, type " << type << " index " << index << llendl; 
	}
	else
	{
		wearable_vec[index] = wearable;
		old_wearable->setLabelUpdated();
		wearableUpdated(wearable);
	}
}

U32 LLAgentWearables::pushWearable(const LLWearableType::EType type, LLWearable *wearable)
{
	if (wearable == NULL)
	{
		// no null wearables please!
		llwarns << "Null wearable sent for type " << type << llendl;
		return MAX_CLOTHING_PER_TYPE;
	}
	if (type < LLWearableType::WT_COUNT || mWearableDatas[type].size() < MAX_CLOTHING_PER_TYPE)
	{
		mWearableDatas[type].push_back(wearable);
		wearableUpdated(wearable);
		return mWearableDatas[type].size()-1;
	}
	return MAX_CLOTHING_PER_TYPE;
}

void LLAgentWearables::wearableUpdated(LLWearable *wearable)
{
	gAgentAvatarp->wearableUpdated(wearable->getType(), FALSE);
	wearable->refreshName();
	wearable->setLabelUpdated();
}

void LLAgentWearables::popWearable(LLWearable *wearable)
{
	if (wearable == NULL)
	{
		// nothing to do here. move along.
		return;
	}

	U32 index = getWearableIndex(wearable);
	LLWearableType::EType type = wearable->getType();

	if (index < MAX_CLOTHING_PER_TYPE && index < getWearableCount(type))
	{
		popWearable(type, index);
	}
}

void LLAgentWearables::popWearable(const LLWearableType::EType type, U32 index)
{
	llassert_always(index == 0);
	LLWearable *wearable = getWearable(type, index);
	if (wearable)
	{
		mWearableDatas[type].erase(mWearableDatas[type].begin() + index);
		gAgentAvatarp->wearableUpdated(wearable->getType(), TRUE);
		wearable->setLabelUpdated();
	}
}

U32	LLAgentWearables::getWearableIndex(const LLWearable *wearable) const
{
	if (wearable == NULL)
	{
		return MAX_CLOTHING_PER_TYPE;
	}

	const LLWearableType::EType type = wearable->getType();
	wearableentry_map_t::const_iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		llwarns << "tried to get wearable index with an invalid type!" << llendl;
		return MAX_CLOTHING_PER_TYPE;
	}
	const wearableentry_vec_t& wearable_vec = wearable_iter->second;
	for(U32 index = 0; index < wearable_vec.size(); index++)
	{
		if (wearable_vec[index] == wearable)
		{
			return index;
		}
	}

	return MAX_CLOTHING_PER_TYPE;
}

const LLWearable* LLAgentWearables::getWearable(const LLWearableType::EType type, U32 index) const
{
	llassert_always(index == 0);
	wearableentry_map_t::const_iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		return NULL;
	}
	const wearableentry_vec_t& wearable_vec = wearable_iter->second;
	if (index>=wearable_vec.size())
	{
		return NULL;
	}
	else
	{
		return wearable_vec[index];
	}
}

LLWearable* LLAgentWearables::getTopWearable(const LLWearableType::EType type)
{
	U32 count = getWearableCount(type);
	if ( count == 0)
	{
		return NULL;
	}

	return getWearable(type, count-1);
}

LLWearable* LLAgentWearables::getBottomWearable(const LLWearableType::EType type)
{
	if (getWearableCount(type) == 0)
	{
		return NULL;
	}

	return getWearable(type, 0);
}

U32 LLAgentWearables::getWearableCount(const LLWearableType::EType type) const
{
	wearableentry_map_t::const_iterator wearable_iter = mWearableDatas.find(type);
	if (wearable_iter == mWearableDatas.end())
	{
		return 0;
	}
	const wearableentry_vec_t& wearable_vec = wearable_iter->second;
	return wearable_vec.size();
}

U32 LLAgentWearables::getWearableCount(const U32 tex_index) const
{
	const LLWearableType::EType wearable_type = LLVOAvatarDictionary::getTEWearableType((LLVOAvatarDefines::ETextureIndex)tex_index);
	return getWearableCount(wearable_type);
}


const LLUUID LLAgentWearables::getWearableItemID(LLWearableType::EType type, U32 index) const
{
	llassert_always(index == 0);
	const LLWearable *wearable = getWearable(type,index);
	if (wearable)
		return wearable->getItemID();
	else
		return LLUUID();
}

const LLUUID LLAgentWearables::getWearableAssetID(LLWearableType::EType type, U32 index) const
{
	llassert_always(index == 0);
	const LLWearable *wearable = getWearable(type,index);
	if (wearable)
		return wearable->getAssetID();
	else
		return LLUUID();
}

BOOL LLAgentWearables::isWearingItem( const LLUUID& item_id ) const
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	return (getWearableFromItemID(base_item_id) != NULL);
}

// MULTI-WEARABLE: DEPRECATED (see backwards compatibility)
// static
// ! BACKWARDS COMPATIBILITY ! When we stop supporting viewer1.23, we can assume
// that viewers have a Current Outfit Folder and won't need this message, and thus
// we can remove/ignore this whole function. EXCEPT gAgentWearables.notifyLoadingStarted
void LLAgentWearables::processAgentInitialWearablesUpdate(LLMessageSystem* mesgsys, void** user_data)
{
	// We should only receive this message a single time.  Ignore subsequent AgentWearablesUpdates
	// that may result from AgentWearablesRequest having been sent more than once.
	if (mInitialWearablesUpdateReceived)
		return;
	mInitialWearablesUpdateReceived = true;

	LLUUID agent_id;
	gMessageSystem->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id );

	if (isAgentAvatarValid() && (agent_id == gAgentAvatarp->getID()))
	{
		gMessageSystem->getU32Fast(_PREHASH_AgentData, _PREHASH_SerialNum, gAgentQueryManager.mUpdateSerialNum);
		
		const S32 NUM_BODY_PARTS = 4;
		S32 num_wearables = gMessageSystem->getNumberOfBlocksFast(_PREHASH_WearableData);
		if (num_wearables < NUM_BODY_PARTS)
		{
			// Transitional state.  Avatars should always have at least their body parts (hair, eyes, shape and skin).
			// The fact that they don't have any here (only a dummy is sent) implies that this account existed
			// before we had wearables, or that the database has gotten messed up.
			return;
		}

		//lldebugs << "processAgentInitialWearablesUpdate()" << llendl;
		// Add wearables
		std::pair<LLUUID, LLUUID> asset_id_array[ LLWearableType::WT_COUNT ];
		for (S32 i=0; i < num_wearables; i++)
		{
			U8 type_u8 = 0;
			gMessageSystem->getU8Fast(_PREHASH_WearableData, _PREHASH_WearableType, type_u8, i );
			if( type_u8 >= LLWearableType::WT_COUNT )
			{
				continue;
			}
			LLWearableType::EType type = (LLWearableType::EType) type_u8;

			LLUUID item_id;
			gMessageSystem->getUUIDFast(_PREHASH_WearableData, _PREHASH_ItemID, item_id, i );

			LLUUID asset_id;
			gMessageSystem->getUUIDFast(_PREHASH_WearableData, _PREHASH_AssetID, asset_id, i );
			if( asset_id.isNull() )
			{
				LLWearable::removeFromAvatar( type, FALSE );
			}
			else
			{
				LLAssetType::EType asset_type = LLWearableType::getAssetType( type );
				if( asset_type == LLAssetType::AT_NONE )
				{
					continue;
				}

				asset_id_array[type] = std::pair<LLUUID, LLUUID>(asset_id,item_id);
			}

			LL_DEBUGS("Wearables") << "       " << LLWearableType::getTypeLabel(type) << " " << asset_id << " item id " << gAgentWearables.getWearableItemID(type, 0).asString() << LL_ENDL;
		}

		LLCOFMgr::instance().fetchCOF();

		// now that we have the asset ids...request the wearable assets
		for(S32 i = 0; i < LLWearableType::WT_COUNT; i++ )
		{
			LL_DEBUGS("Wearables") << "      fetching " << asset_id_array[i].first << LL_ENDL;
			const LLUUID item_id = asset_id_array[i].second;
			if( !item_id.isNull() )
			{
				LLWearableList::instance().getAsset( 
					asset_id_array[i].first,
					LLStringUtil::null,
					LLWearableType::getAssetType( (LLWearableType::EType) i ), 
					LLAgentWearables::onInitialWearableAssetArrived, 
					//This scary cast is to prevent messing with llwearablelist. Since ItemIDs are now tied to wearables, 
					//  we now need to pass the ids to onInitialWearableAssetArrived so LLWearable::setItemID can be called there.
					(void*)(intptr_t)new std::pair<const LLWearableType::EType,const LLUUID>((LLWearableType::EType)i,item_id) );
			}
		}

		// Not really sure where else to put this
		gIdleCallbacks.addFunction(&LLAttachmentsMgr::onIdle, NULL);
	}
}

// A single wearable that the avatar was wearing on start-up has arrived from the database.
// static
void LLAgentWearables::onInitialWearableAssetArrived( LLWearable* wearable, void* userdata )
{
	std::pair<const LLWearableType::EType,const LLUUID>* wearable_data = (std::pair<const LLWearableType::EType,const LLUUID>*)(intptr_t) userdata;
	LLWearableType::EType type = wearable_data->first;
	LLUUID item_id = wearable_data->second;
	delete wearable_data;

	LLVOAvatar* avatar = gAgentAvatarp;
	if( !avatar )
	{
		return;
	}

	if( wearable )
	{
		llassert( type == wearable->getType() );
		wearable->setItemID(item_id);
		gAgentWearables.setWearable(type,0,wearable);

		// disable composites if initial textures are baked
		avatar->setupComposites();
		gAgentWearables.queryWearableCache();

		wearable->writeToAvatar( FALSE );
		avatar->setCompositeUpdatesEnabled(TRUE);
		gInventory.addChangedMask( LLInventoryObserver::LABEL, item_id );
	}
	else
	{
		// Somehow the asset doesn't exist in the database.
		gAgentWearables.recoverMissingWearable( type, 0 );
	}

	gInventory.notifyObservers();

	// Have all the wearables that the avatar was wearing at log-in arrived?
	if( !gAgentWearables.mWearablesLoaded )
	{
		gAgentWearables.mWearablesLoaded = TRUE;
		for( S32 i = 0; i < LLWearableType::WT_COUNT; i++ )
		{
			if( !gAgentWearables.getWearableItemID((LLWearableType::EType)i, 0).isNull() && !gAgentWearables.getWearable((LLWearableType::EType)i, 0) )
			{
				gAgentWearables.mWearablesLoaded = FALSE;
				break;
			}
		}
	}

	if( gAgentWearables.mWearablesLoaded )
	{
		// Make sure that the server's idea of the avatar's wearables actually match the wearables.
		gAgent.sendAgentSetAppearance();

		// Check to see if there are any baked textures that we hadn't uploaded before we logged off last time.
		// If there are any, schedule them to be uploaded as soon as the layer textures they depend on arrive.
		if( !gAgentCamera.cameraCustomizeAvatar() )
		{
			avatar->requestLayerSetUploads();
		}
	}
}

// Normally, all wearables referred to "AgentWearablesUpdate" will correspond to actual assets in the
// database.  If for some reason, we can't load one of those assets, we can try to reconstruct it so that
// the user isn't left without a shape, for example.  (We can do that only after the inventory has loaded.)
void LLAgentWearables::recoverMissingWearable(const LLWearableType::EType type, U32 index)
{
	llassert_always(index == 0);
	// Try to recover by replacing missing wearable with a new one.
	LLNotificationsUtil::add("ReplacedMissingWearable");
	lldebugs << "Wearable " << LLWearableType::getTypeLabel( type ) << " could not be downloaded.  Replaced inventory item with default wearable." << llendl;
	LLWearable* new_wearable = LLWearableList::instance().createNewWearable(type);

	setWearable(type,index,new_wearable);
	new_wearable->writeToAvatar( TRUE );

	// Add a new one in the lost and found folder.
	// (We used to overwrite the "not found" one, but that could potentially
	// destory content.) JC
	const LLUUID lost_and_found_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);
	LLPointer<LLInventoryCallback> cb =
		new addWearableToAgentInventoryCallback(
			LLPointer<LLRefCount>(NULL),
			type,
			index,
			new_wearable,
			addWearableToAgentInventoryCallback::CALL_RECOVERDONE);
	addWearableToAgentInventory( cb, new_wearable, lost_and_found_id, TRUE);
}

void LLAgentWearables::recoverMissingWearableDone()
{
	// Have all the wearables that the avatar was wearing at log-in arrived or been fabricated?
	updateWearablesLoaded();
	if (areWearablesLoaded())
	{
		// Make sure that the server's idea of the avatar's wearables actually match the wearables.
		gAgent.sendAgentSetAppearance();
	}
	else
	{
		gInventory.addChangedMask( LLInventoryObserver::LABEL, LLUUID::null );
		gInventory.notifyObservers();
	}
}

void LLAgentWearables::createStandardWearables(BOOL female)
{
	llwarns << "Creating Standard " << (female ? "female" : "male" )
			<< " Wearables" << llendl;

	if (!isAgentAvatarValid()) return;

	gAgentAvatarp->setSex(female ? SEX_FEMALE : SEX_MALE);

	const BOOL create[LLWearableType::WT_COUNT] = 
	{
		TRUE,  //WT_SHAPE
		TRUE,  //WT_SKIN
		TRUE,  //WT_HAIR
		TRUE,  //WT_EYES
		TRUE,  //WT_SHIRT
		TRUE,  //WT_PANTS
		TRUE,  //WT_SHOES
		TRUE,  //WT_SOCKS
		FALSE, //WT_JACKET
		FALSE, //WT_GLOVES
		TRUE,  //WT_UNDERSHIRT
		TRUE,  //WT_UNDERPANTS
		FALSE, //WT_SKIRT
		FALSE, //WT_ALPHA
		FALSE, //WT_TATTOO
		FALSE, //WT_PHYSICS
	};

	for( S32 i=0; i < LLWearableType::WT_COUNT; i++ )
	{
		bool once = false;
		LLPointer<LLRefCount> donecb = NULL;
		if( create[i] )
		{
			if (!once)
			{
				once = true;
				donecb = new createStandardWearablesAllDoneCallback;
			}
			llassert(getWearableCount((LLWearableType::EType)i) == 0);
			LLWearable* wearable = LLWearableList::instance().createNewWearable((LLWearableType::EType)i);
			// no need to update here...
			LLPointer<LLInventoryCallback> cb =
				new addWearableToAgentInventoryCallback(
					donecb,
					(LLWearableType::EType)i,
					0,
					wearable,
					addWearableToAgentInventoryCallback::CALL_CREATESTANDARDDONE);
			addWearableToAgentInventory(cb, wearable, LLUUID::null, FALSE);
		}
	}
}
void LLAgentWearables::createStandardWearablesDone(S32 type, U32 index)
{
	LLWearable* wearable = getWearable((LLWearableType::EType)type, index);

	if (wearable)
	{
		wearable->writeToAvatar(TRUE);
	}
}

void LLAgentWearables::createStandardWearablesAllDone()
{
	// ... because sendAgentWearablesUpdate will notify inventory
	// observers.
	mWearablesLoaded = TRUE; 
	updateServer();

	// Treat this as the first texture entry message, if none received yet
	gAgentAvatarp->onFirstTEMessageReceived();
}

void LLAgentWearables::makeNewOutfit( 
	const std::string& new_folder_name,
	const LLDynamicArray<S32>& wearables_to_include,
	const LLDynamicArray<S32>& attachments_to_include,
	BOOL rename_clothing)
{
	if (!gAgentAvatarp)
	{
		return;
	}

	BOOL fUseLinks = !gSavedSettings.getBOOL("UseInventoryLinks") ||
					 !gHippoGridManager->getConnectedGrid()->supportsInvLinks();
	BOOL fUseOutfits = gSavedSettings.getBOOL("UseOutfitFolders") &&
					   gHippoGridManager->getConnectedGrid()->supportsInvLinks();

	LLFolderType::EType typeDest = (fUseOutfits) ? LLFolderType::FT_MY_OUTFITS : LLFolderType::FT_CLOTHING;
	LLFolderType::EType typeFolder = (fUseOutfits) ? LLFolderType::FT_OUTFIT : LLFolderType::FT_NONE;

	// First, make a folder for the outfit.
	LLUUID folder_id = gInventory.createNewCategory(gInventory.findCategoryUUIDForType(typeDest), typeFolder, new_folder_name);

	bool found_first_item = false;

	///////////////////
	// Wearables

	if( wearables_to_include.count() )
	{
		// Then, iterate though each of the wearables and save copies of them in the folder.
		S32 i;
		S32 count = wearables_to_include.count();
		LLPointer<LLRefCount> cbdone = NULL;
		for( i = 0; i < count; ++i )
		{
			S32 index = wearables_to_include[i];
			LLWearable* old_wearable = getWearable((LLWearableType::EType)index, 0);
			if( old_wearable )
			{
				LLViewerInventoryItem* item = gInventory.getItem(getWearableItemID((LLWearableType::EType)index, 0));
				llassert(item);
				if (!item)
					continue;
				if (fUseOutfits)
				{
					std::string strOrdering = llformat("@%d", item->getWearableType() * 100);

					link_inventory_item(
						gAgent.getID(),
						item->getLinkedUUID(),
						folder_id,
						item->getName(),
						strOrdering,
						LLAssetType::AT_LINK,
						LLPointer<LLInventoryCallback>(NULL));
				}
				else
				{
					std::string new_name = item->getName();
					if (rename_clothing)
					{
						new_name = new_folder_name;
						new_name.append(" ");
						new_name.append(old_wearable->getTypeLabel());
						LLStringUtil::truncate(new_name, DB_INV_ITEM_NAME_STR_LEN);
					}

					if (fUseLinks || isWearableCopyable((LLWearableType::EType)index, 0))
					{
						LLWearable* new_wearable = LLWearableList::instance().createCopy(old_wearable);
						if (rename_clothing)
						{
							new_wearable->setName(new_name);
						}

						S32 todo = addWearableToAgentInventoryCallback::CALL_NONE;
						if (!found_first_item)
						{
							found_first_item = true;
							/* set the focus to the first item */
							todo |= addWearableToAgentInventoryCallback::CALL_MAKENEWOUTFITDONE;
							/* send the agent wearables update when done */
							cbdone = new sendAgentWearablesUpdateCallback;
						}
						LLPointer<LLInventoryCallback> cb =
							new addWearableToAgentInventoryCallback(
								cbdone,
								(LLWearableType::EType)index,
								0,
								new_wearable,
								todo);
						if (isWearableCopyable((LLWearableType::EType)index, 0))
						{
							copy_inventory_item(
								gAgent.getID(),
								item->getPermissions().getOwner(),
								item->getLinkedUUID(),
								folder_id,
								new_name,
								cb);
						}
						else
						{
							move_inventory_item(
								gAgent.getID(),
								gAgent.getSessionID(),
								item->getLinkedUUID(),
								folder_id,
								new_name,
								cb);
						}
					}
					else
					{
						link_inventory_item(
							gAgent.getID(),
							item->getLinkedUUID(),
							folder_id,
							item->getName(),		// Apparently, links cannot have arbitrary names...
							item->getDescription(),
							LLAssetType::AT_LINK,
							LLPointer<LLInventoryCallback>(NULL));
					}
				}
			}
		}
		gInventory.notifyObservers();
	}


	///////////////////
	// Attachments

	if( attachments_to_include.count() )
	{
		for( S32 i = 0; i < attachments_to_include.count(); i++ )
		{
			S32 attachment_pt = attachments_to_include[i];
			LLViewerJointAttachment* attachment = get_if_there(gAgentAvatarp->mAttachmentPoints, attachment_pt, (LLViewerJointAttachment*)NULL );
			if(!attachment) continue;
			for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
				 attachment_iter != attachment->mAttachedObjects.end();
				 ++attachment_iter)
 			{
				LLViewerObject *attached_object = (*attachment_iter);
				if (!attached_object) continue;
				const LLUUID& item_id = attached_object->getAttachmentItemID();
				if (item_id.isNull()) continue;
				LLInventoryItem* item = gInventory.getItem(item_id);
				if (!item) continue;
				if (fUseOutfits)
				{
					link_inventory_item(
						gAgent.getID(),
						item->getLinkedUUID(),
						folder_id,
						item->getName(),
						item->getDescription(),
						LLAssetType::AT_LINK,
						LLPointer<LLInventoryCallback>(NULL));
				}
				else
				{
					if (fUseLinks || item->getPermissions().allowCopyBy(gAgent.getID()))
					{
						const LLUUID& old_folder_id = item->getParentUUID();

						move_inventory_item(
							gAgent.getID(),
							gAgent.getSessionID(),
							item->getLinkedUUID(),
							folder_id,
							item->getName(),
							LLPointer<LLInventoryCallback>(NULL));

						if (item->getPermissions().allowCopyBy(gAgent.getID()))
						{
							copy_inventory_item(
								gAgent.getID(),
								item->getPermissions().getOwner(),
								item->getLinkedUUID(),
								old_folder_id,
								item->getName(),
								LLPointer<LLInventoryCallback>(NULL));
						}
					}
					else
					{
						link_inventory_item(
							gAgent.getID(),
							item->getLinkedUUID(),
							folder_id,
							item->getName(),
							item->getDescription(),
							LLAssetType::AT_LINK,
							LLPointer<LLInventoryCallback>(NULL));
					}
				}
			}
		}
	} 
}

void LLAgentWearables::makeNewOutfitDone(S32 type, U32 index)
{
	LLUUID first_item_id = getWearableItemID((LLWearableType::EType)type, index);
	// Open the inventory and select the first item we added.
	if( first_item_id.notNull() )
	{
		LLInventoryView* view = LLInventoryView::getActiveInventory();
		if(view)
		{
			view->getPanel()->setSelection(first_item_id, TAKE_FOCUS_NO);
		}
	}
}


void LLAgentWearables::addWearableToAgentInventory(LLPointer<LLInventoryCallback> cb,
												   LLWearable* wearable,
												   const LLUUID& category_id,
												   BOOL notify)
{
	create_inventory_item(gAgent.getID(),
						  gAgent.getSessionID(),
						  category_id,
						  wearable->getTransactionID(),
						  wearable->getName(),
						  wearable->getDescription(),
						  wearable->getAssetType(),
						  LLInventoryType::IT_WEARABLE,
						  wearable->getType(),
						  wearable->getPermissions().getMaskNextOwner(),
						  cb);
}

void LLAgentWearables::removeWearable(const LLWearableType::EType type, bool do_remove_all, U32 index)
{
	llassert_always(index == 0);
	if (gAgent.isTeen() &&
		(type == LLWearableType::WT_UNDERSHIRT || type == LLWearableType::WT_UNDERPANTS))
	{
		// Can't take off underclothing in simple UI mode or on PG accounts
		// TODO: enable the removing of a single undershirt/underpants if multiple are worn. - Nyx
		return;
	}
	if (getWearableCount(type) == 0)
	{
		// no wearables to remove
		return;
	}

// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.1.3b)
	if ( (rlv_handler_t::isEnabled()) && (!gRlvWearableLocks.canRemove(type)) )
	{
		return;
	}
// [/RLVa:KB]
	if (do_remove_all)
	{
		removeWearableFinal(type, do_remove_all, index);
	}
	else
	{
		LLWearable* old_wearable = getWearable(type,index);
		
		if (old_wearable)
		{
			if (old_wearable->isDirty())
			{
				LLSD payload;
				payload["wearable_type"] = (S32)type;
				payload["wearable_index"] = (S32)index;
				// Bring up view-modal dialog: Save changes? Yes, No, Cancel
				LLNotificationsUtil::add("WearableSave", LLSD(), payload, &LLAgentWearables::onRemoveWearableDialog);
				return;
			}
			else
			{
				removeWearableFinal(type, do_remove_all, index);
			}
		}
	}
}


// static 
bool LLAgentWearables::onRemoveWearableDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLWearableType::EType type = (LLWearableType::EType)notification["payload"]["wearable_type"].asInteger();
	S32 index = (S32)notification["payload"]["wearable_index"].asInteger();
	switch(option)
	{
		case 0:  // "Save"
			gAgentWearables.saveWearable(type, index);
			gAgentWearables.removeWearableFinal(type, false, index);
			break;

		case 1:  // "Don't Save"
			gAgentWearables.removeWearableFinal(type, false, index);
			break;

	case 2: // "Cancel"
		break;

	default:
		llassert(0);
		break;
	}
	return false;
}

// Called by removeWearable() and onRemoveWearableDialog() to actually do the removal.
void LLAgentWearables::removeWearableFinal( LLWearableType::EType type, bool do_remove_all, U32 index)
{
	llassert_always(index == 0);
	gInventory.addChangedMask( LLInventoryObserver::LABEL, getWearableItemID(type, index) );
	if (do_remove_all)
	{
		S32 max_entry = mWearableDatas[type].size()-1;
		for (S32 i=max_entry; i>=0; i--)
		{
			LLWearable* old_wearable = getWearable(type,i);
			//queryWearableCache(); // moved below
			if (old_wearable)
			{
				popWearable(old_wearable);
				old_wearable->removeFromAvatar(TRUE);
			}
		}
		mWearableDatas[type].clear();
	}
	else
	{
		LLWearable* old_wearable = getWearable(type, index);
		//queryWearableCache(); // moved below

		if (old_wearable)
		{
			popWearable(old_wearable);
			old_wearable->removeFromAvatar(TRUE);
		}
	}

	queryWearableCache();

	// Update the server
	updateServer();
	gInventory.notifyObservers();
}

static bool isFirstPhysicsWearable(LLWearableType::EType type, LLInventoryItem *new_item, LLWearable *new_wearable)
{
	if (type == LLWearableType::WT_PHYSICS && gSavedSettings.getWarning("FirstPhysicsWearable"))
	{
		class WearableDelayedCallback
		{
		public:
			static void setDelayedWearable( const LLSD& notification, const LLSD& response, LLUUID item_id, LLWearable *wearable )
			{
				if(LLNotification::getSelectedOption(notification, response) == 0) //User selected wear
				{
					gSavedSettings.setWarning("FirstPhysicsWearable",FALSE);
					LLInventoryItem *item = gInventory.getItem(item_id);
					if(item)
						gAgentWearables.setWearableItem(item,wearable); //re-enter.
				}
			}
		};
		LLNotificationsUtil::add("FirstPhysicsWearable",LLSD(),LLSD(),boost::bind(WearableDelayedCallback::setDelayedWearable, _1, _2, new_item->getUUID(),new_wearable));
		return true;
	}
	return false;
}

BOOL LLAgentWearables::needsReplacement(LLWearableType::EType wearableType, S32 remove)
{
	return TRUE;
	/*if (remove) return TRUE;
	
	return getWearable(wearableType) ? TRUE : FALSE;*/
}

// Assumes existing wearables are not dirty.
void LLAgentWearables::setWearableOutfit(const LLInventoryItem::item_array_t& items,
										 const LLDynamicArray< LLWearable* >& wearables,
	BOOL remove )
{
	lldebugs << "setWearableOutfit() start" << llendl;

	BOOL wearables_to_remove[LLWearableType::WT_COUNT];
	wearables_to_remove[LLWearableType::WT_SHAPE]		= FALSE;
	wearables_to_remove[LLWearableType::WT_SKIN]		= FALSE;
	wearables_to_remove[LLWearableType::WT_HAIR]		= FALSE;
	wearables_to_remove[LLWearableType::WT_EYES]		= FALSE;
// [RLVa:KB] - Checked: 2009-07-06 (RLVa-1.1.3b) | Added: RLVa-0.2.2a
	wearables_to_remove[LLWearableType::WT_SHIRT]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_SHIRT);
	wearables_to_remove[LLWearableType::WT_PANTS]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_PANTS);
	wearables_to_remove[LLWearableType::WT_SHOES]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_SHOES);
	wearables_to_remove[LLWearableType::WT_SOCKS]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_SOCKS);
	wearables_to_remove[LLWearableType::WT_JACKET]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_JACKET);
	wearables_to_remove[LLWearableType::WT_GLOVES]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_GLOVES);
	wearables_to_remove[LLWearableType::WT_UNDERSHIRT]	= (!gAgent.isTeen()) && remove && gRlvWearableLocks.canRemove(LLWearableType::WT_UNDERSHIRT);
	wearables_to_remove[LLWearableType::WT_UNDERPANTS]	= (!gAgent.isTeen()) && remove && gRlvWearableLocks.canRemove(LLWearableType::WT_UNDERPANTS);
	wearables_to_remove[LLWearableType::WT_SKIRT]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_SKIRT);
	wearables_to_remove[LLWearableType::WT_ALPHA]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_ALPHA);
	wearables_to_remove[LLWearableType::WT_TATTOO]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_TATTOO);
	wearables_to_remove[LLWearableType::WT_PHYSICS]		= remove && gRlvWearableLocks.canRemove(LLWearableType::WT_PHYSICS);
// [/RLVa:KB]

	S32 count = wearables.count();
	llassert( items.count() == count );

	S32 i;
	for( i = 0; i < count; i++ )
	{
		LLWearable* new_wearable = wearables[i];
		LLPointer<LLInventoryItem> new_item = items[i];

		llassert(new_wearable);
		
		if (new_wearable)
		{
			LLWearableType::EType type = new_wearable->getType();
			wearables_to_remove[type] = FALSE;

			LLWearable* old_wearable = getWearable(type, 0);
			if( old_wearable )
			{
				const LLUUID& old_item_id = getWearableItemID(type, 0);
				if( (old_wearable->getAssetID() == new_wearable->getAssetID()) &&
					(old_item_id == new_item->getUUID()) )
				{
					lldebugs << "No change to wearable asset and item: " << LLWearableType::getTypeName( type ) << llendl;
					continue;
				}

				gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);

				// Assumes existing wearables are not dirty.
				if( old_wearable->isDirty() )
				{
					llassert(0);
					continue;
				}
			}

			if (isFirstPhysicsWearable(type, new_item, new_wearable))
			{
				return;
			}

			new_wearable->setName(new_item->getName());
			new_wearable->setItemID(new_item->getUUID());

			if (LLWearableType::getAssetType(type) == LLAssetType::AT_BODYPART)
			{
				// exactly one wearable per body part
				setWearable(type,0,new_wearable);
			}
			else if(old_wearable)	//Remove when multi-wearables are implemented.
			{
				setWearable(type,0,new_wearable);
			}
			else
			{
				pushWearable(type,new_wearable);
			}
		}
	}

	std::vector<LLWearable*> wearables_being_removed;

	for( i = 0; i < LLWearableType::WT_COUNT; i++ )
	{
		if( wearables_to_remove[i] )
		{
			LLWearable* wearable = getWearable((LLWearableType::EType)i, 0);
			wearables_being_removed.push_back(getWearable((LLWearableType::EType)i, 0));
			popWearable(wearable);
			
			gInventory.addChangedMask(LLInventoryObserver::LABEL, wearable->getItemID());
		}
	}

	gInventory.notifyObservers();

	queryWearableCache();

	std::vector<LLWearable*>::iterator wearable_iter;

	for( wearable_iter = wearables_being_removed.begin(); 
		wearable_iter != wearables_being_removed.end();
		++wearable_iter)
	{
		LLWearable* wearablep = *wearable_iter;
		if (wearablep)
		{
			wearablep->removeFromAvatar( TRUE );
		}
	}

	for( i = 0; i < count; i++ )
	{
		wearables[i]->writeToAvatar( TRUE );
	}

	// Start rendering & update the server
	mWearablesLoaded = TRUE; 
	updateServer();


	lldebugs << "setWearableOutfit() end" << llendl;
}


// User has picked "wear on avatar" from a menu.
void LLAgentWearables::setWearableItem( LLInventoryItem* new_item, LLWearable* new_wearable )
{
	//LLAgentDumper dumper("setWearableItem");
	if (isWearingItem(new_item->getUUID()))
	{
		llwarns << "wearable " << new_item->getUUID() << " is already worn" << llendl;
		return;
	}
	LLWearableType::EType type = new_wearable->getType();


// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.0.0d)
	// Block if: we can't wear on that layer; or we're already wearing something there we can't take off
	if ( (rlv_handler_t::isEnabled()) && (!gRlvWearableLocks.canWear(type)) )
	{
		return;
	}
// [/RLVa:KB]

	if (isFirstPhysicsWearable(type, new_item, new_wearable))
	{
		return;
	}

	LLWearable* old_wearable = getWearable(type,0);
	if( old_wearable )
	{
		const LLUUID& old_item_id = old_wearable->getItemID();
		if( (old_wearable->getAssetID() == new_wearable->getAssetID()) &&
			(old_item_id == new_item->getUUID()) )
		{
			lldebugs << "No change to wearable asset and item: " << LLWearableType::getTypeName( type ) << llendl;
			return;
		}

		if( old_wearable->isDirty() )
		{
			// Bring up modal dialog: Save changes? Yes, No, Cancel
			LLSD payload;
			payload["item_id"] = new_item->getUUID();
			LLNotificationsUtil::add( "WearableSave", LLSD(), payload, boost::bind(onSetWearableDialog, _1, _2, new_wearable));
			return;
		}
	}

	setWearableFinal( new_item, new_wearable );
}

// static 
bool LLAgentWearables::onSetWearableDialog( const LLSD& notification, const LLSD& response, LLWearable* wearable )
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLInventoryItem* new_item = gInventory.getItem( notification["payload"]["item_id"].asUUID());
	U32 index = gAgentWearables.getWearableIndex(wearable);
	if( !new_item )
	{
		delete wearable;
		return false;
	}

	switch( option )
	{
	case 0:  // "Save"
			gAgentWearables.saveWearable(wearable->getType(),index);
		gAgentWearables.setWearableFinal( new_item, wearable );
		break;

	case 1:  // "Don't Save"
		gAgentWearables.setWearableFinal( new_item, wearable );
		break;

	case 2: // "Cancel"
		break;

	default:
		llassert(0);
		break;
	}

	delete wearable;
	return false;
}

// Called from setWearable() and onSetWearableDialog() to actually set the wearable.
void LLAgentWearables::setWearableFinal( LLInventoryItem* new_item, LLWearable* new_wearable )
{
	const LLWearableType::EType type = new_wearable->getType();

	{
		// Replace the old wearable with a new one.
		llassert(new_item->getAssetUUID() == new_wearable->getAssetID());

		LLWearable *old_wearable = getWearable(type,0);
		LLUUID old_item_id;
		if (old_wearable)
		{
			old_item_id = old_wearable->getItemID();
		}
		new_wearable->setItemID(new_item->getUUID());
		setWearable(type,0,new_wearable);

		if (old_item_id.notNull())
		{
			gInventory.addChangedMask(LLInventoryObserver::LABEL, old_item_id);
			gInventory.notifyObservers();
		}
		llinfos << "Replaced current element 0 for type " << type
				<< " size is now " << mWearableDatas[type].size() << llendl;
	}

	//llinfos << "LLVOAvatar::setWearableItem()" << llendl;
	queryWearableCache();
	new_wearable->writeToAvatar( TRUE );

	updateServer();
}

void LLAgentWearables::queryWearableCache()
{
	if (!areWearablesLoaded())
	{
		return;
	}

	// Look up affected baked textures.
	// If they exist:
	//		disallow updates for affected layersets (until dataserver responds with cache request.)
	//		If cache miss, turn updates back on and invalidate composite.
	//		If cache hit, modify baked texture entries.
	//
	// Cache requests contain list of hashes for each baked texture entry.
	// Response is list of valid baked texture assets. (same message)

	gMessageSystem->newMessageFast(_PREHASH_AgentCachedTexture);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->addS32Fast(_PREHASH_SerialNum, gAgentQueryManager.mWearablesCacheQueryID);

	S32 num_queries = 0;
	for (U8 baked_index = 0; baked_index < BAKED_NUM_INDICES; baked_index++ )
	{
		LLUUID hash_id = computeBakedTextureHash((EBakedTextureIndex) baked_index);
		if (hash_id.notNull())
		{
			num_queries++;
			// *NOTE: make sure at least one request gets packed

			ETextureIndex te_index = LLVOAvatarDictionary::bakedToLocalTextureIndex((EBakedTextureIndex)baked_index);

			//llinfos << "Requesting texture for hash " << hash << " in baked texture slot " << baked_index << llendl;
			gMessageSystem->nextBlockFast(_PREHASH_WearableData);
			gMessageSystem->addUUIDFast(_PREHASH_ID, hash_id);
			gMessageSystem->addU8Fast(_PREHASH_TextureIndex, (U8)te_index);
		}

		gAgentQueryManager.mActiveCacheQueries[ baked_index ] = gAgentQueryManager.mWearablesCacheQueryID;
	}
	//VWR-22113: gAgent.getRegion() can return null if invalid, seen here on logout
	if(gAgent.getRegion())
	{
		llinfos << "Requesting texture cache entry for " << num_queries << " baked textures" << llendl;
		gMessageSystem->sendReliable(gAgent.getRegion()->getHost());
		gAgentQueryManager.mNumPendingQueries++;
		gAgentQueryManager.mWearablesCacheQueryID++;
	}
}

LLUUID LLAgentWearables::computeBakedTextureHash(LLVOAvatarDefines::EBakedTextureIndex baked_index,
												 BOOL generate_valid_hash) // Set to false if you want to upload the baked texture w/o putting it in the cache
{
	LLUUID hash_id;
	bool hash_computed = false;
	LLMD5 hash;
	const LLVOAvatarDictionary::BakedEntry *baked_dict = LLVOAvatarDictionary::getInstance()->getBakedTexture(baked_index);

	for (U8 i=0; i < baked_dict->mWearables.size(); i++)
	{
		const LLWearableType::EType baked_type = baked_dict->mWearables[i];
		const U32 num_wearables = getWearableCount(baked_type);
		for (U32 index = 0; index < num_wearables; ++index)
		{
			const LLWearable* wearable = getWearable(baked_type,index);
			if (wearable)
			{
				LLUUID asset_id = wearable->getAssetID();
				hash.update((const unsigned char*)asset_id.mData, UUID_BYTES);
				hash_computed = true;
			}
		}
	}
	if (hash_computed)
	{
		hash.update((const unsigned char*)baked_dict->mWearablesHashID.mData, UUID_BYTES);

		// Add some garbage into the hash so that it becomes invalid.
		if (!generate_valid_hash)
		{
			if (isAgentAvatarValid())
			{
				hash.update((const unsigned char*)gAgentAvatarp->getID().mData, UUID_BYTES);
			}
		}
		hash.finalize();
		hash.raw_digest(hash_id.mData);
	}

	return hash_id;
}

// User has picked "remove from avatar" from a menu.
// static
void LLAgentWearables::userRemoveWearable(const LLWearableType::EType &type, const U32 &index)
{
	if( !(type==LLWearableType::WT_SHAPE || type==LLWearableType::WT_SKIN || type==LLWearableType::WT_HAIR || type==LLWearableType::WT_EYES) ) //&&
		//!((!gAgent.isTeen()) && ( type==WT_UNDERPANTS || type==WT_UNDERSHIRT )) )
	{
		gAgentWearables.removeWearable(type,false,index);
	}
}

//static 
void LLAgentWearables::userRemoveWearablesOfType(const LLWearableType::EType &type)
{
	if (!(type==LLWearableType::WT_SHAPE || type==LLWearableType::WT_SKIN || type==LLWearableType::WT_HAIR || type==LLWearableType::WT_EYES)) //&&
		//!((!gAgent.isTeen()) && (type==LLWearableType::WT_UNDERPANTS || type==LLWearableType::WT_UNDERSHIRT)))
	{
		gAgentWearables.removeWearable(type,true,0);
	}
}

void LLAgentWearables::userRemoveAllClothes()
{
	// We have to do this up front to avoid having to deal with the case of multiple wearables being dirty.
	if (gAgentCamera.cameraCustomizeAvatar())
	{
		gFloaterCustomize->askToSaveIfDirty( LLAgentWearables::userRemoveAllClothesStep2, NULL );
	}
	else
	{
		userRemoveAllClothesStep2( TRUE, NULL );
	}
}

void LLAgentWearables::userRemoveAllClothesStep2( BOOL proceed, void* userdata )
{
	if( proceed )
	{
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_SHIRT );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_PANTS );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_SHOES );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_SOCKS );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_JACKET );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_GLOVES );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_UNDERSHIRT );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_UNDERPANTS );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_SKIRT );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_ALPHA );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_TATTOO );
		gAgentWearables.userRemoveWearablesOfType( LLWearableType::WT_PHYSICS );
	}
}

// Combines userRemoveAllAttachments() and userAttachMultipleAttachments() logic to
// get attachments into desired state with minimal number of adds/removes.
//void LLAgentWearables::userUpdateAttachments(LLInventoryModel::item_array_t& obj_item_array)
// [SL:KB] - Patch: Appearance-SyncAttach | Checked: 2010-09-22 (Catznip-2.2.0a) | Added: Catznip-2.2.0a
void LLAgentWearables::userUpdateAttachments(LLInventoryModel::item_array_t& obj_item_array, bool fAttachOnly)
// [/SL:KB]
{
	// Possible cases:
	// already wearing but not in request set -> take off.
	// already wearing and in request set -> leave alone.
	// not wearing and in request set -> put on.

	if (!isAgentAvatarValid()) return;

	std::set<LLUUID> requested_item_ids;
	std::set<LLUUID> current_item_ids;
	for (S32 i=0; i<obj_item_array.count(); i++)
		requested_item_ids.insert(obj_item_array[i].get()->getLinkedUUID());

	// Build up list of objects to be removed and items currently attached.
	llvo_vec_t objects_to_remove;
	for (LLVOAvatar::attachment_map_t::iterator iter = gAgentAvatarp->mAttachmentPoints.begin(); 
		 iter != gAgentAvatarp->mAttachmentPoints.end();)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
			 attachment_iter != attachment->mAttachedObjects.end();
			 ++attachment_iter)
		{
			LLViewerObject *objectp = (*attachment_iter);
			if (objectp)
			{
				LLUUID object_item_id = objectp->getAttachmentItemID();
				if (requested_item_ids.find(object_item_id) != requested_item_ids.end())
				{
					// Object currently worn, was requested.
					// Flag as currently worn so we won't have to add it again.
					current_item_ids.insert(object_item_id);
				}
				else
				{
					// object currently worn, not requested.
					objects_to_remove.push_back(objectp);
				}
			}
		}
	}

	LLInventoryModel::item_array_t items_to_add;
	for (LLInventoryModel::item_array_t::iterator it = obj_item_array.begin();
		 it != obj_item_array.end();
		 ++it)
	{
		LLUUID linked_id = (*it).get()->getLinkedUUID();
		if (current_item_ids.find(linked_id) != current_item_ids.end())
		{
			// Requested attachment is already worn.
		}
		else
		{
			// Requested attachment is not worn yet.
			items_to_add.push_back(*it);
		}
	}
	// S32 remove_count = objects_to_remove.size();
	// S32 add_count = items_to_add.size();
	// llinfos << "remove " << remove_count << " add " << add_count << llendl;

	// Remove everything in objects_to_remove
//	userRemoveMultipleAttachments(objects_to_remove);
// [SL:KB] - Patch: Appearance-SyncAttach | Checked: 2010-09-22 (Catznip-2.2.0a) | Added: Catznip-2.2.0a
	if (!fAttachOnly)
	{
		userRemoveMultipleAttachments(objects_to_remove);
	}
// [/SL:KB]

	// Add everything in items_to_add
	userAttachMultipleAttachments(items_to_add);
}

void LLAgentWearables::userRemoveMultipleAttachments(llvo_vec_t& objects_to_remove)
{
	if (!isAgentAvatarValid()) return;

// [RLVa:KB] - Checked: 2010-03-04 (RLVa-1.1.3b) | Modified: RLVa-1.2.0a
	// RELEASE-RLVa: [SL-2.0.0] Check our callers and verify that erasing elements from the passed vector won't break random things
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_REMOVE)) )
	{
		llvo_vec_t::iterator itObj = objects_to_remove.begin();
		while (itObj != objects_to_remove.end())
		{
			const LLViewerObject* pAttachObj = *itObj;
			if (gRlvAttachmentLocks.isLockedAttachment(pAttachObj))
			{
				itObj = objects_to_remove.erase(itObj);

				// Fall-back code: re-add the attachment if it got removed from COF somehow (compensates for possible bugs elsewhere)
				LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
				LLLinkedItemIDMatches f(pAttachObj->getAttachmentItemID());
				gInventory.collectDescendentsIf(LLCOFMgr::instance().getCOF(), folders, items, LLInventoryModel::EXCLUDE_TRASH, f);
				RLV_ASSERT( 0 != items.count() );
				if (0 == items.count())
					LLCOFMgr::instance().addAttachment(pAttachObj->getAttachmentItemID());
			}
			else
			{
				++itObj;
			}
		}
	}
// [/RLVa:KB]

	if (objects_to_remove.empty())
		return;

	gMessageSystem->newMessage("ObjectDetach");
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	
	for (llvo_vec_t::iterator it = objects_to_remove.begin();
		 it != objects_to_remove.end();
		 ++it)
	{
		LLViewerObject *objectp = *it;
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
	}
	gMessageSystem->sendReliable(gAgent.getRegionHost());
}

void LLAgentWearables::userRemoveAllAttachments()
{
	if (!isAgentAvatarValid()) return;

	llvo_vec_t objects_to_remove;
	
	for (LLVOAvatar::attachment_map_t::iterator iter = gAgentAvatarp->mAttachmentPoints.begin(); 
		 iter != gAgentAvatarp->mAttachmentPoints.end();)
	{
		LLVOAvatar::attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
			 attachment_iter != attachment->mAttachedObjects.end();
			 ++attachment_iter)
		{
			LLViewerObject *attached_object = (*attachment_iter);
//			if (attached_object)
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.1.3b) | Modified: RLVa-1.1.3b
			if ( (attached_object) && ((!rlv_handler_t::isEnabled()) || (!gRlvAttachmentLocks.isLockedAttachment(attached_object))) )
// [/RLVa:KB]
			{
				objects_to_remove.push_back(attached_object);
			}
		}
	}
	userRemoveMultipleAttachments(objects_to_remove);
}

void LLAgentWearables::userAttachMultipleAttachments(LLInventoryModel::item_array_t& obj_item_array)
{
// [RLVa:KB] - Checked: 2010-03-04 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
	// RELEASE-RLVa: [SL-2.0.0] Check our callers and verify that erasing elements from the passed vector won't break random things
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_ANY)) )
	{
		// Fall-back code: everything should really already have been pruned before we get this far
		for (S32 idxItem = obj_item_array.count() - 1; idxItem >= 0; idxItem--)
		{
			const LLInventoryItem* pItem = obj_item_array.get(idxItem).get();
			if (!gRlvAttachmentLocks.canAttach(pItem))
			{
				obj_item_array.remove(idxItem);
				RLV_ASSERT(false);
			}
		}
	}
// [/RLVa:KB]

	// Build a compound message to send all the objects that need to be rezzed.
	S32 obj_count = obj_item_array.count();

	// Limit number of packets to send
	const S32 MAX_PACKETS_TO_SEND = 10;
	const S32 OBJECTS_PER_PACKET = 4;
	const S32 MAX_OBJECTS_TO_SEND = MAX_PACKETS_TO_SEND * OBJECTS_PER_PACKET;
	if( obj_count > MAX_OBJECTS_TO_SEND )
	{
		obj_count = MAX_OBJECTS_TO_SEND;
	}
				
	// Create an id to keep the parts of the compound message together
	LLUUID compound_msg_id;
	compound_msg_id.generate();
	LLMessageSystem* msg = gMessageSystem;

	for(S32 i = 0; i < obj_count; ++i)
	{
		if( 0 == (i % OBJECTS_PER_PACKET) )
		{
			// Start a new message chunk
			msg->newMessageFast(_PREHASH_RezMultipleAttachmentsFromInv);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			msg->nextBlockFast(_PREHASH_HeaderData);
			msg->addUUIDFast(_PREHASH_CompoundMsgID, compound_msg_id );
			msg->addU8Fast(_PREHASH_TotalObjects, obj_count );
			msg->addBOOLFast(_PREHASH_FirstDetachAll, false );
		}

		const LLInventoryItem* item = obj_item_array.get(i).get();
		bool replace = !gHippoGridManager->getConnectedGrid()->supportsInvLinks();
		msg->nextBlockFast(_PREHASH_ObjectData );
		msg->addUUIDFast(_PREHASH_ItemID, item->getLinkedUUID());
		msg->addUUIDFast(_PREHASH_OwnerID, item->getPermissions().getOwner());
		msg->addU8Fast(_PREHASH_AttachmentPt, replace? 0 : ATTACHMENT_ADD);	// Wear at the previous or default attachment point
		pack_permissions_slam(msg, item->getFlags(), item->getPermissions());
		msg->addStringFast(_PREHASH_Name, item->getName());
		msg->addStringFast(_PREHASH_Description, item->getDescription());

		if( (i+1 == obj_count) || ((OBJECTS_PER_PACKET-1) == (i % OBJECTS_PER_PACKET)) )
		{
			// End of message chunk
			msg->sendReliable( gAgent.getRegion()->getHost() );
		}
	}
}
// MULTI-WEARABLE: DEPRECATED: item pending count relies on old messages that don't support multi-wearables. do not trust to be accurate
void LLAgentWearables::updateWearablesLoaded()
{
	mWearablesLoaded = TRUE;
	for( S32 i = 0; i < LLWearableType::WT_COUNT; i++ )
	{
		if( !getWearableItemID((LLWearableType::EType)i, 0).isNull() && !getWearable((LLWearableType::EType)i, 0) )
		{
			mWearablesLoaded = FALSE;
			break;
		}
	}

}
void LLAgentWearables::updateServer()
{
	sendAgentWearablesUpdate();
	gAgent.sendAgentSetAppearance();
}
