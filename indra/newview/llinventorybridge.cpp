/** 
 * @file llinventorybridge.cpp
 * @brief Implementation of the Inventory-Folder-View-Bridge classes.
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

#include <utility> // for std::pair<>

#include "llinventoryview.h"
#include "llinventorybridge.h"
#include "llinventorydefines.h"
#include "llinventoryfunctions.h"
#include "llinventoryicon.h"
#include "llinventorymodelbackgroundfetch.h"

#include "message.h"

#include "cofmgr.h"
#include "llagent.h"
#include "llagentwearables.h"
#include "llagentcamera.h"
#include "llcallingcard.h"
#include "llcheckboxctrl.h"		// for radio buttons
#include "llradiogroup.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewerfoldertype.h"

#include "llviewercontrol.h"
#include "llfirstuse.h"
#include "llfloateravatarinfo.h"
#include "llfloaterchat.h"
#include "llfloatercustomize.h"
#include "llfloaterproperties.h"

#include "llfloaterworldmap.h"
#include "llfocusmgr.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "llgiveinventory.h" 
#include "lliconctrl.h"
#include "llinventorymodel.h"
#include "llinventoryclipboard.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewlandmark.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "llresmgr.h"
#include "llscrollcontainer.h"
#include "llimview.h"
#include "llviewerassettype.h"
#include "llfoldertype.h"
#include "lltooldraganddrop.h"

#include "llviewertexturelist.h"
#include "llviewerinventory.h"

#include "llviewerobjectlist.h"

#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llwearable.h"
#include "llwearablelist.h"
#include "llviewermessage.h" 
#include "llviewerregion.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"
#include "llselectmgr.h"
#include "llfloateropenobject.h"
// <edit>
#include "llappviewer.h" // System Folders
#include "llfloateranimpreview.h" // for reuploads
#include "llfloaterimagepreview.h" // for reuploads
//#include "llcheats.h"
#include "dofloaterhex.h"
#include "hgfloatertexteditor.h"
#include "statemachine/aifilepicker.h"
// </edit>

// Editing wearables from inventory is an include-hungry feature -.- -SG
#include "llviewerparcelmgr.h"
#include "llfloatertools.h"
#include "lltoolcomp.h"
#include "llviewerjoystick.h"
#include "lltoolmgr.h"

#include "hippogridmanager.h"

// [RLVa:KB]
#include "rlvhandler.h"
#include "llattachmentsmgr.h"
// [/RLVa:KB]

typedef std::pair<LLUUID, LLUUID> two_uuids_t;
typedef std::list<two_uuids_t> two_uuids_list_t;

struct LLMoveInv
{
	LLUUID mObjectID;
	LLUUID mCategoryID;
	two_uuids_list_t mMoveList;
	void (*mCallback)(S32, void*);
	void* mUserData;
};

// Helpers
// bug in busy count inc/dec right now, logic is complex... do we really need it?
void inc_busy_count()
{
// 	gViewerWindow->getWindow()->incBusyCount();
//  check balance of these calls if this code is changed to ever actually
//  *do* something!
}
void dec_busy_count()
{
// 	gViewerWindow->getWindow()->decBusyCount();
//  check balance of these calls if this code is changed to ever actually
//  *do* something!
}

// Function declarations
struct LLWearableHoldingPattern;
void wear_inventory_category_on_avatar(LLInventoryCategory* category, BOOL append, BOOL replace = FALSE);
void wear_inventory_category_on_avatar_step2( BOOL proceed, void* userdata);
void wear_inventory_category_on_avatar_loop(LLWearable* wearable, void*);
void wear_inventory_category_on_avatar_step3(LLWearableHoldingPattern* holder, BOOL append);
void remove_inventory_category_from_avatar(LLInventoryCategory* category);
void remove_inventory_category_from_avatar_step2( BOOL proceed, void* userdata);
bool move_task_inventory_callback(const LLSD& notification, const LLSD& response, LLMoveInv*);
bool confirm_replace_attachment_rez(const LLSD& notification, const LLSD& response);

// <edit>
void gotImageForSaveItemAs(BOOL success, 
											LLViewerTexture *src_vi,
											LLImageRaw* src, 
											LLImageRaw* aux_src, 
											S32 discard_level,
											BOOL final,
											void* userdata);
void gotAssetForSaveItemAs(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status);
// </edit>

struct LLWearInfo
{
	LLUUID	mCategoryID;
	BOOL	mAppend;
	BOOL	mReplace;
};

// [RLVa:KB] - Made this part of LLWearableHoldingPattern
//BOOL gAddToOutfit = FALSE;
// [/RLVa:KB]

// +=================================================+
// |        LLInvFVBridge                            |
// +=================================================+

LLInvFVBridge::LLInvFVBridge(LLInventoryPanel* inventory, 
							 //LLFolderView* root,
							 const LLUUID& uuid) :
	mUUID(uuid), 
	//mRoot(root),
	mInvType(LLInventoryType::IT_NONE),
	mIsLink(FALSE)
{
	mInventoryPanel = inventory;
	const LLInventoryObject* obj = getInventoryObject();
	mIsLink = obj && obj->getIsLinkType();
}

const std::string& LLInvFVBridge::getName() const
{
	LLInventoryObject* obj = getInventoryObject();
	if(obj)
	{
		return obj->getName();
	}
	return LLStringUtil::null;
}

const std::string& LLInvFVBridge::getDisplayName() const
{
	return getName();
}

// Folders have full perms
PermissionMask LLInvFVBridge::getPermissionMask() const
{

	return PERM_ALL;
}

// virtual
LLFolderType::EType LLInvFVBridge::getPreferredType() const
{
	return LLFolderType::FT_NONE;
}


// Folders don't have creation dates.
time_t LLInvFVBridge::getCreationDate() const
{
	return 0;
}

// Can be destroyed (or moved to trash)
BOOL LLInvFVBridge::isItemRemovable()
{
	return get_is_item_removable(getInventoryModel(), mUUID);
}

// Can be moved to another folder
BOOL LLInvFVBridge::isItemMovable()
{
	return TRUE;
}

BOOL LLInvFVBridge::isLink() const
{
	return mIsLink;
}

// *TODO: make sure this does the right thing
void LLInvFVBridge::showProperties()
{
	LLShowProps::showProperties(mUUID);
}

void LLInvFVBridge::removeBatch(LLDynamicArray<LLFolderViewEventListener*>& batch)
{
	// Deactivate gestures when moving them into Trash
	LLInvFVBridge* bridge;
	LLInventoryModel* model = getInventoryModel();
	LLViewerInventoryItem* item = NULL;
	LLViewerInventoryCategory* cat = NULL;
	LLInventoryModel::cat_array_t	descendent_categories;
	LLInventoryModel::item_array_t	descendent_items;
	S32 count = batch.count();
	S32 i,j;
	for(i = 0; i < count; ++i)
	{	
		bridge = (LLInvFVBridge*)(batch.get(i));
		if(!bridge || !bridge->isItemRemovable()) continue;
		item = (LLViewerInventoryItem*)model->getItem(bridge->getUUID());
		if (item)
		{
			if(LLAssetType::AT_GESTURE == item->getType())
			{
				LLGestureMgr::instance().deactivateGesture(item->getUUID());
			}
		}
	}
	for(i = 0; i < count; ++i)
	{		
		bridge = (LLInvFVBridge*)(batch.get(i));
		if(!bridge || !bridge->isItemRemovable()) continue;
		cat = (LLViewerInventoryCategory*)model->getCategory(bridge->getUUID());
		if (cat)
		{
			gInventory.collectDescendents( cat->getUUID(), descendent_categories, descendent_items, FALSE );
			for (j=0; j<descendent_items.count(); j++)
			{
				if(LLAssetType::AT_GESTURE == descendent_items[j]->getType())
				{
					LLGestureMgr::instance().deactivateGesture(descendent_items[j]->getUUID());
				}
			}
		}
	}
	removeBatchNoCheck(batch);
}

void LLInvFVBridge::removeBatchNoCheck(LLDynamicArray<LLFolderViewEventListener*>& batch)
{
	// this method moves a bunch of items and folders to the trash. As
	// per design guidelines for the inventory model, the message is
	// built and the accounting is performed first. After all of that,
	// we call LLInventoryModel::moveObject() to move everything
	// around.
	LLInvFVBridge* bridge;
	LLInventoryModel* model = getInventoryModel();
	if(!model) return;
	LLMessageSystem* msg = gMessageSystem;
	const LLUUID trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	LLViewerInventoryItem* item = NULL;
	uuid_vec_t move_ids;
	LLInventoryModel::update_map_t update;
	bool start_new_message = true;
	S32 count = batch.count();
	S32 i;

	// first, hide any 'preview' floaters that correspond to the items
	// being deleted.
	for(i = 0; i < count; ++i)
	{
		bridge = (LLInvFVBridge*)(batch.get(i));
		if(!bridge || !bridge->isItemRemovable()) continue;
		item = (LLViewerInventoryItem*)model->getItem(bridge->getUUID());
		if(item)
		{
			LLPreview::hide(item->getUUID());
		}
	}

	// do the inventory move to trash

	for(i = 0; i < count; ++i)
	{
		bridge = (LLInvFVBridge*)(batch.get(i));
		if(!bridge || !bridge->isItemRemovable()) continue;
		item = (LLViewerInventoryItem*)model->getItem(bridge->getUUID());
		if(item)
		{
			if(item->getParentUUID() == trash_id) continue;
			move_ids.push_back(item->getUUID());
			// <edit> don't do stuff that messes with gMessageSystem
			//LLPreview::hide(item->getUUID());
			// </edit>
			--update[item->getParentUUID()];
			++update[trash_id];
			// <edit>
			if(!gInventory.isObjectDescendentOf(item->getUUID(), gSystemFolderRoot))
			{
			// </edit>
				if(start_new_message)
				{
					start_new_message = false;
					msg->newMessageFast(_PREHASH_MoveInventoryItem);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					msg->addBOOLFast(_PREHASH_Stamp, TRUE);
				}
				msg->nextBlockFast(_PREHASH_InventoryData);
				msg->addUUIDFast(_PREHASH_ItemID, item->getUUID());
				msg->addUUIDFast(_PREHASH_FolderID, trash_id);
				msg->addString("NewName", NULL);
				if(msg->isSendFullFast(_PREHASH_InventoryData))
				{
					start_new_message = true;
					gAgent.sendReliableMessage();
					gInventory.accountForUpdate(update);
					update.clear();
				}
			// <edit>
			}
			// </edit>
		}
	}
	if(!start_new_message)
	{
		start_new_message = true;
		gAgent.sendReliableMessage();
		gInventory.accountForUpdate(update);
		update.clear();
	}
	for(i = 0; i < count; ++i)
	{
		bridge = (LLInvFVBridge*)(batch.get(i));
		if(!bridge || !bridge->isItemRemovable()) continue;
		LLViewerInventoryCategory* cat = (LLViewerInventoryCategory*)model->getCategory(bridge->getUUID());
		if(cat)
		{
			if(cat->getParentUUID() == trash_id) continue;
			move_ids.push_back(cat->getUUID());
			--update[cat->getParentUUID()];
			++update[trash_id];
			// <edit>
			if(!gInventory.isObjectDescendentOf(cat->getUUID(), gSystemFolderRoot))
			{
			// </edit>
				if(start_new_message)
				{
					start_new_message = false;
					msg->newMessageFast(_PREHASH_MoveInventoryFolder);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					msg->addBOOL("Stamp", TRUE);
				}
				msg->nextBlockFast(_PREHASH_InventoryData);
				msg->addUUIDFast(_PREHASH_FolderID, cat->getUUID());
				msg->addUUIDFast(_PREHASH_ParentID, trash_id);
				if(msg->isSendFullFast(_PREHASH_InventoryData))
				{
					start_new_message = true;
					gAgent.sendReliableMessage();
					gInventory.accountForUpdate(update);
					update.clear();
				}
			// <edit>
			}
			// </edit>
		}
	}
	if(!start_new_message)
	{
		gAgent.sendReliableMessage();
		gInventory.accountForUpdate(update);
	}

	// move everything.
	uuid_vec_t::iterator it = move_ids.begin();
	uuid_vec_t::iterator end = move_ids.end();
	for(; it != end; ++it)
	{
		// <edit> trash problem
		if(gInventory.isObjectDescendentOf(*it, gSystemFolderRoot))
		{
			// if it's a category, delete descendents
			if(gInventory.getCategory(*it))
			{
				LLViewerInventoryCategory* cat = gInventory.getCategory(*it);
				cat->setDescendentCount(0);
				LLInventoryModel::cat_array_t categories;
				LLInventoryModel::item_array_t items;
				gInventory.collectDescendents(cat->getUUID(),
								   categories,
								   items,
								   false); // include trash?
				S32 count = items.count();
				S32 i;
				for(i = 0; i < count; ++i)
				{
					gInventory.deleteObject(items.get(i)->getUUID());
				}
				count = categories.count();
				for(i = 0; i < count; ++i)
				{
					gInventory.deleteObject(categories.get(i)->getUUID());
				}
			}
			// delete it
			gInventory.deleteObject(*it);
		}
		else
		// </edit>
		gInventory.moveObject((*it), trash_id);
	}

	// notify inventory observers.
	model->notifyObservers();
}

BOOL LLInvFVBridge::isClipboardPasteable() const
{
	if (!LLInventoryClipboard::instance().hasContents() || !isAgentInventory())
	{
		return FALSE;
	}
	LLInventoryModel* model = getInventoryModel();
	if (!model)
	{
		return FALSE;
	}

	const LLUUID &agent_id = gAgent.getID();

	LLDynamicArray<LLUUID> objects;
	LLInventoryClipboard::instance().retrieve(objects);
	S32 count = objects.count();
	for(S32 i = 0; i < count; i++)
	{
		const LLUUID &item_id = objects.get(i);

		// Can't paste folders
		const LLInventoryCategory *cat = model->getCategory(item_id);
		if (cat)
		{
			return FALSE;
		}

		const LLInventoryItem *item = model->getItem(item_id);
		if (item)
		{
			if (!item->getPermissions().allowCopyBy(agent_id))
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}

BOOL LLInvFVBridge::isClipboardPasteableAsLink() const
{
	if (!gHippoGridManager->getConnectedGrid()->supportsInvLinks())
	{
		return FALSE;
	}
	if (!LLInventoryClipboard::instance().hasContents() || !isAgentInventory())
	{
		return FALSE;
	}
	const LLInventoryModel* model = getInventoryModel();
	if (!model)
	{
		return FALSE;
	}

	LLDynamicArray<LLUUID> objects;
	LLInventoryClipboard::instance().retrieve(objects);
	S32 count = objects.count();
	for(S32 i = 0; i < count; i++)
	{
		const LLInventoryItem *item = model->getItem(objects.get(i));
		if (item)
		{
			if (!LLAssetType::lookupCanLink(item->getActualType()))
			{
				return FALSE;
			}
		}
		const LLViewerInventoryCategory *cat = model->getCategory(objects.get(i));
		if (cat && LLFolderType::lookupIsProtectedType(cat->getPreferredType()))
		{
			return FALSE;
		}
	}
	return TRUE;
}

void hide_context_entries(LLMenuGL& menu, 
						  const menuentry_vec_t &entries_to_show,
						  const menuentry_vec_t &disabled_entries)
{
	const LLView::child_list_t *list = menu.getChildList();

	for (LLView::child_list_t::const_iterator itor = list->begin(); 
		 itor != list->end(); 
		 ++itor)
	{
		LLView *menu_item = (*itor);
		std::string name = menu_item->getName();

		// descend into split menus:
		LLMenuItemBranchGL* branchp = dynamic_cast<LLMenuItemBranchGL*>(menu_item);
		if ((name == "More") && branchp)
		{
			hide_context_entries(*branchp->getBranch(), entries_to_show, disabled_entries);
		}
		
		
		bool found = false;
		menuentry_vec_t::const_iterator itor2;
		for (itor2 = entries_to_show.begin(); itor2 != entries_to_show.end(); ++itor2)
		{
			if (*itor2 == name)
			{
				found = true;
			}
		}
		if (!found)
		{
			menu_item->setVisible(FALSE);
		}
		else
		{
			for (itor2 = disabled_entries.begin(); itor2 != disabled_entries.end(); ++itor2)
			{
				if (*itor2 == name)
				{
					menu_item->setEnabled(FALSE);
				}
			}
		}
	}
}

// Helper for commonly-used entries
void LLInvFVBridge::getClipboardEntries(bool show_asset_id,
										menuentry_vec_t &items,
										menuentry_vec_t &disabled_items, U32 flags)
{
	const LLInventoryObject *obj = getInventoryObject();

	if (obj)
	{
		if (obj->getIsLinkType())
		{
			items.push_back(std::string("Find Original"));
			if (isLinkedObjectMissing())
			{
				disabled_items.push_back(std::string("Find Original"));
			}
		}
		else
		{
			if (LLAssetType::lookupCanLink(obj->getType()))
			{
				items.push_back(std::string("Find Links"));
			}
			items.push_back(std::string("Rename"));
			if (!isItemRenameable() || (flags & FIRST_SELECTED_ITEM) == 0)
			{
				disabled_items.push_back(std::string("Rename"));
			}

			if (show_asset_id)
			{
				items.push_back(std::string("Copy Asset UUID"));

				bool is_asset_knowable = false;

				LLViewerInventoryItem* inv_item = gInventory.getItem(mUUID);
				if (inv_item)
				{
					is_asset_knowable = LLAssetType::lookupIsAssetIDKnowable(inv_item->getType());
				}
				if ( !is_asset_knowable // disable menu item for Inventory items with unknown asset. EXT-5308
					 || (! ( isItemPermissive() || gAgent.isGodlike() ) )
					 || (flags & FIRST_SELECTED_ITEM) == 0)
				{
					disabled_items.push_back(std::string("Copy Asset UUID"));
				}
			}
			items.push_back(std::string("Copy Separator"));

			items.push_back(std::string("Copy"));
			if (!isItemCopyable())
			{
				disabled_items.push_back(std::string("Copy"));
			}
		}
	}

	// Don't allow items to be pasted directly into the COF.
	if (!isCOFFolder())
	{
	items.push_back(std::string("Paste"));
	}
	if (!isClipboardPasteable() || ((flags & FIRST_SELECTED_ITEM) == 0))
	{
		disabled_items.push_back(std::string("Paste"));
	}

	items.push_back(std::string("Paste As Link"));
	if (!isClipboardPasteableAsLink() || (flags & FIRST_SELECTED_ITEM) == 0)
	{
		disabled_items.push_back(std::string("Paste As Link"));
	}

	items.push_back(std::string("Paste Separator"));

	addDeleteContextMenuOptions(items, disabled_items);

	// If multiple items are selected, disable properties (if it exists).
	/*if ((flags & FIRST_SELECTED_ITEM) == 0)
	{
		disabled_items.push_back(std::string("Properties"));
	}*/
}

void LLInvFVBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	// *TODO: Translate
	lldebugs << "LLInvFVBridge::buildContextMenu()" << llendl;
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;
	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{
		addOpenRightClickMenuOption(items);
		items.push_back(std::string("Properties"));

// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
		if (rlv_handler_t::isEnabled())
		{
			LLInventoryObject* pItem = (mInventoryPanel->getModel()) ? mInventoryPanel->getModel()->getObject(mUUID) : NULL;
			if ( (pItem) &&
				 ( ((LLAssetType::AT_NOTECARD == pItem->getType()) && (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE))) ||
				   ((LLAssetType::AT_LSL_TEXT == pItem->getType()) && (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT))) ||
				   ((LLAssetType::AT_TEXTURE == pItem->getType()) && (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE))) ) )
			{
				disabled_items.push_back(std::string("Open"));
			}
		}
// [/RLVa:KB]

		getClipboardEntries(true, items, disabled_items, flags);
	}
	hide_context_entries(menu, items, disabled_items);
}

