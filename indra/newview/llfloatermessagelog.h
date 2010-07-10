// <edit>
#include "llfloater.h"
#include "llmessagelog.h"
#include "lltemplatemessagereader.h"
#include "llfloatermessagebuilder.h"

class LLMessageLogFilter
{
public:
	LLMessageLogFilter();
	~LLMessageLogFilter();
	BOOL set(std::string filter);
	std::list<std::string> mPositiveNames;
	std::list<std::string> mNegativeNames;
};
class LLMessageLogFilterApply : public LLEventTimer
{
public:
	LLMessageLogFilterApply();
	void cancel();
	BOOL tick();
	S32 mProgress;
	BOOL mFinished;
private:
	std::deque<LLSafeMessageEntry> mQueuedMessages;
	std::deque<LLSafeMessageEntry>::iterator mIter;
};
class LLFloaterMessageLog : public LLFloater, public LLEventTimer
{
public:
	LLFloaterMessageLog();
	~LLFloaterMessageLog();
	static void show();
	BOOL postBuild();
	BOOL tick();
	LLNetListItem* findNetListItem(LLHost host);
	LLNetListItem* findNetListItem(LLUUID id);
	void refreshNetList();
	void refreshNetInfo(BOOL force);
	enum ENetInfoMode { NI_NET, NI_LOG };
	void setNetInfoMode(ENetInfoMode mode);
	static void onLog(LLSafeMessageEntry entry);
	static void conditionalLog(LLPrettyDecodedMessage item);
	static void onCommitNetList(LLUICtrl* ctrl, void* user_data);
	static void onCommitMessageLog(LLUICtrl* ctrl, void* user_data);
	static void onCommitFilter(LLUICtrl* ctrl, void* user_data);
	static BOOL onClickCloseCircuit(void* user_data);
	static bool onConfirmCloseCircuit(const LLSD& notification, const LLSD& response );
	static bool onConfirmRemoveRegion(const LLSD& notification, const LLSD& response );
	static void onClickFilterApply(void* user_data);
	void startApplyingFilter(std::string filter, BOOL force);
	void stopApplyingFilter();
	void updateFilterStatus();
	static BOOL sBusyApplyingFilter;
	LLMessageLogFilterApply* mMessageLogFilterApply;
	static void onClickClearLog(void* user_data);
	static LLFloaterMessageLog* sInstance;
	static std::list<LLNetListItem*> sNetListItems;
	static std::deque<LLSafeMessageEntry> sMessageLogEntries;
	static std::vector<LLPrettyDecodedMessage> sFloaterMessageLogItems;
	static LLMessageLogFilter sMessageLogFilter;
	static std::string sMessageLogFilterString;
	ENetInfoMode mNetInfoMode;
	static void onClickFilterChoice(void* user_data);
	static void onClickFilterMenu(void* user_data);
	static void onClickSendToMessageBuilder(void* user_data);
};
// </edit>
