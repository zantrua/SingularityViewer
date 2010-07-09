// <edit>
#include "llviewerprecompiledheaders.h"
#include "llfloatermessagelog.h"
#include "lluictrlfactory.h"
#include "llworld.h"
#include "llviewerregion.h"
#include "llscrolllistctrl.h"
#include "lltexteditor.h"
#include "llviewerwindow.h" // alertXml
#include "llmessagetemplate.h"
#include <boost/tokenizer.hpp>
#include "llmenugl.h"
#include "llfloatermessagebuilder.h"
#include "llagent.h"

LLMessageLogFilter::LLMessageLogFilter()
{
}
LLMessageLogFilter::~LLMessageLogFilter()
{
}
BOOL LLMessageLogFilter::set(std::string filter)
{
	mPositiveNames.clear();
	mNegativeNames.clear();
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ","",boost::keep_empty_tokens);
	boost::tokenizer<boost::char_separator<char> > tokens(filter, sep);
	boost::tokenizer<boost::char_separator<char> >::iterator end = tokens.end();
	for(boost::tokenizer<boost::char_separator<char> >::iterator iter = tokens.begin(); iter != end; ++iter)
	{
		std::string token = (*iter);
		LLStringUtil::trim(token);
		LLStringUtil::toLower(token);
		BOOL negative = token.find("!") == 0;
		if(negative)
		{
			token = token.substr(1);
			mNegativeNames.push_back(token);
		}
		else
			mPositiveNames.push_back(token);
	}
	return TRUE;
}
////////////////////////////////
// LLMessageLogFilterApply
////////////////////////////////
LLMessageLogFilterApply::LLMessageLogFilterApply()
:	LLEventTimer(0.1f),
	mFinished(FALSE),
	mProgress(0)
{
	mIter = LLFloaterMessageLog::sMessageLogEntries.begin();
}
void LLMessageLogFilterApply::cancel()
{
	mFinished = TRUE;
}
BOOL LLMessageLogFilterApply::tick()
{
	std::deque<LLMessageLogEntry>::iterator end = LLFloaterMessageLog::sMessageLogEntries.end();
	if(mIter == end || !LLFloaterMessageLog::sInstance)
	{
		mFinished = TRUE;
		if(LLFloaterMessageLog::sInstance)
		{
			if(LLFloaterMessageLog::sInstance->mMessageLogFilterApply == this)
			{
				LLFloaterMessageLog::sInstance->stopApplyingFilter();
			}
		}
		return TRUE;
	}
	for(S32 i = 0; i < 256; i++)
	{
		if(mIter == end)
		{
			mFinished = TRUE;
			if(LLFloaterMessageLog::sInstance)
			{
				if(LLFloaterMessageLog::sInstance->mMessageLogFilterApply == this)
				{
					LLFloaterMessageLog::sInstance->stopApplyingFilter();

					//we're done messing with the deque, push all queued items to the main deque
					std::deque<LLMessageLogEntry>::iterator queueIter = mQueuedMessages.begin();
					std::deque<LLMessageLogEntry>::iterator queueEnd = mQueuedMessages.end();

					while(queueIter != queueEnd)
					{
						LLFloaterMessageLog::sInstance->conditionalLog(LLPrettyDecodedMessage((*queueIter)));
						++queueIter;
					}

					mQueuedMessages.clear();
				}
			}

			return TRUE;
		}

		LLFloaterMessageLog::sInstance->conditionalLog(LLPrettyDecodedMessage((*mIter)));
		
		mIter++;
		mProgress++;
	}
	LLFloaterMessageLog::sInstance->updateFilterStatus();
	return FALSE;
}
////////////////////////////////
// LLFloaterMessageLog
////////////////////////////////
LLFloaterMessageLog* LLFloaterMessageLog::sInstance;
std::list<LLNetListItem*> LLFloaterMessageLog::sNetListItems;
std::deque<LLMessageLogEntry> LLFloaterMessageLog::sMessageLogEntries;
std::vector<LLPrettyDecodedMessage> LLFloaterMessageLog::sFloaterMessageLogItems;
LLMessageLogFilter LLFloaterMessageLog::sMessageLogFilter = LLMessageLogFilter();
std::string LLFloaterMessageLog::sMessageLogFilterString("!StartPingCheck !CompletePingCheck !PacketAck !SimulatorViewerTimeMessage !SimStats !AgentUpdate !AgentAnimation !AvatarAnimation !ViewerEffect !CoarseLocationUpdate !LayerData !CameraConstraint !ObjectUpdateCached !RequestMultipleObjects !ObjectUpdate !ObjectUpdateCompressed !ImprovedTerseObjectUpdate !KillObject !ImagePacket !SendXferPacket !ConfirmXferPacket !TransferPacket !SoundTrigger !AttachedSound !PreloadSound");
BOOL LLFloaterMessageLog::sBusyApplyingFilter = FALSE;
LLFloaterMessageLog::LLFloaterMessageLog()
:	LLFloater(),
	LLEventTimer(1.0f),
	mNetInfoMode(NI_NET),
	mMessageLogFilterApply(NULL)
{
	sInstance = this;
	LLMessageLog::setCallback(onLog);
	sMessageLogEntries = LLMessageLog::getDeque();
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_message_log.xml");
}
LLFloaterMessageLog::~LLFloaterMessageLog()
{
	LLMessageLog::setCallback(NULL);
	stopApplyingFilter();
	sInstance = NULL;
	sNetListItems.clear();
	sMessageLogEntries.clear();
	sFloaterMessageLogItems.clear();
}
// static
void LLFloaterMessageLog::show()
{
	if(!sInstance) sInstance = new LLFloaterMessageLog();
	sInstance->open();
}
BOOL LLFloaterMessageLog::postBuild()
{
	childSetCommitCallback("net_list", onCommitNetList, this);
	childSetCommitCallback("message_log", onCommitMessageLog, this);
	childSetAction("filter_choice_btn", onClickFilterChoice, this);
	childSetAction("filter_apply_btn", onClickFilterApply, this);
	childSetCommitCallback("filter_edit", onCommitFilter, this);
	childSetAction("clear_log_btn", onClickClearLog, this);
	childSetAction("send_to_message_builder_btn", onClickSendToMessageBuilder, this);
	childSetText("filter_edit", sMessageLogFilterString);
	refreshNetList();
	refreshNetInfo(TRUE);
	startApplyingFilter(sMessageLogFilterString, TRUE);
	return TRUE;
}
BOOL LLFloaterMessageLog::tick()
{
	refreshNetList();
	refreshNetInfo(FALSE);
	return FALSE;
}
LLNetListItem* LLFloaterMessageLog::findNetListItem(LLHost host)
{
	std::list<LLNetListItem*>::iterator end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		if((*iter)->mCircuitData && (*iter)->mCircuitData->getHost() == host)
			return (*iter);
	return NULL;
}
LLNetListItem* LLFloaterMessageLog::findNetListItem(LLUUID id)
{
	std::list<LLNetListItem*>::iterator end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		if((*iter)->mID == id)
			return (*iter);
	return NULL;
}
void LLFloaterMessageLog::refreshNetList()
{
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("net_list");
	// Update circuit data of net list items
	std::vector<LLCircuitData*> circuits = gMessageSystem->getCircuit()->getCircuitDataList();
	std::vector<LLCircuitData*>::iterator circuits_end = circuits.end();
	for(std::vector<LLCircuitData*>::iterator iter = circuits.begin(); iter != circuits_end; ++iter)
	{
		LLNetListItem* itemp = findNetListItem((*iter)->getHost());
		if(!itemp)
		{
			LLUUID id; id.generate();
			itemp = new LLNetListItem(id);
			sNetListItems.push_back(itemp);
		}
		itemp->mCircuitData = (*iter);
	}
	// Clear circuit data of items whose circuits are gone
	std::list<LLNetListItem*>::iterator items_end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != items_end; ++iter)
	{
		if(std::find(circuits.begin(), circuits.end(), (*iter)->mCircuitData) == circuits.end())
			(*iter)->mCircuitData = NULL;
	}
	// Remove net list items that are totally useless now
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != sNetListItems.end();)
	{
		if((*iter)->mCircuitData == NULL)
			iter = sNetListItems.erase(iter);
		else ++iter;
	}
	// Update names of net list items
	items_end = sNetListItems.end();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != items_end; ++iter)
	{
		LLNetListItem* itemp = (*iter);
		if(itemp->mAutoName)
		{
			if(itemp->mCircuitData)
			{
				LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(itemp->mCircuitData->getHost());
				if(regionp)
				{
					std::string name = regionp->getName();
					if(name == "") name = llformat("%s (awaiting region name)", itemp->mCircuitData->getHost().getString().c_str());
					itemp->mName = name;
					itemp->mPreviousRegionName = name;
				}
				else
				{
					itemp->mName = itemp->mCircuitData->getHost().getString();
					if(itemp->mPreviousRegionName != "")
						itemp->mName.append(llformat(" (was %s)", itemp->mPreviousRegionName.c_str()));
				}
			}
			else
			{
				// an item just for an event queue, not handled yet
				itemp->mName = "Something else";
			}
		}
	}
	// Rebuild scroll list from scratch
	LLUUID selected_id = scrollp->getFirstSelected() ? scrollp->getFirstSelected()->getUUID() : LLUUID::null;
	S32 scroll_pos = scrollp->getScrollPos();
	scrollp->clearRows();
	for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != items_end; ++iter)
	{
		LLNetListItem* itemp = (*iter);
		LLSD element;
		element["id"] = itemp->mID;
		LLSD& text_column = element["columns"][0];
		text_column["column"] = "text";
		text_column["value"] = itemp->mName + (itemp->mCircuitData->getHost() == gAgent.getRegionHost() ? " (main)" : "");
		for(int i = 0; i < 2; i++)
		{
			LLSD& icon_column = element["columns"][i + 1];
			icon_column["column"] = llformat("icon%d", i);
			icon_column["type"] = "icon";
			icon_column["value"] = "";
		}
		LLScrollListItem* scroll_itemp = scrollp->addElement(element);
		BOOL has_live_circuit = itemp->mCircuitData && itemp->mCircuitData->isAlive();
		if(has_live_circuit)
		{
			LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(1);
			icon->setValue("icon_net_close_circuit.tga");
			icon->setClickCallback(onClickCloseCircuit, itemp);
		}
		else
		{
			LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(1);
			icon->setValue("icon_net_close_circuit_gray.tga");
			icon->setClickCallback(NULL, NULL);
		}
		// Event queue isn't even supported yet... FIXME
		LLScrollListIcon* icon = (LLScrollListIcon*)scroll_itemp->getColumn(2);
		icon->setValue("icon_net_close_eventpoll_gray.tga");
		icon->setClickCallback(NULL, NULL);
	}
	if(selected_id.notNull()) scrollp->selectByID(selected_id);
	if(scroll_pos < scrollp->getItemCount()) scrollp->setScrollPos(scroll_pos);
}
void LLFloaterMessageLog::refreshNetInfo(BOOL force)
{
	if(mNetInfoMode != NI_NET) return;
	LLScrollListCtrl* scrollp = getChild<LLScrollListCtrl>("net_list");
	LLScrollListItem* selected_itemp = scrollp->getFirstSelected();
	if(selected_itemp)
	{
		if(!force) if(getChild<LLTextEditor>("net_info")->hasSelection()) return;
		LLNetListItem* itemp = findNetListItem(selected_itemp->getUUID());
		if(itemp)
		{
			std::string info(llformat("%s\n--------------------------------\n\n", itemp->mName.c_str()));
			if(itemp->mCircuitData)
			{
				LLCircuitData* cdp = itemp->mCircuitData;
				info.append("Circuit\n--------------------------------\n");
				info.append(llformat(" * Host: %s\n", cdp->getHost().getString().c_str()));
				S32 seconds = (S32)cdp->getAgeInSeconds();
				S32 minutes = seconds / 60;
				seconds = seconds % 60;
				S32 hours = minutes / 60;
				minutes = minutes % 60;
				info.append(llformat(" * Age: %dh %dm %ds\n", hours, minutes, seconds));
				info.append(llformat(" * Alive: %s\n", cdp->isAlive() ? "yes" : "no"));
				info.append(llformat(" * Blocked: %s\n", cdp->isBlocked() ? "yes" : "no"));
				info.append(llformat(" * Allow timeout: %s\n", cdp->getAllowTimeout() ? "yes" : "no"));
				info.append(llformat(" * Trusted: %s\n", cdp->getTrusted() ? "yes" : "no"));
				info.append(llformat(" * Ping delay: %d\n", cdp->getPingDelay()));
				info.append(llformat(" * Packets out: %d\n", cdp->getPacketsOut()));
				info.append(llformat(" * Bytes out: %d\n", cdp->getBytesOut()));
				info.append(llformat(" * Packets in: %d\n", cdp->getPacketsIn()));
				info.append(llformat(" * Bytes in: %d\n", cdp->getBytesIn()));
				info.append(llformat(" * Endpoint ID: %s\n", cdp->getLocalEndPointID().asString().c_str()));
				info.append(llformat(" * Remote ID: %s\n", cdp->getRemoteID().asString().c_str()));
				info.append(llformat(" * Remote session ID: %s\n", cdp->getRemoteSessionID().asString().c_str()));
			}
			childSetText("net_info", info);
		}
		else childSetText("net_info", std::string(""));
	}
	else childSetText("net_info", std::string(""));
}
void LLFloaterMessageLog::setNetInfoMode(ENetInfoMode mode)
{
	mNetInfoMode = mode;
	if(mNetInfoMode == NI_NET)
		refreshNetInfo(TRUE);
	childSetEnabled("send_to_message_builder_btn", mNetInfoMode == NI_LOG);
}
// static
void LLFloaterMessageLog::onLog(LLMessageLogEntry entry)
{
	//don't mess with the queue while a filter's being applied, or face invalid iterators
	if(!sBusyApplyingFilter)
	{
		sMessageLogEntries.push_back(entry);
		conditionalLog(LLPrettyDecodedMessage(entry));
	}
}
// static
void LLFloaterMessageLog::conditionalLog(LLPrettyDecodedMessage item)
{	
	if(!sBusyApplyingFilter)
		sInstance->childSetText("log_status_text", llformat("Showing %d messages from %d", sFloaterMessageLogItems.size(), sMessageLogEntries.size()));
	std::string find_name = item.mName;
	LLStringUtil::toLower(find_name);
	if(sMessageLogFilter.mPositiveNames.size())
		if(std::find(sMessageLogFilter.mPositiveNames.begin(), sMessageLogFilter.mPositiveNames.end(), find_name) == sMessageLogFilter.mPositiveNames.end())
			return;
	if(std::find(sMessageLogFilter.mNegativeNames.begin(), sMessageLogFilter.mNegativeNames.end(), find_name) != sMessageLogFilter.mNegativeNames.end())
		return;
	sFloaterMessageLogItems.push_back(item); // moved from beginning...
	BOOL outgoing = item.isOutgoing();
	std::string net_name("\?\?\?");
	if(item.mType == LLPrettyDecodedMessage::TEMPLATE)
	{
		LLHost find_host = outgoing ? item.mToHost : item.mFromHost;
		net_name = find_host.getIPandPort();
		std::list<LLNetListItem*>::iterator end = sNetListItems.end();
		for(std::list<LLNetListItem*>::iterator iter = sNetListItems.begin(); iter != end; ++iter)
		{
			if((*iter)->mCircuitData->getHost() == find_host)
			{
				net_name = (*iter)->mName;
				break;
			}
		}
	}
	LLSD element;
	element["id"] = item.mID;
	LLSD& sequence_column = element["columns"][0];
	sequence_column["column"] = "sequence";
	sequence_column["value"] = llformat("%u", item.mSequenceID);
	LLSD& type_column = element["columns"][1];
	type_column["column"] = "type";
	type_column["value"] = item.mType == LLPrettyDecodedMessage::TEMPLATE ? "UDP" : "\?\?\?";
	LLSD& direction_column = element["columns"][2];
	direction_column["column"] = "direction";
	direction_column["value"] = outgoing ? "to" : "from";
	LLSD& net_column = element["columns"][3];
	net_column["column"] = "net";
	net_column["value"] = net_name;
	LLSD& name_column = element["columns"][4];
	name_column["column"] = "name";
	name_column["value"] = item.mName;
	/*
	LLSD& zer_column = element["columns"][5];
	zer_column["column"] = "flag_zer";
	zer_column["type"] = "icon";
	zer_column["value"] = (item.mFlags & LL_ZERO_CODE_FLAG) ? "flag_zer.tga" : "";
	LLSD& rel_column = element["columns"][6];
	rel_column["column"] = "flag_rel";
	rel_column["type"] = "icon";
	rel_column["value"] = (item.mFlags & LL_RELIABLE_FLAG) ? "flag_rel.tga" : "";
	LLSD& rsd_column = element["columns"][7];
	rsd_column["column"] = "flag_rsd";
	rsd_column["type"] = "icon";
	rsd_column["value"] = (item.mFlags & LL_RESENT_FLAG) ? "flag_rsd.tga" : "";
	LLSD& ack_column = element["columns"][8];
	ack_column["column"] = "flag_ack";
	ack_column["type"] = "icon";
	ack_column["value"] = (item.mFlags & LL_ACK_FLAG) ? "flag_ack.tga" : "";
	*/
	LLSD& summary_column = element["columns"][5];
	summary_column["column"] = "summary";
	summary_column["value"] = item.mSummary;
	LLScrollListCtrl* scrollp = sInstance->getChild<LLScrollListCtrl>("message_log");
	S32 scroll_pos = scrollp->getScrollPos();
	scrollp->addElement(element);
	if(scroll_pos > scrollp->getItemCount() - scrollp->getPageLines() - 4)
		scrollp->setScrollPos(scrollp->getItemCount());
}
// static
void LLFloaterMessageLog::onCommitNetList(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->setNetInfoMode(NI_NET);
	floaterp->refreshNetInfo(TRUE);
}
// static
void LLFloaterMessageLog::onCommitMessageLog(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	LLScrollListCtrl* scrollp = floaterp->getChild<LLScrollListCtrl>("message_log");
	LLScrollListItem* selected_itemp = scrollp->getFirstSelected();
	if(!selected_itemp) return;
	LLUUID id = selected_itemp->getUUID();
	std::vector<LLPrettyDecodedMessage>::iterator end = sFloaterMessageLogItems.end();
	for(std::vector<LLPrettyDecodedMessage>::iterator iter = sFloaterMessageLogItems.begin(); iter != end; ++iter)
	{
		if(iter->mID == id)
		{
			floaterp->setNetInfoMode(NI_LOG);
			floaterp->childSetText("net_info", iter->getFull(FALSE));
			break;
		}
	}
}
// static
BOOL LLFloaterMessageLog::onClickCloseCircuit(void* user_data)
{
	LLNetListItem* itemp = (LLNetListItem*)user_data;
	LLCircuitData* cdp = (LLCircuitData*)itemp->mCircuitData;
	if(!cdp) return FALSE;
	LLHost myhost = cdp->getHost();
	LLSD args;
	args["MESSAGE"] = "This will delete local circuit data.\nDo you want to tell the remote host to close the circuit too?";
	LLSD payload;
	payload["circuittoclose"] = myhost.getString(); 
	LLNotifications::instance().add("GenericAlertYesCancel", args, payload, onConfirmCloseCircuit);
	return TRUE;
}
// static
bool LLFloaterMessageLog::onConfirmCloseCircuit(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLCircuitData* cdp = gMessageSystem->mCircuitInfo.findCircuit(LLHost(notification["payload"]["circuittoclose"].asString()));
	if(!cdp) return false;
	LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(cdp->getHost());
	switch(option)
	{
	case 0: // yes
		gMessageSystem->newMessageFast(_PREHASH_CloseCircuit);
		gMessageSystem->sendReliable(cdp->getHost());
		break;
	case 2: // cancel
		return false;
		break;
	case 1: // no
	default:
		break;
	}
	if(gMessageSystem->findCircuitCode(cdp->getHost()))
		gMessageSystem->disableCircuit(cdp->getHost());
	else
		gMessageSystem->getCircuit()->removeCircuitData(cdp->getHost());
	if(regionp)
	{
		LLHost myhost = regionp->getHost();
		LLSD args;
		args["MESSAGE"] = "That host had a region associated with it.\nDo you want to clean that up?";
		LLSD payload;
		payload["regionhost"] = myhost.getString();
		LLNotifications::instance().add("GenericAlertYesCancel", args, payload, onConfirmRemoveRegion);
	}
	return false;
}
// static
bool LLFloaterMessageLog::onConfirmRemoveRegion(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if(option == 0) // yes
		LLWorld::getInstance()->removeRegion(LLHost(notification["payload"]["regionhost"].asString()));
	return false;
}
// static
void LLFloaterMessageLog::onClickFilterApply(void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->startApplyingFilter(floaterp->childGetValue("filter_edit"), FALSE);
}
void LLFloaterMessageLog::startApplyingFilter(std::string filter, BOOL force)
{
	LLMessageLogFilter new_filter = LLMessageLogFilter();
	sMessageLogFilterString = filter;
	new_filter.set(sMessageLogFilterString);
	if(!filter.length() || filter.at(filter.length()-1) != ' ')
		childSetText("filter_edit", filter + " ");
	if(force
		|| (new_filter.mNegativeNames != sMessageLogFilter.mNegativeNames)
		|| (new_filter.mPositiveNames != sMessageLogFilter.mPositiveNames))
	{
		stopApplyingFilter();
		sMessageLogFilter = new_filter;
		sFloaterMessageLogItems.clear();
		getChild<LLScrollListCtrl>("message_log")->clearRows();
		sBusyApplyingFilter = TRUE;
		childSetVisible("message_log", false);
		//childSetVisible("log_status_text", true);
		mMessageLogFilterApply = new LLMessageLogFilterApply();
	}
}
void LLFloaterMessageLog::stopApplyingFilter()
{
	if(mMessageLogFilterApply)
	{
		if(!(mMessageLogFilterApply->mFinished))
			mMessageLogFilterApply->cancel();
		//delete mMessageLogFilterApply;
		sBusyApplyingFilter = FALSE;
		//childSetVisible("log_status_text", false);
		childSetVisible("message_log", true);
		childSetText("log_status_text", llformat("Showing %d messages from %d", sFloaterMessageLogItems.size(), sMessageLogEntries.size()));
	}
}
void LLFloaterMessageLog::updateFilterStatus()
{
	if(!mMessageLogFilterApply || !sBusyApplyingFilter) return;
	S32 progress = mMessageLogFilterApply->mProgress;
	S32 packets = sMessageLogEntries.size();
	S32 matches = sFloaterMessageLogItems.size();
	std::string text = llformat("Applying filter ( %d / %d ), %d matches ...", progress, packets, matches);
	childSetText("log_status_text", text);
}
// static
void LLFloaterMessageLog::onCommitFilter(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->startApplyingFilter(floaterp->childGetValue("filter_edit"), FALSE);
}
// static
void LLFloaterMessageLog::onClickClearLog(void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	floaterp->stopApplyingFilter();
	floaterp->getChild<LLScrollListCtrl>("message_log")->clearRows();
	floaterp->setNetInfoMode(NI_NET);
	sMessageLogEntries.clear();
	sFloaterMessageLogItems.clear();
}
// static
void LLFloaterMessageLog::onClickFilterChoice(void* user_data)
{
	LLMenuGL* menu = new LLMenuGL(LLStringUtil::null);
	menu->append(new LLMenuItemCallGL("No filter", onClickFilterMenu, NULL, (void*)""));
	menu->append(new LLMenuItemCallGL("Fewer spammy messages", onClickFilterMenu, NULL, (void*)"!StartPingCheck !CompletePingCheck !PacketAck !SimulatorViewerTimeMessage !SimStats !AgentUpdate !AgentAnimation !AvatarAnimation !ViewerEffect !CoarseLocationUpdate !LayerData !CameraConstraint !ObjectUpdateCached !RequestMultipleObjects !ObjectUpdate !ObjectUpdateCompressed !ImprovedTerseObjectUpdate !KillObject !ImagePacket !SendXferPacket !ConfirmXferPacket !TransferPacket"));
	menu->append(new LLMenuItemCallGL("Fewer spammy messages (minus sound crap)", onClickFilterMenu, NULL, (void*)"!StartPingCheck !CompletePingCheck !PacketAck !SimulatorViewerTimeMessage !SimStats !AgentUpdate !AgentAnimation !AvatarAnimation !ViewerEffect !CoarseLocationUpdate !LayerData !CameraConstraint !ObjectUpdateCached !RequestMultipleObjects !ObjectUpdate !ObjectUpdateCompressed !ImprovedTerseObjectUpdate !KillObject !ImagePacket !SendXferPacket !ConfirmXferPacket !TransferPacket !SoundTrigger !AttachedSound !PreloadSound"));
	menu->append(new LLMenuItemCallGL("Object updates", onClickFilterMenu, NULL, (void*)"ObjectUpdateCached ObjectUpdate ObjectUpdateCompressed ImprovedTerseObjectUpdate KillObject RequestMultipleObjects"));
	menu->append(new LLMenuItemCallGL("Abnormal", onClickFilterMenu, NULL, (void*)"Invalid TestMessage AddCircuitCode NeighborList AvatarTextureUpdate SimulatorMapUpdate SimulatorSetMap SubscribeLoad UnsubscribeLoad SimulatorReady SimulatorPresentAtLocation SimulatorLoad SimulatorShutdownRequest RegionPresenceRequestByRegionID RegionPresenceRequestByHandle RegionPresenceResponse UpdateSimulator LogDwellTime FeatureDisabled LogFailedMoneyTransaction UserReportInternal SetSimStatusInDatabase SetSimPresenceInDatabase OpenCircuit CloseCircuit DirFindQueryBackend DirPlacesQueryBackend DirClassifiedQueryBackend DirLandQueryBackend DirPopularQueryBackend GroupNoticeAdd DataHomeLocationRequest DataHomeLocationReply DerezContainer ObjectCategory ObjectExportSelected StateSave ReportAutosaveCrash AgentAlertMessage NearestLandingRegionRequest NearestLandingRegionReply NearestLandingRegionUpdated TeleportLandingStatusChanged ConfirmEnableSimulator KickUserAck SystemKickUser AvatarPropertiesRequestBackend UpdateParcel RemoveParcel MergeParcel LogParcelChanges CheckParcelSales ParcelSales StartAuction ConfirmAuctionStart CompleteAuction CancelAuction CheckParcelAuctions ParcelAuctions ChatPass EdgeDataPacket SimStatus ChildAgentUpdate ChildAgentAlive ChildAgentPositionUpdate ChildAgentDying ChildAgentUnknown AtomicPassObject KillChildAgents ScriptSensorRequest ScriptSensorReply DataServerLogout RequestInventoryAsset InventoryAssetResponse TransferInventory TransferInventoryAck EventLocationRequest EventLocationReply MoneyTransferBackend RoutedMoneyBalanceReply SetStartLocation NetTest SetCPURatio SimCrashed NameValuePair RemoveNameValuePair UpdateAttachment RemoveAttachment EmailMessageRequest EmailMessageReply InternalScriptMail ScriptDataRequest ScriptDataReply InviteGroupResponse TallyVotes LiveHelpGroupRequest LiveHelpGroupReply GroupDataUpdate LogTextMessage CreateTrustedCircuit ParcelRename SystemMessage RpcChannelRequest RpcChannelReply RpcScriptRequestInbound RpcScriptRequestInboundForward RpcScriptReplyInbound ScriptMailRegistration Error"));
	menu->updateParent(LLMenuGL::sMenuContainer);
	menu->setCanTearOff(FALSE);
	LLView* buttonp = sInstance->getChild<LLView>("filter_choice_btn");
	S32 x = buttonp->getRect().mLeft;
	S32 y = buttonp->getRect().mBottom;
	LLMenuGL::showPopup(sInstance, menu, x, y);
}
// static
void LLFloaterMessageLog::onClickFilterMenu(void* user_data)
{
	std::string filter = std::string((char*)user_data);
	sInstance->childSetText("filter_edit", filter);
	sInstance->startApplyingFilter(filter, FALSE);
}
// static
void LLFloaterMessageLog::onClickSendToMessageBuilder(void* user_data)
{
	LLFloaterMessageLog* floaterp = (LLFloaterMessageLog*)user_data;
	LLScrollListCtrl* scrollp = floaterp->getChild<LLScrollListCtrl>("message_log");
	LLScrollListItem* selected_itemp = scrollp->getFirstSelected();
	if(!selected_itemp) return;
	LLUUID id = selected_itemp->getUUID();
	std::vector<LLPrettyDecodedMessage>::iterator end = sFloaterMessageLogItems.end();
	for(std::vector<LLPrettyDecodedMessage>::iterator iter = sFloaterMessageLogItems.begin(); iter != end; ++iter)
	{
		if(iter->mID == id)
		{
			std::string message_text = iter->getFull(FALSE);
			LLFloaterMessageBuilder::show(message_text);
			break;
		}
	}
}
// </edit>