void LLInvFVBridge::addTrashContextMenuOptions(menuentry_vec_t &items,
											   menuentry_vec_t &disabled_items)
{
	const LLInventoryObject *obj = getInventoryObject();
	if (obj && obj->getIsLinkType())
	{
		items.push_back(std::string("Find Original"));
		if (isLinkedObjectMissing())
		{
			disabled_items.push_back(std::string("Find Original"));
		}
	}
	items.push_back(std::string("Purge Item"));
	if (!isItemRemovable())
	{
		disabled_items.push_back(std::string("Purge Item"));	
	}
	items.push_back(std::string("Restore Item"));
}

void LLInvFVBridge::addDeleteContextMenuOptions(menuentry_vec_t &items,
												menuentry_vec_t &disabled_items)
{

	const LLInventoryObject *obj = getInventoryObject();

	// Don't allow delete as a direct option from COF folder.
	if (obj && obj->getIsLinkType() && isCOFFolder() && get_is_item_worn(mUUID))
	{
		return;
	}

	// "Remove link" and "Delete" are the same operation.
	if (obj && obj->getIsLinkType() /* && !get_is_item_worn(mUUID)*/)
	{
		items.push_back(std::string("Remove Link"));
	}
	else
	{
		items.push_back(std::string("Delete"));
	}

	if (!isItemRemovable())
	{
		disabled_items.push_back(std::string("Delete"));
	}
}

void LLInvFVBridge::addOpenRightClickMenuOption(menuentry_vec_t &items)
{
	const LLInventoryObject *obj = getInventoryObject();
	const BOOL is_link = (obj && obj->getIsLinkType());

	if (is_link)
		items.push_back(std::string("Open Original"));
	else
		items.push_back(std::string("Open"));
}

// *TODO: remove this
BOOL LLInvFVBridge::startDrag(EDragAndDropType* type, LLUUID* id) const
{
	BOOL rv = FALSE;

	const LLInventoryObject* obj = getInventoryObject();

	if(obj)
	{
		*type = LLViewerAssetType::lookupDragAndDropType(obj->getActualType());
		if(*type == DAD_NONE)
		{
			return FALSE;
		}
		
		*id = obj->getUUID();
		//object_ids.put(obj->getUUID());

		if (*type == DAD_CATEGORY)
		{
			LLInventoryModelBackgroundFetch::instance().start(obj->getUUID());
		}

		rv = TRUE;
	}

	return rv;
}

LLInventoryObject* LLInvFVBridge::getInventoryObject() const
{
	LLInventoryObject* obj = NULL;
	LLInventoryModel* model = getInventoryModel();
	if(model)
	{
		obj = (LLInventoryObject*)model->getObject(mUUID);
	}
	return obj;
}

LLInventoryModel* LLInvFVBridge::getInventoryModel() const
{
	LLInventoryPanel* panel = dynamic_cast<LLInventoryPanel*>(mInventoryPanel);
	return panel ? panel->getModel() : NULL;
}

BOOL LLInvFVBridge::isItemInTrash() const
{
	LLInventoryModel* model = getInventoryModel();
	if(!model) return FALSE;
	const LLUUID trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	return model->isObjectDescendentOf(mUUID, trash_id);
}

BOOL LLInvFVBridge::isLinkedObjectInTrash() const
{
	if (isItemInTrash()) return TRUE;

	const LLInventoryObject *obj = getInventoryObject();
	if (obj && obj->getIsLinkType())
	{
		LLInventoryModel* model = getInventoryModel();
		if(!model) return FALSE;
		const LLUUID trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
		return model->isObjectDescendentOf(obj->getLinkedUUID(), trash_id);
	}
	return FALSE;
}

BOOL LLInvFVBridge::isLinkedObjectMissing() const
{
	const LLInventoryObject *obj = getInventoryObject();
	if (!obj)
	{
		return TRUE;
	}
	if (obj->getIsLinkType() && LLAssetType::lookupIsLinkType(obj->getType()))
	{
		return TRUE;
	}
	return FALSE;
}

BOOL LLInvFVBridge::isAgentInventory() const
{
	const LLInventoryModel* model = getInventoryModel();
	if(!model) return FALSE;
	if(gInventory.getRootFolderID() == mUUID) return TRUE;
	return model->isObjectDescendentOf(mUUID, gInventory.getRootFolderID());
}

BOOL LLInvFVBridge::isCOFFolder() const
{
	return LLCOFMgr::instance().getCOF() == mUUID;
}

BOOL LLInvFVBridge::isItemPermissive() const
{
	return FALSE;
}

// static
void LLInvFVBridge::changeItemParent(LLInventoryModel* model,
									 LLViewerInventoryItem* item,
									 const LLUUID& new_parent_id,
									 BOOL restamp)
{
	change_item_parent(model, item, new_parent_id, restamp);
}

// static
void LLInvFVBridge::changeCategoryParent(LLInventoryModel* model,
										 LLViewerInventoryCategory* cat,
										 const LLUUID& new_parent_id,
										 BOOL restamp)
{
	change_category_parent(model, cat, new_parent_id, restamp);
}

LLInvFVBridge* LLInvFVBridge::createBridge(LLAssetType::EType asset_type,
					   LLAssetType::EType actual_asset_type,
					   LLInventoryType::EType inv_type,
					   LLInventoryPanel* inventory,
					   const LLUUID& uuid,
					   U32 flags)
{
	LLInvFVBridge* new_listener = NULL;
	switch(asset_type)
	{
	case LLAssetType::AT_TEXTURE:
		if(!(inv_type == LLInventoryType::IT_TEXTURE || inv_type == LLInventoryType::IT_SNAPSHOT))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLTextureBridge(inventory, uuid, inv_type);
		break;

	case LLAssetType::AT_SOUND:
		if(!(inv_type == LLInventoryType::IT_SOUND))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLSoundBridge(inventory, uuid);
		break;

	case LLAssetType::AT_LANDMARK:
		if(!(inv_type == LLInventoryType::IT_LANDMARK))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLLandmarkBridge(inventory, uuid, flags);
		break;
		
	case LLAssetType::AT_CALLINGCARD:
		if(!(inv_type == LLInventoryType::IT_CALLINGCARD))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLCallingCardBridge(inventory, uuid);
		break;

	case LLAssetType::AT_SCRIPT:
		if(!(inv_type == LLInventoryType::IT_LSL))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLItemBridge(inventory, uuid);
		break;

	case LLAssetType::AT_OBJECT:
		if(!(inv_type == LLInventoryType::IT_OBJECT || inv_type == LLInventoryType::IT_ATTACHMENT) 
			|| inv_type == LLInventoryType::IT_TEXTURE )	//There's an abundance of objects in inv that have texture (0) as their inv type, right out of unpack. 
															//May have been bug either in an old client, or server version. Either way... it causes a lot of spam over something ultimately harmless.
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLObjectBridge(inventory, uuid, inv_type, flags);
		break;

	case LLAssetType::AT_NOTECARD:
		if(!(inv_type == LLInventoryType::IT_NOTECARD))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLNotecardBridge(inventory, uuid);
		break;

	case LLAssetType::AT_ANIMATION:
		if(!(inv_type == LLInventoryType::IT_ANIMATION))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLAnimationBridge(inventory, uuid);
		break;

	case LLAssetType::AT_GESTURE:
		if(!(inv_type == LLInventoryType::IT_GESTURE))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLGestureBridge(inventory, uuid);
		break;

	case LLAssetType::AT_LSL_TEXT:
		if(!(inv_type == LLInventoryType::IT_LSL))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLLSLTextBridge(inventory, uuid);
		break;

	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_BODYPART:
		if(!(inv_type == LLInventoryType::IT_WEARABLE))
		{
				llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLWearableBridge(inventory, uuid, asset_type, inv_type, (LLWearableType::EType)flags);
		break;

	case LLAssetType::AT_CATEGORY:
	//case LLAssetType::AT_ROOT_CATEGORY:
		if (actual_asset_type == LLAssetType::AT_LINK_FOLDER)
		{
			// Create a link folder handler instead.
			new_listener = new LLLinkFolderBridge(inventory, uuid);
			break;
		}
		new_listener = new LLFolderBridge(inventory, uuid);
		break;
	case LLAssetType::AT_LINK:
	case LLAssetType::AT_LINK_FOLDER:
		// Only should happen for broken links.
		new_listener = new LLLinkItemBridge(inventory, uuid);
		break;
    case LLAssetType::AT_MESH:
		if(!(inv_type == LLInventoryType::IT_MESH))
		{
			llwarns << LLAssetType::lookup(asset_type) << " asset has inventory type " << LLInventoryType::lookupHumanReadable(inv_type) << " on uuid " << uuid << llendl;
		}
		new_listener = new LLMeshBridge(inventory, uuid);
		break;
	default:
		llinfos << "Unhandled asset type (llassetstorage.h): "
				<< (S32)asset_type << llendl;
		break;
	}

	if (new_listener)
	{
		new_listener->mInvType = inv_type;
	}

	return new_listener;
}

// +=================================================+
// |        LLItemBridge                             |
// +=================================================+

void LLItemBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{

	if ("goto" == action)
	{
		gotoItem(folder);
	}
	else if ("open" == action)
	{
		openItem();
		return;
	}
	else if ("properties" == action)
	{
		showProperties();
		return;
	}
	else if ("purge" == action)
	{
		LLInventoryCategory* cat = model->getCategory(mUUID);
		if(cat)
		{
			model->purgeDescendentsOf(mUUID);
		}
		LLInventoryObject* obj = model->getObject(mUUID);
		if(!obj) return;
		obj->removeFromServer();
		LLPreview::hide(mUUID);
		model->deleteObject(mUUID);
		model->notifyObservers();
	}
	else if ("restoreToWorld" == action)
	{
		restoreToWorld();
		return;
	}
	else if ("restore" == action)
	{
		restoreItem();
		return;
	}
	else if ("copy_uuid" == action)
	{
		// Single item only
		LLViewerInventoryItem* item = static_cast<LLViewerInventoryItem*>(getItem());
		if(!item) return;
		LLUUID asset_id = item->getAssetUUID();
		std::string buffer;
		asset_id.toString(buffer);

		gViewerWindow->mWindow->copyTextToClipboard(utf8str_to_wstring(buffer));
		return;
	}
	else if ("copy" == action)
	{
		copyToClipboard();
		return;
	}
	else if ("paste" == action)
	{
		// Single item only
		LLInventoryItem* itemp = model->getItem(mUUID);
		if (!itemp) return;

		LLFolderViewItem* folder_view_itemp = folder->getItemByID(itemp->getParentUUID());
		if (!folder_view_itemp) return;

		folder_view_itemp->getListener()->pasteFromClipboard();
		return;
	}
	else if ("paste_link" == action)
	{
		// Single item only
		LLInventoryItem* itemp = model->getItem(mUUID);
		if (!itemp) return;

		LLFolderViewItem* folder_view_itemp = folder->getItemByID(itemp->getParentUUID());
		if (!folder_view_itemp) return;

		folder_view_itemp->getListener()->pasteLinkFromClipboard();
		return;
	}
	else if("reupload" == action)
	{
		LLInventoryItem* item = model->getItem(mUUID);
		if (item && item->getType() == LLAssetType::AT_TEXTURE)
		{
		  AIFilePicker* filepicker = AIFilePicker::create();
		  filepicker->open(FFLOAD_IMAGE, "", "image");
		  filepicker->run(boost::bind(&LLItemBridge::showFloaterImagePreview, item, filepicker));
		}
	}
	// </edit>
}

// static
void LLItemBridge::showFloaterImagePreview(LLInventoryItem* item, AIFilePicker* filepicker)
{
	if (filepicker->hasFilename())
	{
		LLFloaterImagePreview* floaterp = new LLFloaterImagePreview(filepicker->getFilename(), item);
		LLUICtrlFactory::getInstance()->buildFloater(floaterp, "floater_image_preview.xml");
	}
}

void LLItemBridge::selectItem()
{
	LLViewerInventoryItem* item = static_cast<LLViewerInventoryItem*>(getItem());
	if(item && !item->isComplete())
	{
		// <edit>
		if(!(gInventory.isObjectDescendentOf(mUUID, gSystemFolderRoot)))
		// </edit>
		item->fetchFromServer();
	}
}

void LLItemBridge::restoreItem()
{
	LLViewerInventoryItem* item = static_cast<LLViewerInventoryItem*>(getItem());
	if(item)
	{
		LLInventoryModel* model = getInventoryModel();
		const LLUUID new_parent = model->findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(item->getType()));
		// do not restamp on restore.
		LLInvFVBridge::changeItemParent(model, item, new_parent, FALSE);
	}
}

void LLItemBridge::restoreToWorld()
{
	//Similar functionality to the drag and drop rez logic
	bool remove_from_inventory = false;

	LLViewerInventoryItem* itemp = static_cast<LLViewerInventoryItem*>(getItem());
	if (itemp)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("RezRestoreToWorld");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

		msg->nextBlockFast(_PREHASH_InventoryData);
		itemp->packMessage(msg);
		msg->sendReliable(gAgent.getRegion()->getHost());

	//remove local inventory copy, sim will deal with permissions and removing the item
	//from the actual inventory if its a no-copy etc
	if(!itemp->getPermissions().allowCopyBy(gAgent.getID()))
	{
			remove_from_inventory = true;
	}

	// Check if it's in the trash. (again similar to the normal rez logic)
		const LLUUID trash_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
	if(gInventory.isObjectDescendentOf(itemp->getUUID(), trash_id))
	{
			remove_from_inventory = true;
		}
	}

	if(remove_from_inventory)
	{
		gInventory.deleteObject(itemp->getUUID());
		gInventory.notifyObservers();
	}
}

void LLItemBridge::gotoItem(LLFolderView *folder)
{
	LLInventoryObject *obj = getInventoryObject();
	if (obj && obj->getIsLinkType())
	{
		LLInventoryView *view = LLInventoryView::getActiveInventory();
		if (view)
		{
			view->getPanel()->setSelection(obj->getLinkedUUID(), TAKE_FOCUS_NO);
		}
	}
}

LLUIImagePtr LLItemBridge::getIcon() const
{
	LLInventoryObject *obj = getInventoryObject();
	if (obj) 
	{
		return LLInventoryIcon::getIcon(obj->getType(),
										LLInventoryType::IT_NONE,
										mIsLink);
	}
	
	return LLInventoryIcon::getIcon(LLInventoryIcon::ICONNAME_OBJECT);
}

PermissionMask LLItemBridge::getPermissionMask() const
{
	LLViewerInventoryItem* item = getItem();
	PermissionMask perm_mask = 0;
	if(item) 
	{
		BOOL copy = item->getPermissions().allowCopyBy(gAgent.getID());
		BOOL mod = item->getPermissions().allowModifyBy(gAgent.getID());
		BOOL xfer = item->getPermissions().allowOperationBy(PERM_TRANSFER,
															gAgent.getID());

		if (copy) perm_mask |= PERM_COPY;
		if (mod)  perm_mask |= PERM_MODIFY;
		if (xfer) perm_mask |= PERM_TRANSFER;

	}
	return perm_mask;
}
	
const std::string& LLItemBridge::getDisplayName() const
{
	if(mDisplayName.empty())
	{
		buildDisplayName(getItem(), mDisplayName);
	}
	return mDisplayName;
}

void LLItemBridge::buildDisplayName(LLInventoryItem* item, std::string& name)
{
	if(item) 
	{
		name.assign(item->getName());			
	}
	else
	{
		name.assign(LLStringUtil::null);
	}
}

LLFontGL::StyleFlags LLItemBridge::getLabelStyle() const
{
	U8 font = LLFontGL::NORMAL;
	const LLViewerInventoryItem* item = getItem();

	if (get_is_item_worn(item))
	{
		// llinfos << "BOLD" << llendl;
		font |= LLFontGL::BOLD;
	}
	else if(item && item->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

std::string LLItemBridge::getLabelSuffix() const
{
			// *TODO: Translate
	static std::string NO_COPY = " (no copy)";
	static std::string NO_MOD = " (no modify)";
	static std::string NO_XFER = " (no transfer)";
	static std::string TEMPO = " (temporary)";
			static std::string LINK = " (link)";
	static std::string BROKEN_LINK = "(broken_link)";
	std::string suffix;
	LLInventoryItem* item = getItem();
	if(item) 
	{
		// it's a bit confusing to put nocopy/nomod/etc on calling cards.
		if(LLAssetType::AT_CALLINGCARD != item->getType()
		   && item->getPermissions().getOwner() == gAgent.getID())
		{
			BOOL broken_link = LLAssetType::lookupIsLinkType(item->getType());
			if (broken_link) return BROKEN_LINK;

			BOOL link = item->getIsLinkType();
			if (link) return LINK;

			BOOL copy = item->getPermissions().allowCopyBy(gAgent.getID());
			if (!copy)
			{
				suffix += NO_COPY;
			}
			BOOL mod = item->getPermissions().allowModifyBy(gAgent.getID());
			if (!mod)
			{
				suffix += NO_MOD;
			}
			BOOL xfer = item->getPermissions().allowOperationBy(PERM_TRANSFER,
																gAgent.getID());
			if (!xfer)
			{
				suffix += NO_XFER;
			}
			
			BOOL temp = item->getPermissions().getGroup() == gAgent.getID();

			if (temp)
			{
				suffix += TEMPO;
			}
		}
	}
	return suffix;
}

time_t LLItemBridge::getCreationDate() const
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		return item->getCreationDate();
	}
	return 0;
}


BOOL LLItemBridge::isItemRenameable() const
{
	LLViewerInventoryItem* item = getItem();
	if(item)
	{
		// (For now) Don't allow calling card rename since that may confuse users as to
		// what the calling card points to.
		if (item->getInventoryType() == LLInventoryType::IT_CALLINGCARD)
		{
			return FALSE;
		}

		if (!item->isComplete()) // EXT-8662
		{
			return FALSE;
		}

		return (item->getPermissions().allowModifyBy(gAgent.getID()));
	}
	return FALSE;
}

BOOL LLItemBridge::renameItem(const std::string& new_name)
{
	if(!isItemRenameable())
		return FALSE;
	LLPreview::rename(mUUID, getPrefix() + new_name);
	LLInventoryModel* model = getInventoryModel();
	if(!model)
		return FALSE;
	LLViewerInventoryItem* item = getItem();
	if(item && (item->getName() != new_name))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->rename(new_name);
		buildDisplayName(new_item, mDisplayName);
		new_item->updateServer(FALSE);
		model->updateItem(new_item);

		model->notifyObservers();
	}
	// return FALSE because we either notified observers (& therefore
	// rebuilt) or we didn't update.
	return FALSE;
}


BOOL LLItemBridge::removeItem()
{
	if(!isItemRemovable())
	{
		return FALSE;
	}
	// move it to the trash
	LLPreview::hide(mUUID, TRUE);
	LLInventoryModel* model = getInventoryModel();
	if(!model) return FALSE;
	const LLUUID& trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	LLViewerInventoryItem* item = getItem();
	if (!item) return FALSE;

	// Already in trash
	if (!model->isObjectDescendentOf(mUUID, trash_id))
	{
		// <edit> trash problem
		if(gInventory.isObjectDescendentOf(mUUID, gSystemFolderRoot))
		{
			LLInventoryModel::LLCategoryUpdate up(item->getParentUUID(), -1);
			gInventory.deleteObject(mUUID);
			gInventory.accountForUpdate(up);
			gInventory.notifyObservers();
		}
		else
		// </edit>
		// move to trash, and restamp
		LLInvFVBridge::changeItemParent(model, item, trash_id, TRUE);
		// delete was successful
		return TRUE;
	}
	else
	{
		// tried to delete already item in trash (should purge?)
		return FALSE;
	}
}

BOOL LLItemBridge::isItemCopyable() const
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		// Can't copy worn objects. DEV-15183
		/*if(get_is_item_worn(mUUID))
		{
			return FALSE;
		}*/

		if(!gHippoGridManager->getConnectedGrid()->supportsInvLinks())
			return (item->getPermissions().allowCopyBy(gAgent.getID()));
		// You can never copy a link.
		else if (item->getIsLinkType())
		{
			return FALSE;
		}

		// All items can be copied in god mode since you can
			// at least paste-as-link the item, though you 
			// still may not be able paste the item.
			return TRUE;
		// return (item->getPermissions().allowCopyBy(gAgent.getID()));
	}
	return FALSE;
}

