/** 
 * @file llinventoryview.cpp
 * @brief Implementation of the inventory view and associated stuff.
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

#include "message.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llcallingcard.h"
#include "llcheckboxctrl.h"		// for radio buttons
#include "llradiogroup.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llui.h"

#include "llfirstuse.h"
#include "llfloateravatarinfo.h"
#include "llfloaterchat.h"
#include "llfloatercustomize.h"
#include "llfocusmgr.h"
#include "llfolderview.h"
#include "llgesturemgr.h"
#include "lliconctrl.h"
#include "llinventoryfunctions.h"
#include "llinventoryclipboard.h"
#include "llinventorymodelbackgroundfetch.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewlandmark.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "llresmgr.h"
#include "llscrollcontainer.h"
#include "llscrollbar.h"
#include "llimview.h"
#include "lltooldraganddrop.h"
#include "llviewertexturelist.h"
#include "llviewerinventory.h"
#include "llviewerassettype.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llwearablelist.h"
#include "llappviewer.h"
#include "llviewermessage.h"
#include "llviewerregion.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"
#include "llselectmgr.h"

#include "llsdserialize.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

static LLRegisterWidget<LLInventoryPanel> r("inventory_panel");

LLDynamicArray<LLInventoryView*> LLInventoryView::sActiveViews;

//BOOL LLInventoryView::sOpenNextNewItem = FALSE;
BOOL LLInventoryView::sWearNewClothing = FALSE;
LLUUID LLInventoryView::sWearNewClothingTransactionID;

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

const S32 INV_MIN_WIDTH = 240;
const S32 INV_MIN_HEIGHT = 150;
const S32 INV_FINDER_WIDTH = 160;
const S32 INV_FINDER_HEIGHT = 408;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryPanelObserver
//
// Bridge to support knowing when the inventory has changed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryPanelObserver : public LLInventoryObserver
{
public:
	LLInventoryPanelObserver(LLInventoryPanel* ip) : mIP(ip) {}
	virtual ~LLInventoryPanelObserver() {}
	virtual void changed(U32 mask)
	{
		mIP->modelChanged(mask);
	}
protected:
	LLInventoryPanel* mIP;
};

///----------------------------------------------------------------------------
/// LLInventoryViewFinder
///----------------------------------------------------------------------------

LLInventoryViewFinder::LLInventoryViewFinder(const std::string& name,
						const LLRect& rect,
						LLInventoryView* inventory_view) :
	LLFloater(name, rect, std::string("Filters"), RESIZE_NO,
				INV_FINDER_WIDTH, INV_FINDER_HEIGHT, DRAG_ON_TOP,
				MINIMIZE_NO, CLOSE_YES),
	mInventoryView(inventory_view),
	mFilter(inventory_view->mActivePanel->getFilter())
{

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inventory_view_finder.xml");

	childSetAction("All", selectAllTypes, this);
	childSetAction("None", selectNoTypes, this);

	mSpinSinceHours = getChild<LLSpinCtrl>("spin_hours_ago");
	childSetCommitCallback("spin_hours_ago", onTimeAgo, this);

	mSpinSinceDays = getChild<LLSpinCtrl>("spin_days_ago");
	childSetCommitCallback("spin_days_ago", onTimeAgo, this);

//	mCheckSinceLogoff   = getChild<LLSpinCtrl>("check_since_logoff");
	childSetCommitCallback("check_since_logoff", onCheckSinceLogoff, this);

	childSetAction("Close", onCloseBtn, this);

	updateElementsFromFilter();
}


void LLInventoryViewFinder::onCheckSinceLogoff(LLUICtrl *ctrl, void *user_data)
{
	LLInventoryViewFinder *self = (LLInventoryViewFinder *)user_data;
	if (!self) return;

	bool since_logoff= self->childGetValue("check_since_logoff");
	
	if (!since_logoff && 
	    !(  self->mSpinSinceDays->get() ||  self->mSpinSinceHours->get() ) )
	{
		self->mSpinSinceHours->set(1.0f);
	}	
}

void LLInventoryViewFinder::onTimeAgo(LLUICtrl *ctrl, void *user_data)
{
	LLInventoryViewFinder *self = (LLInventoryViewFinder *)user_data;
	if (!self) return;
	
	bool since_logoff=true;
	if ( self->mSpinSinceDays->get() ||  self->mSpinSinceHours->get() )
	{
		since_logoff = false;
	}
	self->childSetValue("check_since_logoff", since_logoff);
}

void LLInventoryViewFinder::changeFilter(LLInventoryFilter* filter)
{
	mFilter = filter;
	updateElementsFromFilter();
}

void LLInventoryViewFinder::updateElementsFromFilter()
{
	if (!mFilter)
		return;

	// Get data needed for filter display
	U32 filter_types = mFilter->getFilterTypes();
	std::string filter_string = mFilter->getFilterSubString();
	LLInventoryFilter::EFolderShow show_folders = mFilter->getShowFolderState();
	U32 hours = mFilter->getHoursAgo();

	// update the ui elements
	LLFloater::setTitle(mFilter->getName());
	childSetValue("check_animation", (S32) (filter_types & 0x1 << LLInventoryType::IT_ANIMATION));

	childSetValue("check_calling_card", (S32) (filter_types & 0x1 << LLInventoryType::IT_CALLINGCARD));
	childSetValue("check_clothing", (S32) (filter_types & 0x1 << LLInventoryType::IT_WEARABLE));
	childSetValue("check_gesture", (S32) (filter_types & 0x1 << LLInventoryType::IT_GESTURE));
	childSetValue("check_landmark", (S32) (filter_types & 0x1 << LLInventoryType::IT_LANDMARK));
	childSetValue("check_notecard", (S32) (filter_types & 0x1 << LLInventoryType::IT_NOTECARD));
	childSetValue("check_object", (S32) (filter_types & 0x1 << LLInventoryType::IT_OBJECT));
	childSetValue("check_script", (S32) (filter_types & 0x1 << LLInventoryType::IT_LSL));
	childSetValue("check_sound", (S32) (filter_types & 0x1 << LLInventoryType::IT_SOUND));
	childSetValue("check_texture", (S32) (filter_types & 0x1 << LLInventoryType::IT_TEXTURE));
	childSetValue("check_snapshot", (S32) (filter_types & 0x1 << LLInventoryType::IT_SNAPSHOT));
	childSetValue("check_show_empty", show_folders == LLInventoryFilter::SHOW_ALL_FOLDERS);
	childSetValue("check_since_logoff", mFilter->isSinceLogoff());
	mSpinSinceHours->set((F32)(hours % 24));
	mSpinSinceDays->set((F32)(hours / 24));
}

void LLInventoryViewFinder::draw()
{
	U32 filter = 0xffffffff;
	BOOL filtered_by_all_types = TRUE;

	if (!childGetValue("check_animation"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_ANIMATION);
		filtered_by_all_types = FALSE;
	}


	if (!childGetValue("check_calling_card"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_CALLINGCARD);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_clothing"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_WEARABLE);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_gesture"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_GESTURE);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_landmark"))


	{
		filter &= ~(0x1 << LLInventoryType::IT_LANDMARK);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_notecard"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_NOTECARD);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_object"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_OBJECT);
		filter &= ~(0x1 << LLInventoryType::IT_ATTACHMENT);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_script"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_LSL);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_sound"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_SOUND);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_texture"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_TEXTURE);
		filtered_by_all_types = FALSE;
	}

	if (!childGetValue("check_snapshot"))
	{
		filter &= ~(0x1 << LLInventoryType::IT_SNAPSHOT);
		filtered_by_all_types = FALSE;
	}

	if (!filtered_by_all_types)
	{
		// don't include folders in filter, unless I've selected everything
		filter &= ~(0x1 << LLInventoryType::IT_CATEGORY);
	}

	// update the panel, panel will update the filter
	mInventoryView->mActivePanel->setShowFolderState(getCheckShowEmpty() ?
		LLInventoryFilter::SHOW_ALL_FOLDERS : LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
	mInventoryView->mActivePanel->setFilterTypes(filter);
	if (getCheckSinceLogoff())
	{
		mSpinSinceDays->set(0);
		mSpinSinceHours->set(0);
	}
	U32 days = (U32)mSpinSinceDays->get();
	U32 hours = (U32)mSpinSinceHours->get();
	if (hours > 24)
	{
		days += hours / 24;
		hours = (U32)hours % 24;
		mSpinSinceDays->set((F32)days);
		mSpinSinceHours->set((F32)hours);
	}
	hours += days * 24;
	mInventoryView->mActivePanel->setHoursAgo(hours);
	mInventoryView->mActivePanel->setSinceLogoff(getCheckSinceLogoff());
	mInventoryView->setFilterTextFromFilter();

	LLFloater::draw();
}

void  LLInventoryViewFinder::onClose(bool app_quitting)
{
	if (mInventoryView) mInventoryView->getControl("Inventory.ShowFilters")->setValue(FALSE);
	// If you want to reset the filter on close, do it here.  This functionality was
	// hotly debated - Paulm
#if 0
	if (mInventoryView)
	{
		LLInventoryView::onResetFilter((void *)mInventoryView);
	}
#endif
	destroy();
}


BOOL LLInventoryViewFinder::getCheckShowEmpty()
{
	return childGetValue("check_show_empty");
}

BOOL LLInventoryViewFinder::getCheckSinceLogoff()
{
	return childGetValue("check_since_logoff");
}

void LLInventoryViewFinder::onCloseBtn(void* user_data)
{
	LLInventoryViewFinder* finderp = (LLInventoryViewFinder*)user_data;
	finderp->close();
}

// static
void LLInventoryViewFinder::selectAllTypes(void* user_data)
{
	LLInventoryViewFinder* self = (LLInventoryViewFinder*)user_data;
	if(!self) return;

	self->childSetValue("check_animation", TRUE);
	self->childSetValue("check_calling_card", TRUE);
	self->childSetValue("check_clothing", TRUE);
	self->childSetValue("check_gesture", TRUE);
	self->childSetValue("check_landmark", TRUE);
	self->childSetValue("check_notecard", TRUE);
	self->childSetValue("check_object", TRUE);
	self->childSetValue("check_script", TRUE);
	self->childSetValue("check_sound", TRUE);
	self->childSetValue("check_texture", TRUE);
	self->childSetValue("check_snapshot", TRUE);

/*
	self->mCheckCallingCard->set(TRUE);
	self->mCheckClothing->set(TRUE);
	self->mCheckGesture->set(TRUE);
	self->mCheckLandmark->set(TRUE);
	self->mCheckNotecard->set(TRUE);
	self->mCheckObject->set(TRUE);
	self->mCheckScript->set(TRUE);
	self->mCheckSound->set(TRUE);
	self->mCheckTexture->set(TRUE);
	self->mCheckSnapshot->set(TRUE);*/
}

