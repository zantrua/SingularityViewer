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
#include "llfloateravatarlist.h"
#include "llavatarnamecache.h"
#include "llcallbacklist.h"
#include "llfloaterbeacons.h"
#include "llfloaterchat.h"
#include "llfloaterdaycycle.h"
#include "llfloaterenvsettings.h"
#include "llfloaterland.h"
#include "llfloatermap.h"
#include "llfloaterregioninfo.h"
#include "llfloatertools.h"
#include "llfloaterwater.h"
#include "llfloaterwindlight.h"
#include "llfloaterworldmap.h"
#include "llinventoryview.h"
#include "llviewermenu.h"
#include "llviewermessage.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llagentcamera.h"
#include "llagentwearables.h"

#include "rlvhandler.h"
#include "rlvinventory.h"
#include "rlvextensions.h"

// ============================================================================
// Static variable initialization
//

BOOL RlvHandler::m_fEnabled = FALSE;

rlv_handler_t gRlvHandler;

// ============================================================================
// Command specific helper functions
//

// Checked: 2009-08-04 (RLVa-1.0.1d) | Added: RLVa-1.0.1d
static bool rlvParseNotifyOption(const std::string& strOption, S32& nChannel, std::string& strFilter)
{
	boost_tokenizer tokens(strOption, boost::char_separator<char>(";", "", boost::keep_empty_tokens));
	boost_tokenizer::const_iterator itTok = tokens.begin();

	// Extract and sanity check the first token (required) which is the channel
	if ( (itTok == tokens.end()) || (!LLStringUtil::convertToS32(*itTok, nChannel)) || (!RlvUtil::isValidReplyChannel(nChannel)) )
		return false;

	// Second token (optional) is the filter
	strFilter.clear();
	if (++itTok != tokens.end())
	{
		strFilter = *itTok;
		++itTok;
	}

	return (itTok == tokens.end());
}

// ============================================================================
// Constructor/destructor
//

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.0.1d
RlvHandler::RlvHandler() : m_fCanCancelTp(true), m_posSitSource(), m_pGCTimer(NULL), m_pWLSnapshot(NULL)
{
	// Array auto-initialization to 0 is non-standard? (Compiler warning in VC-8.0)
	memset(m_Behaviours, 0, sizeof(S16) * RLV_BHVR_COUNT);
}

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.0.1d
RlvHandler::~RlvHandler()
{
	//delete m_pGCTimer;	// <- deletes itself
	delete m_pWLSnapshot;	// <- delete on NULL is harmless
}

// ============================================================================
// Behaviour related functions
//

bool RlvHandler::hasBehaviourExcept(ERlvBehaviour eBehaviour, const std::string& strOption, const LLUUID& idObj) const
{
	for (rlv_object_map_t::const_iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
		if ( (idObj != itObj->second.m_UUID) && (itObj->second.hasBehaviour(eBehaviour, strOption, false)) )
			return true;
	return false;
}

// Checked: 2009-10-04 (RLVa-1.0.4c) | Modified: RLVa-1.0.4c
bool RlvHandler::isException(ERlvBehaviour eBhvr, const RlvExceptionOption& varOption, ERlvExceptionCheck typeCheck) const
{
	// We need to "strict check" exceptions only if: the restriction is actually in place *and* (isPermissive(eBhvr) == FALSE)
	if (RLV_CHECK_DEFAULT == typeCheck)
		typeCheck = ( (hasBehaviour(eBhvr)) && (!isPermissive(eBhvr)) ) ? RLV_CHECK_STRICT : RLV_CHECK_PERMISSIVE;

	std::list<LLUUID> objList;
	if (RLV_CHECK_STRICT == typeCheck)
	{
		// If we're "strict checking" then we need the UUID of every object that currently has 'eBhvr' restricted
		for (rlv_object_map_t::const_iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
			if (itObj->second.hasBehaviour(eBhvr, !hasBehaviour(RLV_BHVR_PERMISSIVE)))
				objList.push_back(itObj->first);
	}

	for (rlv_exception_map_t::const_iterator itException = m_Exceptions.lower_bound(eBhvr), 
			endException = m_Exceptions.upper_bound(eBhvr); itException != endException; ++itException)
	{
		if (itException->second.varOption == varOption)
		{
			// For permissive checks we just return on the very first match
			if (RLV_CHECK_PERMISSIVE == typeCheck)
				return true;

			// For strict checks we don't return until the list is empty (every object with 'eBhvr' restricted also contains the exception)
			std::list<LLUUID>::iterator itList = std::find(objList.begin(), objList.end(), itException->second.idObject);
			if (itList != objList.end())
				objList.erase(itList);
			if (objList.empty())
				return true;
		}
	}
	return false;
}

// ============================================================================
// Command processing functions
//

// Checked: 2009-11-27 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
void RlvHandler::addBehaviourObserver(RlvBehaviourObserver* pBhvrObserver)
{
	if ( (pBhvrObserver) && (std::find(m_BhvrObservers.begin(), m_BhvrObservers.end(), pBhvrObserver) == m_BhvrObservers.end()) )
		m_BhvrObservers.push_back(pBhvrObserver);
}

// Checked: 2009-11-27 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
void RlvHandler::addCommandHandler(RlvCommandHandler* pCmdHandler)
{
	if ( (pCmdHandler) && (std::find(m_CommandHandlers.begin(), m_CommandHandlers.end(), pCmdHandler) == m_CommandHandlers.end()) )
		m_CommandHandlers.push_back(pCmdHandler);
}

// Checked: 2009-11-27 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
void RlvHandler::removeBehaviourObserver(RlvBehaviourObserver* pBhvrObserver)
{
	if (pBhvrObserver)
		m_BhvrObservers.remove(pBhvrObserver);
}

// Checked: 2009-11-27 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
void RlvHandler::removeCommandHandler(RlvCommandHandler* pCmdHandler)
{
	if (pCmdHandler)
		m_CommandHandlers.remove(pCmdHandler);
}

// Checked: 2009-10-26 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
void RlvHandler::clearCommandHandlers()
{
	std::list<RlvCommandHandler*>::const_iterator itHandler = m_CommandHandlers.begin();
	while (itHandler != m_CommandHandlers.end())
	{
		delete *itHandler;
		++itHandler;
	}
	m_CommandHandlers.clear();
}

// Checked: 2009-06-03 (RLVa-0.2.0h)
void RlvHandler::notifyBehaviourObservers(const RlvCommand& rlvCmd, bool fInternal)
{
	for (std::list<RlvBehaviourObserver*>::const_iterator itBhvrObserver = m_BhvrObservers.begin();
			itBhvrObserver != m_BhvrObservers.end(); ++itBhvrObserver)
	{
		(*itBhvrObserver)->changed(rlvCmd, fInternal);
	}
}

// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
bool RlvHandler::notifyCommandHandlers(rlvCommandHandler f, const RlvCommand& rlvCmd, ERlvCmdRet& eRet, bool fNotifyAll) const
{
	std::list<RlvCommandHandler*>::const_iterator itHandler = m_CommandHandlers.begin(); bool fContinue = true; eRet = RLV_RET_UNKNOWN;
	while ( (itHandler != m_CommandHandlers.end()) && ((fContinue) || (fNotifyAll)) )
	{
		ERlvCmdRet eCmdRet = RLV_RET_UNKNOWN;
		if ((fContinue = !((*itHandler)->*f)(rlvCmd, eCmdRet)) == false)
			eRet = eCmdRet;
		++itHandler;
	}
	RLV_ASSERT( (fContinue) || (eRet != RLV_RET_UNKNOWN) );
	return !fContinue;
}

// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::processCommand(const RlvCommand& rlvCmd, bool fFromObj)
{
	#ifdef RLV_DEBUG
		RLV_INFOS << "[" << rlvCmd.getObjectID() << "]: " << rlvCmd.asString() << RLV_ENDL;
	#endif // RLV_DEBUG

	if (!rlvCmd.isValid())
	{
		#ifdef RLV_DEBUG
			RLV_INFOS << "\t-> invalid syntax" << RLV_ENDL;
		#endif // RLV_DEBUG
		return RLV_RET_FAILED_SYNTAX;
	}

	// Using a stack for executing commands solves a few problems:
	//   - if we passed RlvObject::m_UUID for idObj somewhere and process a @clear then idObj points to invalid/cleared memory at the end
	//   - if command X triggers command Y along the way then getCurrentCommand()/getCurrentObject() still return Y even when finished
	m_CurCommandStack.push(&rlvCmd); m_CurObjectStack.push(rlvCmd.getObjectID());
	const LLUUID& idCurObj = m_CurObjectStack.top();

	ERlvCmdRet eRet = RLV_RET_UNKNOWN;
	switch (rlvCmd.getParamType())
	{
		case RLV_TYPE_ADD:		// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			{
				if ( (m_Behaviours[rlvCmd.getBehaviourType()]) && 
					 ( (RLV_BHVR_SETDEBUG == rlvCmd.getBehaviourType()) || (RLV_BHVR_SETENV == rlvCmd.getBehaviourType()) ) )
				{
					// Some restrictions can only be held by one single object to avoid deadlocks
					#ifdef RLV_DEBUG
						RLV_INFOS << "\t- " << rlvCmd.getBehaviour() << " is already set by another object => discarding" << RLV_ENDL;
					#endif // RLV_DEBUG
					eRet = RLV_RET_FAILED_LOCK;
					break;
				}

				rlv_object_map_t::iterator itObj = m_Objects.find(idCurObj); bool fAdded = false;
				if (itObj != m_Objects.end())
				{
					RlvObject& rlvObj = itObj->second;
					fAdded = rlvObj.addCommand(rlvCmd);
				}
				else
				{
					RlvObject rlvObj(idCurObj);
					fAdded = rlvObj.addCommand(rlvCmd);
					m_Objects.insert(std::pair<LLUUID, RlvObject>(idCurObj, rlvObj));
				}

				#ifdef RLV_DEBUG
					RLV_INFOS << "\t- " << ( (fAdded) ? "adding behaviour" : "skipping duplicate" ) << RLV_ENDL;
				#endif // RLV_DEBUG

				if (fAdded) {	// If FALSE then this was a duplicate, there's no need to handle those
					if (!m_pGCTimer)
						m_pGCTimer = new RlvGCTimer();
					eRet = processAddRemCommand(rlvCmd);
					notifyBehaviourObservers(rlvCmd, !fFromObj);
				}
				else
				{
					eRet = RLV_RET_SUCCESS_DUPLICATE;
				}
			}
			break;
		case RLV_TYPE_REMOVE:		// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			{
				rlv_object_map_t::iterator itObj = m_Objects.find(idCurObj); bool fRemoved = false;
				if (itObj != m_Objects.end())
					fRemoved = itObj->second.removeCommand(rlvCmd);

				#ifdef RLV_DEBUG
					RLV_INFOS << "\t- " << ( (fRemoved)	? "removing behaviour"
														: "skipping remove (unset behaviour or unknown object)") << RLV_ENDL;
				#endif // RLV_DEBUG

				if (fRemoved) {	// Don't handle non-sensical removes
					eRet = processAddRemCommand(rlvCmd);
					notifyBehaviourObservers(rlvCmd, !fFromObj);

					if (0 == itObj->second.m_Commands.size())
					{
						#ifdef RLV_DEBUG
							RLV_INFOS << "\t- command list empty => removing " << idCurObj << RLV_ENDL;
						#endif // RLV_DEBUG
						m_Objects.erase(itObj);
					}
				}
				else
				{
					eRet = RLV_RET_SUCCESS_UNSET;
				}
			}
			break;
		case RLV_TYPE_CLEAR:		// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			eRet = processClearCommand(rlvCmd);
			notifyBehaviourObservers(rlvCmd, !fFromObj);
			break;
		case RLV_TYPE_FORCE:		// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			eRet = processForceCommand(rlvCmd);
			break;
		case RLV_TYPE_REPLY:		// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			eRet = processReplyCommand(rlvCmd);
			break;
		case RLV_TYPE_UNKNOWN:		// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
		default:
			eRet = RLV_RET_FAILED_PARAM;
			break;
	}
	RLV_ASSERT(RLV_RET_UNKNOWN != eRet);

	#ifdef RLV_DEBUG
		RLV_INFOS << "\t--> command " << ((eRet & RLV_RET_SUCCESS) ? "succeeded" : "failed") << RLV_ENDL;
	#endif // RLV_DEBUG

	m_CurCommandStack.pop(); m_CurObjectStack.pop();
	return eRet;
}