BOOL LLItemBridge::copyToClipboard() const
{
	if(isItemCopyable())
	{
		LLInventoryClipboard::instance().add(mUUID);
		return TRUE;
	}
	return FALSE;
}

LLViewerInventoryItem* LLItemBridge::getItem() const
{
	LLViewerInventoryItem* item = NULL;
	LLInventoryModel* model = getInventoryModel();
	if(model)
	{
		item = (LLViewerInventoryItem*)model->getItem(mUUID);
	}
	return item;
}

BOOL LLItemBridge::isItemPermissive() const
{
	LLViewerInventoryItem* item = getItem();
	if(item)
	{
		return item->getIsFullPerm();
	}
	return FALSE;
}

// +=================================================+
// |        LLFolderBridge                           |
// +=================================================+

LLHandle<LLFolderBridge> LLFolderBridge::sSelf;

// Can be moved to another folder
BOOL LLFolderBridge::isItemMovable()
{
	LLInventoryObject* obj = getInventoryObject();
	if(obj)
	{
		return (!LLFolderType::lookupIsProtectedType(((LLInventoryCategory*)obj)->getPreferredType()));
	}
	return FALSE;
}

void LLFolderBridge::selectItem()
{
}


// Can be destroyed (or moved to trash)
BOOL LLFolderBridge::isItemRemovable()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if(!model) 
	{
		return FALSE;
	}

	// <edit>
	// People delete their inventory easily...
	if(mUUID == gInventory.getRootFolderID())
	{
		return FALSE;
	}
	if(!model->isObjectDescendentOf(mUUID, gInventory.getRootFolderID()))
	{
		return FALSE;
	}

	LLVOAvatar* avatar = gAgentAvatarp;
	if( !avatar )
	{
		return FALSE;
	}

	LLInventoryCategory* category = model->getCategory(mUUID);
	if( !category )
	{
		return FALSE;
	}

	if ( (LLFolderType::FT_NONE != category->getPreferredType()) && (LLFolderType::FT_OUTFIT != category->getPreferredType()) )
	{
		return FALSE;
	}

	LLInventoryModel::cat_array_t	descendent_categories;
	LLInventoryModel::item_array_t	descendent_items;
	gInventory.collectDescendents( mUUID, descendent_categories, descendent_items, FALSE );

	S32 i;
	for( i = 0; i < descendent_categories.count(); i++ )
	{
		LLInventoryCategory* category = descendent_categories[i];
		if( LLFolderType::FT_NONE != category->getPreferredType() )
		{
			return FALSE;
		}
	}

	for( i = 0; i < descendent_items.count(); i++ )
	{
		LLInventoryItem* item = descendent_items[i];
		if ((item->getType() == LLAssetType::AT_CLOTHING ||
			 item->getType() == LLAssetType::AT_BODYPART) && !item->getIsLinkType())
		{
			if( gAgentWearables.isWearingItem( item->getUUID() ) )
			{
				return FALSE;
			}
		}
		else if (item->getType() == LLAssetType::AT_OBJECT && !item->getIsLinkType())
		{
			if( avatar->isWearingAttachment( item->getUUID() ) )
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

BOOL LLFolderBridge::isUpToDate() const
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if(!model) return FALSE;
	LLViewerInventoryCategory* category = (LLViewerInventoryCategory*)model->getCategory(mUUID);
	if( !category )
	{
		return FALSE;
	}

	// <edit> trying to make it stop trying to fetch Local Inventory
	return category->getVersion() != LLViewerInventoryCategory::VERSION_UNKNOWN;
	//return (category->getVersion() != LLViewerInventoryCategory::VERSION_UNKNOWN) || (mUUID == gSystemFolderRoot) || (gInventory.isObjectDescendentOf(mUUID, gSystemFolderRoot));
	// </edit>
}

BOOL LLFolderBridge::isClipboardPasteableAsLink() const
{
	// Check normal paste-as-link permissions
	if (!LLInvFVBridge::isClipboardPasteableAsLink())
	{
		return FALSE;
	}

	const LLInventoryModel* model = getInventoryModel();
	if (!model)
	{
		return FALSE;
	}

	const LLViewerInventoryCategory *current_cat = getCategory();
	if (current_cat)
	{
/* TODO
		const BOOL is_in_friend_folder = LLFriendCardsManager::instance().isCategoryInFriendFolder(current_cat);
*/
		const LLUUID &current_cat_id = current_cat->getUUID();
		LLDynamicArray<LLUUID> objects;
		LLInventoryClipboard::instance().retrieve(objects);
		S32 count = objects.count();
		for (S32 i = 0; i < count; i++)
		{
			const LLUUID &obj_id = objects.get(i);
			const LLInventoryCategory *cat = model->getCategory(obj_id);
			if (cat)
			{
				const LLUUID &cat_id = cat->getUUID();
				// Don't allow recursive pasting
				if ((cat_id == current_cat_id) ||
					model->isObjectDescendentOf(current_cat_id, cat_id))
				{
					return FALSE;
				}
			}
/* TODO
			// Don't allow pasting duplicates to the Calling Card/Friends subfolders, see bug EXT-1599
			if (is_in_friend_folder)
			{
				// If object is direct descendent of current Friends subfolder than return false.
				// Note: We can't use 'const LLInventoryCategory *cat', because it may be null
				// in case type of obj_id is LLInventoryItem.
				if (LLFriendCardsManager::instance().isObjDirectDescendentOfCategory(model->getObject(obj_id), current_cat))
				{
					return FALSE;
				}
			}
*/
		}
	}
	return TRUE;

}


BOOL LLFolderBridge::dragCategoryIntoFolder(LLInventoryCategory* inv_cat,
											BOOL drop)
{

	LLInventoryModel* model = getInventoryModel();

	if (!inv_cat) return FALSE; // shouldn't happen, but in case item is incorrectly parented in which case inv_cat will be NULL
	if(!model) return FALSE;
	if (!isAgentAvatarValid()) return FALSE;
	if (!isAgentInventory()) return FALSE; // cannot drag categories into library


	// check to make sure source is agent inventory, and is represented there.
	LLToolDragAndDrop::ESource source = LLToolDragAndDrop::getInstance()->getSource();
	BOOL is_agent_inventory = (model->getCategory(inv_cat->getUUID()) != NULL)
		&& (LLToolDragAndDrop::SOURCE_AGENT == source);

	BOOL accept = FALSE;
	if(is_agent_inventory)
	{
		const LLUUID& cat_id = inv_cat->getUUID();
		const LLUUID &trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH, false);
		const BOOL move_is_into_trash = (mUUID == trash_id) || model->isObjectDescendentOf(mUUID, trash_id);
		BOOL is_movable = (LLFolderType::FT_NONE == inv_cat->getPreferredType());
		if( is_movable )
		{
			LLInventoryModel::cat_array_t	descendent_categories;
			LLInventoryModel::item_array_t	descendent_items;
			gInventory.collectDescendents( cat_id, descendent_categories, descendent_items, FALSE );
			for (S32 i=0; i < descendent_categories.count(); ++i)
			{
				LLInventoryCategory* category = descendent_categories[i];
				if(LLFolderType::lookupIsProtectedType(category->getPreferredType()))
				{
					// ...can't move "special folders" like Textures
					is_movable = FALSE;
					break;
				}
			}
			if( is_movable && move_is_into_trash )
			{
				for (S32 i=0; i < descendent_items.count(); ++i)
				{
					LLInventoryItem* item = descendent_items[i];
					if (get_is_item_worn(item))
					{
						is_movable = FALSE;  // It's generally movable, but not into the trash!
						break;
					}
				}
			}

			// 
			//--------------------------------------------------------------------------------

			accept = is_movable
				&& (mUUID != cat_id)								// Can't move a folder into itself
				&& (mUUID != inv_cat->getParentUUID())				// Avoid moves that would change nothing
				&& !(model->isObjectDescendentOf(mUUID, cat_id));	// Avoid circularity
			if (accept && drop)
			{
				// Look for any gestures and deactivate them
				if (move_is_into_trash)
				{
					for (S32 i = 0; i < descendent_items.count(); i++)
					{
						LLInventoryItem* item = descendent_items[i];
						if (item->getType() == LLAssetType::AT_GESTURE
							&& LLGestureMgr::instance().isGestureActive(item->getUUID()))
						{
							LLGestureMgr::instance().deactivateGesture(item->getUUID());
						}
					}
				}

				// Reparent the folder and restamp children if it's moving
				// into trash.
				LLInvFVBridge::changeCategoryParent(
					model,
					(LLViewerInventoryCategory*)inv_cat,
					mUUID,
					move_is_into_trash);
			}
		}
	}
	else if(LLToolDragAndDrop::SOURCE_WORLD == source)
	{
		// content category has same ID as object itself
		LLUUID object_id = inv_cat->getUUID();
		LLUUID category_id = mUUID;
		accept = move_inv_category_world_to_agent(object_id, category_id, drop);
	}
	return accept;
}

void warn_move_inventory(LLViewerObject* object, LLMoveInv* move_inv)
{
	const char* dialog = NULL;
	if (object->flagScripted())
	{
		dialog = "MoveInventoryFromScriptedObject";
	}
	else
	{
		dialog = "MoveInventoryFromObject";
	}
	LLNotificationsUtil::add(dialog, LLSD(), LLSD(), boost::bind(move_task_inventory_callback, _1, _2, move_inv));
}

// Move/copy all inventory items from the Contents folder of an in-world
// object to the agent's inventory, inside a given category.
BOOL move_inv_category_world_to_agent(const LLUUID& object_id, 
									  const LLUUID& category_id,
									  BOOL drop,
									  void (*callback)(S32, void*),
									  void* user_data)
{
	// Make sure the object exists. If we allowed dragging from
	// anonymous objects, it would be possible to bypass
	// permissions.
	// content category has same ID as object itself
	LLViewerObject* object = gObjectList.findObject(object_id);
	if(!object)
	{
		llinfos << "Object not found for drop." << llendl;
		return FALSE;
	}

	// this folder is coming from an object, as there is only one folder in an object, the root,
	// we need to collect the entire contents and handle them as a group
	LLInventoryObject::object_list_t inventory_objects;
	object->getInventoryContents(inventory_objects);

	if (inventory_objects.empty())
	{
		llinfos << "Object contents not found for drop." << llendl;
		return FALSE;
	}
	
	BOOL accept = TRUE;
	BOOL is_move = FALSE;

	// coming from a task. Need to figure out if the person can
	// move/copy this item.
	LLInventoryObject::object_list_t::iterator it = inventory_objects.begin();
	LLInventoryObject::object_list_t::iterator end = inventory_objects.end();
	for ( ; it != end; ++it)
	{
		// coming from a task. Need to figure out if the person can
		// move/copy this item.
		LLPermissions perm(((LLInventoryItem*)((LLInventoryObject*)(*it)))->getPermissions());
		if((perm.allowCopyBy(gAgent.getID(), gAgent.getGroupID())
			&& perm.allowTransferTo(gAgent.getID())))
//			|| gAgent.isGodlike())
		{
			accept = TRUE;
		}
		else if(object->permYouOwner())
		{
			// If the object cannot be copied, but the object the
			// inventory is owned by the agent, then the item can be
			// moved from the task to agent inventory.
			is_move = TRUE;
			accept = TRUE;
		}
		else
		{
			accept = FALSE;
			break;
		}
	}

	if(drop && accept)
	{
		it = inventory_objects.begin();
		LLInventoryObject::object_list_t::iterator first_it = inventory_objects.begin();
		LLMoveInv* move_inv = new LLMoveInv;
		move_inv->mObjectID = object_id;
		move_inv->mCategoryID = category_id;
		move_inv->mCallback = callback;
		move_inv->mUserData = user_data;

		for ( ; it != end; ++it)
		{
			two_uuids_t two(category_id, (*it)->getUUID());
			move_inv->mMoveList.push_back(two);
		}

		if(is_move)
		{
			// Callback called from within here.
			warn_move_inventory(object, move_inv);
		}
		else
		{
			LLNotification::Params params("MoveInventoryFromObject");
			params.functor(boost::bind(move_task_inventory_callback, _1, _2, move_inv));
			LLNotifications::instance().forceResponse(params, 0);
		}
	}
	return accept;
}

//Used by LLFolderBridge as callback for directory recursion.
class LLRightClickInventoryFetchObserver : public LLInventoryFetchItemsObserver
{
public:
	LLRightClickInventoryFetchObserver(const uuid_vec_t& ids) :
		LLInventoryFetchItemsObserver(ids),
		mCopyItems(false)
	{ };
	LLRightClickInventoryFetchObserver(const uuid_vec_t& ids,
									   const LLUUID& cat_id, 
									   bool copy_items) :
		LLInventoryFetchItemsObserver(ids),
		mCatID(cat_id),
		mCopyItems(copy_items)
		{ };
	virtual void done()
	{
		// we've downloaded all the items, so repaint the dialog
		LLFolderBridge::staticFolderOptionsMenu();

		gInventory.removeObserver(this);
		delete this;
	}
	

protected:
	LLUUID mCatID;
	bool mCopyItems;

};

//Used by LLFolderBridge as callback for directory recursion.
class LLRightClickInventoryFetchDescendentsObserver : public LLInventoryFetchDescendentsObserver
{
public:
	LLRightClickInventoryFetchDescendentsObserver(const uuid_vec_t& ids,
												  bool copy_items) : 
		LLInventoryFetchDescendentsObserver(ids),
		mCopyItems(copy_items) 
	{}
	~LLRightClickInventoryFetchDescendentsObserver() {}
	virtual void done();
protected:
	bool mCopyItems;
};

void LLRightClickInventoryFetchDescendentsObserver::done()
{
	// Avoid passing a NULL-ref as mCompleteFolders.front() down to
	// gInventory.collectDescendents()
	if( mComplete.empty() )
	{
		llwarns << "LLRightClickInventoryFetchDescendentsObserver::done with empty mCompleteFolders" << llendl;
		dec_busy_count();
		gInventory.removeObserver(this);
		delete this;
		return;
	}

	// What we do here is get the complete information on the items in
	// the library, and set up an observer that will wait for that to
	// happen.
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	gInventory.collectDescendents(mComplete.front(),
								  cat_array,
								  item_array,
								  LLInventoryModel::EXCLUDE_TRASH);
	S32 count = item_array.count();
#if 0 // HACK/TODO: Why?
	// This early causes a giant menu to get produced, and doesn't seem to be needed.
	if(!count)
	{
		llwarns << "Nothing fetched in category " << mComplete.front()
				<< llendl;
		dec_busy_count();
		gInventory.removeObserver(this);
		delete this;
		return;
	}
#endif

	uuid_vec_t ids;
	for(S32 i = 0; i < count; ++i)
	{
		ids.push_back(item_array.get(i)->getUUID());
	}

	LLRightClickInventoryFetchObserver* outfit = new LLRightClickInventoryFetchObserver(ids, mComplete.front(), mCopyItems);

	// clean up, and remove this as an observer since the call to the
	// outfit could notify observers and throw us into an infinite
	// loop.
	dec_busy_count();
	gInventory.removeObserver(this);
	delete this;

	// increment busy count and either tell the inventory to check &
	// call done, or add this object to the inventory for observation.
	inc_busy_count();

	// do the fetch
	outfit->startFetch();
	outfit->done();				//Not interested in waiting and this will be right 99% of the time.
//Uncomment the following code for laggy Inventory UI.
/*	if(outfit->isEverythingComplete())
	{
		// everything is already here - call done.
		outfit->done();
	}
	else
	{
		// it's all on it's way - add an observer, and the inventory
		// will call done for us when everything is here.
		gInventory.addObserver(outfit);
	}*/
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryWearObserver
//
// Observer for "copy and wear" operation to support knowing 
// when the all of the contents have been added to inventory.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInventoryCopyAndWearObserver : public LLInventoryObserver
{
public:
	LLInventoryCopyAndWearObserver(const LLUUID& cat_id, int count, bool folder_added=false) :
		mCatID(cat_id), mContentsCount(count), mFolderAdded(folder_added) {}
	virtual ~LLInventoryCopyAndWearObserver() {}
	virtual void changed(U32 mask);

protected:
	LLUUID mCatID;
	int    mContentsCount;
	bool   mFolderAdded;
};



void LLInventoryCopyAndWearObserver::changed(U32 mask)
{
	if((mask & (LLInventoryObserver::ADD)) != 0)
	{
		if (!mFolderAdded) 
		{
			const std::set<LLUUID>& changed_items = gInventory.getChangedIDs();

			std::set<LLUUID>::const_iterator id_it = changed_items.begin();
			std::set<LLUUID>::const_iterator id_end = changed_items.end();
			for (;id_it != id_end; ++id_it)
			{
				if ((*id_it) == mCatID) 
				{
					mFolderAdded = TRUE;
					break;
				}
			}
		}

		if (mFolderAdded) 
		{
			LLViewerInventoryCategory* category = gInventory.getCategory(mCatID);

			if (NULL == category)
			{
				llwarns << "gInventory.getCategory(" << mCatID
					<< ") was NULL" << llendl;
			}
			else
			{
				if (category->getDescendentCount() ==
				    mContentsCount)
				{
					gInventory.removeObserver(this);
					wear_inventory_category(category, FALSE, TRUE);
					delete this;
				}
			}		
		}

	}
}



void LLFolderBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if ("open" == action)
	{
		openItem();
		return;
	}
	else if ("paste" == action)
	{
		pasteFromClipboard();
		return;
	}
	else if ("paste_link" == action)
	{
		pasteLinkFromClipboard();
		return;
	}
	else if ("properties" == action)
	{
		showProperties();
		return;
	}
	else if ("replaceoutfit" == action)
	{
		modifyOutfit(FALSE);
		return;
	}
	else if ("addtooutfit" == action)
	{
		modifyOutfit(TRUE);
		return;
	}
	else if ("wearitems" == action)
	{
		modifyOutfit(TRUE, TRUE);
	}
	else if ("removefromoutfit" == action)
	{
		LLInventoryModel* model = getInventoryModel();
		if(!model) return;
		LLViewerInventoryCategory* cat = getCategory();
		if(!cat) return;
		
		remove_inventory_category_from_avatar ( cat );
		return;
	}	
	else if ("purge" == action)
	{		
		LLViewerInventoryCategory* cat;
		cat = (LLViewerInventoryCategory*)getCategory();

		if(cat)
		{
			model->purgeDescendentsOf(mUUID);
		}
		LLInventoryObject* obj = model->getObject(mUUID);
		if(!obj) return;
		obj->removeFromServer();
		model->deleteObject(mUUID);
		model->notifyObservers();
	}
	else if ("restore" == action)
	{
		restoreItem();
		return;
	}
}

void LLFolderBridge::openItem()
{
	lldebugs << "LLFolderBridge::openItem()" << llendl;
	LLInventoryModel* model = getInventoryModel();
	if(!model) return;
	if(mUUID.isNull()) return;
	bool fetching_inventory = model->fetchDescendentsOf(mUUID);
	// Only change folder type if we have the folder contents.
	if (!fetching_inventory)
	{
		// Disabling this for now, it's causing crash when new items are added to folders
		// since folder type may change before new item item has finished processing.
		// determineFolderType();
	}
}

BOOL LLFolderBridge::isItemRenameable() const
{
	return get_is_category_renameable(getInventoryModel(), mUUID);
}

void LLFolderBridge::restoreItem()
{
	LLViewerInventoryCategory* cat;
	cat = (LLViewerInventoryCategory*)getCategory();
	if(cat)
	{
		LLInventoryModel* model = getInventoryModel();
		const LLUUID new_parent = model->findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(cat->getType()));
		// do not restamp children on restore
		LLInvFVBridge::changeCategoryParent(model, cat, new_parent, FALSE);
	}
}

LLFolderType::EType LLFolderBridge::getPreferredType() const
{
	LLFolderType::EType preferred_type = LLFolderType::FT_NONE;
	LLViewerInventoryCategory* cat = getCategory();
	if(cat)
	{
		preferred_type = cat->getPreferredType();
	}

	return preferred_type;
}