//static
void LLInventoryViewFinder::selectNoTypes(void* user_data)
{
	LLInventoryViewFinder* self = (LLInventoryViewFinder*)user_data;
	if(!self) return;

	/*
	self->childSetValue("check_animation", FALSE);
	self->mCheckCallingCard->set(FALSE);
	self->mCheckClothing->set(FALSE);
	self->mCheckGesture->set(FALSE);
	self->mCheckLandmark->set(FALSE);
	self->mCheckNotecard->set(FALSE);
	self->mCheckObject->set(FALSE);
	self->mCheckScript->set(FALSE);
	self->mCheckSound->set(FALSE);
	self->mCheckTexture->set(FALSE);
	self->mCheckSnapshot->set(FALSE);*/


	self->childSetValue("check_animation", FALSE);
	self->childSetValue("check_calling_card", FALSE);
	self->childSetValue("check_clothing", FALSE);
	self->childSetValue("check_gesture", FALSE);
	self->childSetValue("check_landmark", FALSE);
	self->childSetValue("check_notecard", FALSE);
	self->childSetValue("check_object", FALSE);
	self->childSetValue("check_script", FALSE);
	self->childSetValue("check_sound", FALSE);
	self->childSetValue("check_texture", FALSE);
	self->childSetValue("check_snapshot", FALSE);
}


///----------------------------------------------------------------------------
/// LLInventoryView
///----------------------------------------------------------------------------
// Default constructor
LLInventoryView::LLInventoryView(const std::string& name,
								 const std::string& rect,
								 LLInventoryModel* inventory) :
	LLFloater(name, rect, std::string("Inventory"), RESIZE_YES,
			  INV_MIN_WIDTH, INV_MIN_HEIGHT, DRAG_ON_TOP,
			  MINIMIZE_NO, CLOSE_YES),
	mActivePanel(NULL)
	//LLHandle<LLFloater> mFinderHandle takes care of its own initialization
{
	init(inventory);
}

LLInventoryView::LLInventoryView(const std::string& name,
								 const LLRect& rect,
								 LLInventoryModel* inventory) :
	LLFloater(name, rect, std::string("Inventory"), RESIZE_YES,
			  INV_MIN_WIDTH, INV_MIN_HEIGHT, DRAG_ON_TOP,
			  MINIMIZE_NO, CLOSE_YES),
	mActivePanel(NULL)
	//LLHandle<LLFloater> mFinderHandle takes care of its own initialization
{
	init(inventory);
	setRect(rect); // override XML
}


