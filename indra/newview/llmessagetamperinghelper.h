// <edit>]
#ifndef LL_LLMESSAGETAMPERINGHELPER_H
#define LL_LLMESSAGETAMPERINGHELPER_H
#include "llfloater.h"
#include "lltemplatemessagereader.h"
#include "llmessagelog.h"
#include "llfloatermessagebuilder.h"

class LLMessageTamperingHelper
{
public:
	static void init();
	static void tamperCallback(std::string init_text, LLHost tamperCircuit);
};

#endif
// </edit>