// Icons for folders are based on the preferred type
LLUIImagePtr LLFolderBridge::getIcon() const
{
	LLFolderType::EType preferred_type = LLFolderType::FT_NONE;
	LLViewerInventoryCategory* cat = getCategory();
	if(cat)
	{
		preferred_type = cat->getPreferredType();
	}
	return getIcon(preferred_type);
}

// static
LLUIImagePtr LLFolderBridge::getIcon(LLFolderType::EType preferred_type)
{
	return LLUI::getUIImage(LLViewerFolderType::lookupIconName(preferred_type, FALSE));
		/*case LLAssetType::AT_MESH:
			control = "inv_folder_mesh.tga";
			break;*/
}

LLUIImagePtr LLFolderBridge::getOpenIcon() const
{
	return LLUI::getUIImage(LLViewerFolderType::lookupIconName(getPreferredType(), TRUE));

}

BOOL LLFolderBridge::renameItem(const std::string& new_name)
{
	rename_category(getInventoryModel(), mUUID, new_name);

	// return FALSE because we either notified observers (& therefore
	// rebuilt) or we didn't update.
	return FALSE;
}

BOOL LLFolderBridge::removeItem()
{
	if(!isItemRemovable())
	{
		return FALSE;
	}
	// move it to the trash
	LLPreview::hide(mUUID);
	LLInventoryModel* model = mInventoryPanel->getModel();
	if(!model) return FALSE;

	LLUUID trash_id;
	trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);

	// Look for any gestures and deactivate them
	LLInventoryModel::cat_array_t	descendent_categories;
	LLInventoryModel::item_array_t	descendent_items;
	gInventory.collectDescendents( mUUID, descendent_categories, descendent_items, FALSE );

	S32 i;
	for (i = 0; i < descendent_items.count(); i++)
	{
		LLInventoryItem* item = descendent_items[i];
		if (item->getType() == LLAssetType::AT_GESTURE
			&& LLGestureMgr::instance().isGestureActive(item->getUUID()))
		{
			LLGestureMgr::instance().deactivateGesture(item->getUUID());
		}
	}

	// go ahead and do the normal remove if no 'last calling
	// cards' are being removed.
	LLViewerInventoryCategory* cat = getCategory();
	if(cat)
	{
		// <edit> trash problem
		if(gInventory.isObjectDescendentOf(cat->getUUID(), gSystemFolderRoot))
		{
			S32 descendents = cat->getDescendentCount();
			if(descendents > 0)
			{
				LLInventoryModel::LLCategoryUpdate up(cat->getUUID(), -descendents);
				gInventory.accountForUpdate(up);
			}
			cat->setDescendentCount(0);
			LLInventoryModel::cat_array_t categories;
			LLInventoryModel::item_array_t items;
			gInventory.collectDescendents(cat->getUUID(),
							   categories,
							   items,
							   false); // include trash?
			S32 count = items.count();
			S32 i;
			for(i = 0; i < count; ++i)
			{
				gInventory.deleteObject(items.get(i)->getUUID());
			}
			count = categories.count();
			for(i = 0; i < count; ++i)
			{
				gInventory.deleteObject(categories.get(i)->getUUID());
			}

			LLInventoryModel::LLCategoryUpdate up(cat->getParentUUID(), -descendents);
			gInventory.deleteObject(cat->getUUID());
			gInventory.accountForUpdate(up);
			gInventory.notifyObservers();
		}
		else
		// </edit>
		LLInvFVBridge::changeCategoryParent(model, cat, trash_id, TRUE);
	}

	return TRUE;
}

BOOL LLFolderBridge::isClipboardPasteable() const
{
	if(LLInventoryClipboard::instance().hasContents() && isAgentInventory())
	{
		return TRUE;
	}
	return FALSE;
}

void LLFolderBridge::pasteFromClipboard()
{
	LLInventoryModel* model = getInventoryModel();
	if(model && isClipboardPasteable())
	{
		const LLUUID parent_id(mUUID);

		LLDynamicArray<LLUUID> objects;
		LLInventoryClipboard::instance().retrieve(objects);
		for (LLDynamicArray<LLUUID>::const_iterator iter = objects.begin();
			 iter != objects.end();
			 ++iter)
		{
			const LLUUID& item_id = (*iter);
			LLInventoryItem *item = model->getItem(item_id);
			if (item)
			{
				{
				copy_inventory_item(
					gAgent.getID(),
					item->getPermissions().getOwner(),
					item->getUUID(),
					parent_id,
					std::string(),
					LLPointer<LLInventoryCallback>(NULL));
			}
		}
	}
}
}

void LLFolderBridge::pasteLinkFromClipboard()
{
	LLInventoryModel* model = getInventoryModel();
	if(model)
	{
		const LLUUID parent_id(mUUID);

		LLDynamicArray<LLUUID> objects;
		LLInventoryClipboard::instance().retrieve(objects);
		for (LLDynamicArray<LLUUID>::const_iterator iter = objects.begin();
			 iter != objects.end();
			 ++iter)
		{
			const LLUUID &object_id = (*iter);
			if (LLInventoryItem *item = model->getItem(object_id))
			{
				link_inventory_item(
					gAgent.getID(),
					item->getLinkedUUID(),
					parent_id,
					item->getName(),
					item->getDescription(),
					LLAssetType::AT_LINK,
					LLPointer<LLInventoryCallback>(NULL));
			}
		}
	}
}

void LLFolderBridge::staticFolderOptionsMenu()
{
	LLFolderBridge* selfp = sSelf.get();
	if (selfp)
	{
		selfp->folderOptionsMenu();
	}
}

void LLFolderBridge::folderOptionsMenu()
{
	// *TODO: Translate
	
	LLInventoryModel* model = getInventoryModel();
	if(!model) return;

	const LLInventoryCategory* category = model->getCategory(mUUID);
	if(!category) return;
	
	const LLUUID trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	if (trash_id == mUUID) return;
	if (isItemInTrash()) return;
	if (!isAgentInventory()) return;

	LLFolderType::EType type = category->getPreferredType();
	const bool is_system_folder = LLFolderType::lookupIsProtectedType(type);
	// calling card related functionality for folders.

	// Only enable calling-card related options for non-default folders.
	if (!is_system_folder)
	{
		LLIsType is_callingcard(LLAssetType::AT_CALLINGCARD);
		if (mCallingCards || checkFolderForContentsOfType(model, is_callingcard))
		{
			mItems.push_back(std::string("Calling Card Separator"));
			mItems.push_back(std::string("Conference Chat Folder"));
			mItems.push_back(std::string("IM All Contacts In Folder"));
		}
	}

	if (!isItemRemovable())
	{
		mDisabledItems.push_back(std::string("Delete"));
	}

	// wearables related functionality for folders.
	//is_wearable
	LLFindWearables is_wearable;
	LLIsType is_object( LLAssetType::AT_OBJECT );
	LLIsType is_gesture( LLAssetType::AT_GESTURE );

	if (mWearables ||
		checkFolderForContentsOfType(model, is_wearable)  ||
		checkFolderForContentsOfType(model, is_object) ||
		checkFolderForContentsOfType(model, is_gesture) )
	{
		mItems.push_back(std::string("Folder Wearables Separator"));

		// Only enable add/replace outfit for non-default folders.
		if (!is_system_folder)
		{
			if (gHippoGridManager->getConnectedGrid()->supportsInvLinks() &&
			// Adding an outfit onto another (versus replacing) doesn't make sense.
				type != LLFolderType::FT_OUTFIT)
		{
				mItems.push_back(std::string("Add To Outfit"));
			}
			mItems.push_back(std::string("Wear Items"));
			mItems.push_back(std::string("Replace Outfit"));
		}
		mItems.push_back(std::string("Take Off Items"));
	}
	LLMenuGL* menup = dynamic_cast<LLMenuGL*>(mMenu.get());
	if (menup)
	{
		hide_context_entries(*menup, mItems, mDisabledItems);
	}
}

BOOL LLFolderBridge::checkFolderForContentsOfType(LLInventoryModel* model, LLInventoryCollectFunctor& is_type)
{
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	model->collectDescendentsIf(mUUID,
								cat_array,
								item_array,
								LLInventoryModel::EXCLUDE_TRASH,
								is_type);
	return ((item_array.count() > 0) ? TRUE : FALSE );
}

// Flags unused
void LLFolderBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	mItems.clear();
	mDisabledItems.clear();

	lldebugs << "LLFolderBridge::buildContextMenu()" << llendl;

//	std::vector<std::string> disabled_items;
	LLInventoryModel* model = getInventoryModel();
	if(!model) return;
	LLUUID cof_id = LLCOFMgr::instance().getCOF();
	const LLUUID trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH);
	const LLUUID lost_and_found_id = model->findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND);

// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
	// Fixes LL bug
	mItems.clear();
	mDisabledItems.clear();
// [/RLVa:KB]

	if (lost_and_found_id == mUUID)
	{
		// This is the lost+found folder.
		  mItems.push_back(std::string("Empty Lost And Found"));
	}
	
	// clear out old menu and folder pointers
	mMenu.markDead();
	sSelf.markDead();
	if (cof_id == mUUID)
	{
		mItems.push_back(std::string("Take Off Items"));
	}
	else if(trash_id == mUUID)
	{
		// This is the trash.
		mItems.push_back(std::string("Empty Trash"));
	}
	else if(isItemInTrash())
	{
		// This is a folder in the trash.
		mItems.clear(); // clear any items that used to exist
		addTrashContextMenuOptions(mItems, mDisabledItems);
	}
	else if(isAgentInventory()) // do not allow creating in library
	{
		//LLViewerInventoryCategory *cat =  getCategory();
		// BAP removed protected check to re-enable standard ops in untyped folders.
		// Not sure what the right thing is to do here.
		//if (!isCOFFolder() && cat && (cat->getPreferredType() != LLFolderType::FT_OUTFIT))
		{
		mItems.push_back(std::string("New Folder"));
		mItems.push_back(std::string("New Script"));
		mItems.push_back(std::string("New Note"));
		mItems.push_back(std::string("New Gesture"));
		mItems.push_back(std::string("New Clothes"));
		mItems.push_back(std::string("New Body Parts"));

		getClipboardEntries(false, mItems, mDisabledItems, flags);
		}
		/*else
		{
			// Want some but not all of the items from getClipboardEntries for outfits.
			if (cat && (cat->getPreferredType() == LLFolderType::FT_OUTFIT))
			{
				mItems.push_back(std::string("Rename"));

				addDeleteContextMenuOptions(mItems, mDisabledItems);
				// EXT-4030: disallow deletion of currently worn outfit
				const LLViewerInventoryItem *base_outfit_link = LLAppearanceMgr::instance().getBaseOutfitLink();
				if (base_outfit_link && (cat == base_outfit_link->getLinkedCategory()))
				{
					mDisabledItems.push_back(std::string("Delete"));
				}
			}
		}*/

		//Added by spatters to force inventory pull on right-click to display folder options correctly. 07-17-06
		mCallingCards = mWearables = FALSE;

		LLIsType is_callingcard(LLAssetType::AT_CALLINGCARD);
		if (checkFolderForContentsOfType(model, is_callingcard))
		{
			mCallingCards=TRUE;
		}
		
		LLFindWearables is_wearable;
		LLIsType is_object( LLAssetType::AT_OBJECT );
		LLIsType is_gesture( LLAssetType::AT_GESTURE );

		if (checkFolderForContentsOfType(model, is_wearable)  ||
			checkFolderForContentsOfType(model, is_object) ||
			checkFolderForContentsOfType(model, is_gesture) )
		{
			mWearables=TRUE;
		}
		

		uuid_vec_t folders;
		LLViewerInventoryCategory* category = (LLViewerInventoryCategory*)model->getCategory(mUUID);
		if (category)
		{
			folders.push_back(category->getUUID());
		}

		mMenu = menu.getHandle();
		sSelf = getHandle();
		LLRightClickInventoryFetchDescendentsObserver* fetch = new LLRightClickInventoryFetchDescendentsObserver(folders, FALSE);
		fetch->startFetch();
		inc_busy_count();
		if(fetch->isFinished())
		{
			// everything is already here - call done.
			fetch->done();
		}
		else
		{
			// it's all on it's way - add an observer, and the inventory
			// will call done for us when everything is here.
			gInventory.addObserver(fetch);
		}
	}
	else
	{
		mItems.push_back(std::string("--no options--"));
		mDisabledItems.push_back(std::string("--no options--"));

	}
	hide_context_entries(menu, mItems, mDisabledItems);
}

BOOL LLFolderBridge::hasChildren() const
{
	LLInventoryModel* model = getInventoryModel();
	if(!model) return FALSE;
	LLInventoryModel::EHasChildren has_children;
	has_children = gInventory.categoryHasChildren(mUUID);
	return has_children != LLInventoryModel::CHILDREN_NO;
}

BOOL LLFolderBridge::dragOrDrop(MASK mask, BOOL drop,
								EDragAndDropType cargo_type,
								void* cargo_data)
{
	LLInventoryItem* inv_item = (LLInventoryItem*)cargo_data;

	//llinfos << "LLFolderBridge::dragOrDrop()" << llendl;
	BOOL accept = FALSE;
	switch(cargo_type)
	{
	case DAD_TEXTURE:
	case DAD_SOUND:
	case DAD_CALLINGCARD:
	case DAD_LANDMARK:
	case DAD_SCRIPT:
	case DAD_OBJECT:
	case DAD_NOTECARD:
	case DAD_CLOTHING:
	case DAD_BODYPART:
	case DAD_ANIMATION:
	case DAD_GESTURE:
	case DAD_LINK:
		accept = dragItemIntoFolder(inv_item,
									drop);
		break;
	case DAD_CATEGORY:
		accept = dragCategoryIntoFolder((LLInventoryCategory*)inv_item,
										drop);
		break;
	default:
		break;
	}
	return accept;
}

LLViewerInventoryCategory* LLFolderBridge::getCategory() const
{
	LLViewerInventoryCategory* cat = NULL;
	LLInventoryModel* model = getInventoryModel();
	if(model)
	{
		cat = (LLViewerInventoryCategory*)model->getCategory(mUUID);
	}
	return cat;
}


// static
void LLFolderBridge::pasteClipboard(void* user_data)
{
	LLFolderBridge* self = (LLFolderBridge*)user_data;
	if(self) self->pasteFromClipboard();
}

void LLFolderBridge::createNewCategory(void* user_data)
{
	LLFolderBridge* bridge = (LLFolderBridge*)user_data;
	if(!bridge) return;
	LLInventoryPanel* panel = dynamic_cast<LLInventoryPanel*>(bridge->mInventoryPanel);
	if (!panel) return;
	LLInventoryModel* model = panel->getModel();
	if(!model) return;
	LLUUID id;
	id = model->createNewCategory(bridge->getUUID(),
								  LLFolderType::FT_NONE,
								  LLStringUtil::null);
	model->notifyObservers();

	// At this point, the bridge has probably been deleted, but the
	// view is still there.
	panel->setSelection(id, TAKE_FOCUS_YES);
}

void LLFolderBridge::createNewShirt(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_SHIRT);
}

void LLFolderBridge::createNewPants(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_PANTS);
}

void LLFolderBridge::createNewShoes(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_SHOES);
}

void LLFolderBridge::createNewSocks(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_SOCKS);
}

void LLFolderBridge::createNewJacket(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_JACKET);
}

void LLFolderBridge::createNewSkirt(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_SKIRT);
}

void LLFolderBridge::createNewGloves(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_GLOVES);
}

void LLFolderBridge::createNewUndershirt(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_UNDERSHIRT);
}

void LLFolderBridge::createNewUnderpants(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_UNDERPANTS);
}

void LLFolderBridge::createNewAlpha(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_ALPHA);
}

void LLFolderBridge::createNewTattoo(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_TATTOO);
}

void LLFolderBridge::createNewShape(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_SHAPE);
}

void LLFolderBridge::createNewSkin(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_SKIN);
}

void LLFolderBridge::createNewHair(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_HAIR);
}

void LLFolderBridge::createNewEyes(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_EYES);
}

void LLFolderBridge::createNewPhysics(void* user_data)
{
	LLFolderBridge::createWearable((LLFolderBridge*)user_data, LLWearableType::WT_PHYSICS);
}
// static
void LLFolderBridge::createWearable(LLFolderBridge* bridge, LLWearableType::EType type)
{
	if(!bridge) return;
	LLUUID parent_id = bridge->getUUID();
	createWearable(parent_id, type);
}

// Separate function so can be called by global menu as well as right-click
// menu.
// static
void LLFolderBridge::createWearable(LLUUID parent_id, LLWearableType::EType type)
{
	LLWearable* wearable = LLWearableList::instance().createNewWearable(type);
	LLAssetType::EType asset_type = wearable->getAssetType();
	LLInventoryType::EType inv_type = LLInventoryType::IT_WEARABLE;
	create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
		parent_id, wearable->getTransactionID(), wearable->getName(),
		wearable->getDescription(), asset_type, inv_type, wearable->getType(),
		wearable->getPermissions().getMaskNextOwner(),
		LLPointer<LLInventoryCallback>(NULL));
}

void LLFolderBridge::modifyOutfit(BOOL append, BOOL replace)
{
	LLInventoryModel* model = getInventoryModel();
	if(!model) return;
	LLViewerInventoryCategory* cat = getCategory();
	if(!cat) return;
	
	wear_inventory_category_on_avatar(cat, append, replace);
}

// helper stuff
bool move_task_inventory_callback(const LLSD& notification, const LLSD& response, LLMoveInv* move_inv)
{
	LLFloaterOpenObject::LLCatAndWear* cat_and_wear = (LLFloaterOpenObject::LLCatAndWear* )move_inv->mUserData;
	LLViewerObject* object = gObjectList.findObject(move_inv->mObjectID);
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if(option == 0 && object)
	{
		if (cat_and_wear && cat_and_wear->mWear)
		{
			LLInventoryObject::object_list_t inventory_objects;
			object->getInventoryContents(inventory_objects);
			int contents_count = inventory_objects.size()-1; //subtract one for containing folder

			LLInventoryCopyAndWearObserver* inventoryObserver = new LLInventoryCopyAndWearObserver(cat_and_wear->mCatID, contents_count, cat_and_wear->mFolderResponded);
			gInventory.addObserver(inventoryObserver);
		}

		two_uuids_list_t::iterator move_it;
		for (move_it = move_inv->mMoveList.begin(); 
			move_it != move_inv->mMoveList.end(); 
			++move_it)
		{
			object->moveInventory(move_it->first, move_it->second);
		}

		// update the UI.
		dialog_refresh_all();
	}

	if (move_inv->mCallback)
	{
		move_inv->mCallback(option, move_inv->mUserData);
	}

	delete move_inv;
	return false;
}