// Checked: 2010-02-27 (RLVa-1.2.0a) | Modified: RLVa-1.1.0f
void RlvHandler::processRetainedCommands(ERlvBehaviour eBhvrFilter /*=RLV_BHVR_UNKNOWN*/, ERlvParamType eTypeFilter /*=RLV_TYPE_UNKNOWN*/)
{
	rlv_command_list_t::iterator itCmd = m_Retained.begin(), itCurCmd;
	while (itCmd != m_Retained.end())
	{
		itCurCmd = itCmd++;  // Point the loop iterator ahead

		const RlvCommand& rlvCmd = *itCurCmd;
		if ( ((RLV_BHVR_UNKNOWN == eBhvrFilter) || (rlvCmd.getBehaviourType() == eBhvrFilter)) && 
		     ((RLV_TYPE_UNKNOWN == eTypeFilter) || (rlvCmd.getParamType() == eTypeFilter)) )
		{
			processCommand(rlvCmd, true);
			m_Retained.erase(itCurCmd);
		}
	}
}

ERlvCmdRet RlvHandler::processClearCommand(const RlvCommand& rlvCmd)
{
	const std::string& strFilter = rlvCmd.getParam(); std::string strCmdRem;

	rlv_object_map_t::const_iterator itObj = m_Objects.find(rlvCmd.getObjectID());
	if (itObj != m_Objects.end())	// No sense in clearing if we don't have any commands for this object
	{
		const RlvObject& rlvObj = itObj->second; bool fContinue = true;
		for (rlv_command_list_t::const_iterator itCmd = rlvObj.m_Commands.begin(), itCurCmd; 
				((fContinue) && (itCmd != rlvObj.m_Commands.end())); )
		{
			itCurCmd = itCmd++;		// Point itCmd ahead so it won't get invalidated if/when we erase a command

			const RlvCommand& rlvCmdRem = *itCurCmd; strCmdRem = rlvCmdRem.asString();
			if ( (strFilter.empty()) || (std::string::npos != strCmdRem.find(strFilter)) )
			{
				fContinue = (rlvObj.m_Commands.size() > 1); // rlvObj will become invalid once we remove the last command
				processCommand(rlvCmd.getObjectID(), strCmdRem.append("=y"), false);
			}
		}
	}

	// Let our observers know about clear commands
	ERlvCmdRet eRet = RLV_RET_SUCCESS;
	notifyCommandHandlers(&RlvCommandHandler::onClearCommand, rlvCmd, eRet, true);

	return RLV_RET_SUCCESS; // Don't fail clear commands even if the object didn't exist since it confuses people
}

// ============================================================================
// Externally invoked event handlers
//

// Checked: 2010-08-29 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvHandler::onSitOrStand(bool fSitting)
{
	#ifdef RLV_EXTENSION_STARTLOCATION
	if (rlv_handler_t::isEnabled())
	{
		RlvSettings::updateLoginLastLocation();
	}
	#endif // RLV_EXTENSION_STARTLOCATION

	if ( (hasBehaviour(RLV_BHVR_STANDTP)) && (!fSitting) && (!m_posSitSource.isExactlyZero()) )
	{
		RlvUtil::forceTp(m_posSitSource);
		m_posSitSource.setZero();
	}
}

// Checked: 2010-03-11 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvHandler::onAttach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt)
{
	// Assertion - pAttachObj is never NULL always specifies the root
	RLV_ASSERT( (pAttachObj) && (pAttachObj == pAttachObj->getRootEdit()) );
	// Sanity check - we need to be called *after* LLViewerJointAttachment::addObject()
	RLV_ASSERT( (pAttachPt) && (pAttachPt->isObjectAttached(pAttachObj)) );
	if ( (!pAttachObj) || (!pAttachPt) || (!pAttachPt->isObjectAttached(pAttachObj)) )
		return;

	// Check if we already have an RlvObject instance for this object or one of its child prims
	for (rlv_object_map_t::iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
	{
		// Only if we haven't been able to find this object (= attachment that rezzed in) or if it's a rezzed prim attached from in-world
		if ( (!itObj->second.m_fLookup) || (!itObj->second.m_idxAttachPt) )
		{
			const LLViewerObject* pObj = gObjectList.findObject(itObj->first);
			if ( (pObj) && (pObj->getRootEdit()->getID() == pAttachObj->getID()) )
			{
				// Reset any lookup information we might have for this object
				itObj->second.m_fLookup = true;
				itObj->second.m_idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pAttachObj);
				itObj->second.m_idRoot = pAttachObj->getID();

				// We need to check this object for an active "@detach=n" and actually lock it down now that it's been attached somewhere
				if (itObj->second.hasBehaviour(RLV_BHVR_DETACH, false))
					gRlvAttachmentLocks.addAttachmentLock(pAttachObj->getID(), itObj->second.m_UUID);
			}
		}
	}

	// Fetch the inventory item if it isn't already (we need it in case of a reattach-on-detach) and rename it if appropriate
	if ( (STATE_STARTED == LLStartUp::getStartupState()) && (gInventory.isInventoryUsable()) )
	{
		RlvRenameOnWearObserver* pFetcher = new RlvRenameOnWearObserver(pAttachObj->getAttachmentItemID());
		pFetcher->startFetch();
		if (pFetcher->isFinished())
			pFetcher->done();
		else
			gInventory.addObserver(pFetcher);
	}
}

// Checked: 2010-03-11 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvHandler::onDetach(const LLViewerObject* pAttachObj, const LLViewerJointAttachment* pAttachPt)
{
	// Assertion - pAttachObj is never NULL always specifies the root
	RLV_ASSERT( (pAttachObj) && (pAttachObj == pAttachObj->getRootEdit()) );
	// Sanity check - we need to be called *before* LLViewerJointAttachment::removeObject()
	RLV_ASSERT( (pAttachPt) && (pAttachPt->isObjectAttached(pAttachObj)) );
	if ( (!pAttachObj) || (!pAttachPt) || (!pAttachPt->isObjectAttached(pAttachObj)) )
		return;

	// If the attachment is no longer attached then then the user "Drop"'ed this attachment somehow
	if (!pAttachObj->isAttachment())
	{
		// Check if we have any RlvObject instances for this object (or any of its child prims)
		for (rlv_object_map_t::iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
		{
			if ( (itObj->second.m_fLookup) && (itObj->second.m_idRoot == pAttachObj->getID()) )
			{
				// Clear the attachment point lookup since it's now an in-world prim
				itObj->second.m_idxAttachPt = false;

				// If this object has an active "@detach=n" then we need to release the attachment lock since it's no longer attached
				if (itObj->second.hasBehaviour(RLV_BHVR_DETACH, false))
					gRlvAttachmentLocks.removeAttachmentLock(pAttachObj->getID(), itObj->second.m_UUID);
			}
		}
	}
	else
	{
		// If it's still attached then we need to clean up any restrictions this object (or one of its child prims) may still have set
		rlv_object_map_t::iterator itObj = m_Objects.begin(), itCurObj;
		while (itObj != m_Objects.end())
		{
			itCurObj = itObj++;	// @clear will invalidate our iterator so point it ahead now
#ifdef RLV_DEBUG
			bool itObj = true;
			RLV_ASSERT(itObj);	// Little hack to push itObj out of scope and prevent it from being accidentally used below
#endif // RLV_DEBUG

			// NOTE: ObjectKill seems to happen in reverse (child prims are killed before the root is) so we can't use gObjectList here
			if (itCurObj->second.m_idRoot == pAttachObj->getID())
			{
				RLV_INFOS << "Clearing " << itCurObj->first.asString() << ":" << RLV_ENDL;
				processCommand(itCurObj->second.m_UUID, "clear", true);
				RLV_INFOS << "\t-> done" << RLV_ENDL;
			}
		}
	}
}

// Checked: 2010-03-13 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
bool RlvHandler::onGC()
{
	rlv_object_map_t::iterator itObj = m_Objects.begin(), itCurObj;
	while (itObj != m_Objects.end())
	{
		itCurObj = itObj++;	// @clear will invalidate our iterator so point it ahead now
#ifdef RLV_DEBUG
		bool itObj = true;
		RLV_ASSERT(itObj);
#endif // RLV_DEBUG

		const LLViewerObject* pObj = gObjectList.findObject(itCurObj->second.m_UUID);
		if (!pObj)
		{
			// If the RlvObject once existed in gObjectList and now doesn't then expire it right away
			// If the RlvObject never existed in gObjectList and still doesn't then increase its "lookup misses" counter
			// but if that reaches 20 (we run every 30 seconds so that's about 10 minutes) then we'll expire it too
			if ( (itCurObj->second.m_fLookup) || (++itCurObj->second.m_nLookupMisses > 20) )
			{
				RLV_INFOS << "Garbage collecting " << itCurObj->first.asString() << ":" << RLV_ENDL;
				processCommand(itCurObj->first, "clear", true);
				RLV_INFOS << "\t-> done" << RLV_ENDL;
			}
		}
		else
		{
			// Assertion: if the GC encounters an RlvObject instance that hasn't existed in gObjectList up until now then
			//            it has to be a rezzed prim (if it was an attachment then RlvHandler::onAttach() should have caught it)
			RLV_ASSERT( (itCurObj->second.m_fLookup) || (!pObj->isAttachment()) );
			if (!itCurObj->second.m_fLookup)
			{
				RLV_INFOS << "Resolved missing object " << itCurObj->first.asString() << RLV_ENDL;
				itCurObj->second.m_fLookup = true;
				itCurObj->second.m_idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pObj);
				itCurObj->second.m_idRoot = pObj->getRootEdit()->getID();

				// NOTE: the following code should NEVER run (see assertion above), but just to be double-triple safety sure
				//	-> if it does run it likely means that there's a @detach=n in a *child* prim that we couldn't look up in onAttach()
				//  -> since RLV doesn't currently support @detach=n from child prims it's actually not such a big deal right now but still
				if ( (pObj->isAttachment()) && (itCurObj->second.hasBehaviour(RLV_BHVR_DETACH, false)) )
					gRlvAttachmentLocks.addAttachmentLock(pObj->getID(), itCurObj->second.m_UUID);
			}
		}
	}

	RLV_ASSERT(gRlvAttachmentLocks.verifyAttachmentLocks()); // Verify that we haven't leaked any attachment locks somehow

	return (0 != m_Objects.size());	// GC will kill itself if it has nothing to do
}

// Checked: 2009-11-26 (RLVa-1.1.0f) | Added: RLVa-1.1.0f
void RlvHandler::onIdleStartup(void* pParam)
{
	LLTimer* pTimer = (LLTimer*)pParam;
	if (LLStartUp::getStartupState() < STATE_STARTED)
	{
		// We don't want to run this *too* often
		if ( (LLStartUp::getStartupState() >= STATE_MISC) && (pTimer->getElapsedTimeF32() >= 2.0) )
		{
			gRlvHandler.processRetainedCommands(RLV_BHVR_VERSION, RLV_TYPE_REPLY);
			gRlvHandler.processRetainedCommands(RLV_BHVR_VERSIONNEW, RLV_TYPE_REPLY);
			gRlvHandler.processRetainedCommands(RLV_BHVR_VERSIONNUM, RLV_TYPE_REPLY);
			pTimer->reset();
		}
	}
	else
	{
		// Clean-up
		gIdleCallbacks.deleteFunction(onIdleStartup, pParam);
		delete pTimer;
	}
}

// Checked: 2010-03-09 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvHandler::onLoginComplete()
{
	RlvAttachPtLookup::initLookupTable();

	RlvInventory::instance().fetchWornItems();
	RlvInventory::instance().fetchSharedInventory();

	#ifdef RLV_EXTENSION_STARTLOCATION
	RlvSettings::updateLoginLastLocation();
	#endif // RLV_EXTENSION_STARTLOCATION

	processRetainedCommands();
}

// ============================================================================
// String/chat censoring functions
//

// LL must have included an strlen for UTF8 *somewhere* but I can't seem to find it so this one is home grown
size_t utf8str_strlen(const std::string& utf8)
{
	const char* pUTF8 = utf8.c_str(); size_t length = 0;
	for (int idx = 0, cnt = utf8.length(); idx < cnt ;idx++)
	{
		// We're looking for characters that don't start with 10 as their high bits
		if ((pUTF8[idx] & 0xC0) != 0x80)
			length++;
	}
	return length;
}

