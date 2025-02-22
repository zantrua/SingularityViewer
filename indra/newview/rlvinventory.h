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

#ifndef RLV_INVENTORY_H
#define RLV_INVENTORY_H

#include "llmemory.h"
#include "llviewerinventory.h"
#include "llinventoryfunctions.h"

#include "rlvhelper.h"
#include "rlvlocks.h"

// ============================================================================
// RlvInventory class declaration
//

// TODO-RLVa: [RLVa-1.2.0] Make all of this static rather than a singleton?
class RlvInventory : public LLSingleton<RlvInventory>
{
protected:
	RlvInventory() : m_fFetchStarted(false), m_fFetchComplete(false) {}

	/*
	 * #RLV Shared inventory
	 */
public:
	// Find all folders that match a supplied criteria (clears the output array)
	bool                       findSharedFolders(const std::string& strCriteria, LLInventoryModel::cat_array_t& folders) const;
	// Gets the shared path for any shared items present in idItems (clears the output array)
	bool                       getPath(const uuid_vec_t& idItems, LLInventoryModel::cat_array_t& folders) const;
	// Returns a pointer to the shared root folder (if there is one)
	LLViewerInventoryCategory* getSharedRoot() const;
	// Returns a subfolder of idParent that starts with strFolderName (exact match > partial match)
	LLViewerInventoryCategory* getSharedFolder(const LLUUID& idParent, const std::string& strFolderName) const;
	// Looks up a folder from a path (relative to the shared root)
	LLViewerInventoryCategory* getSharedFolder(const std::string& strPath) const;
	// Returns the path of the supplied folder (relative to the shared root)
	std::string                getSharedPath(const LLViewerInventoryCategory* pFolder) const;
	// Returns TRUE if the supplied folder is a descendent of the #RLV folder
	bool                       isSharedFolder(const LLUUID& idFolder);

	/*
	 * Inventory fetching
	 */
public:
	void fetchSharedInventory();
	void fetchWornItems();
	void fetchWornItem(const LLUUID& idItem);
protected:
	void fetchSharedLinks();

	/*
	 * General purpose helper functions
	 */
public:
	// Returns the number of direct descendents of pFolder that have the specified type asset type
	static S32 getDirectDescendentsCount(const LLInventoryCategory* pFolder, LLAssetType::EType filterType);
	// A "folded folder" is a folder whose items logically belong to the grandparent rather than the parent
	static bool isFoldedFolder(const LLInventoryCategory* pFolder, bool fCheckComposite);

	/*
	 * Member variables
	 */
protected:
	bool m_fFetchStarted;			// TRUE if we fired off an inventory fetch
	bool m_fFetchComplete;			// TRUE if everything was fetched

private:
	static const std::string cstrSharedRoot;
	friend class RlvSharedInventoryFetcher;
	friend class LLSingleton<RlvInventory>;
};

// ============================================================================
// RlvRenameOnWearObserver - Handles "auto-rename-on-wear" for (linked) items living under #RLV
//

class RlvRenameOnWearObserver : public LLInventoryFetchItemsObserver
{
public:
	RlvRenameOnWearObserver(const LLUUID& idItem) : LLInventoryFetchItemsObserver(idItem) {}
	virtual ~RlvRenameOnWearObserver() {}
	virtual void done();
protected:
	void doneIdle();
};

// ============================================================================
// "Give to #RLV" helper classes
//

// [See LLInventoryTransactionObserver which says it's not entirely complete?]
// NOTE: the offer may span mulitple BulkUpdateInventory messages so if we're no longer around then (ie due to "delete this") then
//       we'll miss those; in this specific case we only care about the *folder* though and that will be in the very first message
class RlvGiveToRLVTaskOffer : public LLInventoryObserver
{
public:
	RlvGiveToRLVTaskOffer(const LLUUID& idTransaction) : m_idTransaction(idTransaction) {}
	virtual void changed(U32 mask);
protected:
	virtual void done();
	void doneIdle();

	typedef std::vector<LLUUID> folder_ref_t;
	folder_ref_t m_Folders;
	LLUUID       m_idTransaction;
};

class RlvGiveToRLVAgentOffer : public LLInventoryFetchDescendentsObserver
{
public:
	RlvGiveToRLVAgentOffer(const LLUUID& cat_id) : LLInventoryFetchDescendentsObserver(cat_id) {}
	virtual void done();
protected:
	void doneIdle();
};

// ============================================================================
// RlvCriteriaCategoryCollector - Criteria based folder matching filter used by @findfolder and @findfolders
//

class RlvCriteriaCategoryCollector : public LLInventoryCollectFunctor
{
public:
	RlvCriteriaCategoryCollector(const std::string& strCriteria)
	{
		std::string::size_type idxIt, idxLast = 0;
		while (idxLast < strCriteria.length())
		{
			idxIt = strCriteria.find("&&", idxLast);
			if (std::string::npos == idxIt)
				idxIt = strCriteria.length();
			if (idxIt != idxLast)
				m_Criteria.push_back(strCriteria.substr(idxLast, idxIt - idxLast));
			idxLast = idxIt + 2;
		}
	}
	virtual ~RlvCriteriaCategoryCollector() {}