BOOL LLFolderBridge::dragItemIntoFolder(LLInventoryItem* inv_item,
										BOOL drop)
{
	LLInventoryModel* model = getInventoryModel();

	if(!model || !inv_item) return FALSE;
	if(!isAgentInventory()) return FALSE; // cannot drag into library
	if (!isAgentAvatarValid()) return FALSE;

	const LLUUID &current_outfit_id = model->findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT, false);
	const BOOL move_is_into_current_outfit = (mUUID == current_outfit_id);

	LLToolDragAndDrop::ESource source = LLToolDragAndDrop::getInstance()->getSource();
	BOOL accept = FALSE;
	LLViewerObject* object = NULL;
	if(LLToolDragAndDrop::SOURCE_AGENT == source)
	{
		const LLUUID &trash_id = model->findCategoryUUIDForType(LLFolderType::FT_TRASH, false);

		const BOOL move_is_into_trash = (mUUID == trash_id) || model->isObjectDescendentOf(mUUID, trash_id);
		const BOOL move_is_outof_current_outfit = model->isObjectDescendentOf(inv_item->getUUID(), current_outfit_id);
		//--------------------------------------------------------------------------------
		// Determine if item can be moved.
		//

		BOOL is_movable = TRUE;

		switch (inv_item->getActualType())
		{
		//case LLFolderType::FT_ROOT_CATEGORY:
		//	is_movable = FALSE;
		//	break;

		case LLAssetType::AT_CATEGORY:
			is_movable = !LLFolderType::lookupIsProtectedType(((LLInventoryCategory*)inv_item)->getPreferredType());
			break;
		default:
			break;
		}

		if (is_movable && move_is_outof_current_outfit)
		{
			is_movable = FALSE;	// Don't allow dragging links out of COF
		}
		else if(is_movable && move_is_into_trash)
		{
			is_movable &= inv_item->getIsLinkType() || !get_is_item_worn(inv_item->getUUID());
		}
 
		accept = is_movable && (mUUID != inv_item->getParentUUID());
		if(accept && drop)
		{
			if (inv_item->getType() == LLAssetType::AT_GESTURE
				&& LLGestureMgr::instance().isGestureActive(inv_item->getUUID()) && move_is_into_trash)
			{
				LLGestureMgr::instance().deactivateGesture(inv_item->getUUID());
			}
			// If an item is being dragged between windows, unselect
			// everything in the active window so that we don't follow
			// the selection to its new location (which is very
			// annoying).
			if (LLInventoryView::getActiveInventory())
			{
				LLInventoryPanel* active_panel = LLInventoryView::getActiveInventory()->getPanel();
				if (active_panel && (mInventoryPanel != active_panel))
				{
					active_panel->unSelectAll();
				}
			}

			if (move_is_into_current_outfit)
			{
				switch (inv_item->getType())
				{
					case LLAssetType::AT_BODYPART:
					case LLAssetType::AT_CLOTHING:
						wear_inventory_item_on_avatar(inv_item);
						break;
					case LLAssetType::AT_OBJECT:
						rez_attachment((LLViewerInventoryItem*)inv_item, NULL, false);
						break;
					/*
					case LLAssetType::AT_GESTURE:
						break;
					*/
					default:
						break;
				}
			}
			else
			{
				// restamp if the move is into the trash.
				LLInvFVBridge::changeItemParent(
					model,
					(LLViewerInventoryItem*)inv_item,
					mUUID,
					move_is_into_trash);
			}

			// 
			//--------------------------------------------------------------------------------

		}
	}
	else if(LLToolDragAndDrop::SOURCE_WORLD == source)
	{
		// Make sure the object exists. If we allowed dragging from
		// anonymous objects, it would be possible to bypass
		// permissions.
		object = gObjectList.findObject(inv_item->getParentUUID());
		if(!object)
		{
			llinfos << "Object not found for drop." << llendl;
			return FALSE;
		}

		// coming from a task. Need to figure out if the person can
		// move/copy this item.
		LLPermissions perm(inv_item->getPermissions());
		BOOL is_move = FALSE;
		if((perm.allowCopyBy(gAgent.getID(), gAgent.getGroupID())
			&& perm.allowTransferTo(gAgent.getID())))
//		   || gAgent.isGodlike())
			
		{
			accept = TRUE;
		}
		else if(object->permYouOwner())
		{
			// If the object cannot be copied, but the object the
			// inventory is owned by the agent, then the item can be
			// moved from the task to agent inventory.
			is_move = TRUE;
			accept = TRUE;
		}
		if(drop && accept)
		{
			LLMoveInv* move_inv = new LLMoveInv;
			move_inv->mObjectID = inv_item->getParentUUID();
			two_uuids_t item_pair(mUUID, inv_item->getUUID());
			move_inv->mMoveList.push_back(item_pair);
			move_inv->mCallback = NULL;
			move_inv->mUserData = NULL;
			if(is_move)
			{
				warn_move_inventory(object, move_inv);
			}
			else
			{
				LLNotification::Params params("MoveInventoryFromObject");
				params.functor(boost::bind(move_task_inventory_callback, _1, _2, move_inv));
				LLNotifications::instance().forceResponse(params, 0);
			}
		}
		
	}
	else if(LLToolDragAndDrop::SOURCE_NOTECARD == source)
	{
		accept = TRUE;
		if(drop)
		{
			copy_inventory_from_notecard(LLToolDragAndDrop::getInstance()->getObjectID(),
				LLToolDragAndDrop::getInstance()->getSourceID(), inv_item);
		}
	}
	else if(LLToolDragAndDrop::SOURCE_LIBRARY == source)
	{
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)inv_item;
		if(item && item->isComplete())
		{
			accept = TRUE;
			if(drop)
			{
				copy_inventory_item(
					gAgent.getID(),
					inv_item->getPermissions().getOwner(),
					inv_item->getUUID(),
					mUUID,
					std::string(),
					LLPointer<LLInventoryCallback>(NULL));
			}
		}
	}
	else
	{
		llwarns << "unhandled drag source" << llendl;
	}
	return accept;
}

// +=================================================+
// |        LLTextureBridge                          |
// +=================================================+

LLUIImagePtr LLTextureBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_TEXTURE, mInvType);
}
	
void open_texture(const LLUUID& item_id, 
				   const std::string& title,
				   BOOL show_keep_discard,
				   const LLUUID& source_id,
				   BOOL take_focus)
{
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
	if (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE))
	{
		RlvNotifications::notifyBlockedViewTexture();
		return;
	}
// [/RLVa:KB]

	// See if we can bring an exiting preview to the front
	if( !LLPreview::show( item_id, take_focus ) )
	{
		// There isn't one, so make a new preview
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
		rect.translate( left - rect.mLeft, top - rect.mTop );

		LLPreviewTexture* preview;
		preview = new LLPreviewTexture("preview texture",
										  rect,
										  title,
										  item_id,
										  LLUUID::null,
										  show_keep_discard);
		preview->setSourceID(source_id);
		if(take_focus) preview->setFocus(TRUE);

		gFloaterView->adjustToFitScreen(preview, FALSE);
	}
}

void LLTextureBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if(item)
	{
		open_texture(mUUID, getPrefix() + item->getName(), FALSE);
	}
}

// +=================================================+
// |        LLSoundBridge                            |
// +=================================================+

void LLSoundBridge::openItem()
{
// Changed this back to the way it USED to work:
// only open the preview dialog through the contextual right-click menu
// double-click just plays the sound

	LLViewerInventoryItem* item = getItem();
	if(item)
	{
		openSoundPreview((void*)this);
		//send_uuid_sound_trigger(item->getAssetUUID(), 1.0);
	}

//	if(!LLPreview::show(mUUID))
//	{
//		S32 left, top;
//		gFloaterView->getNewFloaterPosition(&left, &top);
//		LLRect rect = gSavedSettings.getRect("PreviewSoundRect");
//		rect.translate(left - rect.mLeft, top - rect.mTop);
//			new LLPreviewSound("preview sound",
//							   rect,
//							   getPrefix() + getName(),
//							   mUUID));
//	}
}

void LLSoundBridge::previewItem()
{
	LLViewerInventoryItem* item = getItem();
	if(item)
	{
		send_sound_trigger(item->getAssetUUID(), 1.0);
	}
}

void LLSoundBridge::openSoundPreview(void* which)
{
	LLSoundBridge *me = (LLSoundBridge *)which;
	if(!LLPreview::show(me->mUUID))
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewSoundRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewSound* preview = new LLPreviewSound("preview sound",
										   rect,
										   me->getPrefix() + me->getName(),
										   me->mUUID);
		preview->setFocus(TRUE);
		// Keep entirely onscreen.
		gFloaterView->adjustToFitScreen(preview, FALSE);
	}
}

void LLSoundBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	lldebugs << "LLTextureBridge::buildContextMenu()" << llendl;
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	// *TODO: Translate
	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{
		items.push_back(std::string("Sound Open"));
		items.push_back(std::string("Properties"));

		getClipboardEntries(true, items, disabled_items, flags);
	}

	items.push_back(std::string("Sound Separator"));
	items.push_back(std::string("Sound Play"));

	hide_context_entries(menu, items, disabled_items);
}

// +=================================================+
// |        LLLandmarkBridge                         |
// +=================================================+

LLLandmarkBridge::LLLandmarkBridge(LLInventoryPanel* inventory, 
								   //LLFolderView* root,
								   const LLUUID& uuid, 
								   U32 flags/* = 0x00*/) :
	LLItemBridge(inventory, /*root,*/ uuid)
{
	mVisited = FALSE;
	if (flags & LLInventoryItemFlags::II_FLAGS_LANDMARK_VISITED)
	{
		mVisited = TRUE;
	}
}

LLUIImagePtr LLLandmarkBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_LANDMARK, LLInventoryType::IT_LANDMARK, mVisited, FALSE);
}

void LLLandmarkBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;

	// *TODO: Translate
	lldebugs << "LLLandmarkBridge::buildContextMenu()" << llendl;
	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{
		items.push_back(std::string("Landmark Open"));
		items.push_back(std::string("Properties"));

		getClipboardEntries(true, items, disabled_items, flags);
	}

	items.push_back(std::string("Landmark Separator"));
	items.push_back(std::string("Teleport To Landmark"));

	hide_context_entries(menu, items, disabled_items);

}

// virtual
void LLLandmarkBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if ("teleport" == action)
	{
		LLViewerInventoryItem* item = getItem();
		if(item)
		{
			gAgent.teleportViaLandmark(item->getAssetUUID());

			// we now automatically track the landmark you're teleporting to
			// because you'll probably arrive at a telehub instead
			if( gFloaterWorldMap )
			{
				//Emerald claims this needs to be the item UUID, rather than the asset UUID. -HgB
				gFloaterWorldMap->trackLandmark( item->getUUID() );
			}
		}
	}
	if ("about" == action)
	{
		LLViewerInventoryItem* item = getItem();
		if(item)
		{
			open_landmark(item, std::string("  ") + getPrefix() + item->getName(), FALSE);
		}
	}
	else LLItemBridge::performAction(folder, model, action);
}

void open_landmark(LLViewerInventoryItem* inv_item,
				   const std::string& title,
				   BOOL show_keep_discard,
				   const LLUUID& source_id,
				   BOOL take_focus)
{
	// See if we can bring an exiting preview to the front
	if( !LLPreview::show( inv_item->getUUID(), take_focus ) )
	{
		// There isn't one, so make a new preview
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewLandmarkRect");
		rect.translate( left - rect.mLeft, top - rect.mTop );

		LLPreviewLandmark* preview = new LLPreviewLandmark(title,
								  rect,
								  title,
								  inv_item->getUUID(),
								  show_keep_discard,
								  inv_item);
		preview->setSourceID(source_id);
		if(take_focus) preview->setFocus(TRUE);
		// keep onscreen
		gFloaterView->adjustToFitScreen(preview, FALSE);
	}
}

static bool open_landmark_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	LLUUID asset_id = notification["payload"]["asset_id"].asUUID();
	LLUUID item_id = notification["payload"]["item_id"].asUUID();
	if (option == 0)
	{
		// HACK: This is to demonstrate teleport on double click for landmarks
		gAgent.teleportViaLandmark( asset_id );

		// we now automatically track the landmark you're teleporting to
		// because you'll probably arrive at a telehub instead
		if( gFloaterWorldMap )
		{
			//Emerald says this needs to be the item id instead of the asset ID. -HgB
			gFloaterWorldMap->trackLandmark( item_id );
		}
	}

	return false;
}
static LLNotificationFunctorRegistration open_landmark_callback_reg("TeleportFromLandmark", open_landmark_callback);


void LLLandmarkBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if( item )
	{
		// Opening (double-clicking) a landmark immediately teleports,
		// but warns you the first time.
		// open_landmark(item, std::string("  ") + getPrefix() + item->getName(), FALSE);
		LLSD payload;
		payload["asset_id"] = item->getAssetUUID();
		payload["item_id"] = item->getUUID();
		LLNotificationsUtil::add("TeleportFromLandmark", LLSD(), payload);
	}
}


// +=================================================+
// |        LLCallingCardObserver                    |
// +=================================================+
class LLCallingCardObserver : public LLFriendObserver
{
public:
	LLCallingCardObserver(LLCallingCardBridge* bridge) : mBridgep(bridge) {}
	virtual ~LLCallingCardObserver() { mBridgep = NULL; }
	virtual void changed(U32 mask)
{
	mBridgep->refreshFolderViewItem();
}
protected:
	LLCallingCardBridge* mBridgep;
};

// +=================================================+
// |        LLCallingCardBridge                      |
// +=================================================+

LLCallingCardBridge::LLCallingCardBridge(LLInventoryPanel* inventory, 
										 //LLFolderView* root,
										 const LLUUID& uuid ) :
	LLItemBridge(inventory, /*root,*/ uuid)
{
	mObserver = new LLCallingCardObserver(this);
	LLAvatarTracker::instance().addObserver(mObserver);
}

LLCallingCardBridge::~LLCallingCardBridge()
{
	LLAvatarTracker::instance().removeObserver(mObserver);
	delete mObserver;
}

void LLCallingCardBridge::refreshFolderViewItem()
{
	LLFolderViewItem* itemp = mInventoryPanel ? mInventoryPanel->getRootFolder()->getItemByID(mUUID) : NULL;
	if (itemp)
	{
		itemp->refresh();
	}
}

// virtual
void LLCallingCardBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if ("begin_im" == action)
	{
		LLViewerInventoryItem *item = getItem();
		if (item && (item->getCreatorUUID() != gAgent.getID()) &&
			(!item->getCreatorUUID().isNull()))
		{
			gIMMgr->setFloaterOpen(TRUE);
			gIMMgr->addSession(item->getName(), IM_NOTHING_SPECIAL, item->getCreatorUUID());
		}
	}
	else if ("lure" == action)
	{
		LLViewerInventoryItem *item = getItem();
		if (item && (item->getCreatorUUID() != gAgent.getID()) &&
			(!item->getCreatorUUID().isNull()))
		{
			handle_lure(item->getCreatorUUID());
		}
	}
	else LLItemBridge::performAction(folder, model, action);
}

LLUIImagePtr LLCallingCardBridge::getIcon() const
{
	BOOL online = FALSE;
	LLViewerInventoryItem* item = getItem();
	if(item)
	{
		online = LLAvatarTracker::instance().isBuddyOnline(item->getCreatorUUID());
	}
	return LLInventoryIcon::getIcon(LLAssetType::AT_CALLINGCARD, LLInventoryType::IT_CALLINGCARD, online, FALSE);
}

std::string LLCallingCardBridge::getLabelSuffix() const
{
	LLViewerInventoryItem* item = getItem();
	if( item && LLAvatarTracker::instance().isBuddyOnline(item->getCreatorUUID()) )
	{
		return LLItemBridge::getLabelSuffix() + " (online)";
	}
	else
	{
		return LLItemBridge::getLabelSuffix();
	}
}

void LLCallingCardBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if(item && !item->getCreatorUUID().isNull())
	{
		BOOL online;
		online = LLAvatarTracker::instance().isBuddyOnline(item->getCreatorUUID());
		LLFloaterAvatarInfo::showFromFriend(item->getCreatorUUID(), online);
	}
}

void LLCallingCardBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	// *TODO: Translate
	lldebugs << "LLCallingCardBridge::buildContextMenu()" << llendl;
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;

	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{
		addOpenRightClickMenuOption(items);
		items.push_back(std::string("Properties"));

		getClipboardEntries(true, items, disabled_items, flags);

		LLInventoryItem* item = getItem();
		BOOL good_card = (item
						  && (LLUUID::null != item->getCreatorUUID())
						  && (item->getCreatorUUID() != gAgent.getID()));
		BOOL user_online = FALSE;
		if (item)
		{
			user_online = (LLAvatarTracker::instance().isBuddyOnline(item->getCreatorUUID()));
		}
		items.push_back(std::string("Send Instant Message Separator"));
		items.push_back(std::string("Send Instant Message"));
		items.push_back(std::string("Offer Teleport..."));
		items.push_back(std::string("Conference Chat"));

		if (!good_card)
		{
			disabled_items.push_back(std::string("Send Instant Message"));
		}
		if (!good_card || !user_online)
		{
			disabled_items.push_back(std::string("Offer Teleport..."));
			disabled_items.push_back(std::string("Conference Chat"));
		}
	}
	hide_context_entries(menu, items, disabled_items);
}

BOOL LLCallingCardBridge::dragOrDrop(MASK mask, BOOL drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data)
{
	LLViewerInventoryItem* item = getItem();
	BOOL rv = FALSE;
	if(item)
	{
		// check the type
		switch(cargo_type)
		{
		case DAD_TEXTURE:
		case DAD_SOUND:
		case DAD_LANDMARK:
		case DAD_SCRIPT:
		case DAD_CLOTHING:
		case DAD_OBJECT:
		case DAD_NOTECARD:
		case DAD_BODYPART:
		case DAD_ANIMATION:
		case DAD_GESTURE:
			{
				LLInventoryItem* inv_item = (LLInventoryItem*)cargo_data;
				const LLPermissions& perm = inv_item->getPermissions();
				if(gInventory.getItem(inv_item->getUUID())
				   && perm.allowOperationBy(PERM_TRANSFER, gAgent.getID()))
				{
					rv = TRUE;
					if(drop)
					{
						LLGiveInventory::doGiveInventoryItem(item->getCreatorUUID(),
														 (LLInventoryItem*)cargo_data);
					}
				}
				else
				{
					// It's not in the user's inventory (it's probably in
					// an object's contents), so disallow dragging it here.
					// You can't give something you don't yet have.
					rv = FALSE;
				}
				break;
			}
		case DAD_CATEGORY:
			{
				LLInventoryCategory* inv_cat = (LLInventoryCategory*)cargo_data;
				if( gInventory.getCategory( inv_cat->getUUID() ) )
				{
					rv = TRUE;
					if(drop)
					{
						LLGiveInventory::doGiveInventoryCategory(
							item->getCreatorUUID(),
							inv_cat);
					}
				}
				else
				{
					// It's not in the user's inventory (it's probably in
					// an object's contents), so disallow dragging it here.
					// You can't give something you don't yet have.
					rv = FALSE;
				}
				break;
			}
		default:
			break;
		}
	}
	return rv;
}

// +=================================================+
// |        LLNotecardBridge                         |
// +=================================================+

void open_notecard(LLViewerInventoryItem* inv_item,
				   const std::string& title,
				   const LLUUID& object_id,
				   BOOL show_keep_discard,
				   const LLUUID& source_id,
				   BOOL take_focus)
{
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
	if (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE))
	{
		RlvNotifications::notifyBlockedViewNote();
		return;
	}
// [/RLVa:KB]

	// See if we can bring an existing preview to the front
	if(!LLPreview::show(inv_item->getUUID(), take_focus))
	{
		// There isn't one, so make a new preview
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("NotecardEditorRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewNotecard* preview;
		preview = new LLPreviewNotecard("preview notecard", rect, title,
						inv_item->getUUID(), object_id, inv_item->getAssetUUID(),
						show_keep_discard, inv_item);
		preview->setSourceID(source_id);
		if(take_focus) preview->setFocus(TRUE);
		// Force to be entirely onscreen.
		gFloaterView->adjustToFitScreen(preview, FALSE);

		//if (source_id.notNull())
		//{
		//	// look for existing tabbed view for content from same source
		//	LLPreview* existing_preview = LLPreview::getPreviewForSource(source_id);
		//	if (existing_preview)
		//	{
		//		// found existing preview from this source
		//		// is it already hosted in a multi-preview window?
		//		LLMultiPreview* preview_hostp = (LLMultiPreview*)existing_preview->getHost();
		//		if (!preview_hostp)
		//		{
		//			// create new multipreview if it doesn't exist
		//			LLMultiPreview* preview_hostp = new LLMultiPreview(existing_preview->getRect());
		//			preview_hostp->addFloater(existing_preview);
		//		}
		//		// add this preview to existing host
		//		preview_hostp->addFloater(preview);
		//	}
		//}
	}
}


void LLNotecardBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if (item)
	{
		open_notecard(item, getPrefix() + item->getName(), LLUUID::null, FALSE);
	}
}


// +=================================================+
// |        LLGestureBridge                          |
// +=================================================+

LLFontGL::StyleFlags LLGestureBridge::getLabelStyle() const
{
	U8 font = LLFontGL::NORMAL;

	if (LLGestureMgr::instance().isGestureActive(mUUID))
	{
		font |= LLFontGL::BOLD;
	}

	const LLViewerInventoryItem* item = getItem();
	if (item && item->getIsLinkType())
	{
		font |= LLFontGL::ITALIC;
	}

	return (LLFontGL::StyleFlags)font;
}