// TODO-RLV: works, but more testing won't hurt
std::string utf8str_chtruncate(const std::string& utf8, size_t length)
{
	if (0 == length)
		return std::string();
	if (utf8.length() <= length)
		return utf8;

	const char* pUTF8 = utf8.c_str(); int idx = 0;
	while ( (pUTF8[idx]) && (length > 0) )
	{
		// We're looking for characters that don't start with 10 as their high bits
		if ((pUTF8[idx] & 0xC0) != 0x80)
			length--;
		idx++;
	}

	return utf8.substr(0, idx);
}

// Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.0.0f
void RlvHandler::filterChat(std::string& strUTF8Text, bool fFilterEmote) const
{
	if (strUTF8Text.empty())
		return;

	if (RlvUtil::isEmote(strUTF8Text))				// Check if it's an emote
	{
		if (fFilterEmote)							// Emote filtering depends on fFilterEmote
		{
			if ( (strUTF8Text.find_first_of("\"()*=^_?~") != std::string::npos) || 
				 (strUTF8Text.find(" -") != std::string::npos) || (strUTF8Text.find("- ") != std::string::npos) || 
				 (strUTF8Text.find("''") != std::string::npos) )
			{
				strUTF8Text = "...";				// Emote contains illegal character (or character sequence)
			}
			else if (!hasBehaviour(RLV_BHVR_EMOTE))
			{
				int idx = strUTF8Text.find('.');	// Truncate at 20 characters or at the dot (whichever is shorter)
				strUTF8Text = utf8str_chtruncate(strUTF8Text, ( (idx > 0) && (idx < 20) ) ? idx + 1 : 20);
			}
		}
	} 
	else if (strUTF8Text[0] == '/')					// Not an emote, but starts with a '/'
	{
		if (utf8str_strlen(strUTF8Text) > 7)		// Allow as long if it's 6 characters or less
			strUTF8Text = "...";
	}
	else if ((strUTF8Text.length() < 4) || (strUTF8Text.compare(0, 2, "((")) || (strUTF8Text.compare(strUTF8Text.length() - 2, 2, "))")))
	{
		strUTF8Text = "...";						// Regular chat (not OOC)
	}
}

// Checked: 2010-02-27 (RLVa-1.2.0b) | Modified: RLVa-1.2.0a
bool RlvHandler::redirectChatOrEmote(const std::string& strUTF8Text) const
{
	// Sanity check - @redirchat only for chat and @rediremote only for emotes
	ERlvBehaviour eBhvr = (!RlvUtil::isEmote(strUTF8Text)) ? RLV_BHVR_REDIRCHAT : RLV_BHVR_REDIREMOTE;
	if (!hasBehaviour(eBhvr))
		return false;

	if (RLV_BHVR_REDIRCHAT == eBhvr)
	{
		std::string strText = strUTF8Text;
		filterChat(strText, false);
		if (strText != "...")
			return false;	// @sendchat wouldn't filter it so @redirchat won't redirect it either
	}

	for (rlv_exception_map_t::const_iterator itRedir = m_Exceptions.lower_bound(eBhvr), 
			endRedir = m_Exceptions.upper_bound(eBhvr); itRedir != endRedir; ++itRedir)
	{
		S32 nChannel = boost::get<S32>(itRedir->second.varOption);
		if ( (!hasBehaviour(RLV_BHVR_SENDCHANNEL)) || (isException(RLV_BHVR_SENDCHANNEL, nChannel)) )
			RlvUtil::sendChatReply(nChannel, strUTF8Text);
	}

	return true;
}

// ============================================================================
// Composite folders
//

#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
	bool RlvHandler::getCompositeInfo(const LLInventoryCategory* pFolder, std::string* pstrName) const
	{
		if (pFolder)
		{
			// Composite folder naming: ^\.?[Folder]
			const std::string& cstrFolder = pFolder->getName();
			std::string::size_type idxStart = cstrFolder.find('['), idxEnd = cstrFolder.find(']', idxStart);
			if ( ((0 == idxStart) || (1 == idxStart)) && (idxEnd - idxStart > 1) )
			{
				if (pstrName)
					pstrName->assign(cstrFolder.substr(idxStart + 1, idxEnd - idxStart - 1));
				return true;
			}
		}
		return false;
	}

	// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
	bool RlvHandler::getCompositeInfo(const LLUUID& idItem, std::string* pstrName, LLViewerInventoryCategory** ppFolder) const
	{
		LLViewerInventoryCategory* pRlvRoot; LLViewerInventoryItem* pItem;
		if ( (idItem.notNull()) && ((pRlvRoot = getSharedRoot()) != NULL) && 
			 (gInventory.isObjectDescendentOf(idItem, pRlvRoot->getUUID())) && ((pItem = gInventory.getItem(idItem)) != NULL) )
		{
			// We know it's an item in a folder under the shared root (we need its parent if it's a folded folder)
			LLViewerInventoryCategory* pFolder = gInventory.getCategory(pItem->getParentUUID());
			if (isFoldedFolder(pFolder, true, false))	// Don't check if the folder is a composite folder
				pFolder = gInventory.getCategory(pFolder->getParentUUID());

			if ( (pFolder) && (getCompositeInfo(pFolder, pstrName)) )
			{
				if (ppFolder)
					*ppFolder = pFolder;
				return true;
			}
		}
		return false;
	}
#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

#ifdef RLV_EXPERIMENTAL_COMPOSITE_FOLDING
	// Checked:
	inline bool RlvHandler::isHiddenCompositeItem(const LLUUID& idItem, const std::string& cstrItemType) const
	{
		// An item that's part of a composite folder will be hidden from @getoutfit and @getattach if:
		//   (1) the composite name specifies either a wearable layer or an attachment point
		//   (2) the specified wearable layer or attachment point is worn and resides in the folder
		//   (3) cstrItemType isn't the specified wearable layer or attach point
		//
		// Example: #RLV/Separates/Shoes/ChiChi Pumps/.[shoes] with items: "Shoe Base", "Shoe (left foot)" and "Shoe (right foot)"
		//   -> as long as "Shoe Base" is worn, @getattach should not reflect "left foot", nor "right foot"
		std::string strComposite; LLViewerInventoryCategory* pFolder;
		LLWearableType::EType type; S32 idxAttachPt;
		if ( (getCompositeInfo(idItem, &strComposite, &pFolder)) && (cstrItemType != strComposite) )
		{
			LLUUID idCompositeItem;
			if ((type = LLWearable::typeNameToType(strComposite)) != LLWearableType::WT_NONE)
			{
				idCompositeItem = gAgentWearables.getWearableItemID(type);
			}
			else if ((idxAttachPt = getAttachPointIndex(strComposite, true)) != 0)
			{
				LLVOAvatar* pAvatar; LLViewerJointAttachment* pAttachmentPt;
				if ( ((pAvatar = gAgentAvatarp) != NULL) && 
					 ((pAttachmentPt = get_if_there(pAvatar->mAttachmentPoints, idxAttachPt, (LLViewerJointAttachment*)NULL)) != NULL) )
				{
					idCompositeItem = pAttachmentPt->getItemID();
				}
			}

			if ( (idCompositeItem.notNull()) && (gInventory.isObjectDescendentOf(idCompositeItem, pFolder->getUUID())) )
				return true;
		}
		return false;
	}
#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDING

#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
	bool RlvHandler::canTakeOffComposite(const LLInventoryCategory* pFolder) const
	{
		// Sanity check - if there's no folder or no avatar then there is nothing to take off
		LLVOAvatar* pAvatar = gAgentAvatarp;
		if ( (!pFolder) || (!pAvatar) )
			return false;
		// Sanity check - if nothing is locked then we can definitely take it off
		if ( (!hasBehaviour(RLV_BHVR_REMOUTFIT)) && (!hasLockedAttachment(RLV_LOCK_REMOVE)) )
			return true;

		LLInventoryModel::cat_array_t folders;
		LLInventoryModel::item_array_t items;
		RlvWearableItemCollector functor(pFolder->getUUID(), true, false);
		gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, functor);

		for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
		{
			const LLViewerInventoryItem* pItem = items.get(idxItem);
			switch (pItem->getType())
			{
				case LLAssetType::AT_BODYPART:
				case LLAssetType::AT_CLOTHING:
					{
						LLWearable* pWearable = gAgent.getWearableFromWearableItem(pItem->getUUID());
						if ( (pWearable) && (!isRemovable(pWearable->getType())) )
							return false;	// If one wearable in the folder is non-removeable then the entire folder should be
					}
					break;
				case LLAssetType::AT_OBJECT:
					{
						LLViewerObject* pObj = pAvatar->getWornAttachment(pItem->getUUID());
						if ( (pObj != NULL) && (isLockedAttachment(pObj, RLV_LOCK_REMOVE)) )
							return false;	// If one attachment in the folder is non-detachable then the entire folder should be
					}
					break;
				default:
					break;
			}
		}
		return true;
	}

	// Checked: 2009-12-18 (RLVa-1.1.0k) | Modified: RLVa-1.1.0i
	bool RlvHandler::canWearComposite(const LLInventoryCategory* pFolder) const
	{
		// Sanity check - if there's no folder or no avatar then there is nothing to wear
		LLVOAvatar* pAvatar = gAgentAvatarp;
		if ( (!pFolder) || (!pAvatar) )
			return false;
		// Sanity check - if nothing is locked then we can definitely wear it
		if ( (!hasBehaviour(RLV_BHVR_ADDOUTFIT)) && (!hasBehaviour(RLV_BHVR_REMOUTFIT)) && (!hasLockedAttachment(RLV_LOCK_ANY)) )
			return true;

		LLInventoryModel::cat_array_t folders;
		LLInventoryModel::item_array_t items;
		RlvWearableItemCollector functor(pFolder->getUUID(), true, false);
		gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, functor);

		for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
		{
			LLViewerInventoryItem* pItem = items.get(idxItem);

			if (RlvForceWearLegacy::isWearingItem(pItem))
				continue; // Don't examine any items we're already wearing

			// A wearable layer or attachment point:
			//   - can't be "add locked"
			//   - can't be worn and "remove locked"
			//   - can't be worn and have its item belong to a *different* composite folder that we can't take off
			switch (pItem->getType())
			{
				case LLAssetType::AT_BODYPART:
				case LLAssetType::AT_CLOTHING:
					{
						// NOTE: without its asset we don't know what type the wearable is so we need to look at the item's flags instead
						LLWearableType::EType wtType = (LLWearableType::EType)(pItem->getFlags() & LLInventoryItemFlags::II_FLAGS_WEARABLES_MASK);
						LLViewerInventoryCategory* pFolder;
						if ( (!isWearable(wtType)) ||
							 ( (gAgent.getWearable(wtType)) && (!isRemovable(wtType)) ) || 
							 ( (gRlvHandler.getCompositeInfo(gAgentWearables.getWearableItemID(wtType), NULL, &pFolder)) &&
							   (pFolder->getUUID() != pItem->getParentUUID()) && (!gRlvHandler.canTakeOffComposite(pFolder)) ) )
						{
							return false;
						}
					}
					break;
				case LLAssetType::AT_OBJECT:
					{
						// If we made it here then *something* is add/remove locked so we absolutely need to know its attachment point
						LLViewerJointAttachment* pAttachPt = getAttachPoint(pItem, true); 
						LLViewerInventoryCategory* pFolder;
						if ( (!pAttachPt) || (isLockedAttachment(pAttachPt, RLV_LOCK_ADD)) ||
							 ( (pAttachPt->getObject()) && (isLockedAttachment(pAttachPt, RLV_LOCK_REMOVE)) ) ||
							 ( (gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pFolder)) &&
							   (pFolder->getUUID() != pItem->getParentUUID()) && (!gRlvHandler.canTakeOffComposite(pFolder)) ) )
						{
							return false;
						}
					}
					break;
				default:
					break;
			}
		}
		return true;
	}
#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

// ============================================================================
// Initialization helper functions
//