void LLInventoryView::init(LLInventoryModel* inventory)
{
	// Callbacks
	init_inventory_actions(this);

	// Controls
	addBoolControl("Inventory.ShowFilters", FALSE);
	addBoolControl("Inventory.SortByName", FALSE);
	addBoolControl("Inventory.SortByDate", TRUE);
	addBoolControl("Inventory.FoldersAlwaysByName", TRUE);
	addBoolControl("Inventory.SystemFoldersToTop", TRUE);
	updateSortControls();

	addBoolControl("Inventory.SearchName", TRUE);
	addBoolControl("Inventory.SearchDesc", FALSE);
	addBoolControl("Inventory.SearchCreator", FALSE);

	mSavedFolderState = new LLSaveFolderState();
	mSavedFolderState->setApply(FALSE);

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_inventory.xml", NULL);

	mFilterTabs = (LLTabContainer*)getChild<LLTabContainer>("inventory filter tabs");

	// Set up the default inv. panel/filter settings.
	mActivePanel = getChild<LLInventoryPanel>("All Items");
	if (mActivePanel)
	{
		// "All Items" is the previous only view, so it gets the InventorySortOrder
		mActivePanel->setSortOrder(gSavedSettings.getU32(LLInventoryPanel::DEFAULT_SORT_ORDER));
		mActivePanel->getFilter()->markDefault();
		mActivePanel->getRootFolder()->applyFunctorRecursively(*mSavedFolderState);
		mActivePanel->setSelectCallback(onSelectionChange, mActivePanel);
	}
	LLInventoryPanel* recent_items_panel = getChild<LLInventoryPanel>("Recent Items");
	if (recent_items_panel)
	{
		recent_items_panel->setSinceLogoff(TRUE);
		recent_items_panel->setSortOrder(gSavedSettings.getU32(LLInventoryPanel::RECENTITEMS_SORT_ORDER));
		recent_items_panel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
		recent_items_panel->getFilter()->markDefault();
		recent_items_panel->setSelectCallback(onSelectionChange, recent_items_panel);
	}
	LLInventoryPanel* worn_items_panel = getChild<LLInventoryPanel>("Worn Items");
	if (worn_items_panel)
	{
		worn_items_panel->setSortOrder(gSavedSettings.getU32(LLInventoryPanel::WORNITEMS_SORT_ORDER));
		worn_items_panel->setShowFolderState(LLInventoryFilter::SHOW_NON_EMPTY_FOLDERS);
		worn_items_panel->getFilter()->markDefault();
		worn_items_panel->setFilterWorn(true);
		worn_items_panel->setSelectCallback(onSelectionChange, worn_items_panel);
	}

	// Now load the stored settings from disk, if available.
	std::ostringstream filterSaveName;
	filterSaveName << gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "filters.xml");
	llinfos << "LLInventoryView::init: reading from " << filterSaveName << llendl;
	llifstream file(filterSaveName.str());
	LLSD savedFilterState;
	if (file.is_open())
	{
		LLSDSerialize::fromXML(savedFilterState, file);
		file.close();

		// Load the persistent "Recent Items" settings.
		// Note that the "All Items" and "Worn Items" settings do not persist per-account.
		if(recent_items_panel)
		{
			if(savedFilterState.has(recent_items_panel->getFilter()->getName()))
			{
				LLSD recent_items = savedFilterState.get(
					recent_items_panel->getFilter()->getName());
				recent_items_panel->getFilter()->fromLLSD(recent_items);
			}
		}
	}


	mSearchEditor = getChild<LLSearchEditor>("inventory search editor");
	if (mSearchEditor)
	{
		mSearchEditor->setSearchCallback(onSearchEdit, this);
	}

	mQuickFilterCombo = getChild<LLComboBox>("Quick Filter");

	if (mQuickFilterCombo)
	{
		mQuickFilterCombo->setCommitCallback(onQuickFilterCommit);
	}


	sActiveViews.put(this);

	gInventory.addObserver(this);
}

BOOL LLInventoryView::postBuild()
{
	childSetTabChangeCallback("inventory filter tabs", "All Items", onFilterSelected, this);
	childSetTabChangeCallback("inventory filter tabs", "Recent Items", onFilterSelected, this);
	childSetTabChangeCallback("inventory filter tabs", "Worn Items", onFilterSelected, this);

	childSetAction("Inventory.ResetAll",onResetAll,this);
	childSetAction("Inventory.ExpandAll",onExpandAll,this);
	childSetAction("collapse_btn", onCollapseAll, this);

	//panel->getFilter()->markDefault();
	return TRUE;
}

// Destroys the object
LLInventoryView::~LLInventoryView( void )
{
	// Save the filters state.
	LLSD filterRoot;
	LLInventoryPanel* all_items_panel = getChild<LLInventoryPanel>("All Items");
	if (all_items_panel)
	{
		LLInventoryFilter* filter = all_items_panel->getFilter();
		LLSD filterState;
		filter->toLLSD(filterState);
		filterRoot[filter->getName()] = filterState;
	}

	LLInventoryPanel* recent_items_panel = getChild<LLInventoryPanel>("Recent Items");
	if (recent_items_panel)
	{
		LLInventoryFilter* filter = recent_items_panel->getFilter();
		LLSD filterState;
		filter->toLLSD(filterState);
		filterRoot[filter->getName()] = filterState;
	}
	
	LLInventoryPanel* worn_items_panel = getChild<LLInventoryPanel>("Worn Items");
	if (worn_items_panel)
	{
		LLInventoryFilter* filter = worn_items_panel->getFilter();
		LLSD filterState;
		filter->toLLSD(filterState);
		filterRoot[filter->getName()] = filterState;
	}

	std::ostringstream filterSaveName;
	filterSaveName << gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "filters.xml");
	llofstream filtersFile(filterSaveName.str());
	if(!LLSDSerialize::toPrettyXML(filterRoot, filtersFile))
	{
		llwarns << "Could not write to filters save file " << filterSaveName << llendl;
	}
	else
		filtersFile.close();

	sActiveViews.removeObj(this);
	gInventory.removeObserver(this);
	delete mSavedFolderState;
}

void LLInventoryView::draw()
{
 	if (LLInventoryModelBackgroundFetch::instance().isEverythingFetched())
	{
		LLLocale locale(LLLocale::USER_LOCALE);
		std::ostringstream title;
		title << "Inventory";
		std::string item_count_string;
		LLResMgr::getInstance()->getIntegerString(item_count_string, gInventory.getItemCount());
		title << " (" << item_count_string << " items)";
		title << mFilterText;
		setTitle(title.str());
	}
	if (mActivePanel && mSearchEditor)
	{
		mSearchEditor->setText(mActivePanel->getFilterSubString());
	}

        if (mActivePanel && mQuickFilterCombo)
        {
                refreshQuickFilter( mQuickFilterCombo );
        }

	LLFloater::draw();
}

void LLInventoryView::startSearch()
{
	// this forces focus to line editor portion of search editor
	if (mSearchEditor)
	{
		mSearchEditor->focusFirstItem(TRUE);
	}
}

// virtual, from LLView
void LLInventoryView::setVisible( BOOL visible )
{
	gSavedSettings.setBOOL("ShowInventory", visible);
	LLFloater::setVisible(visible);
}

// Destroy all but the last floater, which is made invisible.
void LLInventoryView::onClose(bool app_quitting)
{
//	S32 count = sActiveViews.count();
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
	// See LLInventoryView::closeAll() on why we're doing it this way
	S32 count = 0;
	for (S32 idx = 0, cnt = sActiveViews.count(); idx < cnt; idx++)
	{
		if (!sActiveViews.get(idx)->isDead())
			count++;
	}
// [/RLVa:KB]

	if (count > 1)
	{
		destroy();
	}
	else
	{
		if (!app_quitting)
		{
			gSavedSettings.setBOOL("ShowInventory", FALSE);
		}
		// clear filters, but save user's folder state first
		if (!mActivePanel->getRootFolder()->isFilterModified())
		{
			mSavedFolderState->setApply(FALSE);
			mActivePanel->getRootFolder()->applyFunctorRecursively(*mSavedFolderState);
		}
		
		// onClearSearch(this);

		// pass up
		LLFloater::setVisible(FALSE);
	}
}

BOOL LLInventoryView::handleKeyHere(KEY key, MASK mask)
{
	LLFolderView* root_folder = mActivePanel ? mActivePanel->getRootFolder() : NULL;
	if (root_folder)
	{
		// first check for user accepting current search results
		if (mSearchEditor 
			&& mSearchEditor->hasFocus()
		    && (key == KEY_RETURN 
		    	|| key == KEY_DOWN)
		    && mask == MASK_NONE)
		{
			// move focus to inventory proper
			root_folder->setFocus(TRUE);
			root_folder->scrollToShowSelection();
			return TRUE;
		}

		if (root_folder->hasFocus() && key == KEY_UP)
		{
			startSearch();
		}
	}

	return LLFloater::handleKeyHere(key, mask);

}