std::string LLGestureBridge::getLabelSuffix() const
{
	if( LLGestureMgr::instance().isGestureActive(mUUID) )
	{
		return LLItemBridge::getLabelSuffix() + " (active)";
	}
	else
	{
		return LLItemBridge::getLabelSuffix();
	}
}

// virtual
void LLGestureBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if ("activate" == action)
	{
		LLGestureMgr::instance().activateGesture(mUUID);

		LLViewerInventoryItem* item = gInventory.getItem(mUUID);
		if (!item) return;

		// Since we just changed the suffix to indicate (active)
		// the server doesn't need to know, just the viewer.
		gInventory.updateItem(item);
		gInventory.notifyObservers();
	}
	else if ("deactivate" == action)
	{
		LLGestureMgr::instance().deactivateGesture(mUUID);

		LLViewerInventoryItem* item = gInventory.getItem(mUUID);
		if (!item) return;

		// Since we just changed the suffix to indicate (active)
		// the server doesn't need to know, just the viewer.
		gInventory.updateItem(item);
		gInventory.notifyObservers();
	}
	else LLItemBridge::performAction(folder, model, action);
}

void LLGestureBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	if (!item) return;

	// See if we can bring an existing preview to the front
	if(!LLPreview::show(mUUID))
	{
		LLUUID item_id = mUUID;
		std::string title = getPrefix() + item->getName();
		LLUUID object_id = LLUUID::null;

		// TODO: save the rectangle
		LLPreviewGesture* preview = LLPreviewGesture::show(title, item_id, object_id);
		preview->setFocus(TRUE);

		// Force to be entirely onscreen.
		gFloaterView->adjustToFitScreen(preview, FALSE);
	}
}

BOOL LLGestureBridge::removeItem()
{
	// Grab class information locally since *this may be deleted
	// within this function.  Not a great pattern...
	const LLInventoryModel* model = getInventoryModel();
	if(!model)
	{
		return FALSE;
	}
	const LLUUID item_id = mUUID;
	
	// Force close the preview window, if it exists
	LLGestureMgr::instance().deactivateGesture(item_id);
	
	// If deactivateGesture deleted *this, then return out immediately.
	if (!model->getObject(item_id))
	{
		return TRUE;
	}

	return LLItemBridge::removeItem();
}

void LLGestureBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	// *TODO: Translate
	lldebugs << "LLGestureBridge::buildContextMenu()" << llendl;
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;
	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{

		addOpenRightClickMenuOption(items);
		items.push_back(std::string("Properties"));

		getClipboardEntries(true, items, disabled_items, flags);

		items.push_back(std::string("Gesture Separator"));
		if (LLGestureMgr::instance().isGestureActive(getUUID()))
		{
		items.push_back(std::string("Deactivate"));
	}
		else
		{
			items.push_back(std::string("Activate"));
		}
	}
	hide_context_entries(menu, items, disabled_items);
}

// +=================================================+
// |        LLAnimationBridge                        |
// +=================================================+

void LLAnimationBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	// *TODO: Translate
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;

	lldebugs << "LLAnimationBridge::buildContextMenu()" << llendl;
	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{
		items.push_back(std::string("Animation Open"));
		items.push_back(std::string("Properties"));

		getClipboardEntries(true, items, disabled_items, flags);
	}

	items.push_back(std::string("Animation Separator"));
	items.push_back(std::string("Animation Play"));
	items.push_back(std::string("Animation Audition"));

	hide_context_entries(menu, items, disabled_items);

}

// virtual
void LLAnimationBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if ((action == "playworld") || (action == "playlocal"))
	{
		// See if we can bring an existing preview to the front
		if(LLPreview::show( mUUID ))
			return;
	
		LLViewerInventoryItem *item = getItem();
		if( item )
		{
			S32 activate = 0;
		if ("playworld" == action) activate = 1;
		if ("playlocal" == action) activate = 2;

				S32 left, top;
				gFloaterView->getNewFloaterPosition(&left, &top);
				LLRect rect = gSavedSettings.getRect("PreviewAnimRect");
				rect.translate( left - rect.mLeft, top - rect.mTop );
				LLPreviewAnim* preview = new LLPreviewAnim("preview anim",
										rect,
										getPrefix() + item->getName(),
										mUUID,
										activate);
				// Force to be entirely onscreen.
				gFloaterView->adjustToFitScreen(preview, FALSE);
			}
		}
	else
	{
		LLItemBridge::performAction(folder, model, action);
	}
}

void LLAnimationBridge::openItem()
{
	// See if we can bring an existing preview to the front
	if(LLPreview::show( mUUID ))
		return;
			
		LLViewerInventoryItem* item = getItem();
		if( item )
		{
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewAnimRect");
			rect.translate( left - rect.mLeft, top - rect.mTop );
			LLPreviewAnim* preview = new LLPreviewAnim("preview anim",
									rect,
									getPrefix() + item->getName(),
									mUUID,
									0);
			preview->setFocus(TRUE);
			// Force to be entirely onscreen.
			gFloaterView->adjustToFitScreen(preview, FALSE);
		}
	}

// +=================================================+
// |        LLObjectBridge                           |
// +=================================================+

// static
LLUUID LLObjectBridge::sContextMenuItemID;

LLObjectBridge::LLObjectBridge(LLInventoryPanel* inventory, 
							   const LLUUID& uuid, 
							   LLInventoryType::EType type, 
							   U32 flags) :
	LLItemBridge(inventory, uuid)
{
	mAttachPt = (flags & 0xff); // low bye of inventory flags
	mIsMultiObject = ( flags & LLInventoryItemFlags::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS ) ?  TRUE: FALSE;
	mInvType = type;
}

BOOL LLObjectBridge::isItemRemovable()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return FALSE;
	}
	const LLInventoryObject *obj = model->getItem(mUUID);
	if (obj && obj->getIsLinkType())
	{
		return TRUE;
	}
	// <edit>
	//LLVOAvatar* avatar = gAgentAvatarp;
	//if(!avatar) return FALSE;
	//if(avatar->isWearingAttachment(mUUID)) return FALSE;
	// </edit>
	return LLInvFVBridge::isItemRemovable();
}

LLUIImagePtr LLObjectBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_OBJECT, mInvType, mAttachPt, mIsMultiObject );
}

LLInventoryObject* LLObjectBridge::getObject() const
{
	LLInventoryObject* object = NULL;
	LLInventoryModel* model = getInventoryModel();
	if(model)
	{
		object = (LLInventoryObject*)model->getObject(mUUID);
	}
	return object;
}

// virtual
void LLObjectBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if ("attach" == action || "wear_add" == action)
	{
		bool replace = ("attach" == action); // Replace if "Wear"ing.
		LLUUID object_id = mUUID;
		LLViewerInventoryItem* item;
		item = (LLViewerInventoryItem*)gInventory.getItem(object_id);
		if(item && gInventory.isObjectDescendentOf(object_id, gInventory.getRootFolderID()))
		{
			rez_attachment(item, NULL, replace);
		}
		else if(item && item->isComplete())
		{
			// must be in library. copy it to our inventory and put it on.
			LLPointer<LLInventoryCallback> cb = new RezAttachmentCallback(0, replace);
			copy_inventory_item(
				gAgent.getID(),
				item->getPermissions().getOwner(),
				item->getUUID(),
				LLUUID::null,
				std::string(),
				cb);
		}
		gFocusMgr.setKeyboardFocus(NULL);
	}
	else if ("detach" == action)
	{
		LLInventoryItem* item = gInventory.getItem(mUUID);
		if(item)
		{
			LLVOAvatar::detachAttachmentIntoInventory(item->getLinkedUUID());
		}
	}
	else if ("edit" == action)
	{
		if (gRlvHandler.hasBehaviour(RLV_BHVR_EDIT))
			return;
		LLVOAvatar* avatarp = gAgentAvatarp;
		if (!avatarp)
			return;
		LLViewerObject* objectp = avatarp->getWornAttachment(mUUID);
		if (!objectp)
			return;

		// [Selective copy/paste from LLObjectEdit::handleEvent()]
		LLViewerParcelMgr::getInstance()->deselectLand();
		LLSelectMgr::getInstance()->deselectAll();

		if (gAgentCamera.getFocusOnAvatar() && !LLToolMgr::getInstance()->inEdit())
		{
			if (objectp->isHUDAttachment() || !gSavedSettings.getBOOL("EditCameraMovement"))
			{
				// always freeze camera in space, even if camera doesn't move
				// so, for example, follow cam scripts can't affect you when in build mode
				gAgentCamera.setFocusGlobal(gAgentCamera.calcFocusPositionTargetGlobal(), LLUUID::null);
				gAgentCamera.setFocusOnAvatar(FALSE, ANIMATE);
			}
			else
			{
				gAgentCamera.setFocusOnAvatar(FALSE, ANIMATE);

				// zoom in on object center instead of where we clicked, as we need to see the manipulator handles
				gAgentCamera.setFocusGlobal(objectp->getPositionGlobal(), objectp->getID());
				gAgentCamera.cameraZoomIn(0.666f);
				gAgentCamera.cameraOrbitOver( 30.f * DEG_TO_RAD );
				gViewerWindow->moveCursorToCenter();
			}
		}

		gFloaterTools->open();
	
		LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
		gFloaterTools->setEditTool( LLToolCompTranslate::getInstance() );

		LLViewerJoystick::getInstance()->moveObjects(true);
		LLViewerJoystick::getInstance()->setNeedsReset(true);

		LLSelectMgr::getInstance()->selectObjectAndFamily(objectp);

		// Could be first use
		LLFirstUse::useBuild();
	}
	else LLItemBridge::performAction(folder, model, action);
}

void LLObjectBridge::openItem()
{
	LLVOAvatar* avatar = gAgentAvatarp;
	if (!avatar)
	{
		return;
	}
	if (avatar->isWearingAttachment(mUUID))
	{
// [RLVa:KB]
		if ( !(rlv_handler_t::isEnabled()) || (gRlvAttachmentLocks.canDetach(getItem())))
			performAction(NULL, NULL, "detach");
// [/RLVa:KB]		
	}
	else
	{
		performAction(NULL, NULL, "attach");
	}
}

std::string LLObjectBridge::getLabelSuffix() const
{
	LLVOAvatar* avatar = gAgentAvatarp;
	if( avatar && avatar->isWearingAttachment( mUUID ) )
	{
		std::string attachment_point_name = avatar->getAttachedPointName(mUUID);
		LLStringUtil::toLower(attachment_point_name);
		LLViewerObject* pObj = (rlv_handler_t::isEnabled()) ? avatar->getWornAttachment( mUUID ) : NULL;
// [RLVa:KB]
		if ( pObj && (gRlvAttachmentLocks.isLockedAttachment(pObj) || 
					gRlvAttachmentLocks.isLockedAttachmentPoint(RlvAttachPtLookup::getAttachPointIndex(pObj),RLV_LOCK_REMOVE)))
		{
			return LLItemBridge::getLabelSuffix() + std::string(" (locked to ") + attachment_point_name + std::string(")");
		}
// [/RLVa:KB]
		return LLItemBridge::getLabelSuffix() + std::string(" (worn on ") + attachment_point_name + std::string(")");
	}
	else
	{
		// <edit> testzone attachpt
		if(avatar)
		{
			std::map<S32, std::pair<LLUUID,LLUUID> >::iterator iter = avatar->mUnsupportedAttachmentPoints.begin();
			std::map<S32, std::pair<LLUUID,LLUUID> >::iterator end = avatar->mUnsupportedAttachmentPoints.end();
			for( ; iter != end; ++iter)
				if((*iter).second.first == mUUID)
				{
// [RLVa:KB]
					LLViewerObject* pObj = (rlv_handler_t::isEnabled()) ? gObjectList.findObject((*iter).second.second) : NULL;
					if ( pObj && (gRlvAttachmentLocks.isLockedAttachment(pObj) || 
								gRlvAttachmentLocks.isLockedAttachmentPoint(RlvAttachPtLookup::getAttachPointIndex(pObj),RLV_LOCK_REMOVE)))
					{
						return LLItemBridge::getLabelSuffix() + std::string(" (locked to unsupported point %d)", (*iter).first);
					}
// [/RLVa:KB]
					return LLItemBridge::getLabelSuffix() + llformat(" (worn on unsupported point %d)", (*iter).first);
				}
		}
		// </edit>
		return LLItemBridge::getLabelSuffix();
	}
}

void rez_attachment(LLViewerInventoryItem* item, LLViewerJointAttachment* attachment, bool replace)
{
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.1a) | Added: RLVa-1.2.1a
	// If no attachment point was specified, try looking it up from the item name
	if ( (rlv_handler_t::isEnabled()) && (!attachment) && (gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_ANY)) )
	{
		attachment = RlvAttachPtLookup::getAttachPoint(item);
	}
// [/RLVa:KB]

	S32 attach_pt = 0;
	if (gAgentAvatarp && attachment)
	{
		for (LLVOAvatar::attachment_map_t::iterator iter = gAgentAvatarp->mAttachmentPoints.begin();
			 iter != gAgentAvatarp->mAttachmentPoints.end(); ++iter)
		{
			if (iter->second == attachment)
			{
				attach_pt = iter->first;
				break;
			}
		}
	}

	LLSD payload;
	payload["item_id"] = item->getLinkedUUID(); // Wear the base object in case this is a link.
	payload["attachment_point"] = attach_pt;
	payload["is_add"] = !replace;

	if (replace && attachment && attachment->getNumObjects() > 0)
	{
// [RLVa:KB] - Checked: 2010-08-25 (RLVa-1.2.1a) | Modified: RLVa-1.2.1a
		// Block if we can't "replace wear" what's currently there
		if ( (rlv_handler_t::isEnabled()) && ((gRlvAttachmentLocks.canAttach(attachment) & RLV_WEAR_REPLACE) == 0)  )
			return;
// [/RLVa:KB]
	LLNotificationsUtil::add("ReplaceAttachment", LLSD(), payload, confirm_replace_attachment_rez);
	}
	else
	{
// [RLVa:KB] - Checked: 2010-08-07 (RLVa-1.2.0i) | Modified: RLVa-1.2.0i
		// Block wearing anything on a non-attachable attachment point
		if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachmentPoint(attach_pt, RLV_LOCK_ADD)) )
			return;
// [/RLVa:KB]
		LLNotifications::instance().forceResponse(LLNotification::Params("ReplaceAttachment").payload(payload), 0/*YES*/);
	}
}

bool confirm_replace_attachment_rez(const LLSD& notification, const LLSD& response)
{
	if (!gAgentAvatarp->canAttachMoreObjects())
	{
		LLSD args;
		args["MAX_ATTACHMENTS"] = llformat("%d", MAX_AGENT_ATTACHMENTS);
		LLNotificationsUtil::add("MaxAttachmentsOnOutfit", args);
		return false;
	}

	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0/*YES*/)
	{
		LLUUID item_id = notification["payload"]["item_id"].asUUID();
		LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
		
		if (itemp)
		{
			// Queue up attachments to be sent in next idle tick, this way the
			// attachments are batched up all into one message versus each attachment
			// being sent in its own separate attachments message.
			U8 attachment_pt = notification["payload"]["attachment_point"].asInteger();
			BOOL is_add = notification["payload"]["is_add"].asBoolean();

			LLAttachmentsMgr::instance().addAttachment(item_id, attachment_pt, is_add);
		}
	}
	return false;
}
LLNotificationFunctorRegistration confirm_replace_attachment_rez_reg("ReplaceAttachment", confirm_replace_attachment_rez);

void LLObjectBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	// *TODO: Translate
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;
	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
		}

	else
	{
		items.push_back(std::string("Properties"));

		getClipboardEntries(true, items, disabled_items, flags);

		LLObjectBridge::sContextMenuItemID = mUUID;

		LLInventoryItem* item = getItem();
		if(item)
		{
			LLVOAvatar *avatarp = gAgentAvatarp;
			if( !avatarp )
			{
				return;
			}
			

			if( get_is_item_worn( mUUID ) )
			{
				items.push_back(std::string("Attach Separator"));
				items.push_back(std::string("Detach From Yourself"));
				items.push_back(std::string("Wearable Edit"));
// [RLVa:KB] - Checked: 2010-02-27 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a | OK
				if ( (rlv_handler_t::isEnabled()) && (!gRlvAttachmentLocks.canDetach(item)) )
					disabled_items.push_back(std::string("Detach From Yourself"));
// [/RLVa:KB]
			}
			else
			// <edit> testzone attachpt
			if( avatarp->isWearingUnsupportedAttachment( mUUID ) )
			{
				items.push_back(std::string("Detach From Yourself"));
			}
			else
			// </edit>
			if( !isItemInTrash() )
			{
				items.push_back(std::string("Attach Separator"));
				items.push_back(std::string("Object Wear"));
				if (gHippoGridManager->getConnectedGrid()->supportsInvLinks())
					items.push_back(std::string("Object Add"));
				if (!avatarp->canAttachMoreObjects())
				{
					disabled_items.push_back(std::string("Object Add"));
				}
				items.push_back(std::string("Attach To"));
				items.push_back(std::string("Attach To HUD"));
				// commented out for DEV-32347 - AND Commented back in for non-morons. -HgB
				items.push_back(std::string("Restore to Last Position"));
				
				if (!avatarp->canAttachMoreObjects())
				{
					disabled_items.push_back(std::string("Object Wear"));
					disabled_items.push_back(std::string("Object Add"));
					disabled_items.push_back(std::string("Attach To"));
					disabled_items.push_back(std::string("Attach To HUD"));
				}
// [RLVa:KB] - Checked: 2010-09-03 (RLVa-1.2.1a) | Modified: RLVa-1.2.1a | OK
				else if (rlv_handler_t::isEnabled())
				{
					ERlvWearMask eWearMask = gRlvAttachmentLocks.canAttach(item);
					if ((eWearMask & RLV_WEAR_REPLACE) == 0)
						disabled_items.push_back(std::string("Object Wear"));
					if ((eWearMask & RLV_WEAR_ADD) == 0)
						disabled_items.push_back(std::string("Object Add"));
				}
// [/RLVa:KB]

				LLMenuGL* attach_menu = menu.getChildMenuByName("Attach To", TRUE);
				LLMenuGL* attach_hud_menu = menu.getChildMenuByName("Attach To HUD", TRUE);
				if (attach_menu
					&& (attach_menu->getChildCount() == 0)
					&& attach_hud_menu
					&& (attach_hud_menu->getChildCount() == 0)
					&& avatarp)
				{
					for (LLVOAvatar::attachment_map_t::iterator iter = avatarp->mAttachmentPoints.begin(); 
						 iter != avatarp->mAttachmentPoints.end(); )
					{
						LLVOAvatar::attachment_map_t::iterator curiter = iter++;
						LLViewerJointAttachment* attachment = curiter->second;
						LLMenuItemCallGL *new_item;
						if (attachment->getIsHUDAttachment())
						{
// [RLVa:KB] - Checked: 2009-07-06 (RLVa-1.0.0c)
							attach_hud_menu->append(new_item = new LLMenuItemCallGL(attachment->getName(), 
								NULL, //&LLObjectBridge::attachToAvatar, 
								(rlv_handler_t::isEnabled()) ? &rlvAttachToEnabler : NULL,
								&attach_label, (void*)attachment));
// [/RLVa:KB]
						}
						else
						{
// [RLVa:KB] - Checked: 2009-07-06 (RLVa-1.0.0c)
							attach_menu->append(new_item = new LLMenuItemCallGL(attachment->getName(), 
								NULL, //&LLObjectBridge::attachToAvatar,
								(rlv_handler_t::isEnabled()) ? &rlvAttachToEnabler : NULL,
								&attach_label, (void*)attachment));
// [/RLVa:KB]
						}

						LLSimpleListener* callback = mInventoryPanel->getListenerByName("Inventory.AttachObject");
						
						if (callback)
						{
							new_item->addListener(callback, "on_click", LLSD(attachment->getName()));
						}
					}
					LLMenuItemCallGL *new_item = new LLMenuItemCallGL("Custom...", NULL, NULL);
					attach_menu->append(new_item);
					LLSimpleListener* callback = mInventoryPanel->getListenerByName("Inventory.AttachCustom");
					new_item->addListener(callback, "on_click", LLSD());
				}
			}
		}
	}
	hide_context_entries(menu, items, disabled_items);
}