// Checked: 2009-11-25 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
BOOL RlvHandler::setEnabled(BOOL fEnable)
{
	if (m_fEnabled == fEnable)
		return fEnable;

	if (fEnable)
	{
		// Initialize the command lookup table
		RlvCommand::initLookupTable();

		// Initialize static classes
		RlvSettings::initClass();
		RlvStrings::initClass();

		gRlvHandler.addCommandHandler(new RlvExtGetSet());

		// Fetch shared inventory if we're enabled after logon
		if (LLStartUp::getStartupState() >= STATE_CLEANUP)
			RlvInventory::instance().fetchSharedInventory();

		m_fEnabled = TRUE;
	}

	#ifdef RLV_ADVANCED_MENU
		// RELEASE-RLVa: LL defines CLIENT_MENU_NAME but we can't get to it from here so we need to keep those two in sync manually
		LLMenuGL* pClientMenu = NULL;
		if ( (gMenuBarView) && ((pClientMenu = gMenuBarView->getChildMenuByName("Advanced", FALSE)) != NULL) )
		{
			pClientMenu->setItemVisible("RLVa", m_fEnabled);
			pClientMenu->setItemEnabled("RLVa", m_fEnabled);
		}
	#endif // RLV_ADVANCED_MENU

	return m_fEnabled;		// Return enabled/disabled state
}

BOOL RlvHandler::canDisable()
{
	return FALSE;
}

// ============================================================================
// Command handlers (RLV_TYPE_ADD and RLV_TYPE_REMOVE)
//

#define VERIFY_OPTION(x)		{ if (!(x)) { eRet = RLV_RET_FAILED_OPTION; break; } }
#define VERIFY_OPTION_REF(x)	{ if (!(x)) { eRet = RLV_RET_FAILED_OPTION; break; } fRefCount = true; }

// Checked: 2010-03-03 (RLVa-1.1.3b) | Modified: RLVa-1.2.0a
ERlvCmdRet RlvHandler::processAddRemCommand(const RlvCommand& rlvCmd)
{
	// NOTE: - at this point the command has already been:
	//            * added to the RlvObject
	//            * removed from the RlvObject (which still exists at this point even if this is the last restriction)
	//       - the object's UUID may or may not exist in gObjectList (see handling of @detach=n|y)
	ERlvBehaviour eBhvr = rlvCmd.getBehaviourType(); ERlvParamType eType = rlvCmd.getParamType();
	
	ERlvCmdRet eRet = RLV_RET_SUCCESS; bool fRefCount = false, fRefreshHover = false; const std::string& strOption = rlvCmd.getOption();
	switch (eBhvr)
	{
		case RLV_BHVR_DETACH:				// @detach[:<option>]=n|y
			eRet = onAddRemDetach(rlvCmd, fRefCount);
			break;
		case RLV_BHVR_ADDATTACH:			// @addattach[:<option>]=n|y
		case RLV_BHVR_REMATTACH:			// @addattach[:<option>]=n|y
			eRet = onAddRemAttach(rlvCmd, fRefCount);
			break;
		case RLV_BHVR_SETENV:				// @setenv=n|y
			eRet = onAddRemSetEnv(rlvCmd, fRefCount);
			break;
		case RLV_BHVR_ADDOUTFIT:			// @addoutfit[:<layer>]=n|y			- Checked: 2010-08-29 (RLVa-1.1.3b) | Modified: RLVa-1.2.1c
		case RLV_BHVR_REMOUTFIT:			// @remoutfit[:<layer>]=n|y			- Checked: 2010-08-29 (RLVa-1.1.3b) | Modified: RLVa-1.2.1c
			{
				// If there's an option it should specify a wearable type name (reference count on no option *and* a valid option)
				RlvCommandOptionGeneric rlvCmdOption(rlvCmd.getOption());
				VERIFY_OPTION_REF( (rlvCmdOption.isEmpty()) || (rlvCmdOption.isWearableType()) );

				// We need to flush any queued force-wear commands before changing the restrictions
				RlvForceWear::instance().done();

				ERlvLockMask eLock = (RLV_BHVR_ADDOUTFIT == eBhvr) ? RLV_LOCK_ADD : RLV_LOCK_REMOVE;
				for (int idxType = 0; idxType < LLWearableType::WT_COUNT; idxType++)
				{
					if ( (rlvCmdOption.isEmpty()) || ((LLWearableType::EType)idxType == rlvCmdOption.getWearableType()) )
					{
						if (RLV_TYPE_ADD == eType)
							gRlvWearableLocks.addWearableTypeLock((LLWearableType::EType)idxType, rlvCmd.getObjectID(), eLock);
						else
							gRlvWearableLocks.removeWearableTypeLock((LLWearableType::EType)idxType, rlvCmd.getObjectID(), eLock);
					}
				}
			}
			break;
		case RLV_BHVR_REDIRCHAT:			// @redirchat:<channel>=n|y			- Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.1.0h
		case RLV_BHVR_REDIREMOTE:			// @rediremote:<channel>=n|y		- Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.1.0h
			{
				// There should be an option which should specify a valid reply channel (if there's an empty option the command is invalid)
				S32 nChannel = 0;
				VERIFY_OPTION_REF( (LLStringUtil::convertToS32(strOption, nChannel)) && (RlvUtil::isValidReplyChannel(nChannel)) );

				if (RLV_TYPE_ADD == eType) 
					addException(rlvCmd.getObjectID(), eBhvr, nChannel);
				else
					removeException(rlvCmd.getObjectID(), eBhvr, nChannel);
			}
			break;
		case RLV_BHVR_SENDCHANNEL:			// @sendchannel[:<channel>]=n|y		- Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.1.0h
			{
				// If there's an option then it should be a valid (= positive and non-zero) chat channel
				S32 nChannel = 0;
				if ( (LLStringUtil::convertToS32(strOption, nChannel)) && (nChannel > 0) )
				{
					if (RLV_TYPE_ADD == eType) 
						addException(rlvCmd.getObjectID(), eBhvr, nChannel);
					else
						removeException(rlvCmd.getObjectID(), eBhvr, nChannel);
					break;
				}
				VERIFY_OPTION_REF(strOption.empty());
			}
			break;
		case RLV_BHVR_NOTIFY:				// @notify:<params>=add|rem			- Checked: 2010-03-03 (RLVa-1.1.3a) | Modified: RLVa-1.2.0a
			{
				// There should be an option that we can successfully parse (if there's an empty option the command is invalid)
				S32 nChannel; std::string strFilter;
				VERIFY_OPTION_REF( (!strOption.empty()) && (rlvParseNotifyOption(strOption, nChannel, strFilter)) );

				if (RLV_TYPE_ADD == eType)
					RlvBehaviourNotifyHandler::getInstance()->addNotify(rlvCmd.getObjectID(), nChannel, strFilter);
				else
					RlvBehaviourNotifyHandler::getInstance()->removeNotify(rlvCmd.getObjectID(), nChannel, strFilter);
			}
			break;
		case RLV_BHVR_SHOWINV:				// @showinv=n|y						- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
			{
				VERIFY_OPTION_REF(strOption.empty());

				if (RLV_TYPE_ADD == eType)
				{
					// Close all open inventory windows
					LLInventoryView::closeAll();
				}
			}
			break;
		case RLV_BHVR_SHOWMINIMAP:			// @showminimap=n|y					- Checked: 2009-12-05 (RLVa-1.1.0g) | Modified: RLVa-1.1.0g
			{
				VERIFY_OPTION_REF(strOption.empty());

				if (RLV_TYPE_ADD == eType)
				{
					// Simulate clicking the Minimap button [see LLToolBar::onClickRadar()]
					#if RLV_TARGET < RLV_MAKE_TARGET(1, 23, 0)			// Version: 1.22.11
						if (gFloaterMap->getVisible())
							LLFloaterMap::toggle(NULL);
					#else												// Version: 1.23.4
						if (LLFloaterMap::instanceVisible())
							LLFloaterMap::hideInstance();
					#endif
				}
			}
			break;
		case RLV_BHVR_SHOWWORLDMAP:			// @showworldmap=n|y				- Checked: 2009-12-05 (RLVa-1.1.0g) | Modified: RLVa-1.1.0g
			{
				VERIFY_OPTION_REF(strOption.empty());

				if (RLV_TYPE_ADD == eType)
				{
					// Simulate clicking the Map button [see LLToolBar::onClickMap()]
					if (gFloaterWorldMap->getVisible())
						LLFloaterWorldMap::toggle(NULL);
				}
			}
			break;
		case RLV_BHVR_SHOWLOC:				// @showloc=n|y						- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
			{
				VERIFY_OPTION_REF(strOption.empty());

				if (RLV_TYPE_ADD == eType)
				{
					// If this is the first @showloc=n restriction refresh all object text so we can filter it if necessary
					fRefreshHover = (0 == m_Behaviours[RLV_BHVR_SHOWLOC]);

					// Close the "About Land" floater if it's currently visible
					if (LLFloaterLand::instanceVisible())
						LLFloaterLand::hideInstance();

					// Close the "Estate Tools" floater is it's currently visible
					if (LLFloaterRegionInfo::instanceVisible())
						LLFloaterRegionInfo::hideInstance();

					// NOTE: we should close the "God Tools" floater as well, but since calling LLFloaterGodTools::instance() always
					//       creates a new instance of the floater and since it's very unlikely to be open it's just better not to
				}
				else
				{
					// If this is the last @showloc=n restriction refresh all object text in case anything needs restoring
					fRefreshHover = (1 == m_Behaviours[RLV_BHVR_SHOWLOC]);
				}
			}
			break;
		case RLV_BHVR_SHOWNAMES:			// @shownames=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
			{
				VERIFY_OPTION_REF(strOption.empty());

				if (RLV_TYPE_ADD == eType)
				{
					// If this is the first @shownames=n restriction refresh all object text so we can filter it if necessary
					fRefreshHover = (0 == m_Behaviours[RLV_BHVR_SHOWNAMES]);

					// Force the use of the "display name" cache so we can filter both display and legacy names
					if (0 == m_Behaviours[RLV_BHVR_SHOWNAMES])
					{
						LLAvatarNameCache::setForceDisplayNames(true);
					}

					// Close the "Active Speakers" panel if it's currently visible
					LLFloaterChat::getInstance()->childSetVisible("active_speakers_panel", false);

					// Close the "Avatar List/Radar" floater if it's currently visible
					if ( (LLFloaterAvatarList::getInstance()) && (LLFloaterAvatarList::getInstance()->getVisible()) )
						LLFloaterAvatarList::toggle(NULL);
				}
				else
				{
					// If this is the last @shownames=n restriction refresh all object text in case anything needs restoring
					fRefreshHover = (1 == m_Behaviours[RLV_BHVR_SHOWNAMES]);

					// Return the use of display names back to the user's preferences on the last @shownames=n restriction
					if (1 == m_Behaviours[RLV_BHVR_SHOWNAMES])
					{
						LLAvatarNameCache::setForceDisplayNames(false);
						LLAvatarNameCache::setUseDisplayNames(gSavedSettings.getS32("PhoenixNameSystem") != 0);
					}
				}
			}
			break;
		case RLV_BHVR_SHOWHOVERTEXTALL:		// @showhovertextall=n|y			- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_SHOWHOVERTEXTWORLD:	// @showhovertextworld=n|y			- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_SHOWHOVERTEXTHUD:		// @showhovertexthud=n|y			- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
			{
				VERIFY_OPTION_REF(strOption.empty());

				// Refresh all object text on the first/last add/rem(LLHUDText::setStringUTF8() decides what needs clearing and what doesn't)
				fRefreshHover = ((RLV_TYPE_ADD == eType) && (0 == m_Behaviours[eBhvr])) || 
					            ((RLV_TYPE_REMOVE == eType) && (1 == m_Behaviours[eBhvr]));
			}
			break;
		case RLV_BHVR_SHOWHOVERTEXT:		// @showhovertext:<uuid>=n|y		- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
			{
				// There should be an option and it should specify a valid UUID
				LLUUID idException(strOption);
				VERIFY_OPTION_REF(idException.notNull());

				if (RLV_TYPE_ADD == eType)
					addException(rlvCmd.getObjectID(), eBhvr, idException);
				else
					removeException(rlvCmd.getObjectID(), eBhvr, idException);

				// Clear/restore the object's hover text as needed
				LLViewerObject* pObj = gObjectList.findObject(idException);
				if ( (pObj) && (pObj->mText.notNull()) && (!pObj->mText->getPreFilteredText().empty()) )
					pObj->mText->setString( (RLV_TYPE_ADD == eType) ? "" : pObj->mText->getPreFilteredText());
			}
			break;
		case RLV_BHVR_EDIT:					// @edit=n|y						- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
			{
				VERIFY_OPTION_REF(strOption.empty());

				if (RLV_TYPE_ADD == eType)
				{
					// Turn off "View / Highlight Transparent"
					LLDrawPoolAlpha::sShowDebugAlpha = FALSE;

					// Close the Beacons floater if it's open
					if (LLFloaterBeacons::instanceVisible())
						LLFloaterBeacons::toggleInstance();

					// Get rid of the build floater if it's open [copy/paste from toggle_build_mode()]
					if (gFloaterTools->getVisible())
					{
						gAgentCamera.resetView(FALSE);
						gFloaterTools->close();
						gViewerWindow->showCursor();
					}
				}
			}
			break;
#ifdef RLV_EXTENSION_CMD_TOUCHXXX
		case RLV_BHVR_TOUCH:				// @touch:<uuid>=n					- Checked: 2010-01-01 (RLVa-1.1.0l) | Added: RLVa-1.1.0l
			{
				// There should be an option and it should specify a valid UUID
				LLUUID idException(strOption);
				VERIFY_OPTION_REF(idException.notNull());

				if (RLV_TYPE_ADD == eType)
					addException(rlvCmd.getObjectID(), eBhvr, idException);
				else
					removeException(rlvCmd.getObjectID(), eBhvr, idException);
			}
			break;
#endif // RLV_EXTENSION_CMD_TOUCHXXX
		case RLV_BHVR_FLY:					// @fly=n|y							- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
			{
				VERIFY_OPTION_REF(strOption.empty());

				if (RLV_TYPE_ADD == eType)
					gAgent.setFlying(FALSE);
			}
			break;
		// The following block is only valid if there's no option
		case RLV_BHVR_EMOTE:				// @emote=n|y						- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_SENDCHAT:				// @sendchat=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_CHATWHISPER:			// @chatwhisper=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_CHATNORMAL:			// @chatnormal=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_CHATSHOUT:			// @chatshout=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_PERMISSIVE:			// @permissive=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_STANDTP:				// @standtp=n|y						- Checked: 2010-08-29 (RLVa-1.2.1c)
		case RLV_BHVR_TPLM:					// @tplm=n|y						- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_TPLOC:				// @tploc=n|y						- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_VIEWNOTE:				// @viewnote=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_VIEWSCRIPT:			// @viewscript=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_VIEWTEXTURE:			// @viewtexture=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_ACCEPTPERMISSION:		// @acceptpermission=n|y			- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
#ifdef RLV_EXTENSION_CMD_ALLOWIDLE
		case RLV_BHVR_ALLOWIDLE:			// @allowidle=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
#endif // RLV_EXTENSION_CMD_ALLOWIDLE
		case RLV_BHVR_REZ:					// @rez=n|y							- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_FARTOUCH:				// @fartouch=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
#ifdef RLV_EXTENSION_CMD_INTERACT
		case RLV_BHVR_INTERACT:				// @interact=n|y					- Checked: 2010-01-01 (RLVa-1.1.0l) | Added: RLVa-1.1.0l
#endif // RLV_EXTENSION_CMD_INTERACT
		case RLV_BHVR_UNSIT:				// @unsit=n|y						- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_SIT:					// @sit=n|y							- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_SITTP:				// @sittp=n|y						- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_SETDEBUG:				// @setdebug=n|y					- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
			VERIFY_OPTION_REF(strOption.empty());
			break;
		// The following block is only valid if there's no option (= restriction) or if it specifies a valid UUID (= behaviour exception)
		case RLV_BHVR_RECVCHAT:				// @recvchat[:<uuid>]=n|y			- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_RECVEMOTE:			// @recvemote[:<uuid>]=n|y			- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_SENDIM:				// @sendim[:<uuid>]=n|y				- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_RECVIM:				// @recvim[:<uuid>]=n|y				- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_TPLURE:				// @tplure[:<uuid>]=n|y				- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
		case RLV_BHVR_ACCEPTTP:				// @accepttp[:<uuid>]=n|y			- Checked: 2009-12-05 (RLVa-1.1.0h) | Modified: RLVa-1.1.0h
#ifdef RLV_EXTENSION_CMD_TOUCHXXX
		case RLV_BHVR_TOUCHWORLD:			// @touchworld[:<uuid>=n|y			- Checked: 2010-01-01 (RLVa-1.1.0l) | Added: RLVa-1.1.0l
		case RLV_BHVR_TOUCHATTACH:			// @touchattach[:<uuid>=n|y			- Checked: 2010-01-01 (RLVa-1.1.0l) | Added: RLVa-1.1.0l
		case RLV_BHVR_TOUCHHUD:				// @touchhud[:<uuid>=n|y			- Checked: 2010-01-01 (RLVa-1.1.0l) | Added: RLVa-1.1.0l
#endif // RLV_EXTENSION_CMD_TOUCHXXX
			{
				LLUUID idException(strOption);
				if (idException.notNull())		// If there's an option then it should specify a valid UUID
				{
					if (RLV_TYPE_ADD == eType)
						addException(rlvCmd.getObjectID(), eBhvr, idException);
					else
						removeException(rlvCmd.getObjectID(), eBhvr, idException);
					break;
				}
				VERIFY_OPTION_REF(strOption.empty());
			}
			break;
		case RLV_BHVR_UNKNOWN:
			// Pass unknown commands on to registered command handlers
			return (notifyCommandHandlers(&RlvCommandHandler::onAddRemCommand, rlvCmd, eRet, false)) ? eRet : RLV_RET_FAILED_UNKNOWN;
		default:
			// Fail with "Invalid param" if none of the above handled it
			eRet = RLV_RET_FAILED_PARAM;
			break;
	}

	// If this command represents a behaviour restriction that's been added/removed then we need to do some additional processing
	if ( (RLV_RET_SUCCESS == eRet) && (fRefCount) )
	{
		if (RLV_TYPE_ADD == eType)
		{
			if (rlvCmd.isStrict())
				addException(rlvCmd.getObjectID(), RLV_BHVR_PERMISSIVE, eBhvr);
			m_Behaviours[eBhvr]++;
		}
		else
		{
			if (rlvCmd.isStrict())
				removeException(rlvCmd.getObjectID(), RLV_BHVR_PERMISSIVE, eBhvr);
			m_Behaviours[eBhvr]--;
		}

		// Since canShowHoverText() uses hasBehaviour() refreshing object text needs to wait until after we've reference counted
		if (fRefreshHover)
			LLHUDText::refreshAllObjectText();

		// Since RlvSettings::updateLoginLastLocation() uses hasBehaviour() it needs to be called after we've reference counted
		#ifdef RLV_EXTENSION_STARTLOCATION
			RlvSettings::updateLoginLastLocation();
		#endif // RLV_EXTENSION_STARTLOCATION

		// Show an - optional - notification on every global behaviour change
		#ifdef RLV_EXTENSION_NOTIFY_BEHAVIOUR
		if ( ((RLV_TYPE_ADD == eType) && (1 == m_Behaviours[eBhvr])) || (0 == m_Behaviours[eBhvr]) )
			RlvNotifications::notifyBehaviour(eBhvr, eType);
		#endif // RLV_EXTENSION_NOTIFY_BEHAVIOUR
	}

	return eRet;
}