void LLInventoryView::changed(U32 mask)
{
	std::ostringstream title;
	title << "Inventory";
 	if (LLInventoryModelBackgroundFetch::instance().backgroundFetchActive())
	{
		LLLocale locale(LLLocale::USER_LOCALE);
		std::string item_count_string;
		LLResMgr::getInstance()->getIntegerString(item_count_string, gInventory.getItemCount());
		title << " (Fetched " << item_count_string << " items...)";
	}
	title << mFilterText;
	setTitle(title.str());

}

// static
// *TODO: remove take_keyboard_focus param
LLInventoryView* LLInventoryView::showAgentInventory(BOOL take_keyboard_focus)
{
	if (gDisconnected || gNoRender)
	{
		return NULL;
	}

// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g)
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV))
	{
		return NULL;
	}
// [/RLVa:KB]

	LLInventoryView* iv = LLInventoryView::getActiveInventory();
#if 0 && !LL_RELEASE_FOR_DOWNLOAD
	if (sActiveViews.count() == 1)
	{
		delete iv;
		iv = NULL;
	}
#endif
	if(!iv && !gAgentCamera.cameraMouselook())
	{
		// create one.
		iv = new LLInventoryView(std::string("Inventory"),
								 std::string("FloaterInventoryRect"),
								 &gInventory);
		iv->open();
		// keep onscreen
		gFloaterView->adjustToFitScreen(iv, FALSE);

		gSavedSettings.setBOOL("ShowInventory", TRUE);
	}
	if(iv)
	{
		// Make sure it's in front and it makes a noise
		iv->setTitle(std::string("Inventory"));
		iv->open();		/*Flawfinder: ignore*/
	}
	//if (take_keyboard_focus)
	//{
	//	iv->startSearch();
	//	gFocusMgr.triggerFocusFlash();
	//}
	return iv;
}

// static
LLInventoryView* LLInventoryView::getActiveInventory()
{
	LLInventoryView* iv = NULL;
	S32 count = sActiveViews.count();
	if(count > 0)
	{
		iv = sActiveViews.get(0);
		S32 z_order = gFloaterView->getZOrder(iv);
		S32 z_next = 0;
		LLInventoryView* next_iv = NULL;
		for(S32 i = 1; i < count; ++i)
		{
			next_iv = sActiveViews.get(i);
			z_next = gFloaterView->getZOrder(next_iv);
			if(z_next < z_order)
			{
				iv = next_iv;
				z_order = z_next;
			}
		}
	}
	return iv;
}

// static
void LLInventoryView::toggleVisibility()
{
	S32 count = sActiveViews.count();
	if (0 == count)
	{
		showAgentInventory(TRUE);
	}
	else if (1 == count)
	{
		if (sActiveViews.get(0)->getVisible())
		{
			sActiveViews.get(0)->close();
			gSavedSettings.setBOOL("ShowInventory", FALSE);
		}
		else
		{
			showAgentInventory(TRUE);
		}
	}
	else
	{
		// With more than one open, we know at least one
		// is visible.

		// Close all the last one spawned.
		S32 last_index = sActiveViews.count() - 1;
		sActiveViews.get(last_index)->close();
	}
}

// static
void LLInventoryView::cleanup()
{
	S32 count = sActiveViews.count();
	for (S32 i = 0; i < count; i++)
	{
		sActiveViews.get(i)->destroy();
	}
}

void LLInventoryView::toggleFindOptions()
{
	LLFloater *floater = getFinder();
	if (!floater)
	{
		LLInventoryViewFinder * finder = new LLInventoryViewFinder(std::string("Inventory Finder"),
										LLRect(getRect().mLeft - INV_FINDER_WIDTH, getRect().mTop, getRect().mLeft, getRect().mTop - INV_FINDER_HEIGHT),
										this);
		mFinderHandle = finder->getHandle();
		finder->open();		/*Flawfinder: ignore*/
		addDependentFloater(mFinderHandle);

		// start background fetch of folders
		LLInventoryModelBackgroundFetch::instance().start();

		mFloaterControls[std::string("Inventory.ShowFilters")]->setValue(TRUE);
	}
	else
	{
		floater->close();

		mFloaterControls[std::string("Inventory.ShowFilters")]->setValue(FALSE);
	}
}

void LLInventoryView::updateSortControls()
{
	U32 order = mActivePanel ? mActivePanel->getSortOrder() : gSavedSettings.getU32("InventorySortOrder");
	bool sort_by_date = order & LLInventoryFilter::SO_DATE;
	bool folders_by_name = order & LLInventoryFilter::SO_FOLDERS_BY_NAME;
	bool sys_folders_on_top = order & LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;

	getControl("Inventory.SortByDate")->setValue(sort_by_date);
	getControl("Inventory.SortByName")->setValue(!sort_by_date);
	getControl("Inventory.FoldersAlwaysByName")->setValue(folders_by_name);
	getControl("Inventory.SystemFoldersToTop")->setValue(sys_folders_on_top);
}

// static
BOOL LLInventoryView::filtersVisible(void* user_data)
{
	LLInventoryView* self = (LLInventoryView*)user_data;
	if(!self) return FALSE;

	return self->getFinder() != NULL;
}

// static
void LLInventoryView::onClearSearch(void* user_data)
{
	LLInventoryView* self = (LLInventoryView*)user_data;
	if(!self) return;

	LLFloater *finder = self->getFinder();
	if (self->mActivePanel)
	{
		self->mActivePanel->setFilterSubString(LLStringUtil::null);
		self->mActivePanel->setFilterTypes(0xffffffff);
	}

	if (finder)
	{
		LLInventoryViewFinder::selectAllTypes(finder);
	}

	// re-open folders that were initially open
	if (self->mActivePanel)
	{
		self->mSavedFolderState->setApply(TRUE);
		self->mActivePanel->getRootFolder()->applyFunctorRecursively(*self->mSavedFolderState);
		LLOpenFoldersWithSelection opener;
		self->mActivePanel->getRootFolder()->applyFunctorRecursively(opener);
		self->mActivePanel->getRootFolder()->scrollToShowSelection();
	}
}

//static
void LLInventoryView::onSearchEdit(const std::string& search_string, void* user_data )
{
	if (search_string == "")
	{
		onClearSearch(user_data);
	}
	LLInventoryView* self = (LLInventoryView*)user_data;
	if (!self->mActivePanel)
	{
		return;
	}

	LLInventoryModelBackgroundFetch::instance().start();

	std::string filter_text = search_string;
	std::string uppercase_search_string = filter_text;
	LLStringUtil::toUpper(uppercase_search_string);
	if (self->mActivePanel->getFilterSubString().empty() && uppercase_search_string.empty())
	{
			// current filter and new filter empty, do nothing
			return;
	}

	// save current folder open state if no filter currently applied
	if (!self->mActivePanel->getRootFolder()->isFilterModified())
	{
		self->mSavedFolderState->setApply(FALSE);
		self->mActivePanel->getRootFolder()->applyFunctorRecursively(*self->mSavedFolderState);
	}

	// set new filter string
	self->mActivePanel->setFilterSubString(uppercase_search_string);
}