BOOL LLObjectBridge::renameItem(const std::string& new_name)
{
	if(!isItemRenameable())
		return FALSE;
	LLPreview::rename(mUUID, getPrefix() + new_name);
	LLInventoryModel* model = getInventoryModel();
	if(!model)
		return FALSE;
	LLViewerInventoryItem* item = getItem();
	if(item && (item->getName() != new_name))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->rename(new_name);
		buildDisplayName(new_item, mDisplayName);
		new_item->updateServer(FALSE);
		model->updateItem(new_item);
		model->notifyObservers();

		if (isAgentAvatarValid())
		{
			LLViewerObject* obj = gAgentAvatarp->getWornAttachment( item->getUUID() );
			if(obj)
			{
				LLSelectMgr::getInstance()->deselectAll();
				LLSelectMgr::getInstance()->addAsIndividual( obj, SELECT_ALL_TES, FALSE );
				LLSelectMgr::getInstance()->selectionSetObjectName( new_name );
				LLSelectMgr::getInstance()->deselectAll();
			}
		}
	}
	// return FALSE because we either notified observers (& therefore
	// rebuilt) or we didn't update.
	return FALSE;
}

// +=================================================+
// |        LLLSLTextBridge                          |
// +=================================================+

void LLLSLTextBridge::openItem()
{
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
	if (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT))
	{
		RlvNotifications::notifyBlockedViewScript();
		return;
	}
// [/RLVa:KB]

	// See if we can bring an exiting preview to the front
	if(!LLPreview::show(mUUID))
	{
		LLViewerInventoryItem* item = getItem();
		if (item)
		{
			// There isn't one, so make a new preview
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewScriptRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);
			
			LLPreviewLSL* preview =	new LLPreviewLSL("preview lsl text",
											 rect,
											 getPrefix() + item->getName(),
											 mUUID);
			preview->setFocus(TRUE);
			// keep onscreen
			gFloaterView->adjustToFitScreen(preview, FALSE);
		}
	}
}

// +=================================================+
// |        LLWearableBridge                         |
// +=================================================+

LLWearableBridge::LLWearableBridge(LLInventoryPanel* inventory, 
								   //LLFolderView* root, 
								   const LLUUID& uuid, 
								   LLAssetType::EType asset_type, 
								   LLInventoryType::EType inv_type, 
								   LLWearableType::EType wearable_type) :
	LLItemBridge(inventory, /*root,*/ uuid),
	mAssetType( asset_type ),
	mWearableType(wearable_type)
{
	mInvType = inv_type;
}

// *NOTE: hack to get from avatar inventory to avatar
void wear_inventory_item_on_avatar( LLInventoryItem* item )
{
	if(item)
	{
		lldebugs << "wear_inventory_item_on_avatar( " << item->getName()
				 << " )" << llendl;
			
		LLWearableList::instance().getAsset(item->getAssetUUID(),
							   item->getName(),
							   item->getType(),
							   LLWearableBridge::onWearOnAvatarArrived,
							   new LLUUID(item->getLinkedUUID()));
	}
}

// [RLVa:KB] - Checked: 2009-12-18 (RLVa-1.1.0i) | Added: RLVa-1.1.0i
// Moved to llinventorybridge.h because we need it in RlvForceWearLegacy
/*
struct LLFoundData
{
	LLFoundData(const LLUUID& item_id,
				const LLUUID& asset_id,
				const std::string& name,
				LLAssetType::EType asset_type) :
		mItemID(item_id),
		mAssetID(asset_id),
		mName(name),
		mAssetType(asset_type),
		mWearable( NULL ) {}
	
	LLUUID mItemID;
	LLUUID mAssetID;
	std::string mName;
	LLAssetType::EType mAssetType;
	LLWearable* mWearable;
};

struct LLWearableHoldingPattern
{
	LLWearableHoldingPattern() : mResolved(0) {}
	~LLWearableHoldingPattern()
	{
		for_each(mFoundList.begin(), mFoundList.end(), DeletePointer());
		mFoundList.clear();
	}
	typedef std::list<LLFoundData*> found_list_t;
	found_list_t mFoundList;
	S32 mResolved;
};

*/
// [/RLVa:KB]

class LLOutfitObserver : public LLInventoryFetchItemsObserver
{
public:
	LLOutfitObserver(const uuid_vec_t& ids, const LLUUID& cat_id, bool copy_items, bool append) :
		LLInventoryFetchItemsObserver(ids),
		mCatID(cat_id),
		mCopyItems(copy_items),
		mAppend(append)
	{}
	~LLOutfitObserver() {}
	virtual void done(); //public

protected:
	LLUUID mCatID;
	bool mCopyItems;
	bool mAppend;
};


class LLWearInventoryCategoryCallback : public LLInventoryCallback
{
public:
	LLWearInventoryCategoryCallback(const LLUUID& cat_id, bool append)
	{
		mCatID = cat_id;
		mAppend = append;
	}
	void fire(const LLUUID& item_id)
	{
		/*
		 * Do nothing.  We only care about the destructor
		 *
		 * The reason for this is that this callback is used in a hack where the
		 * same callback is given to dozens of items, and the destructor is called
		 * after the last item has fired the event and dereferenced it -- if all
		 * the events actually fire!
		 */
	}

protected:
	~LLWearInventoryCategoryCallback()
	{
		// Is the destructor called by ordinary dereference, or because the app's shutting down?
		// If the inventory callback manager goes away, we're shutting down, no longer want the callback.
		if( LLInventoryCallbackManager::is_instantiated() )
		{
			wear_inventory_category_on_avatar(gInventory.getCategory(mCatID), mAppend);
		}
		else
		{
			llwarns << "Dropping unhandled LLWearInventoryCategoryCallback" << llendl;
		}
	}

private:
	LLUUID mCatID;
	bool mAppend;
};

void LLOutfitObserver::done()
{
	// We now have an outfit ready to be copied to agent inventory. Do
	// it, and wear that outfit normally.
	if(mCopyItems)
	{
		LLInventoryCategory* cat = gInventory.getCategory(mCatID);
		std::string name;
		if(!cat)
		{
			// should never happen.
			name = "New Outfit";
		}
		else
		{
			name = cat->getName();
		}
		LLViewerInventoryItem* item = NULL;
		uuid_vec_t::iterator it = mComplete.begin();
		uuid_vec_t::iterator end = mComplete.end();
		LLUUID pid;
		for(; it < end; ++it)
		{
			item = (LLViewerInventoryItem*)gInventory.getItem(*it);
			if(item)
			{
				if(LLInventoryType::IT_GESTURE == item->getInventoryType())
				{
					pid = gInventory.findCategoryUUIDForType(LLFolderType::FT_GESTURE);
				}
				else
				{
					pid = gInventory.findCategoryUUIDForType(LLFolderType::FT_CLOTHING);
				}
				break;
			}
		}
		if(pid.isNull())
		{
			pid = gInventory.getRootFolderID();
		}
		
		LLUUID cat_id = gInventory.createNewCategory(
			pid,
			LLFolderType::FT_NONE,
			name);
		mCatID = cat_id;
		LLPointer<LLInventoryCallback> cb = new LLWearInventoryCategoryCallback(mCatID, mAppend);
		it = mComplete.begin();
		for(; it < end; ++it)
		{
			item = (LLViewerInventoryItem*)gInventory.getItem(*it);
			if(item)
			{
				copy_inventory_item(
					gAgent.getID(),
					item->getPermissions().getOwner(),
					item->getUUID(),
					cat_id,
					std::string(),
					cb);
			}
		}
	}
	else
	{
		// Wear the inventory category.
		wear_inventory_category_on_avatar(gInventory.getCategory(mCatID), mAppend);
	}
}

class LLOutfitFetch : public LLInventoryFetchDescendentsObserver
{
public:
	LLOutfitFetch(const LLUUID& id, bool copy_items, bool append) : mCopyItems(copy_items), mAppend(append) {}
	~LLOutfitFetch() {}
	virtual void done();
protected:
	bool mCopyItems;
	bool mAppend;
};

void LLOutfitFetch::done()
{
	// What we do here is get the complete information on the items in
	// the library, and set up an observer that will wait for that to
	// happen.
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	gInventory.collectDescendents(mComplete.front(),
								  cat_array,
								  item_array,
								  LLInventoryModel::EXCLUDE_TRASH);
	S32 count = item_array.count();
	if(!count)
	{
		llwarns << "Nothing fetched in category " << mComplete.front()
				<< llendl;
		dec_busy_count();
		gInventory.removeObserver(this);
		delete this;
		return;
	}

	uuid_vec_t ids;
	for(S32 i = 0; i < count; ++i)
	{
		ids.push_back(item_array.get(i)->getUUID());
	}

	LLOutfitObserver* outfit = new LLOutfitObserver(ids, mComplete.front(), mCopyItems, mAppend);
	

	// clean up, and remove this as an observer since the call to the
	// outfit could notify observers and throw us into an infinite
	// loop.
	dec_busy_count();
	gInventory.removeObserver(this);
	delete this;

	// increment busy count and either tell the inventory to check &
	// call done, or add this object to the inventory for observation.
	inc_busy_count();

	// do the fetch
	outfit->startFetch();
	if(outfit->isFinished())
	{
		// everything is already here - call done.
		outfit->done();
	}
	else
	{
		// it's all on it's way - add an observer, and the inventory
		// will call done for us when everything is here.
		gInventory.addObserver(outfit);
	}
}

void wear_outfit_by_name(const std::string& name)
{
	llinfos << "Wearing category " << name << llendl;
	inc_busy_count();

	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	LLNameCategoryCollector has_name(name);
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(),
									cat_array,
									item_array,
									LLInventoryModel::EXCLUDE_TRASH,
									has_name);
	bool copy_items = false;
	LLInventoryCategory* cat = NULL;
	if (cat_array.count() > 0)
	{
		// Just wear the first one that matches
		cat = cat_array.get(0);
	}
	else
	{
		gInventory.collectDescendentsIf(LLUUID::null,
										cat_array,
										item_array,
										LLInventoryModel::EXCLUDE_TRASH,
										has_name);
		if(cat_array.count() > 0)
		{
			cat = cat_array.get(0);
			copy_items = true;
		}
	}

	if(cat)
	{
		wear_inventory_category(cat, copy_items, false);
	}
	else
	{
		llwarns << "Couldn't find outfit " <<name<< " in wear_outfit_by_name()"
				<< llendl;
	}

	dec_busy_count();
}

void wear_inventory_category(LLInventoryCategory* category, bool copy, bool append)
{
	if(!category) return;

	lldebugs << "wear_inventory_category( " << category->getName()
			 << " )" << llendl;
	// What we do here is get the complete information on the items in
	// the inventory, and set up an observer that will wait for that to
	// happen.
	LLOutfitFetch* outfit = new LLOutfitFetch(category->getUUID(), copy, append);
	outfit->startFetch();
	inc_busy_count();
	if(outfit->isFinished())
	{
		// everything is already here - call done.
		outfit->done();
	}
	else
	{
		// it's all on it's way - add an observer, and the inventory
		// will call done for us when everything is here.
		gInventory.addObserver(outfit);
	}
}

// *NOTE: hack to get from avatar inventory to avatar
void wear_inventory_category_on_avatar(LLInventoryCategory* category, BOOL append, BOOL replace)
{	
	// Avoid unintentionally overwriting old wearables.  We have to do
	// this up front to avoid having to deal with the case of multiple
	// wearables being dirty.
	if(!category) return;
	lldebugs << "wear_inventory_category_on_avatar( " << category->getName()
			 << " )" << llendl;
			 	
	LLWearInfo* userdata = new LLWearInfo;
	userdata->mAppend = append;
	userdata->mReplace = replace;
	userdata->mCategoryID = category->getUUID();

	if( gFloaterCustomize )
	{
		gFloaterCustomize->askToSaveIfDirty(
			wear_inventory_category_on_avatar_step2,
			userdata);
	}
	else
	{
		wear_inventory_category_on_avatar_step2(
			TRUE,
			userdata );
	}
}


void wear_inventory_category_on_avatar_step2( BOOL proceed, void* userdata )
{
	LLWearInfo* wear_info = (LLWearInfo*)userdata;
	if (!wear_info) return;

	// Find all the wearables that are in the category's subtree.	
	lldebugs << "wear_inventory_category_on_avatar_step2()" << llendl;
	if(proceed)
	{
		LLInventoryModel::cat_array_t cat_array;
		LLInventoryModel::item_array_t item_array;
		LLFindWearables is_wearable;
		gInventory.collectDescendentsIf(wear_info->mCategoryID,
										cat_array,
										item_array,
										LLInventoryModel::EXCLUDE_TRASH,
										is_wearable);
		S32 i;
		S32 wearable_count = item_array.count();

		LLInventoryModel::item_array_t obj_items_new;
		LLCOFMgr::getDescendentsOfAssetType(wear_info->mCategoryID, obj_items_new, LLAssetType::AT_OBJECT, false);
		S32 obj_count = obj_items_new.count();

		// Find all gestures in this folder
		LLInventoryModel::cat_array_t	gest_cat_array;
		LLInventoryModel::item_array_t	gest_item_array;
		LLIsType is_gesture( LLAssetType::AT_GESTURE );
		gInventory.collectDescendentsIf(wear_info->mCategoryID,
										gest_cat_array,
										gest_item_array,
										LLInventoryModel::EXCLUDE_TRASH,
										is_gesture);
		S32 gest_count = gest_item_array.count();

		if( !wearable_count && !obj_count && !gest_count)
		{
			LLNotificationsUtil::add("CouldNotPutOnOutfit");
			delete wear_info;
			return;
		}

		// Processes that take time should show the busy cursor
		if (wearable_count > 0 || obj_count > 0)
		{
			inc_busy_count();
		}

		// Activate all gestures in this folder
		if (gest_count > 0)
		{
			llinfos << "Activating " << gest_count << " gestures" << llendl;

			LLGestureMgr::instance().activateGestures(gest_item_array);

			// Update the inventory item labels to reflect the fact
			// they are active.
			LLViewerInventoryCategory* catp = gInventory.getCategory(wear_info->mCategoryID);
			if (catp)
			{
				gInventory.updateCategory(catp);
				gInventory.notifyObservers();
			}
		}

		if(wearable_count > 0)
		{
			// Note: can't do normal iteration, because if all the
			// wearables can be resolved immediately, then the
			// callback will be called (and this object deleted)
			// before the final getNextData().
//			LLWearableHoldingPattern* holder = new LLWearableHoldingPattern;
// [RLVa:KB] - Checked: 2009-12-18 (RLVa-1.1.0i) | Added: RLVa-1.1.0i
			LLWearableHoldingPattern* holder = new LLWearableHoldingPattern(wear_info->mAppend);
// [/RLVa:KB]
			LLFoundData* found;
			LLDynamicArray<LLFoundData*> found_container;
			for(i = 0; i  < wearable_count; ++i)
			{
				found = new LLFoundData(item_array.get(i)->getLinkedUUID(),
										item_array.get(i)->getAssetUUID(),
										item_array.get(i)->getName(),
										item_array.get(i)->getType());
				holder->mFoundList.push_front(found);
				found_container.put(found);
			}
			for(i = 0; i < wearable_count; ++i)
			{
// [RLVa:KB] - Part of LLWearableHoldingPattern
//				gAddToOutfit = wear_info->mAppend;
// [/RLVa:KB]

				found = found_container.get(i);
				LLWearableList::instance().getAsset(found->mAssetID,
										found->mName,
									   found->mAssetType,
									   wear_inventory_category_on_avatar_loop,
									   (void*)holder);
			}
		}

		const LLUUID idCOF = LLCOFMgr::instance().getCOF();

		//
		// - Attachments: include COF contents only if appending.
		//
		if (!wear_info->mReplace)
		{
			LLInventoryModel::item_array_t obj_items;
			if (wear_info->mAppend)
				LLCOFMgr::getDescendentsOfAssetType(idCOF, obj_items, LLAssetType::AT_OBJECT, false);
// [RLVa:KB] - Checked: 2010-03-05 (RLVa-1.2.0z) | Modified: RLVa-1.2.0b
			else if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_ANY)) )
			{
				// Make sure that all currently locked attachments remain in COF when replacing
				LLCOFMgr::getDescendentsOfAssetType(idCOF, obj_items, LLAssetType::AT_OBJECT, false);
				obj_items.erase(std::remove_if(obj_items.begin(), obj_items.end(), rlvPredCanRemoveItem), obj_items.end());
			}
// [/RLVa:KB]
//			getDescendentsOfAssetType(category, obj_items, LLAssetType::AT_OBJECT, false);
// [RLVa:KB] - Checked: 2010-03-05 (RLVa-1.2.0z) | Modified: RLVa-1.2.0b
			// Filter out any new attachments that can't be worn before adding them
			if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.hasLockedAttachmentPoint(RLV_LOCK_ANY)) )
				obj_items_new.erase(std::remove_if(obj_items_new.begin(), obj_items_new.end(), 
						RlvPredCanNotWearItem(RLV_WEAR_ADD)), obj_items_new.end());
			for (S32 idxObjNew = 0; idxObjNew < obj_items_new.count(); idxObjNew++)
				RlvAttachmentLockWatchdog::instance().onWearAttachment(obj_items_new.get(idxObjNew).get() , RLV_WEAR_ADD);
			obj_items.insert(obj_items.end(), obj_items_new.begin(), obj_items_new.end());
// [/RLVa:KB]

			LLAgentWearables::userUpdateAttachments(obj_items);
		}
		else
		{
			for (S32 idxItem = 0, cntItem = obj_items_new.count(); idxItem < cntItem; idxItem++)
			{
				LLInventoryItem* pItem = obj_items_new.get(idxItem);

// [RLVa:KB] - Checked: 2010-11-21 (RLVa-1.1.3c) | Added: RLVa-1.1.3c
				if ( (rlv_handler_t::isEnabled()) && (!(RLV_WEAR_REPLACE & gRlvAttachmentLocks.canAttach(pItem))) )
					continue;
// [/RLVa:KB]

				LLAttachmentsMgr::instance().addAttachment(pItem->getLinkedUUID(), 0, false);
			}
		}

		if (!wear_info->mAppend)
			LLCOFMgr::instance().addBOFLink(wear_info->mCategoryID);
	}
	delete wear_info;
	wear_info = NULL;
}

void wear_inventory_category_on_avatar_loop(LLWearable* wearable, void* data)
{
	LLWearableHoldingPattern* holder = (LLWearableHoldingPattern*)data;
// [RLVa:KB] - Part of LLWearableHoldingPattern
//	BOOL append= gAddToOutfit;
// [/RLVa:KB]
	


	if(wearable)
	{
		for (LLWearableHoldingPattern::found_list_t::iterator iter = holder->mFoundList.begin();
			 iter != holder->mFoundList.end(); ++iter)
		{
			LLFoundData* data = *iter;
			if(wearable->getAssetID() == data->mAssetID)
			{
				data->mWearable = wearable;
				break;
			}
		}
	}
	holder->mResolved += 1;
	if(holder->mResolved >= (S32)holder->mFoundList.size())
	{
//		wear_inventory_category_on_avatar_step3(holder, append);
// [RLVa:KB] - Checked: 2009-12-18 (RLVa-1.1.0i) | Added: RLVa-1.1.0i
		wear_inventory_category_on_avatar_step3(holder, holder->mAddToOutfit);
// [/RLVa:KB]
	}
}