// Checked: 2010-03-03 (RLVa-1.1.3b) | Modified: RLVa-1.2.0a
ERlvCmdRet RlvHandler::onAddRemAttach(const RlvCommand& rlvCmd, bool& fRefCount)
{
	RLV_ASSERT( (RLV_TYPE_ADD == rlvCmd.getParamType()) || (RLV_TYPE_REMOVE == rlvCmd.getParamType()) );
	RLV_ASSERT( (RLV_BHVR_ADDATTACH == rlvCmd.getBehaviourType()) || (RLV_BHVR_REMATTACH == rlvCmd.getBehaviourType()) );

	// Sanity check - if there's an option it should specify a valid attachment point name
	S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(rlvCmd.getOption());
	if ( (!idxAttachPt) && (!rlvCmd.getOption().empty())  )
		return RLV_RET_FAILED_OPTION;

	LLVOAvatar* pAvatar = gAgentAvatarp;
	if (!pAvatar)
		return RLV_RET_FAILED;

	// We need to flush any queued force-wear commands before changing the restrictions
	RlvForceWear::instance().done();

	ERlvLockMask eLock = (RLV_BHVR_REMATTACH == rlvCmd.getBehaviourType()) ? RLV_LOCK_REMOVE : RLV_LOCK_ADD;
	for (LLVOAvatar::attachment_map_t::const_iterator itAttach = pAvatar->mAttachmentPoints.begin(); 
			itAttach != pAvatar->mAttachmentPoints.end(); ++itAttach)
	{
		if ( (0 == idxAttachPt) || (itAttach->first == idxAttachPt) )
		{
			if (RLV_TYPE_ADD == rlvCmd.getParamType())
				gRlvAttachmentLocks.addAttachmentPointLock(itAttach->first, rlvCmd.getObjectID(), eLock);
			else
				gRlvAttachmentLocks.removeAttachmentPointLock(itAttach->first, rlvCmd.getObjectID(), eLock);
		}
	}

	fRefCount = rlvCmd.getOption().empty();	// Only reference count global locks
	return RLV_RET_SUCCESS;
}