//static
void LLInventoryView::onQuickFilterCommit(LLUICtrl* ctrl, void* user_data)
{

	LLComboBox* quickfilter = (LLComboBox*)ctrl;


	LLInventoryView* view = (LLInventoryView*)(quickfilter->getParent());
	if (!view->mActivePanel)
	{
		return;
	}


	std::string item_type = quickfilter->getSimple();
	U32 filter_type;

	if (view->getString("filter_type_animation") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_ANIMATION;
	}

	else if (view->getString("filter_type_callingcard") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_CALLINGCARD;
	}

	else if (view->getString("filter_type_wearable") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_WEARABLE;
	}

	else if (view->getString("filter_type_gesture") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_GESTURE;
	}

	else if (view->getString("filter_type_landmark") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_LANDMARK;
	}

	else if (view->getString("filter_type_notecard") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_NOTECARD;
	}

	else if (view->getString("filter_type_object") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_OBJECT;
	}

	else if (view->getString("filter_type_script") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_LSL;
	}

	else if (view->getString("filter_type_sound") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_SOUND;
	}

	else if (view->getString("filter_type_texture") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_TEXTURE;
	}

	else if (view->getString("filter_type_snapshot") == item_type)
	{
		filter_type = 0x1 << LLInventoryType::IT_SNAPSHOT;
	}

	else if (view->getString("filter_type_custom") == item_type)
	{
		// When they select custom, show the floater then return
		if( !(view->filtersVisible(view)) )
		{
			view->toggleFindOptions();
		}
		return;
	}

	else if (view->getString("filter_type_all") == item_type)
	{
		// Show all types
		filter_type = 0xffffffff;
	}

	else
	{
		llwarns << "Ignoring unknown filter: " << item_type << llendl;
		return;
	}

	view->mActivePanel->setFilterTypes( filter_type );


	// Force the filters window to update itself, if it's open.
	LLInventoryViewFinder* finder = view->getFinder();
	if( finder )
	{
		finder->updateElementsFromFilter();
	}

	// llinfos << "Quick Filter: " << item_type << llendl;

}



//static
void LLInventoryView::refreshQuickFilter(LLUICtrl* ctrl)
{

	LLInventoryView* view = (LLInventoryView*)(ctrl->getParent());
	if (!view->mActivePanel)
	{
		return;
	}

	LLComboBox* quickfilter = view->getChild<LLComboBox>("Quick Filter");
	if (!quickfilter)
	{
		return;
	}


	U32 filter_type = view->mActivePanel->getFilterTypes();


  // Mask to extract only the bit fields we care about.
  // *TODO: There's probably a cleaner way to construct this mask.
  U32 filter_mask = 0;
  filter_mask |= (0x1 << LLInventoryType::IT_ANIMATION);
  filter_mask |= (0x1 << LLInventoryType::IT_CALLINGCARD);
  filter_mask |= (0x1 << LLInventoryType::IT_WEARABLE);
  filter_mask |= (0x1 << LLInventoryType::IT_GESTURE);
  filter_mask |= (0x1 << LLInventoryType::IT_LANDMARK);
  filter_mask |= (0x1 << LLInventoryType::IT_NOTECARD);
  filter_mask |= (0x1 << LLInventoryType::IT_OBJECT);
  filter_mask |= (0x1 << LLInventoryType::IT_LSL);
  filter_mask |= (0x1 << LLInventoryType::IT_SOUND);
  filter_mask |= (0x1 << LLInventoryType::IT_TEXTURE);
  filter_mask |= (0x1 << LLInventoryType::IT_SNAPSHOT);


  filter_type &= filter_mask;


  //llinfos << "filter_type: " << filter_type << llendl;

	std::string selection;


	if (filter_type == filter_mask)
	{
		selection = view->getString("filter_type_all");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_ANIMATION))
	{
		selection = view->getString("filter_type_animation");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_CALLINGCARD))
	{
		selection = view->getString("filter_type_callingcard");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_WEARABLE))
	{
		selection = view->getString("filter_type_wearable");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_GESTURE))
	{
		selection = view->getString("filter_type_gesture");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_LANDMARK))
	{
		selection = view->getString("filter_type_landmark");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_NOTECARD))
	{
		selection = view->getString("filter_type_notecard");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_OBJECT))
	{
		selection = view->getString("filter_type_object");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_LSL))
	{
		selection = view->getString("filter_type_script");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_SOUND))
	{
		selection = view->getString("filter_type_sound");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_TEXTURE))
	{
		selection = view->getString("filter_type_texture");
	}

	else if (filter_type == (0x1 << LLInventoryType::IT_SNAPSHOT))
	{
		selection = view->getString("filter_type_snapshot");
	}

	else
	{
		selection = view->getString("filter_type_custom");
	}


	// Select the chosen item by label text
	BOOL result = quickfilter->setSimple( (selection) );

  if( !result )
  {
    llinfos << "The item didn't exist: " << selection << llendl;
  }

}



// static
// BOOL LLInventoryView::incrementalFind(LLFolderViewItem* first_item, const char *find_text, BOOL backward)
// {
// 	LLInventoryView* active_view = NULL;

// 	for (S32 i = 0; i < sActiveViews.count(); i++)
// 	{
// 		if (gFocusMgr.childHasKeyboardFocus(sActiveViews[i]))
// 		{
// 			active_view = sActiveViews[i];
// 			break;
// 		}
// 	}

// 	if (!active_view)
// 	{
// 		return FALSE;
// 	}

// 	std::string search_string(find_text);

// 	if (search_string.empty())
// 	{
// 		return FALSE;
// 	}

// 	if (active_view->mActivePanel &&
// 		active_view->mActivePanel->getRootFolder()->search(first_item, search_string, backward))
// 	{
// 		return TRUE;
// 	}

// 	return FALSE;
// }

void LLInventoryView::onResetAll(void* userdata)
{
	LLInventoryView* self = (LLInventoryView*) userdata;
	self->mActivePanel = (LLInventoryPanel*)self->childGetVisibleTab("inventory filter tabs");

	if (!self->mActivePanel)
	{
		return;
	}
	if (self->mActivePanel && self->mSearchEditor)
	{
		self->mSearchEditor->setText(LLStringUtil::null);
	}
	self->onSearchEdit("",userdata);
	self->mActivePanel->closeAllFolders();
}

//static
void LLInventoryView::onExpandAll(void* userdata)
{
	LLInventoryView* self = (LLInventoryView*) userdata;
	self->mActivePanel = (LLInventoryPanel*)self->childGetVisibleTab("inventory filter tabs");

	if (!self->mActivePanel)
	{
		return;
	}
	self->mActivePanel->openAllFolders();
}


//static
void LLInventoryView::onCollapseAll(void* userdata)
{
	LLInventoryView* self = (LLInventoryView*) userdata;
	self->mActivePanel = (LLInventoryPanel*)self->childGetVisibleTab("inventory filter tabs");

	if (!self->mActivePanel)
	{
		return;
	}
	self->mActivePanel->closeAllFolders();
}