void wear_inventory_category_on_avatar_step3(LLWearableHoldingPattern* holder, BOOL append)
{
	lldebugs << "wear_inventory_category_on_avatar_step3()" << llendl;
	LLInventoryItem::item_array_t items;
	LLDynamicArray< LLWearable* > wearables;

	// For each wearable type, find the first instance in the category
	// that we recursed through.
	for( S32 i = 0; i < LLWearableType::WT_COUNT; i++ )
	{
		for (LLWearableHoldingPattern::found_list_t::iterator iter = holder->mFoundList.begin();
			 iter != holder->mFoundList.end(); ++iter)
		{
			LLFoundData* data = *iter;
			LLWearable* wearable = data->mWearable;
			if( wearable && ((S32)wearable->getType() == i) )
			{
				LLViewerInventoryItem* item;
				item = (LLViewerInventoryItem*)gInventory.getLinkedItem(data->mItemID);
				if( item && (item->getAssetUUID() == wearable->getAssetID()) )
				{
				//RN: after discussing with Brashears, I disabled this code
				//Metadata should reside in the item, not the asset
				//And this code does not handle failed asset uploads properly
//					if(!wearable->isMatchedToInventoryItem(item ))
//					{
//						wearable = LLWearableList::instance().createWearableMatchedToInventoryItem( wearable, item );
//						// Now that we have an asset that matches the
//						// item, update the item to point to the new
//						// asset.
//						item->setAssetUUID(wearable->getID());
//						item->updateAssetOnServer();
//					}
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.1.3b) | Modified: RLVa-1.1.3b
					if (!gRlvWearableLocks.canWear(wearable->getType()))
					{
						continue;
					}
// [/RLVa:KB]
					items.put(item);
					wearables.put(wearable);
				}
				break;
			}
		}
	}

	if(wearables.count() > 0)
	{
		gAgentWearables.setWearableOutfit(items, wearables, !append);
		gInventory.notifyObservers();
	}

	delete holder;

	dec_busy_count();
}

void remove_inventory_category_from_avatar( LLInventoryCategory* category )
{
	if(!category) return;
	lldebugs << "remove_inventory_category_from_avatar( " << category->getName()
			 << " )" << llendl;
			 
	
	LLUUID* uuid	= new LLUUID(category->getUUID());

	if( gFloaterCustomize )
	{
		gFloaterCustomize->askToSaveIfDirty(
			remove_inventory_category_from_avatar_step2,
			uuid);
	}
	else
	{
		remove_inventory_category_from_avatar_step2(
			TRUE,
			uuid );
	}
}

struct OnRemoveStruct
{
	LLUUID mUUID;
	OnRemoveStruct(const LLUUID& uuid):
		mUUID(uuid)
	{
	}
};

void remove_inventory_category_from_avatar_step2( BOOL proceed, void* userdata)
{

	// Find all the wearables that are in the category's subtree.
	LLUUID* category_id = (LLUUID *)userdata;
	
	lldebugs << "remove_inventory_category_from_avatar_step2()" << llendl;
	if(proceed)
	{
		LLInventoryModel::cat_array_t cat_array;
		LLInventoryModel::item_array_t item_array;
		LLFindWearables is_wearable;
		gInventory.collectDescendentsIf(*category_id,
										cat_array,
										item_array,
										LLInventoryModel::EXCLUDE_TRASH,
										is_wearable);
		S32 i;
		S32 wearable_count = item_array.count();

		LLInventoryModel::cat_array_t	obj_cat_array;
		LLInventoryModel::item_array_t	obj_item_array;
		LLIsType is_object( LLAssetType::AT_OBJECT );
		gInventory.collectDescendentsIf(*category_id,
										obj_cat_array,
										obj_item_array,
										LLInventoryModel::EXCLUDE_TRASH,
										is_object);
		S32 obj_count = obj_item_array.count();

		// Find all gestures in this folder
		LLInventoryModel::cat_array_t	gest_cat_array;
		LLInventoryModel::item_array_t	gest_item_array;
		LLIsType is_gesture( LLAssetType::AT_GESTURE );
		gInventory.collectDescendentsIf(*category_id,
										gest_cat_array,
										gest_item_array,
										LLInventoryModel::EXCLUDE_TRASH,
										is_gesture);
		S32 gest_count = gest_item_array.count();

		if (wearable_count > 0)	//Loop through wearables.  If worn, remove.
		{
			for(i = 0; i  < wearable_count; ++i)
			{
				LLViewerInventoryItem *item = item_array.get(i);
//				if( gAgentWearables.isWearingItem (item_array.get(i)->getUUID()) )
// [RLVa:KB] - Checked: 2009-07-07 (RLVa-1.1.3b) | Modified: RLVa-0.2.2a
				LLWearable* pWearable = gAgentWearables.getWearableFromItemID(item_array.get(i)->getLinkedUUID());
				if ( (pWearable) && ( (!rlv_handler_t::isEnabled()) || (gRlvWearableLocks.canRemove(pWearable->getType())) ) )
// [/RLVa:KB]
				{
					LLWearableList::instance().getAsset(				item->getAssetUUID(),
														item->getName(),
														item->getType(),
														LLWearableBridge::onRemoveFromAvatarArrived,
														new OnRemoveStruct(item->getLinkedUUID()));
				}
			}
		}
		
		if (obj_count > 0)
		{
			for(i = 0; i  < obj_count; ++i)
			{
				LLViewerInventoryItem *obj_item = obj_item_array.get(i);
				if (get_is_item_worn(obj_item->getUUID()))
				{
					LLVOAvatar::detachAttachmentIntoInventory(obj_item->getLinkedUUID());
				}
			}
		}

		if (gest_count > 0)
		{
			for(i = 0; i  < gest_count; ++i)
			{
				LLViewerInventoryItem *gest_item = gest_item_array.get(i);
				if (get_is_item_worn(gest_item->getUUID()))
				{
					LLGestureMgr::instance().deactivateGesture( gest_item_array.get(i)->getLinkedUUID() );
					gInventory.updateItem( gest_item );
					gInventory.notifyObservers();
				}

			}
		}
	}
	delete category_id;
	category_id = NULL;
}

BOOL LLWearableBridge::renameItem(const std::string& new_name)
{
	if (get_is_item_worn(getItem()))
	{
		gAgentWearables.setWearableName( mUUID, new_name );
	}
	return LLItemBridge::renameItem(new_name);
}

BOOL LLWearableBridge::isItemRemovable()
{
	LLInventoryModel* model = mInventoryPanel->getModel();
	if (!model)
	{
		return FALSE;
	}
	const LLInventoryObject *obj = model->getItem(mUUID);
	if (obj && obj->getIsLinkType())
	{
		return TRUE;
	}
	// <edit>
	//if(gAgentWearables.isWearingItem(mUUID)) return FALSE;
	// </edit>
	return LLInvFVBridge::isItemRemovable();
}

std::string LLWearableBridge::getLabelSuffix() const
{
	if (get_is_item_worn(getItem()))
	{
		if ( (rlv_handler_t::isEnabled()) && (!gRlvWearableLocks.canRemove(getItem())) )
		{
			return LLItemBridge::getLabelSuffix() + " (locked)";
		}
		return LLItemBridge::getLabelSuffix() + " (worn)";
	}
	else
	{
		return LLItemBridge::getLabelSuffix();
	}
}

LLUIImagePtr LLWearableBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(mAssetType, mInvType, mWearableType, FALSE);
}

// virtual
void LLWearableBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if ("wear" == action)
	{
		wearOnAvatar();
	}
	else if ("edit" == action)
	{
		editOnAvatar();
		return;
	}
	else if ("take_off" == action)
	{
		if(gAgentWearables.isWearingItem(mUUID))
		{
			LLViewerInventoryItem* item = getItem();
			if (item)
			{
				LLWearableList::instance().getAsset(item->getAssetUUID(),
										item->getName(),
									item->getType(),
									LLWearableBridge::onRemoveFromAvatarArrived,
									new OnRemoveStruct(item->getLinkedUUID()));
			}
		}
	}
	else LLItemBridge::performAction(folder, model, action);
}

void LLWearableBridge::openItem()
{
	if( isItemInTrash() )
	{
		LLNotificationsUtil::add("CannotWearTrash");
	}
	else if(isAgentInventory())
	{
		if (gAgentWearables.isWearingItem(mUUID))
		{
			performAction(NULL, NULL, "take_off");
		}
		else
 		{
			performAction(NULL, NULL, "wear");
		}
	}
	else
	{
		// must be in the inventory library. copy it to our inventory
		// and put it on right away.
		LLViewerInventoryItem* item = getItem();
		if(item && item->isComplete())
		{
			LLPointer<LLInventoryCallback> cb = new WearOnAvatarCallback();
			copy_inventory_item(
				gAgent.getID(),
				item->getPermissions().getOwner(),
				item->getUUID(),
				LLUUID::null,
				std::string(),
				cb);
		}
		else if(item)
		{
			// *TODO: We should fetch the item details, and then do
			// the operation above.
			LLNotificationsUtil::add("CannotWearInfoNotComplete");
		}
	}
}

void LLWearableBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	// *TODO: Translate
	lldebugs << "LLWearableBridge::buildContextMenu()" << llendl;
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;
	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{	// FWIW, it looks like SUPPRESS_OPEN_ITEM is not set anywhere
		BOOL can_open = ((flags & SUPPRESS_OPEN_ITEM) != SUPPRESS_OPEN_ITEM);

		// If we have clothing, don't add "Open" as it's the same action as "Wear"   SL-18976
		LLViewerInventoryItem* item = getItem();
		if (can_open && item)
		{
			can_open = (item->getType() != LLAssetType::AT_CLOTHING) &&
				(item->getType() != LLAssetType::AT_BODYPART);
		}
		if (can_open)
		{
			addOpenRightClickMenuOption(items);
		}
		else
		{
			disabled_items.push_back(std::string("Open"));
			disabled_items.push_back(std::string("Open Original"));
		}

		items.push_back(std::string("Properties"));

		getClipboardEntries(true, items, disabled_items, flags);

		items.push_back(std::string("Wearable Separator"));
		
		items.push_back(std::string("Wearable Wear"));
		items.push_back(std::string("Wearable Edit"));

		if ((flags & FIRST_SELECTED_ITEM) == 0)
		{
			disabled_items.push_back(std::string("Wearable Edit"));
		}
		// Don't allow items to be worn if their baseobj is in the trash.
		if (isLinkedObjectInTrash() || isLinkedObjectMissing() || isCOFFolder())
		{
			disabled_items.push_back(std::string("Wearable And Object Wear"));
			disabled_items.push_back(std::string("Wearable Add"));
			disabled_items.push_back(std::string("Wearable Edit"));
		}

		// Disable wear and take off based on whether the item is worn.
		if(item)
		{
			switch (item->getType())
			{
				case LLAssetType::AT_CLOTHING:
			items.push_back(std::string("Take Off"));
					// Fallthrough since clothing and bodypart share wear options
				case LLAssetType::AT_BODYPART:
					if (get_is_item_worn(item->getUUID()))
					{
						disabled_items.push_back(std::string("Wearable And Wear"));
						disabled_items.push_back(std::string("Wearable Add"));
		}
					else
					{
						items.push_back(std::string("Wearable Wear"));
						disabled_items.push_back(std::string("Take Off"));
						disabled_items.push_back(std::string("Wearable Edit"));
	}

					/*if (LLWearableType::getAllowMultiwear(mWearableType))
					{
						items.push_back(std::string("Wearable Add"));
						if (gAgentWearables.getWearableCount(mWearableType) >= LLAgentWearables::MAX_CLOTHING_PER_TYPE)
						{
							disabled_items.push_back(std::string("Wearable Add"));
						}
					}
					break;*/
				default:
					break;
			}
		}
	}
	hide_context_entries(menu, items, disabled_items);
}

// Called from menus
// static
BOOL LLWearableBridge::canWearOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if(!self) return FALSE;
	if(!self->isAgentInventory())
	{
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)self->getItem();
		if(!item || !item->isComplete()) return FALSE;
	}
	return (!get_is_item_worn(self->getItem()));
}

// Called from menus
// static
void LLWearableBridge::onWearOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if(!self) return;
	self->wearOnAvatar();
}

void LLWearableBridge::wearOnAvatar()
{
	// Don't wear anything until initial wearables are loaded, can
	// destroy clothing items.
	if (!gAgentWearables.areWearablesLoaded()) 
	{
		LLNotificationsUtil::add("CanNotChangeAppearanceUntilLoaded");
		return;
	}

	LLViewerInventoryItem* item = getItem();
	if(item)
	{
		if(!isAgentInventory())
		{
			LLPointer<LLInventoryCallback> cb = new WearOnAvatarCallback();
			copy_inventory_item(
				gAgent.getID(),
				item->getPermissions().getOwner(),
				item->getUUID(),
				LLUUID::null,
				std::string(),
				cb);
		}
		else
		{
			wear_inventory_item_on_avatar(item);
		}
	}
}

// static
void LLWearableBridge::onWearOnAvatarArrived( LLWearable* wearable, void* userdata )
{
	LLUUID* item_id = (LLUUID*) userdata;
	if(wearable)
	{
		LLViewerInventoryItem* item = NULL;
		item = (LLViewerInventoryItem*)gInventory.getItem(*item_id);
		if(item)
		{
			if(item->getAssetUUID() == wearable->getAssetID())
			{
				//RN: after discussing with Brashears, I disabled this code
				//Metadata should reside in the item, not the asset
				//And this code does not handle failed asset uploads properly

//				if(!wearable->isMatchedToInventoryItem(item))
//				{
//					LLWearable* new_wearable = LLWearableList::instance().createWearableMatchedToInventoryItem( wearable, item );
//
//					// Now that we have an asset that matches the
//					// item, update the item to point to the new
//					// asset.
//					item->setAssetUUID(new_wearable->getID());
//					item->updateAssetOnServer();
//					wearable = new_wearable;
//				}
				gAgentWearables.setWearableItem(item, wearable);
				gInventory.notifyObservers();
				//self->getFolderItem()->refreshFromRoot();
			}
			else
			{
				llinfos << "By the time wearable asset arrived, its inv item already pointed to a different asset." << llendl;
			}
		}
	}
	delete item_id;
}

// static
BOOL LLWearableBridge::canEditOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if(!self) return FALSE;

	return (get_is_item_worn(self->getItem()));
}

// static 
void LLWearableBridge::onEditOnAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if(self)
	{
		self->editOnAvatar();
	}
}

void LLWearableBridge::editOnAvatar()
{
	LLUUID linked_id = gInventory.getLinkedItemID(mUUID);
	LLWearable* wearable = gAgentWearables.getWearableFromItemID(linked_id);
	if( wearable )
	{
		// Set the tab to the right wearable.
		LLFloaterCustomize::setCurrentWearableType( wearable->getType() );

		if( CAMERA_MODE_CUSTOMIZE_AVATAR != gAgentCamera.getCameraMode() )
		{
			// Start Avatar Customization
			gAgentCamera.changeCameraToCustomizeAvatar();
		}
	}
}

// static
BOOL LLWearableBridge::canRemoveFromAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if( self && (LLAssetType::AT_BODYPART != self->mAssetType) )
	{
		return get_is_item_worn(self->getItem());
	}
	return FALSE;
}

// static 
void LLWearableBridge::onRemoveFromAvatar(void* user_data)
{
	LLWearableBridge* self = (LLWearableBridge*)user_data;
	if(!self) return;
	if(get_is_item_worn(self->getItem()))
	{
		LLViewerInventoryItem* item = self->getItem();
		if (item)
		{
			LLWearableList::instance().getAsset(item->getAssetUUID(),
									item->getName(),
								   item->getType(),
								   onRemoveFromAvatarArrived,
								   new OnRemoveStruct(LLUUID(self->mUUID)));
		}
	}
}

// static
void LLWearableBridge::onRemoveFromAvatarArrived(LLWearable* wearable,
												 void* userdata)
{
	OnRemoveStruct *on_remove_struct = (OnRemoveStruct*) userdata;
	const LLUUID &item_id = gInventory.getLinkedItemID(on_remove_struct->mUUID);
	if(wearable)
	{
		if( get_is_item_worn( item_id ) )
		{
			LLWearableType::EType type = wearable->getType();
	
			if( !(type==LLWearableType::WT_SHAPE || type==LLWearableType::WT_SKIN || type==LLWearableType::WT_HAIR || type==LLWearableType::WT_EYES) ) //&&
				//!((!gAgent.isTeen()) && ( type==WT_UNDERPANTS || type==WT_UNDERSHIRT )) )
			{
				gAgentWearables.removeWearable( type, false, 0 );	// TODO: MULTI-WEARABLE
			}
		}
	}
	delete on_remove_struct;
}


// +=================================================+
// |        LLLinkItemBridge                         |
// +=================================================+
// For broken links

std::string LLLinkItemBridge::sPrefix("Link: ");


LLUIImagePtr LLLinkItemBridge::getIcon() const
{
	if (LLViewerInventoryItem *item = getItem())
	{
		U32 attachment_point = (item->getFlags() & 0xff); // low byte of inventory flags
		bool is_multi =  LLInventoryItemFlags::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS & item->getFlags();

		return LLInventoryIcon::getIcon(item->getActualType(), item->getInventoryType(), attachment_point, is_multi);
	}
	return LLInventoryIcon::getIcon(LLAssetType::AT_LINK, LLInventoryType::IT_NONE, 0, FALSE);
}

void LLLinkItemBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	// *TODO: Translate
	lldebugs << "LLLink::buildContextMenu()" << llendl;
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;

	items.push_back(std::string("Find Original"));
	disabled_items.push_back(std::string("Find Original"));
	
	if(isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{
		items.push_back(std::string("Properties"));
		addDeleteContextMenuOptions(items, disabled_items);
		}
	hide_context_entries(menu, items, disabled_items);
	}

// +=================================================+
// |        LLLinkBridge                             |
// +=================================================+
// For broken folder links.
std::string LLLinkFolderBridge::sPrefix("Link: ");
LLUIImagePtr LLLinkFolderBridge::getIcon() const
{
	return LLUI::getUIImage("inv_link_folder.tga");
}

void LLLinkFolderBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	// *TODO: Translate
	lldebugs << "LLLink::buildContextMenu()" << llendl;
	menuentry_vec_t items;
	menuentry_vec_t disabled_items;

	if (isItemInTrash())
	{
		addTrashContextMenuOptions(items, disabled_items);
	}
	else
	{
		items.push_back(std::string("Find Original"));
		addDeleteContextMenuOptions(items, disabled_items);
		}
	hide_context_entries(menu, items, disabled_items);
}

void LLLinkFolderBridge::performAction(LLFolderView* folder, LLInventoryModel* model, std::string action)
{
	if ("goto" == action)
	{
		gotoItem(folder);
		return;
	}
	LLItemBridge::performAction(folder,model,action);
}

void LLLinkFolderBridge::gotoItem(LLFolderView *folder)
{
	const LLUUID &cat_uuid = getFolderID();
	if (!cat_uuid.isNull())
	{
		if (LLFolderViewItem *base_folder = folder->getItemByID(cat_uuid))
		{
			if (LLInventoryModel* model = getInventoryModel())
			{
				model->fetchDescendentsOf(cat_uuid);
			}
			base_folder->setOpen(TRUE);
			folder->setSelectionFromRoot(base_folder,TRUE);
			folder->scrollToShowSelection();
		}
	}
}

const LLUUID &LLLinkFolderBridge::getFolderID() const
{
	if (LLViewerInventoryItem *link_item = getItem())
	{
		if (const LLViewerInventoryCategory *cat = link_item->getLinkedCategory())
		{
			const LLUUID& cat_uuid = cat->getUUID();
			return cat_uuid;
		}
	}
	return LLUUID::null;
}

// +=================================================+
// |        LLMeshBridge                             |
// +=================================================+

LLUIImagePtr LLMeshBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(LLAssetType::AT_MESH, LLInventoryType::IT_MESH, 0, FALSE);
}

void LLMeshBridge::openItem()
{
	LLViewerInventoryItem* item = getItem();
	
	if (item)
	{
		// open mesh
	}
}

void LLMeshBridge::previewItem()
{
	LLViewerInventoryItem* item = getItem();
	if(item)
	{
		// preview mesh
	}
}


void LLMeshBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	lldebugs << "LLMeshBridge::buildContextMenu()" << llendl;
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if(isItemInTrash())
	{
		items.push_back(std::string("Purge Item"));
		if (!isItemRemovable())
		{
			disabled_items.push_back(std::string("Purge Item"));
		}

		items.push_back(std::string("Restore Item"));
	}
	else
	{
		items.push_back(std::string("Properties"));

		getClipboardEntries(true, items, disabled_items, flags);
	}


	hide_context_entries(menu, items, disabled_items);
}

