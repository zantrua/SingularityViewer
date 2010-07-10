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
#include "llmessagelog.h"

std::map<std::string, int> LLMessageTamperer::tamperedTypes;
void (*(LLMessageTamperer::sCallback))(std::string, LLHost) = NULL;

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

//static
void LLMessageTamperer::setCallback(void (*callback)(std::string, LLHost))
{
	sCallback = callback;
}

//static
void LLMessageTamperer::tamper(LLHost from_host, LLHost to_host, U8* data, S32 data_size)
{
	if(!tamperingAny || !sCallback) return;

	LLSafeMessageEntry entry = LLSafeMessageEntry(LLSafeMessageEntry::TEMPLATE, from_host, to_host, data, data_size);

	if(!entry.mDataSize || !entry.mData.size()) return;

	std::string templateName = entry.getTemplateName();

	if(templateName == "[INVALID]" || templateName == "[UNSUPPORTED]") return;
	if(tamperedTypes[templateName] == MessageDirection::NONE) return;

	//message is outgoing, not tampering outgoing
	if(entry.isOutgoing() && !(tamperedTypes[templateName] & MessageDirection::OUTBOUND)) return;
	//message is incoming, not tampering incoming
	if(!entry.isOutgoing() && !(tamperedTypes[templateName] & MessageDirection::INBOUND)) return;

	LLPrettyDecodedMessage prettyEntry(entry);

	sCallback(prettyEntry.getFull(), to_host);

	//do callback here!
}

//<edit>