//static
void LLInventoryView::onFilterSelected(void* userdata, bool from_click)
{
	LLInventoryView* self = (LLInventoryView*) userdata;
	LLInventoryFilter* filter;

	LLInventoryViewFinder *finder = self->getFinder();
	// Find my index
	self->mActivePanel = (LLInventoryPanel*)self->childGetVisibleTab("inventory filter tabs");

	if (!self->mActivePanel)
	{
		return;
	}
	filter = self->mActivePanel->getFilter();
	if (finder)
	{
		finder->changeFilter(filter);
	}
	if (filter->isActive())
	{
		// If our filter is active we may be the first thing requiring a fetch so we better start it here.
		LLInventoryModelBackgroundFetch::instance().start();
	}
	self->setFilterTextFromFilter();
	self->updateSortControls();
}

// static
void LLInventoryView::onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action, void* data)
{
	LLInventoryPanel* panel = (LLInventoryPanel*)data;
	LLFolderView* fv = panel->getRootFolder();
	if (fv->needsAutoRename()) // auto-selecting a new user-created asset and preparing to rename
	{
		fv->setNeedsAutoRename(FALSE);
		if (items.size()) // new asset is visible and selected
		{
			fv->startRenamingSelectedItem();
		}
	}
}

BOOL LLInventoryView::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
										 EDragAndDropType cargo_type,
										 void* cargo_data,
										 EAcceptance* accept,
										 std::string& tooltip_msg)
{
	// Check to see if we are auto scrolling from the last frame
	LLInventoryPanel* panel = (LLInventoryPanel*)this->getActivePanel();
	BOOL needsToScroll = panel->getScrollableContainer()->needsToScroll(x, y, LLScrollableContainerView::VERTICAL);
	if(mFilterTabs)
	{
		if(needsToScroll)
		{
			mFilterTabs->startDragAndDropDelayTimer();
		}
	}
	
	BOOL handled = LLFloater::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);

	return handled;
}

const std::string LLInventoryPanel::DEFAULT_SORT_ORDER = std::string("InventorySortOrder");
const std::string LLInventoryPanel::RECENTITEMS_SORT_ORDER = std::string("RecentItemsSortOrder");
const std::string LLInventoryPanel::WORNITEMS_SORT_ORDER = std::string("WornItemsSortOrder");
const std::string LLInventoryPanel::INHERIT_SORT_ORDER = std::string("");

LLInventoryPanel::LLInventoryPanel(const std::string& name,
								    const std::string& sort_order_setting,
									const LLRect& rect,
									LLInventoryModel* inventory,
									BOOL allow_multi_select,
									LLView *parent_view) :
	LLPanel(name, rect, TRUE),
	mInventory(inventory),
	mInventoryObserver(NULL),
	mFolderRoot(NULL),
	mScroller(NULL),
	mAllowMultiSelect(allow_multi_select),
	mSortOrderSetting(sort_order_setting)
{
	setBackgroundColor(gColors.getColor("InventoryBackgroundColor"));
	setBackgroundVisible(TRUE);
	setBackgroundOpaque(TRUE);
}

BOOL LLInventoryPanel::postBuild()
{
	init_inventory_panel_actions(this);

	LLRect folder_rect(0,
					   0,
					   getRect().getWidth(),
					   0);
	mFolderRoot = new LLFolderView(getName(), NULL, folder_rect, LLUUID::null, this);
	mFolderRoot->setAllowMultiSelect(mAllowMultiSelect);

	// scroller
	LLRect scroller_view_rect = getRect();
	scroller_view_rect.translate(-scroller_view_rect.mLeft, -scroller_view_rect.mBottom);
	mScroller = new LLScrollableContainerView(std::string("Inventory Scroller"),
											   scroller_view_rect,
											  mFolderRoot);
	mScroller->setFollowsAll();
	mScroller->setReserveScrollCorner(TRUE);
	addChild(mScroller);
	mFolderRoot->setScrollContainer(mScroller);

	// set up the callbacks from the inventory we're viewing, and then
	// build everything.
	mInventoryObserver = new LLInventoryPanelObserver(this);
	mInventory->addObserver(mInventoryObserver);
	rebuildViewsFor(LLUUID::null, LLInventoryObserver::ADD);

	// bit of a hack to make sure the inventory is open.
	mFolderRoot->openFolder(std::string("My Inventory"));

	if (mSortOrderSetting != INHERIT_SORT_ORDER)
	{
		setSortOrder(gSavedSettings.getU32(mSortOrderSetting));
	}
	else
	{
		setSortOrder(gSavedSettings.getU32(DEFAULT_SORT_ORDER));
	}
	mFolderRoot->setSortOrder(mFolderRoot->getFilter()->getSortOrder());


	return TRUE;
}

LLInventoryPanel::~LLInventoryPanel()
{
	if (mFolderRoot)
	{
		U32 sort_order = mFolderRoot->getSortOrder();
		if (mSortOrderSetting != INHERIT_SORT_ORDER)
		{
			gSavedSettings.setU32(mSortOrderSetting, sort_order);
		}
	}

	// LLView destructor will take care of the sub-views.
	mInventory->removeObserver(mInventoryObserver);
	delete mInventoryObserver;
	mScroller = NULL;
}

// virtual
LLXMLNodePtr LLInventoryPanel::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLPanel::getXML(false); // Do not print out children

	node->setName(LL_INVENTORY_PANEL_TAG);

		node->createChild("allow_multi_select", TRUE)->setBoolValue(mFolderRoot->getAllowMultiSelect());

	return node;
}

LLView* LLInventoryPanel::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLInventoryPanel* panel;

	std::string name("inventory_panel");
	node->getAttributeString("name", name);

	BOOL allow_multi_select = TRUE;
	node->getAttributeBOOL("allow_multi_select", allow_multi_select);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	std::string sort_order(INHERIT_SORT_ORDER);
	node->getAttributeString("sort_order", sort_order);

	panel = new LLInventoryPanel(name, sort_order,
								 rect, &gInventory,
								 allow_multi_select, parent);

	panel->initFromXML(node, parent);

	panel->postBuild();

	return panel;
}

void LLInventoryPanel::draw()
{
	// select the desired item (in case it wasn't loaded when the selection was requested)
	if (mSelectThisID.notNull())
	{
		setSelection(mSelectThisID, false);
	}
	LLPanel::draw();
}

LLInventoryFilter* LLInventoryPanel::getFilter()
{
	if (mFolderRoot)
	{
		return mFolderRoot->getFilter();
	}
	return NULL;
}

const LLInventoryFilter* LLInventoryPanel::getFilter() const
{
	if (mFolderRoot)
	{
		return mFolderRoot->getFilter();
	}
	return NULL;
}

void LLInventoryPanel::setFilterTypes(U32 filter_types)
{
	getFilter()->setFilterTypes(filter_types);
}	

U32 LLInventoryPanel::getFilterTypes() const 
{ 
	return mFolderRoot->getFilterTypes(); 
}

U32 LLInventoryPanel::getFilterPermMask() const 
{ 
	return mFolderRoot->getFilterPermissions(); 
}

void LLInventoryPanel::setFilterPermMask(PermissionMask filter_perm_mask)
{
	getFilter()->setFilterPermissions(filter_perm_mask);
}

void LLInventoryPanel::setFilterWorn(bool worn)
{
	getFilter()->setFilterWorn(worn);
}

void LLInventoryPanel::setFilterSubString(const std::string& string)
{
	getFilter()->setFilterSubString(string);
}