	virtual bool operator()(LLInventoryCategory* pFolder, LLInventoryItem* pItem)
	{
		if ( (!pFolder) || (m_Criteria.empty()) )	// We're only interested in matching folders, we don't care about items
			return false;							// (if there are no criteria then we don't want to return a match)

		std::string strFolderName = pFolder->getName();
		LLStringUtil::toLower(strFolderName);

		// NOTE: hidden or "give to #RLV" folders can never be a match
		if ( (strFolderName.empty()) ||	
			 (RLV_FOLDER_PREFIX_HIDDEN == strFolderName[0]) || (RLV_FOLDER_PREFIX_PUTINV == strFolderName[0]) )
		{
			return false;
		}

		for (std::list<std::string>::const_iterator itCrit = m_Criteria.begin(); itCrit != m_Criteria.end(); ++itCrit)
			if (std::string::npos == strFolderName.find(*itCrit))
				return false;
		return true;
	}

protected:
	std::list<std::string> m_Criteria;
};

// ============================================================================
// RlvWearableItemCollector - Inventory item filter used by attach/detach/attachall/detachall/getinvworn
//

class RlvWearableItemCollector : public LLInventoryCollectFunctor
{
public:
	RlvWearableItemCollector(const LLInventoryCategory* pFolder, RlvForceWear::EWearAction eAction, RlvForceWear::EWearFlags eFlags);
	virtual ~RlvWearableItemCollector() {}

	virtual bool operator()(LLInventoryCategory* pFolder, LLInventoryItem* pItem);

	const LLUUID&             getFoldedParent(const LLUUID& idFolder) const;
	RlvForceWear::EWearAction getWearAction(const LLUUID& idFolder) const;
	RlvForceWear::EWearAction getWearActionNormal(const LLInventoryCategory* pFolder);
	RlvForceWear::EWearAction getWearActionFolded(const LLInventoryCategory* pFolder);
	bool                      isLinkedFolder(const LLUUID& idFolder);
protected:
	const LLUUID              m_idFolder;
	RlvForceWear::EWearAction m_eWearAction;
	RlvForceWear::EWearFlags  m_eWearFlags;

	bool onCollectFolder(const LLInventoryCategory* pFolder);
	bool onCollectItem(const LLInventoryItem* pItem);

	std::list<LLUUID> m_Folded;
	std::list<LLUUID> m_Linked;
	std::list<LLUUID> m_Wearable;
	std::map<LLUUID, LLUUID>                    m_FoldingMap;
	std::map<LLUUID, RlvForceWear::EWearAction> m_WearActionMap;

	std::string m_strWearAddPrefix;
	std::string m_strWearReplacePrefix;
};

// ============================================================================
// General purpose inventory helper classes
//

class RlvIsLinkType : public LLInventoryCollectFunctor
{
public:
	RlvIsLinkType() {}
	virtual ~RlvIsLinkType() {}
	virtual bool operator()(LLInventoryCategory* pFolder, LLInventoryItem* pItem) { return (pItem) && (pItem->getIsLinkType()); }
};

class RlvItemFetcher : public LLInventoryFetchItemsObserver
{
public:
	RlvItemFetcher(const uuid_vec_t& cat_ids) : LLInventoryFetchItemsObserver(cat_ids) {}
	RlvItemFetcher(const LLUUID& cat_id) : LLInventoryFetchItemsObserver(cat_id) {}
	virtual ~RlvItemFetcher() {}
	virtual void done() {}
};

// ============================================================================
// RlvInventory inlined member functions
//

// Checked: 2010-03-19 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
inline bool RlvInventory::isFoldedFolder(const LLInventoryCategory* pFolder, bool fCheckComposite)
{
	return
	  // If legacy naming isn't enabled we can return early if the folder name doesn't start with a '.' (= the most common case)
	  (pFolder) && ( (RlvSettings::getEnableLegacyNaming()) || (RLV_FOLDER_PREFIX_HIDDEN == pFolder->getName().at(0)) ) &&
	  (
		// .(<attachpt>) type folder
		(0 != RlvAttachPtLookup::getAttachPointIndex(pFolder))
		// .(nostrip) folder
		|| ( (pFolder) && (".("RLV_FOLDER_FLAG_NOSTRIP")" == pFolder->getName()) )
		// Composite folder (if composite folders are enabled and we're asked to look for them)
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		|| ( (fCheckComposite) && (RlvSettings::getEnableComposites()) &&
		     (pFolder) && (RLV_FOLDER_PREFIX_HIDDEN == pFolder->getName().at(0)) && (isCompositeFolder(pFolder)) )
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	  );
}

// Checked: 2010-08-29 (RLVa-1.2.0c) | Added: RLVa-1.2.0c
inline bool RlvInventory::isSharedFolder(const LLUUID& idFolder)
{
	const LLViewerInventoryCategory* pRlvRoot = getSharedRoot();
	return (pRlvRoot) ? (pRlvRoot->getUUID() != idFolder) && (gInventory.isObjectDescendentOf(idFolder, pRlvRoot->getUUID())) : false;
}

// ============================================================================
// RlvWearableItemCollector inlined member functions
//

// Checked: 2010-09-30 (RLVa-1.2.1d) | Added: RLVa-1.2.1d
inline bool RlvWearableItemCollector::isLinkedFolder(const LLUUID& idFolder)
{
	return (!m_Linked.empty()) && (m_Linked.end() != std::find(m_Linked.begin(), m_Linked.end(), idFolder));
}

// ============================================================================

#endif // RLV_INVENTORY_H
