//<edit>
//2010 Hazim Gazov
//WTFPL v2

#ifndef LLMESSAGETAMPERER_H
#define LLMESSAGETAMPERER_H

#include <string.h>
#include "stdtypes.h"
#include <map>
#include "llmessagetamperer.h"
#include "llmessagetemplate.h"
#include "llmessageconfig.h"
#include <boost/tokenizer.hpp>
#include "llmessagelog.h"

class LLMessageTamperer
{
public:
	static std::map<std::string, int> tamperedTypes;
	static bool tamperingAny;
	static bool busyTampering;
	static bool sendingTampered;

	static bool isTampered(std::string messageType, bool inbound);
	static bool isAnythingTampered();
	static void tamper(LLHost from_host, LLHost to_host, U8* data, S32 data_size);
	static void setCallback(void (*callback)(std::string, LLHost));

private:
	static void (*sCallback)(std::string, LLHost);
};

struct MessageDirection
{
	enum enumMessageDirection
	{
		NONE,
		INBOUND,
		OUTBOUND
	};
};

#endif // LLMESSAGETAMPERER_H
//</edit>