const std::string LLInventoryPanel::getFilterSubString() 
{ 
	return mFolderRoot->getFilterSubString(); 
}

void LLInventoryPanel::setSortOrder(U32 order)
{
	getFilter()->setSortOrder(order);
	if (getFilter()->isModified())
	{
		mFolderRoot->setSortOrder(order);
		// try to keep selection onscreen, even if it wasn't to start with
		mFolderRoot->scrollToShowSelection();
	}
}

U32 LLInventoryPanel::getSortOrder() const 
{ 
	return mFolderRoot->getSortOrder(); 
}

void LLInventoryPanel::setSinceLogoff(BOOL sl)
{
	getFilter()->setDateRangeLastLogoff(sl);
}

void LLInventoryPanel::setHoursAgo(U32 hours)
{
	getFilter()->setHoursAgo(hours);
}

void LLInventoryPanel::setShowFolderState(LLInventoryFilter::EFolderShow show)
{
	getFilter()->setShowFolderState(show);
}

LLInventoryFilter::EFolderShow LLInventoryPanel::getShowFolderState()
{
	return getFilter()->getShowFolderState();
}

void LLInventoryPanel::modelChanged(U32 mask)
{
	LLFastTimer t2(LLFastTimer::FTM_REFRESH);

	bool handled = false;

	//if (!mViewsInitialized) return;

	const LLInventoryModel* model = getModel();
	if (!model) return;

	const LLInventoryModel::changed_items_t& changed_items = model->getChangedIDs();
	if (changed_items.empty()) return;

	for (LLInventoryModel::changed_items_t::const_iterator items_iter = changed_items.begin();
		 items_iter != changed_items.end();
		 ++items_iter)
	{
		const LLUUID& item_id = (*items_iter);
		const LLInventoryObject* model_item = model->getObject(item_id);
		LLFolderViewItem* view_item = mFolderRoot->getItemByID(item_id);

		// LLFolderViewFolder is derived from LLFolderViewItem so dynamic_cast from item
		// to folder is the fast way to get a folder without searching through folders tree.
		//LLFolderViewFolder* view_folder = dynamic_cast<LLFolderViewFolder*>(view_item);

		//////////////////////////////
		// LABEL Operation
		// Empty out the display name for relabel.
		if (mask & LLInventoryObserver::LABEL)
		{
			handled = true;
			if (view_item)
			{
				// Request refresh on this item (also flags for filtering)
				LLInvFVBridge* bridge = (LLInvFVBridge*)view_item->getListener();
				if(bridge)
				{	// Clear the display name first, so it gets properly re-built during refresh()
					bridge->clearDisplayName();

					view_item->refresh();
				}
			}
		}

		//////////////////////////////
		// REBUILD Operation
		// Destroy and regenerate the UI.
		/*if (mask & LLInventoryObserver::REBUILD)
		{
			handled = true;
			if (model_item && view_item)
			{
				view_item->destroyView();
			}
			view_item = buildNewViews(item_id);
			view_folder = dynamic_cast<LLFolderViewFolder *>(view_item);
		}*/

		//////////////////////////////
		// INTERNAL Operation
		// This could be anything.  For now, just refresh the item.
		if (mask & LLInventoryObserver::INTERNAL)
		{
			if (view_item)
			{
				view_item->refresh();
			}
		}

		//////////////////////////////
		// SORT Operation
		// Sort the folder.
		/*if (mask & LLInventoryObserver::SORT)
		{
			if (view_folder)
			{
				view_folder->requestSort();
			}
		}*/

		// We don't typically care which of these masks the item is actually flagged with, since the masks
		// may not be accurate (e.g. in the main inventory panel, I move an item from My Inventory into
		// Landmarks; this is a STRUCTURE change for that panel but is an ADD change for the Landmarks
		// panel).  What's relevant is that the item and UI are probably out of sync and thus need to be
		// resynchronized.
		if (mask & (LLInventoryObserver::STRUCTURE |
					LLInventoryObserver::ADD |
					LLInventoryObserver::REMOVE))
		{
			handled = true;

			//////////////////////////////
			// ADD Operation
			// Item exists in memory but a UI element hasn't been created for it.
			if (model_item && !view_item)
			{
				// Add the UI element for this item.
				buildNewViews(item_id);
				// Select any newly created object that has the auto rename at top of folder root set.
				if(mFolderRoot->getRoot()->needsAutoRename())
				{
					setSelection(item_id, FALSE);
				}
			}

			//////////////////////////////
			// STRUCTURE Operation
			// This item already exists in both memory and UI.  It was probably reparented.
			else if (model_item && view_item)
			{
				// Don't process the item if it is the root
				if (view_item->getRoot() != view_item)
				{
					LLFolderViewFolder* new_parent = (LLFolderViewFolder*)mFolderRoot->getItemByID(model_item->getParentUUID());
					// Item has been moved.
					if (view_item->getParentFolder() != new_parent)
					{
						if (new_parent != NULL)
						{
							// Item is to be moved and we found its new parent in the panel's directory, so move the item's UI.
							view_item->getParentFolder()->extractItem(view_item);
							view_item->addToFolder(new_parent, mFolderRoot);
						}
						else
						{
							// Item is to be moved outside the panel's directory (e.g. moved to trash for a panel that
							// doesn't include trash).  Just remove the item's UI.
							view_item->destroyView();
						}
					}
				}
			}

			//////////////////////////////
			// REMOVE Operation
			// This item has been removed from memory, but its associated UI element still exists.
			else if (!model_item && view_item)
			{
				// Remove the item's UI.
				view_item->destroyView();
			}
		}
	}
}

LLFolderView* LLInventoryPanel::getRootFolder() 
{ 
	return mFolderRoot; 
}
const LLUUID& LLInventoryPanel::getRootFolderID() const
{
	return mFolderRoot->getListener()->getUUID();
}
void LLInventoryPanel::rebuildViewsFor(const LLUUID& id, U32 mask)
{
	// Destroy the old view for this ID so we can rebuild it.
	LLFolderViewItem* old_view = mFolderRoot->getItemByID(id);
	if (old_view && id.notNull())
	{
		old_view->destroyView();
	}

	buildNewViews(id);
}

LLFolderViewFolder * LLInventoryPanel::createFolderViewFolder(LLInvFVBridge * bridge)
{
	return new LLFolderViewFolder(
		bridge->getDisplayName(),
		bridge->getIcon(),
		mFolderRoot,
		bridge);
}

LLFolderViewItem * LLInventoryPanel::createFolderViewItem(LLInvFVBridge * bridge)
{
	return new LLFolderViewItem(
		bridge->getDisplayName(),
		bridge->getIcon(),
		bridge->getCreationDate(),
		mFolderRoot,
		bridge);
}

