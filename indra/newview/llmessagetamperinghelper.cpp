// <edit>
#include "llviewerprecompiledheaders.h"
#include "llfloaterchat.h"
#include "llmessagelog.h"
#include "llmessagetamperer.h"
#include "llmessagetamperinghelper.h"
#include "llfloatermessagebuilder.h"
#include "llfloatermessagetamperer.h"
#include "llnotifications.h"

//static
void LLMessageTamperingHelper::init()
{
	LLMessageTamperer::setCallback(tamperCallback);
}

//static
void LLMessageTamperingHelper::tamperCallback(std::string init_text, LLHost tamperCircuit)
{
	llinfos << init_text << llendl;
	/*
	LLNetListItem* itemp = (LLNetListItem*)user_data;
	LLCircuitData* cdp = (LLCircuitData*)itemp->mCircuitData;
	if(!cdp) return FALSE;
	LLHost myhost = cdp->getHost();
	LLSD args;
	args["MESSAGE"] = "This will delete local circuit data.\nDo you want to tell the remote host to close the circuit too?";
	LLSD payload;
	payload["circuittoclose"] = myhost.getString();
	LLNotifications::instance().add("TamperMessage", args, payload, onConfirmCloseCircuit);*/
}

// </edit>