// Checked: 2010-02-28 (RLVa-1.1.3b) | Modified: RLVa-1.2.0a
ERlvCmdRet RlvHandler::onAddRemDetach(const RlvCommand& rlvCmd, bool& fRefCount)
{
	RLV_ASSERT( (RLV_TYPE_ADD == rlvCmd.getParamType()) || (RLV_TYPE_REMOVE == rlvCmd.getParamType()) );
	RLV_ASSERT(RLV_BHVR_DETACH == rlvCmd.getBehaviourType());

	// We need to flush any queued force-wear commands before changing the restrictions
	RlvForceWear::instance().done();

	if (rlvCmd.getOption().empty())	// @detach=n|y - RLV_LOCK_REMOVE locks an attachment *object*
	{
		// The object may or may not exist (it may not have rezzed yet, or it may have already been killed):
		//   * @detach=n: - if it has rezzed then we'll already have looked up what we need (see next line if it's not an attachment)
		//                - if it hasn't rezzed yet then it's a @detach=n from a non-attachment and RlvHandler::onAttach() takes care of it
		//   * @detach=y: - if it ever rezzed as an attachment we'll have cached the UUID of its root
		//                - if it never rezzed as an attachment there won't be a lock to remove
		rlv_object_map_t::const_iterator itObj = m_Objects.find(rlvCmd.getObjectID());
		if ( (itObj != m_Objects.end()) && (itObj->second.m_fLookup) && (itObj->second.m_idxAttachPt) )
		{
			if (RLV_TYPE_ADD == rlvCmd.getParamType())
				gRlvAttachmentLocks.addAttachmentLock(itObj->second.m_idRoot, itObj->first);
			else
				gRlvAttachmentLocks.removeAttachmentLock(itObj->second.m_idRoot, itObj->first);
		}
	}
	else							// @detach:<attachpt>=n|y - RLV_LOCK_ADD and RLV_LOCK_REMOVE locks an attachment *point*
	{
		// The attachment point index should always be non-zero for @detach:<attachpt>=n|y
		S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(rlvCmd.getOption());
		if (0 == idxAttachPt)
			return RLV_RET_FAILED_OPTION;

		if (RLV_TYPE_ADD == rlvCmd.getParamType())
			gRlvAttachmentLocks.addAttachmentPointLock(idxAttachPt, rlvCmd.getObjectID(), (ERlvLockMask)(RLV_LOCK_ADD | RLV_LOCK_REMOVE));
		else
			gRlvAttachmentLocks.removeAttachmentPointLock(idxAttachPt, rlvCmd.getObjectID(), (ERlvLockMask)(RLV_LOCK_ADD | RLV_LOCK_REMOVE));
	}

	fRefCount = false;	// Don't reference count @detach[:<option>]=n
	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-18 (RLVa-1.2.0e) | Modified: RLVa-1.2.0a
ERlvCmdRet RlvHandler::onAddRemSetEnv(const RlvCommand& rlvCmd, bool& fRefCount)
{
	// Sanity check - there shouldn't be an option
	if (!rlvCmd.getOption().empty())
		return RLV_RET_FAILED_OPTION;
	if (RlvSettings::getNoSetEnv())
		return RLV_RET_FAILED_DISABLED;

	if (RLV_TYPE_ADD == rlvCmd.getParamType())
	{
		// Only close the floaters if their instance exists and they're actually visible
		if ( (LLFloaterEnvSettings::isOpen()) && (LLFloaterEnvSettings::instance()->getVisible()) )
			LLFloaterEnvSettings::instance()->close();
		if ( (LLFloaterWindLight::isOpen()) && (LLFloaterWindLight::instance()->getVisible()) )
			LLFloaterWindLight::instance()->close();
		if ( (LLFloaterWater::isOpen()) && (LLFloaterWater::instance()->getVisible()) )
			LLFloaterWater::instance()->close();
		if ( (LLFloaterDayCycle::isOpen()) && (LLFloaterDayCycle::instance()->getVisible()) )
			LLFloaterDayCycle::instance()->close();

		// Save the current WindLight params so we can restore them on @setenv=y
		RLV_ASSERT(!m_pWLSnapshot);
		if (m_pWLSnapshot)
			delete m_pWLSnapshot;
		m_pWLSnapshot = RlvWLSnapshot::takeSnapshot();
	}
	else
	{
		// Restore WindLight parameters to what they were before @setenv=n was issued
		RlvWLSnapshot::restoreSnapshot(m_pWLSnapshot);
		delete m_pWLSnapshot;
		m_pWLSnapshot = NULL;
	}
	fRefCount = true;

	return RLV_RET_SUCCESS;
}

// ============================================================================
// Command handlers (RLV_TYPE_FORCE)
//

// Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.1.0j
ERlvCmdRet RlvHandler::processForceCommand(const RlvCommand& rlvCmd) const
{
	RLV_ASSERT(RLV_TYPE_FORCE == rlvCmd.getParamType());

	ERlvCmdRet eRet = RLV_RET_SUCCESS;
	switch (rlvCmd.getBehaviourType())
	{
		case RLV_BHVR_DETACH:		// @detach[:<option>]=force				- Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
		case RLV_BHVR_REMATTACH:	// @remattach[:<option>]=force
			{
				RlvCommandOptionGeneric rlvCmdOption(rlvCmd.getOption());
				if (rlvCmdOption.isSharedFolder())
					eRet = onForceWear(rlvCmdOption.getSharedFolder(), RLV_BHVR_DETACH);
				else
					eRet = onForceRemAttach(rlvCmd);
			}
			break;
		case RLV_BHVR_REMOUTFIT:	// @remoutfit[:<option>]=force
			{
				RlvCommandOptionGeneric rlvCmdOption(rlvCmd.getOption());
				if (rlvCmdOption.isSharedFolder())
					eRet = onForceWear(rlvCmdOption.getSharedFolder(), RLV_BHVR_DETACH);
				else
					eRet = onForceRemOutfit(rlvCmd);
			}
			break;
		case RLV_BHVR_UNSIT:		// @unsit=force							- Checked: 2010-03-18 (RLVa-1.2.0c) | Modified: RLVa-0.2.0g
			{
				VERIFY_OPTION(rlvCmd.getOption().empty());
				LLVOAvatar* pAvatar = gAgentAvatarp;
				if ( (pAvatar) && (pAvatar->isSitting()) && (!hasBehaviourExcept(RLV_BHVR_UNSIT, rlvCmd.getObjectID())) )
				{
					gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
					send_agent_update(TRUE, TRUE);	// See behaviour notes on why we have to force an agent update here
				}
			}
			break;
		case RLV_BHVR_SIT:			// @sit:<option>=force
			eRet = onForceSit(rlvCmd);
			break;
		case RLV_BHVR_TPTO:			// @tpto:<option>=force					- Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.0.0h
			{
				eRet = RLV_RET_FAILED_OPTION;
				if ( (!rlvCmd.getOption().empty()) && (std::string::npos == rlvCmd.getOption().find_first_not_of("0123456789/.")) )
				{
					LLVector3d posGlobal;

					boost_tokenizer tokens(rlvCmd.getOption(), boost::char_separator<char>("/", "", boost::keep_empty_tokens)); int idx = 0;
					for (boost_tokenizer::const_iterator itToken = tokens.begin(); itToken != tokens.end(); ++itToken)
					{
						if (idx < 3)
							LLStringUtil::convertToF64(*itToken, posGlobal[idx++]);
					}

					if (idx == 3)
					{
						gAgent.teleportViaLocation(posGlobal);
						eRet = RLV_RET_SUCCESS;
					}
				}
			}
			break;
		case RLV_BHVR_ATTACH:
		case RLV_BHVR_ATTACHOVER:
		case RLV_BHVR_ATTACHALL:
		case RLV_BHVR_ATTACHALLOVER:
		case RLV_BHVR_DETACHALL:
			{
				RlvCommandOptionGeneric rlvCmdOption(rlvCmd.getOption());
				VERIFY_OPTION(rlvCmdOption.isSharedFolder());

				eRet = onForceWear(rlvCmdOption.getSharedFolder(), rlvCmd.getBehaviourType());
			}
			break;
		case RLV_BHVR_ATTACHTHIS:
		case RLV_BHVR_ATTACHTHISOVER:
		case RLV_BHVR_DETACHTHIS:
		case RLV_BHVR_ATTACHALLTHIS:
		case RLV_BHVR_ATTACHALLTHISOVER:
		case RLV_BHVR_DETACHALLTHIS:
			{
				RlvCommandOptionGetPath rlvGetPathOption(rlvCmd);
				VERIFY_OPTION(rlvGetPathOption.isValid());

				LLInventoryModel::cat_array_t folders;
				if (RlvInventory::instance().getPath(rlvGetPathOption.getItemIDs(), folders))
				{
					for (S32 idxFolder = 0, cntFolder = folders.count(); idxFolder < cntFolder; idxFolder++)
						onForceWear(folders.get(idxFolder), rlvCmd.getBehaviourType());
				}
			}
			break;
		case RLV_BHVR_DETACHME:		// @detachme=force						- Checked: 2010-09-04 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
			{
				VERIFY_OPTION(rlvCmd.getOption().empty());
				// NOTE: @detachme should respect locks but shouldn't respect things like nostrip
				const LLViewerObject* pAttachObj = gObjectList.findObject(rlvCmd.getObjectID());
				if ( (pAttachObj) && (pAttachObj->isAttachment()) )
				{
					LLVOAvatar::detachAttachmentIntoInventory(pAttachObj->getAttachmentItemID());
				}
			}
			break;
		case RLV_BHVR_UNKNOWN:
			// Pass unknown commands on to registered command handlers
			return (notifyCommandHandlers(&RlvCommandHandler::onForceCommand, rlvCmd, eRet, false)) ? eRet : RLV_RET_FAILED_UNKNOWN;
		default:
			// Fail with "Invalid param" if none of the above handled it
			eRet = RLV_RET_FAILED_PARAM;
			break;
	}
	return eRet;
}

// Checked: 2010-08-29 (RLVa-1.1.3b) | Modified: RLVa-1.2.1c
ERlvCmdRet RlvHandler::onForceRemAttach(const RlvCommand& rlvCmd) const
{
	RLV_ASSERT(RLV_TYPE_FORCE == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_REMATTACH == rlvCmd.getBehaviourType()) || (RLV_BHVR_DETACH == rlvCmd.getBehaviourType()) );

	LLVOAvatar* pAvatar = gAgentAvatarp;
	if (!pAvatar)
		return RLV_RET_FAILED;

	RlvCommandOptionGeneric rlvCmdOption(rlvCmd.getOption());
	// @remattach:<attachpt>=force - force detach single attachment point
	if (rlvCmdOption.isAttachmentPoint())
	{
		RlvForceWear::instance().forceDetach(rlvCmdOption.getAttachmentPoint());
		return RLV_RET_SUCCESS;
	}
	// @remattach:<group>=force - force detach attachments points belonging to <group>
	// @remattach=force         - force detach all attachments points
	else if ( (rlvCmdOption.isAttachmentPointGroup()) || (rlvCmdOption.isEmpty()) )
	{
		for (LLVOAvatar::attachment_map_t::const_iterator itAttach = pAvatar->mAttachmentPoints.begin(); 
				itAttach != pAvatar->mAttachmentPoints.end(); ++itAttach)
		{
			const LLViewerJointAttachment* pAttachPt = itAttach->second;
			if ( (pAttachPt) && (pAttachPt->getNumObjects()) &&
				 ((rlvCmdOption.isEmpty()) || (rlvAttachGroupFromIndex(pAttachPt->getGroup()) == rlvCmdOption.getAttachmentPointGroup())) )
			{
				RlvForceWear::instance().forceDetach(pAttachPt);
			}
		}
		return RLV_RET_SUCCESS;
	}
	return RLV_RET_FAILED_OPTION;
}

