//<edit>
//2010 Hazim Gazov
//WTFPL v2

#include <string.h>
#include "stdtypes.h"
#include <map>
#include "llmessagetamperer.h"
#include "llmessagetemplate.h"
#include "llmessageconfig.h"
#include <boost/tokenizer.hpp>

std::map<std::string, int> LLMessageTamperer::tamperedTypes;

//keep this set at false until we actually start tampering with a message type to avoid any needless checks
bool LLMessageTamperer::tamperingAny = false;

//static
bool LLMessageTamperer::isTampered(std::string messageType, bool inbound)
{
	if(tamperedTypes[messageType] == MessageDirection::NONE)
		return false;

	if(inbound)
	{
		return (tamperedTypes[messageType] & MessageDirection::INBOUND);
	}
	else
	{
		return (tamperedTypes[messageType] & MessageDirection::OUTBOUND);
	}
}

//static
bool LLMessageTamperer::isAnythingTampered()
{
	std::map<std::string, int>::iterator message_types_end = tamperedTypes.end();
	std::map<std::string, int>::iterator message_types_iter;

	for(message_types_iter = tamperedTypes.begin(); message_types_iter != message_types_end; ++message_types_iter)
	{
		if((*message_types_iter).second != MessageDirection::NONE)
		{
			tamperingAny = true;
			return true;
		}
	}

	tamperingAny = false;
	return false;
}

//<edit>