void LLInventoryPanel::buildNewViews(const LLUUID& id)
{
	LLInventoryObject* const objectp = gInventory.getObject(id);
	LLFolderViewItem* itemp = NULL;
	

	if (objectp)
	{	
		const LLUUID &parent_id = objectp->getParentUUID();
		LLFolderViewFolder* parent_folder = (LLFolderViewFolder*)mFolderRoot->getItemByID(parent_id);
		if (objectp->getType() <= LLAssetType::AT_NONE ||
			objectp->getType() >= LLAssetType::AT_COUNT)
		{
  				llwarns << "LLInventoryPanel::buildNewViews called with invalid objectp->mType : "
  						<< ((S32) objectp->getType()) << " name " << objectp->getName() << " UUID " << objectp->getUUID()
  						<< llendl;
		}
		else if ((objectp->getType() == LLAssetType::AT_CATEGORY) &&
				(objectp->getActualType() != LLAssetType::AT_LINK_FOLDER)) // build new view for category
		{
			LLInvFVBridge* new_listener = LLInvFVBridge::createBridge(objectp->getType(),
													objectp->getType(),
													LLInventoryType::IT_CATEGORY,
													this,
													objectp->getUUID());

			if (new_listener)
  				{
					LLFolderViewFolder* folderp = createFolderViewFolder(new_listener);
  					folderp->setItemSortOrder(mFolderRoot->getSortOrder());
				itemp = folderp;
			}
		}
		else // build new view for item
		{
			LLInventoryItem* item = (LLInventoryItem*)objectp;
			LLInvFVBridge* new_listener = LLInvFVBridge::createBridge(
				item->getType(),
				item->getActualType(),
				item->getInventoryType(),
				this,
				item->getUUID(),
				item->getFlags());
  				if (new_listener)
  				{
					itemp = createFolderViewItem(new_listener);
  				}
		}

		

		if (itemp)
		{
			if (parent_folder)
			{
				itemp->addToFolder(parent_folder, mFolderRoot);
			}
			else
			{
				llwarns << "Couldn't find parent folder for child " << itemp->getLabel() << llendl;
				delete itemp;
			}
		}
	}

	// If this is a folder, add the children of the folder and recursively add any 
	// child folders.
	if (id.isNull()
		||	(objectp
			&& objectp->getType() == LLAssetType::AT_CATEGORY))
	{
		LLViewerInventoryCategory::cat_array_t* categories;
		LLViewerInventoryItem::item_array_t* items;
		mInventory->lockDirectDescendentArrays(id, categories, items);
		
		if(categories)
		{
			for (LLViewerInventoryCategory::cat_array_t::const_iterator cat_iter = categories->begin();
				 cat_iter != categories->end();
				 ++cat_iter)
			{
				const LLViewerInventoryCategory* cat = (*cat_iter);
				buildNewViews(cat->getUUID());
			}
		}
		
		if(items)
		{
			for (LLViewerInventoryItem::item_array_t::const_iterator item_iter = items->begin();
				 item_iter != items->end();
				 ++item_iter)
			{
				const LLViewerInventoryItem* item = (*item_iter);
				buildNewViews(item->getUUID());
			}
		}
		mInventory->unlockDirectDescendentArrays(id);
	}
}

struct LLConfirmPurgeData
{
	LLUUID mID;
	LLInventoryModel* mModel;
};

class LLIsNotWorn : public LLInventoryCollectFunctor
{
public:
	LLIsNotWorn() {}
	virtual ~LLIsNotWorn() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		return !gAgentWearables.isWearingItem(item->getUUID());
	}
};

class LLOpenFolderByID : public LLFolderViewFunctor
{
public:
	LLOpenFolderByID(const LLUUID& id) : mID(id) {}
	virtual ~LLOpenFolderByID() {}
	virtual void doFolder(LLFolderViewFolder* folder)
		{
			if (folder->getListener() && folder->getListener()->getUUID() == mID) folder->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_UP);
		}
	virtual void doItem(LLFolderViewItem* item) {}
protected:
	const LLUUID& mID;
};


void LLInventoryPanel::openSelected()
{
	LLFolderViewItem* folder_item = mFolderRoot->getCurSelectedItem();
	if(!folder_item) return;
	LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getListener();
	if(!bridge) return;
	bridge->openItem();
}

void LLInventoryPanel::unSelectAll()	
{ 
	mFolderRoot->setSelection(NULL, FALSE, FALSE); 
}


BOOL LLInventoryPanel::handleHover(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLView::handleHover(x, y, mask);
	if(handled)
	{
		ECursorType cursor = getWindow()->getCursor();
		if (LLInventoryModelBackgroundFetch::instance().backgroundFetchActive() && cursor == UI_CURSOR_ARROW)
		{
			// replace arrow cursor with arrow and hourglass cursor
			getWindow()->setCursor(UI_CURSOR_WORKING);
		}
	}
	else
	{
		getWindow()->setCursor(UI_CURSOR_ARROW);
	}
	return TRUE;
}

BOOL LLInventoryPanel::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg)
{

	BOOL handled = LLPanel::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);

	if (handled)
	{
		mFolderRoot->setDragAndDropThisFrame();
	}

	return handled;
}


void LLInventoryPanel::openAllFolders()
{
	mFolderRoot->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_DOWN);
	mFolderRoot->arrangeAll();
}

void LLInventoryPanel::closeAllFolders()
{
	mFolderRoot->setOpenArrangeRecursively(FALSE, LLFolderViewFolder::RECURSE_DOWN);
	mFolderRoot->arrangeAll();
}

void LLInventoryPanel::openDefaultFolderForType(LLAssetType::EType type)
{
	LLUUID category_id = mInventory->findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(type));
	LLOpenFolderByID opener(category_id);
	mFolderRoot->applyFunctorRecursively(opener);
}

void LLInventoryPanel::setSelection(const LLUUID& obj_id, BOOL take_keyboard_focus)
{
	LLFolderViewItem* itemp = mFolderRoot->getItemByID(obj_id);
	if(itemp && itemp->getListener())
	{
		itemp->getListener()->arrangeAndSet(itemp, TRUE, take_keyboard_focus);
		mSelectThisID.setNull();
		return;
	}
	else
	{
		// save the desired item to be selected later (if/when ready)
		mSelectThisID = obj_id;
	}
}
void LLInventoryPanel::setSelectCallback(LLFolderView::SelectCallback callback, void* user_data) 
{
	if (mFolderRoot)
	{
	 	mFolderRoot->setSelectCallback(callback, user_data);
	}
}
void LLInventoryPanel::clearSelection()
{
	mFolderRoot->clearSelection();
	mSelectThisID.setNull();
}

void LLInventoryPanel::createNewItem(const std::string& name,
									const LLUUID& parent_id,
									LLAssetType::EType asset_type,
									LLInventoryType::EType inv_type,
									U32 next_owner_perm)
{
	std::string desc;
	LLViewerAssetType::generateDescriptionFor(asset_type, desc);
	next_owner_perm = (next_owner_perm) ? next_owner_perm : PERM_MOVE | PERM_TRANSFER;

	
	if (inv_type == LLInventoryType::IT_GESTURE)
	{
		LLPointer<LLInventoryCallback> cb = new CreateGestureCallback();
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							  parent_id, LLTransactionID::tnull, name, desc, asset_type, inv_type,
							  NOT_WEARABLE, next_owner_perm, cb);
	}
	else
	{
		LLPointer<LLInventoryCallback> cb = NULL;
		create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							  parent_id, LLTransactionID::tnull, name, desc, asset_type, inv_type,
							  NOT_WEARABLE, next_owner_perm, cb);
	}
	
}	

BOOL LLInventoryPanel::getSinceLogoff()
{
	return getFilter()->isSinceLogoff();
}

// DEBUG ONLY
// static 
void LLInventoryPanel::dumpSelectionInformation(void* user_data)
{
	LLInventoryPanel* iv = (LLInventoryPanel*)user_data;
	iv->mFolderRoot->dumpSelectionInformation();
}
