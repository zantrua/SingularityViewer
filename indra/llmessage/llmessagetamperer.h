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

class LLMessageTamperer
{
public:
	static std::map<std::string, int> tamperedTypes;
	static bool tamperingAny;

	static bool isTampered(std::string messageType, bool inbound);
	static bool isAnythingTampered();
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