// Checked: 2010-08-29 (RLVa-1.1.3b) | Modified: RLVa-1.2.1c
ERlvCmdRet RlvHandler::onForceRemOutfit(const RlvCommand& rlvCmd) const
{
	RlvCommandOptionGeneric rlvCmdOption(rlvCmd.getOption());
	if ( (!rlvCmdOption.isWearableType()) && (!rlvCmdOption.isEmpty()) )
		return RLV_RET_FAILED_OPTION;

	for (int idxType = 0; idxType < LLWearableType::WT_COUNT; idxType++)
	{
		if ( (rlvCmdOption.isEmpty()) || ((LLWearableType::EType)idxType == rlvCmdOption.getWearableType()))
			RlvForceWear::instance().forceRemove((LLWearableType::EType)idxType);
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-18 (RLVa-1.2.0c) | Modified: RLVa-1.1.0j
ERlvCmdRet RlvHandler::onForceSit(const RlvCommand& rlvCmd) const
{
	LLViewerObject* pObj = NULL; LLUUID idTarget(rlvCmd.getOption());
	// Sanity checking - we need to know about the object and it should identify a prim/linkset
	if ( (idTarget.isNull()) || ((pObj = gObjectList.findObject(idTarget)) == NULL) || (LL_PCODE_VOLUME != pObj->getPCode()) )
		return RLV_RET_FAILED_OPTION;

	if (!canSit(pObj))
		return RLV_RET_FAILED_LOCK;
	else if ( (hasBehaviour(RLV_BHVR_STANDTP)) && (gAgentAvatarp) )
	{
		if (gAgentAvatarp->isSitting())
			return RLV_RET_FAILED_LOCK;
		m_posSitSource = gAgent.getPositionGlobal();
	}

	// Copy/paste from handle_sit_or_stand() [see http://wiki.secondlife.com/wiki/AgentRequestSit]
	gMessageSystem->newMessageFast(_PREHASH_AgentRequestSit);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_TargetObject);
	gMessageSystem->addUUIDFast(_PREHASH_TargetID, pObj->mID);
	// Offset: "a rough position in local coordinates for the edge to sit on"
	// (we might not even be looking at the object so I don't think we can supply the offset to an edge)
	gMessageSystem->addVector3Fast(_PREHASH_Offset, LLVector3::zero);
	pObj->getRegion()->sendReliableMessage();

	return RLV_RET_SUCCESS;
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
ERlvCmdRet RlvHandler::onForceWear(const LLViewerInventoryCategory* pFolder, ERlvBehaviour eBhvr) const
{
	if ( (pFolder) && (!RlvInventory::instance().isSharedFolder(pFolder->getUUID())) )
		return RLV_RET_FAILED_OPTION;

	RlvForceWear::EWearAction eAction = RlvForceWear::ACTION_WEAR_REPLACE;
	if ( (RLV_BHVR_ATTACHOVER == eBhvr) || (RLV_BHVR_ATTACHTHISOVER == eBhvr) || 
		 (RLV_BHVR_ATTACHALLOVER == eBhvr) || (RLV_BHVR_ATTACHALLTHISOVER == eBhvr) )
	{
		eAction = RlvForceWear::ACTION_WEAR_ADD;
	}
	else if ( (RLV_BHVR_DETACH == eBhvr) || (RLV_BHVR_DETACHTHIS == eBhvr) || 
		      (RLV_BHVR_DETACHALL == eBhvr) || (RLV_BHVR_DETACHALLTHIS == eBhvr) )
	{
		eAction = RlvForceWear::ACTION_REMOVE;
	}

	RlvForceWear::EWearFlags eFlags = RlvForceWear::FLAG_DEFAULT;
	if ( (RLV_BHVR_ATTACHALL == eBhvr) || (RLV_BHVR_ATTACHALLOVER == eBhvr) || (RLV_BHVR_DETACHALL == eBhvr) ||
		 (RLV_BHVR_ATTACHALLTHIS == eBhvr) || (RLV_BHVR_ATTACHALLTHISOVER == eBhvr) || (RLV_BHVR_DETACHALLTHIS == eBhvr) )
	{
		eFlags = (RlvForceWear::EWearFlags)(eFlags | RlvForceWear::FLAG_MATCHALL);
	}

	RlvForceWear::instance().forceFolder(pFolder, eAction, eFlags);
	return RLV_RET_SUCCESS;
}

// ============================================================================
// Command handlers (RLV_TYPE_REPLY)
//

// Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::processReplyCommand(const RlvCommand& rlvCmd) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());

	// Sanity check - <param> should specify a - valid - reply channel
	S32 nChannel;
	if ( (!LLStringUtil::convertToS32(rlvCmd.getParam(), nChannel)) || (!RlvUtil::isValidReplyChannel(nChannel)) )
		return RLV_RET_FAILED_PARAM;

	ERlvCmdRet eRet = RLV_RET_SUCCESS; std::string strReply;
	switch (rlvCmd.getBehaviourType())
	{
		case RLV_BHVR_VERSION:			// @version=<channel>					- Checked: 2010-03-27 (RLVa-1.2.0b)
		case RLV_BHVR_VERSIONNEW:		// @versionnew=<channel>				- Checked: 2010-03-27 (RLVa-1.2.0b) | Added: RLVa-1.2.0b
			// NOTE: RLV will respond even if there's an option
			strReply = RlvStrings::getVersion(RLV_BHVR_VERSION == rlvCmd.getBehaviourType());
			break;
		case RLV_BHVR_VERSIONNUM:		// @versionnum=<channel>				- Checked: 2009-11-26 (RLVa-1.1.0f) | Added: RLVa-1.0.4b
			// NOTE: RLV will respond even if there's an option
			strReply = RlvStrings::getVersionNum();
			break;
		case RLV_BHVR_GETATTACH:		// @getattach[:<layer>]=<channel>
			eRet = onGetAttach(rlvCmd, strReply);
			break;
#ifdef RLV_EXTENSION_CMD_GETXXXNAMES
		case RLV_BHVR_GETATTACHNAMES:	// @getattachnames[:<grp>]=<channel>
		case RLV_BHVR_GETADDATTACHNAMES:// @getaddattachnames[:<grp>]=<channel>
		case RLV_BHVR_GETREMATTACHNAMES:// @getremattachnames[:<grp>]=<channel>
			eRet = onGetAttachNames(rlvCmd, strReply);
			break;
#endif // RLV_EXTENSION_CMD_GETXXXNAMES
		case RLV_BHVR_GETOUTFIT:		// @getoutfit[:<layer>]=<channel>
			eRet = onGetOutfit(rlvCmd, strReply);
			break;
#ifdef RLV_EXTENSION_CMD_GETXXXNAMES
		case RLV_BHVR_GETOUTFITNAMES:	// @getoutfitnames=<channel>
		case RLV_BHVR_GETADDOUTFITNAMES:// @getaddoutfitnames=<channel>
		case RLV_BHVR_GETREMOUTFITNAMES:// @getremoutfitnames=<channel>
			eRet = onGetOutfitNames(rlvCmd, strReply);
			break;
#endif // RLV_EXTENSION_CMD_GETXXXNAMES
		case RLV_BHVR_FINDFOLDER:		// @findfolder:<criteria>=<channel>
#ifdef RLV_EXTENSION_CMD_FINDFOLDERS
		case RLV_BHVR_FINDFOLDERS:		// @findfolders:<criteria>=<channel>
#endif // RLV_EXTENSION_CMD_FINDFOLDERS
			eRet = onFindFolder(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETPATH:			// @getpath[:<option>]=<channel>
		case RLV_BHVR_GETPATHNEW:		// @getpathnew[:<option>]=<channel>
			eRet = onGetPath(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETINV:			// @getinv[:<path>]=<channel>
			eRet = onGetInv(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETINVWORN:		// @getinvworn[:path]=<channel>
			eRet = onGetInvWorn(rlvCmd, strReply);
			break;
		case RLV_BHVR_GETSITID:			// @getsitid=<channel>					- Checked: 2009-11-26 (RLVa-1.1.0f)
			{
				// NOTE: RLV 1.16.1 returns a NULL UUID if we're not sitting
				LLVOAvatar* pAvatar = gAgentAvatarp; LLUUID idSitObj;
				if ( (pAvatar) && (pAvatar->isSitting()) )
				{
					// LLVOAvatar inherits from 2 classes so make sure we get the right vfptr
					LLViewerObject* pAvatarObj = dynamic_cast<LLViewerObject*>(pAvatar), *pParent;
					// (If there is a parent, we need to upcast it from LLXform to LLViewerObject to get its UUID)
					if ( (pAvatarObj) && ((pParent = static_cast<LLViewerObject*>(pAvatarObj->getRoot())) != pAvatarObj) )
						idSitObj = pParent->getID();
				}
				strReply = idSitObj.asString();
			}
			break;
#ifdef RLV_EXTENSION_CMD_GETCOMMAND
		case RLV_BHVR_GETCOMMAND:		// @getcommand:<option>=<channel>		- Checked: 2010-12-11 (RLVa-1.2.2c) | Added: RLVa-1.2.2c
			{
				RlvCommand::bhvr_map_t cmdList;
				if (RlvCommand::getCommands(cmdList, rlvCmd.getOption()))
					for (RlvCommand::bhvr_map_t::const_iterator itCmd = cmdList.begin(); itCmd != cmdList.end(); ++itCmd)
						strReply.append("/").append(itCmd->first);
			}
			break;
#endif // RLV_EXTENSION_CMD_GETCOMMAND
		case RLV_BHVR_GETSTATUS:		// @getstatus[:<option>]=<channel>		- Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			{
				// NOTE: specification says response should start with '/' but RLV-1.16.1 returns an empty string when no rules are set
				rlv_object_map_t::const_iterator itObj = m_Objects.find(rlvCmd.getObjectID());
				if (itObj != m_Objects.end())
					strReply = itObj->second.getStatusString(rlvCmd.getOption());
			}
			break;
		case RLV_BHVR_GETSTATUSALL:		// @getstatusall[:<option>]=<channel>	- Checked: 2009-11-26 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
			{
				// NOTE: specification says response should start with '/' but RLV-1.16.1 returns an empty string when no rules are set
				for (rlv_object_map_t::const_iterator itObj = m_Objects.begin(); itObj != m_Objects.end(); ++itObj)
					strReply += itObj->second.getStatusString(rlvCmd.getOption());
			}
			break;
		case RLV_BHVR_UNKNOWN:
			// Pass unknown commands on to registered command handlers
			return (notifyCommandHandlers(&RlvCommandHandler::onReplyCommand, rlvCmd, eRet, false)) ? eRet : RLV_RET_FAILED_UNKNOWN;
		default:
			// Fail with "Invalid param" if none of the above handled it
			return RLV_RET_FAILED_PARAM;
	}

	// If we made it this far then:
	//   - the command was handled successfully so we send off the response
	//   - the command failed but we still send off an - empty - response to keep the issuing script from blocking
	RlvUtil::sendChatReply(nChannel, strReply);

	return eRet;
}

// Checked: 2010-04-07 (RLVa-1.1.3a) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::onFindFolder(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_FINDFOLDER == rlvCmd.getBehaviourType()) || (RLV_BHVR_FINDFOLDERS == rlvCmd.getBehaviourType()) ); 

	// (Compatibility: RLV 1.16.1 returns the first random folder it finds while we return a blank on no option)
	if (rlvCmd.getOption().empty())
		return RLV_RET_FAILED_OPTION;

	LLInventoryModel::cat_array_t folders;
	if (RlvInventory::instance().findSharedFolders(rlvCmd.getOption(), folders))
	{
		if (RLV_BHVR_FINDFOLDER == rlvCmd.getBehaviourType())
		{
			// We need to return an "in depth" result so whoever has the most '/' is our lucky winner
			// (maxSlashes needs to be initialized to -1 since children of the #RLV folder won't have '/' in their shared path)
			int maxSlashes = -1, curSlashes; std::string strFolderName;
			for (S32 idxFolder = 0, cntFolder = folders.count(); idxFolder < cntFolder; idxFolder++)
			{
				strFolderName = RlvInventory::instance().getSharedPath(folders.get(idxFolder));

				curSlashes = std::count(strFolderName.begin(), strFolderName.end(), '/');
				if (curSlashes > maxSlashes)
				{
					maxSlashes = curSlashes;
					strReply = strFolderName;
				}
			}
		}
		else if (RLV_BHVR_FINDFOLDERS == rlvCmd.getBehaviourType())
		{
			for (S32 idxFolder = 0, cntFolder = folders.count(); idxFolder < cntFolder; idxFolder++)
			{
				if (!strReply.empty())
					strReply.push_back(',');
				strReply += RlvInventory::instance().getSharedPath(folders.get(idxFolder));
			}
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-19 (RLVa-1.1.3b) | Modified: RLVa-1.1.0e
ERlvCmdRet RlvHandler::onGetAttach(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT(RLV_BHVR_GETATTACH == rlvCmd.getBehaviourType());

	LLVOAvatar* pAvatar = gAgentAvatarp;
	if (!pAvatar)
		return RLV_RET_FAILED;

	// Sanity check - <option> should specify an attachment point or be empty
	S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(rlvCmd.getOption());
	if ( (idxAttachPt == 0) && (!rlvCmd.getOption().empty()) )
		return RLV_RET_FAILED_OPTION;

	// If we're fetching all worn attachments then the reply should start with 0
	if (0 == idxAttachPt)
		strReply.push_back('0');

	for (LLVOAvatar::attachment_map_t::const_iterator itAttach = pAvatar->mAttachmentPoints.begin(); 
			itAttach != pAvatar->mAttachmentPoints.end(); ++itAttach)
	{
		const LLViewerJointAttachment* pAttachPt = itAttach->second;
		if ( (0 == idxAttachPt) || (itAttach->first == idxAttachPt) )
		{
			bool fWorn = (pAttachPt->getNumObjects()) && 
				( (!RlvSettings::getHideLockedAttach()) || (RlvForceWear::isForceDetachable(pAttachPt, true, rlvCmd.getObjectID())) );
			strReply.push_back( (fWorn) ? '1' : '0' );
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-19 (RLVa-1.1.3b) | Added: RLVa-1.1.0e
ERlvCmdRet RlvHandler::onGetAttachNames(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_GETATTACHNAMES == rlvCmd.getBehaviourType()) || (RLV_BHVR_GETADDATTACHNAMES == rlvCmd.getBehaviourType()) || 
		        (RLV_BHVR_GETREMATTACHNAMES == rlvCmd.getBehaviourType()) );

	LLVOAvatar* pAvatar = gAgentAvatarp;
	if (!pAvatar)
		return RLV_RET_FAILED;

	ERlvAttachGroupType eAttachGroup = rlvAttachGroupFromString(rlvCmd.getOption());
	for (LLVOAvatar::attachment_map_t::const_iterator itAttach = pAvatar->mAttachmentPoints.begin(); 
			itAttach != pAvatar->mAttachmentPoints.end(); ++itAttach)
	{
		const LLViewerJointAttachment* pAttachPt = itAttach->second;
		if ( (RLV_ATTACHGROUP_INVALID == eAttachGroup) || (rlvAttachGroupFromIndex(pAttachPt->getGroup()) == eAttachGroup) )
		{
			bool fAdd = false;
			switch (rlvCmd.getBehaviourType())
			{
				case RLV_BHVR_GETATTACHNAMES:		// Every attachment point that has an attached object (locked or unlocked)
					fAdd = (pAttachPt->getNumObjects());
					break;
/*
				case RLV_BHVR_GETADDATTACHNAMES:	// Every attachment point that can be worn on (but ignore any locks set by the issuer)
					fAdd = (!isLockedAttachmentExcept(itAttach->first, RLV_LOCK_ADD, gObjectList.findObject(idObj))) &&
						   ( (pAttachPt->getObject() == NULL) || 
						     (!isLockedAttachmentExcept(itAttach->first, RLV_LOCK_REMOVE, gObjectList.findObject(idObj))) );
					break;
*/
				case RLV_BHVR_GETREMATTACHNAMES:	// Every attachment point that can be detached (but ignore any locks set by the issuer)
					fAdd = RlvForceWear::isForceDetachable(pAttachPt, true, rlvCmd.getObjectID());
					break;
				default:
					break;
			}

			if (fAdd)
			{
				if (!strReply.empty())
					strReply.push_back(',');
				strReply.append(pAttachPt->getName());
			}
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-09 (RLVa-1.2.0a) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::onGetInv(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT(RLV_BHVR_GETINV == rlvCmd.getBehaviourType());

	const LLViewerInventoryCategory* pFolder = RlvInventory::instance().getSharedFolder(rlvCmd.getOption());
	if (!pFolder)
		return (RlvInventory::instance().getSharedRoot() != NULL) ? RLV_RET_FAILED_OPTION : RLV_RET_FAILED_NOSHAREDROOT;

	LLInventoryModel::cat_array_t* pFolders; LLInventoryModel::item_array_t* pItems;
	gInventory.getDirectDescendentsOf(pFolder->getUUID(), pFolders, pItems);
	if (!pFolders)
		return RLV_RET_FAILED;

	for (S32 idxFolder = 0, cntFolder = pFolders->count(); idxFolder < cntFolder; idxFolder++)
	{
		// Return all folders that:
		//   - aren't hidden
		//   - aren't a folded folder (only really matters when "Enable Legacy Naming" is enabled - see related blog post)
		//     (we can skip checking for .<composite> folders since the ones we'll want to hide start with '.' anyway)
		const std::string& strFolder = pFolders->get(idxFolder)->getName();
		if ( (!strFolder.empty()) && (RLV_FOLDER_PREFIX_HIDDEN != strFolder[0]) && 
			 (!RlvInventory::isFoldedFolder(pFolders->get(idxFolder).get(), false)) )
		{
			if (!strReply.empty())
				strReply.push_back(',');
			strReply += strFolder;
		}
	}
	return RLV_RET_SUCCESS;
}

struct rlv_wear_info { U32 cntWorn, cntTotal, cntChildWorn, cntChildTotal; };

// Checked: 2010-04-05 (RLVa-1.1.3a) | Modified: RLVa-1.1.0f
ERlvCmdRet RlvHandler::onGetInvWorn(const RlvCommand& rlvCmd, std::string& strReply) const
{
	// Sanity check - gAgentAvatarp can't be NULL [see RlvForceWearLegacy::isWearingItem()]
	if (!gAgentAvatarp)
		return RLV_RET_FAILED;
	// Sanity check - folder should exist
	LLViewerInventoryCategory* pFolder = RlvInventory::instance().getSharedFolder(rlvCmd.getOption());
	if (!pFolder)
		return (RlvInventory::instance().getSharedRoot() != NULL) ? RLV_RET_FAILED_OPTION : RLV_RET_FAILED_NOSHAREDROOT;

	// Collect everything @attachall would be attaching
	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	RlvWearableItemCollector f(pFolder, RlvForceWear::ACTION_WEAR_REPLACE, RlvForceWear::FLAG_MATCHALL);
	gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, f, TRUE);

	rlv_wear_info wi = {0};

	// Add all the folders to a lookup map 
	std::map<LLUUID, rlv_wear_info> mapFolders;
	mapFolders.insert(std::pair<LLUUID, rlv_wear_info>(pFolder->getUUID(), wi));
	for (S32 idxFolder = 0, cntFolder = folders.count(); idxFolder < cntFolder; idxFolder++)
		mapFolders.insert(std::pair<LLUUID, rlv_wear_info>(folders.get(idxFolder)->getUUID(), wi));

	// Iterate over all the found items
	LLViewerInventoryItem* pItem; std::map<LLUUID, rlv_wear_info>::iterator itFolder;
	for (S32 idxItem = 0, cntItem = items.count(); idxItem < cntItem; idxItem++)
	{
		pItem = items.get(idxItem);
		if (!RlvForceWear::isWearableItem(pItem))
			continue;

		// The "folded parent" is the folder this item should be considered a direct descendent of (may or may not match actual parent)
		const LLUUID& idParent = f.getFoldedParent(pItem->getParentUUID());

		// Walk up the tree: sooner or later one of the parents will be a folder in the map
		LLViewerInventoryCategory* pParent = gInventory.getCategory(idParent);
		while ( (itFolder = mapFolders.find(pParent->getUUID())) == mapFolders.end() )
			pParent = gInventory.getCategory(pParent->getParentUUID());

		U32 &cntWorn  = (idParent == pParent->getUUID()) ? itFolder->second.cntWorn : itFolder->second.cntChildWorn, 
			&cntTotal = (idParent == pParent->getUUID()) ? itFolder->second.cntTotal : itFolder->second.cntChildTotal;

		if (RlvForceWear::isWearingItem(pItem))
			cntWorn++;
		cntTotal++;
	}

	// Extract the result for the main folder
	itFolder = mapFolders.find(pFolder->getUUID());
	wi.cntWorn = itFolder->second.cntWorn;
	wi.cntTotal = itFolder->second.cntTotal;
	mapFolders.erase(itFolder);

	// Build the result for each child folder
	for (itFolder = mapFolders.begin(); itFolder != mapFolders.end(); ++itFolder)
	{
		rlv_wear_info& wiFolder = itFolder->second;

		wi.cntChildWorn += wiFolder.cntWorn + wiFolder.cntChildWorn;
		wi.cntChildTotal += wiFolder.cntTotal + wiFolder.cntChildTotal;

		strReply += llformat(",%s|%d%d", gInventory.getCategory(itFolder->first)->getName().c_str(),
		 (0 == wiFolder.cntTotal) ? 0 : (0 == wiFolder.cntWorn) ? 1 : (wiFolder.cntWorn != wiFolder.cntTotal) ? 2 : 3,
		 (0 == wiFolder.cntChildTotal) ? 0 : (0 == wiFolder.cntChildWorn) ? 1 : (wiFolder.cntChildWorn != wiFolder.cntChildTotal) ? 2 : 3
		);
	}

	// Now just prepend the root and done
	strReply = llformat("|%d%d", (0 == wi.cntTotal) ? 0 : (0 == wi.cntWorn) ? 1 : (wi.cntWorn != wi.cntTotal) ? 2 : 3,
		(0 == wi.cntChildTotal) ? 0 : (0 == wi.cntChildWorn) ? 1 : (wi.cntChildWorn != wi.cntChildTotal) ? 2: 3) + strReply;

	return RLV_RET_SUCCESS;
}

// Checked: 2010-03-19 (RLVa-1.2.0c) | Modified: RLVa-1.2.0a
ERlvCmdRet RlvHandler::onGetOutfit(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT(RLV_BHVR_GETOUTFIT == rlvCmd.getBehaviourType());

	// (Compatibility: RLV-1.16.1 will execute @getoutfit=<channel> if <layer> is invalid while we just return failure)
	LLWearableType::EType wtType = LLWearableType::typeNameToType(rlvCmd.getOption());
	if ( (LLWearableType::WT_NONE == wtType) && (!rlvCmd.getOption().empty()) )
		return RLV_RET_FAILED_OPTION;

	const LLWearableType::EType wtRlvTypes[] =
		{ 
			LLWearableType::WT_GLOVES, LLWearableType::WT_JACKET, LLWearableType::WT_PANTS, LLWearableType::WT_SHIRT, LLWearableType::WT_SHOES, LLWearableType::WT_SKIRT, LLWearableType::WT_SOCKS, 
			LLWearableType::WT_UNDERPANTS, LLWearableType::WT_UNDERSHIRT, LLWearableType::WT_SKIN, LLWearableType::WT_EYES, LLWearableType::WT_HAIR, LLWearableType::WT_SHAPE, LLWearableType::WT_ALPHA, LLWearableType::WT_TATTOO, LLWearableType::WT_PHYSICS
		};

	for (int idxType = 0; idxType < sizeof(wtRlvTypes) / sizeof(wtRlvTypes[0]); ++idxType)
	{
		if ( (LLWearableType::WT_NONE == wtType) || (wtRlvTypes[idxType] == wtType) )
		{
			// We never hide body parts, even if they're "locked" and we're hiding locked layers
			// (nor do we hide a layer if the issuing object is the only one that has this layer locked)
			bool fWorn = (gAgentWearables.getWearable(wtRlvTypes[idxType], 0)) && 	// TODO: MULTI-WEARABLE
				( (!RlvSettings::getHideLockedLayers()) || 
				  (LLAssetType::AT_BODYPART == LLWearableType::getAssetType(wtRlvTypes[idxType])) ||
				  (RlvForceWear::isForceRemovable(wtRlvTypes[idxType], true, rlvCmd.getObjectID())) );
			strReply.push_back( (fWorn) ? '1' : '0' );
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2009-11-21 (RLVa-1.1.0f) | Added: RLVa-1.1.0e
ERlvCmdRet RlvHandler::onGetOutfitNames(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_GETOUTFITNAMES == rlvCmd.getBehaviourType()) || (RLV_BHVR_GETADDOUTFITNAMES == rlvCmd.getBehaviourType()) || 
		        (RLV_BHVR_GETREMOUTFITNAMES == rlvCmd.getBehaviourType()) );

	// Sanity check - all these commands are optionless
	if (!rlvCmd.getOption().empty())
		return RLV_RET_FAILED_OPTION;

	// RELEASE-RLVa: [SL-2.0.0] Needs revisiting/rewriting once 'LLAgentWearables::MAX_WEARABLES_PER_TYPE > 1'
	for (int idxType = 0; idxType < LLWearableType::WT_COUNT; idxType++)
	{
		bool fAdd = false; LLWearableType::EType wtType = (LLWearableType::EType)idxType;
		switch (rlvCmd.getBehaviourType())
		{
			case RLV_BHVR_GETOUTFITNAMES:		// Every layer that's worn
				fAdd = (gAgentWearables.getWearable(wtType, 0) != NULL);	// TODO: MULTI-WEARABLE
				break;
/*
			case RLV_BHVR_GETADDOUTFITNAMES:	// Every layer that can be worn on (but ignore any locks set by the issuer)
				fAdd = (isWearable(wtType)) && ( (gAgent.getWearable(wtType) == NULL) || (isRemovableExcept(wtType, idObj)) );
				break;
*/
			case RLV_BHVR_GETREMOUTFITNAMES:	// Every layer that can be removed (but ignore any locks set by the issuer)
				fAdd = RlvForceWear::isForceRemovable(wtType);
				break;
			default:
				break;
		}

		if (fAdd)
		{
			if (!strReply.empty())
				strReply.push_back(',');
			strReply.append(LLWearableType::getTypeName((LLWearableType::EType)idxType));
		}
	}
	return RLV_RET_SUCCESS;
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
ERlvCmdRet RlvHandler::onGetPath(const RlvCommand& rlvCmd, std::string& strReply) const
{
	RLV_ASSERT(RLV_TYPE_REPLY == rlvCmd.getParamType());
	RLV_ASSERT( (RLV_BHVR_GETPATH == rlvCmd.getBehaviourType()) || (RLV_BHVR_GETPATHNEW == rlvCmd.getBehaviourType()) ); 

	RlvCommandOptionGetPath rlvGetPathOption(rlvCmd);
	if (!rlvGetPathOption.isValid())
		return RLV_RET_FAILED_OPTION;

	LLInventoryModel::cat_array_t folders;
	if (RlvInventory::instance().getPath(rlvGetPathOption.getItemIDs(), folders))
	{
		if (RLV_BHVR_GETPATH == rlvCmd.getBehaviourType())
		{
			strReply = RlvInventory::instance().getSharedPath(folders.get(0));
		}
		else if (RLV_BHVR_GETPATHNEW == rlvCmd.getBehaviourType())
		{
			for (S32 idxFolder = 0, cntFolder = folders.count(); idxFolder < cntFolder; idxFolder++)
			{
				if (!strReply.empty())
					strReply.push_back(',');
				strReply += RlvInventory::instance().getSharedPath(folders.get(idxFolder));
			}
		}
	}
	return RLV_RET_SUCCESS;
}

// ============================================================================
