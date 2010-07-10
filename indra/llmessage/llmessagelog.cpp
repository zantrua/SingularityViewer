// <edit>
#include "llmessagelog.h"
#include "v4math.h"
#include "v3math.h"
#include "v3dmath.h"
#include "llquaternion.h"
#include "llmessagetemplate.h"
#include <boost/tokenizer.hpp>
#include "lltemplatemessagereader.h"
#include "lltemplatemessagebuilder.h"

LLSafeMessageEntry::LLSafeMessageEntry(EType type, LLHost from_host, LLHost to_host, U8* data, S32 data_size)
:	mType(type),
	mFromHost(from_host),
	mToHost(to_host),
	mDataSize(data_size)
{
	if(data)
	{
		mData.resize(data_size);
		memcpy(&(mData[0]), data, data_size);
	}
}
LLSafeMessageEntry::LLSafeMessageEntry(EType type, LLHost from_host, LLHost to_host, std::vector<U8> data, S32 data_size)
:	mType(type),
	mFromHost(from_host),
	mToHost(to_host),
	mDataSize(data_size),
	mData(data)
{
}
LLSafeMessageEntry::~LLSafeMessageEntry()
{
}
std::string LLSafeMessageEntry::getTemplateName()
{
	LLTemplateMessageReader tempTemplateMessageReader(gMessageSystem->mMessageNumbers);
	if(mType == TEMPLATE)
	{
		S32 decode_len = mDataSize;
		std::vector<U8> DecodeBuffer(MAX_PACKET_LEN,0);
		memcpy(&(DecodeBuffer[0]),&(mData[0]),decode_len);
		U8* decodep = &(DecodeBuffer[0]);
		gMessageSystem->zeroCodeExpand(&decodep, &decode_len);
		if(decode_len < 7)
			return (std::string("[INVALID]"));
		else
		{
			tempTemplateMessageReader.clearMessage();
			if(!tempTemplateMessageReader.validateMessage(decodep, decode_len, mFromHost, TRUE))
				return (std::string("[INVALID]"));
			else
			{
				if(!tempTemplateMessageReader.decodeData(decodep, mFromHost, TRUE))
					return (std::string("[INVALID]"));
				else
				{
					LLMessageTemplate* messageTemplate = tempTemplateMessageReader.getTemplate();
					return std::string(messageTemplate->mName);
				}
			}
		}
	} else {
		//no CAPS support yet :(
		return std::string("[UNSUPPORTED]");
	}
}
BOOL LLSafeMessageEntry::isOutgoing()
{
	return mFromHost == LLHost(16777343, gMessageSystem->getListenPort());
}

U32 LLMessageLog::sMaxSize = 4096; // testzone fixme todo boom
std::deque<LLSafeMessageEntry> LLMessageLog::sDeque;
void (*(LLMessageLog::sCallback))(LLSafeMessageEntry);
void LLMessageLog::setMaxSize(U32 size)
{
	sMaxSize = size;
	while(sDeque.size() > sMaxSize)
		sDeque.pop_front();
}
void LLMessageLog::setCallback(void (*callback)(LLSafeMessageEntry))
{
	sCallback = callback;
}
void LLMessageLog::log(LLHost from_host, LLHost to_host, U8* data, S32 data_size)
{
	LLSafeMessageEntry entry = LLSafeMessageEntry(LLSafeMessageEntry::TEMPLATE, from_host, to_host, data, data_size);
	if(!entry.mDataSize || !entry.mData.size()) return;
	if(sCallback) sCallback(entry);
	if(!sMaxSize) return;
	sDeque.push_back(entry);
	if(sDeque.size() > sMaxSize)
		sDeque.pop_front();
}
std::deque<LLSafeMessageEntry> LLMessageLog::getDeque()
{
	return sDeque;
}

///////////////////////////
//Pretty Message Decoding//
///////////////////////////

LLTemplateMessageReader* LLPrettyDecodedMessage::sTemplateMessageReader = NULL;
LLPrettyDecodedMessage::LLPrettyDecodedMessage(LLSafeMessageEntry entry)
:	LLSafeMessageEntry(entry.mType, entry.mFromHost, entry.mToHost, entry.mData, entry.mDataSize)
{
	if(!sTemplateMessageReader)
	{
		sTemplateMessageReader = new LLTemplateMessageReader(gMessageSystem->mMessageNumbers);
	}
	mID.generate();
	mSequenceID = 0;
	if(mType == TEMPLATE)
	{
		BOOL decode_invalid = FALSE;
		S32 decode_len = mDataSize;
		std::vector<U8> DecodeBuffer(MAX_PACKET_LEN,0);
		memcpy(&(DecodeBuffer[0]),&(mData[0]),decode_len);
		U8* decodep = &(DecodeBuffer[0]);
		mFlags = DecodeBuffer[0];
		gMessageSystem->zeroCodeExpand(&decodep, &decode_len);
		if(decode_len < 7)
			decode_invalid = TRUE;
		else
		{
			mSequenceID = ntohl(*((U32*)(&decodep[1])));
			sTemplateMessageReader->clearMessage();
			if(!sTemplateMessageReader->validateMessage(decodep, decode_len, mFromHost, TRUE))
				decode_invalid = TRUE;
			else
			{
				if(!sTemplateMessageReader->decodeData(decodep, mFromHost, TRUE))
					decode_invalid = TRUE;
				else
				{
					LLMessageTemplate* temp = sTemplateMessageReader->getTemplate();
					mName = temp->mName;
					mSummary = "";

					if(mFlags)
					{
						mSummary.append(" [ ");
						if(mFlags & LL_ZERO_CODE_FLAG)
							mSummary.append(" Zer ");
						if(mFlags & LL_RELIABLE_FLAG)
							mSummary.append(" Rel ");
						if(mFlags & LL_RESENT_FLAG)
							mSummary.append(" Rsd ");
						if(mFlags & LL_ACK_FLAG)
							mSummary.append(" Ack ");
						mSummary.append(" ] ");
					}

					LLMessageTemplate::message_block_map_t::iterator blocks_end = temp->mMemberBlocks.end();
					for (LLMessageTemplate::message_block_map_t::iterator blocks_iter = temp->mMemberBlocks.begin();
						 blocks_iter != blocks_end; ++blocks_iter)
					{
						LLMessageBlock* block = (*blocks_iter);
						const char* block_name = block->mName;
						S32 num_blocks = sTemplateMessageReader->getNumberOfBlocks(block_name);
						if(!num_blocks)
							mSummary.append(" { } ");
						else if(num_blocks > 1)
							mSummary.append(llformat(" %s [ %d ] { ... } ", block_name, num_blocks));
						else for(S32 i = 0; i < 1; i++)
						{
							mSummary.append(" { ");
							LLMessageBlock::message_variable_map_t::iterator var_end = block->mMemberVariables.end();
							for (LLMessageBlock::message_variable_map_t::iterator var_iter = block->mMemberVariables.begin();
								 var_iter != var_end; ++var_iter)
							{
								LLMessageVariable* variable = (*var_iter);
								const char* var_name = variable->getName();
								BOOL returned_hex;
								std::string value = getString(sTemplateMessageReader, block_name, i, var_name, variable->getType(), returned_hex, TRUE);
								mSummary.append(llformat(" %s=%s ", var_name, value.c_str()));
							}
							mSummary.append(" } ");
							if(mSummary.length() > 255) break;
						}
						if(mSummary.length() > 255)
						{
							mSummary.append(" ... ");
							break;
						}
					} // blocks_iter
				} // decode_valid
			}
		}
		if(decode_invalid)
		{
			mName = "Invalid";
			mSummary = "";
			for(S32 i = 0; i < mDataSize; i++)
				mSummary.append(llformat("%02X ", mData[i]));
		}
	}
	else // not template
	{
		mName = "SOMETHING ELSE";
		mSummary = "TODO: SOMETHING ELSE";
	}
}
LLPrettyDecodedMessage::~LLPrettyDecodedMessage()
{
}
std::string LLPrettyDecodedMessage::getFull(BOOL show_header)
{
	std::string full("");
	if(mType == TEMPLATE)
	{
		BOOL decode_invalid = FALSE;
		S32 decode_len = mDataSize;
		std::vector<U8> DecodeBuffer(MAX_PACKET_LEN,0);
		memcpy(&(DecodeBuffer[0]),&(mData[0]),decode_len);
		U8* decodep = &(DecodeBuffer[0]);
		gMessageSystem->zeroCodeExpand(&decodep, &decode_len);
		if(decode_len < 7)
			decode_invalid = TRUE;
		else
		{
			sTemplateMessageReader->clearMessage();
			if(!sTemplateMessageReader->validateMessage(decodep, decode_len, mFromHost, TRUE))
				decode_invalid = TRUE;
			else
			{
				if(!sTemplateMessageReader->decodeData(decodep, mFromHost, TRUE))
					decode_invalid = TRUE;
				else
				{
					LLMessageTemplate* temp = sTemplateMessageReader->getTemplate();
					full.append(isOutgoing() ? "out " : "in ");
					full.append(llformat("%s\n", temp->mName));
					if(show_header)
					{
						full.append("[Header]\n");
						full.append(llformat("SequenceID = %u\n", mSequenceID));
						full.append(llformat("LL_ZERO_CODE_FLAG = %s\n", (mFlags & LL_ZERO_CODE_FLAG) ? "True" : "False"));
						full.append(llformat("LL_RELIABLE_FLAG = %s\n", (mFlags & LL_RELIABLE_FLAG) ? "True" : "False"));
						full.append(llformat("LL_RESENT_FLAG = %s\n", (mFlags & LL_RESENT_FLAG) ? "True" : "False"));
						full.append(llformat("LL_ACK_FLAG = %s\n", (mFlags & LL_ACK_FLAG) ? "True" : "False"));
					}
					LLMessageTemplate::message_block_map_t::iterator blocks_end = temp->mMemberBlocks.end();
					for (LLMessageTemplate::message_block_map_t::iterator blocks_iter = temp->mMemberBlocks.begin();
						 blocks_iter != blocks_end; ++blocks_iter)
					{
						LLMessageBlock* block = (*blocks_iter);
						const char* block_name = block->mName;
						S32 num_blocks = sTemplateMessageReader->getNumberOfBlocks(block_name);
						for(S32 i = 0; i < num_blocks; i++)
						{
							full.append(llformat("[%s]\n", block->mName));
							LLMessageBlock::message_variable_map_t::iterator var_end = block->mMemberVariables.end();
							for (LLMessageBlock::message_variable_map_t::iterator var_iter = block->mMemberVariables.begin();
								 var_iter != var_end; ++var_iter)
							{
								LLMessageVariable* variable = (*var_iter);
								const char* var_name = variable->getName();
								BOOL returned_hex;
								std::string value = getString(sTemplateMessageReader, block_name, i, var_name, variable->getType(), returned_hex);
								if(returned_hex)
									full.append(llformat("%s =| ", var_name));
								else
									full.append(llformat("%s = ", var_name));
								// llformat has a 1024 char limit!?
								full.append(value);
								full.append("\n");
							}
						}
					} // blocks_iter
				} // decode_valid
			}
		}
		if(decode_invalid)
		{
			full = isOutgoing() ? "out" : "in";
			full.append("\n");
			for(S32 i = 0; i < mDataSize; i++)
				full.append(llformat("%02X ", mData[i]));
		}
	}
	else // not template
	{
		full = "FIXME";
	}
	return full;
}
// static
std::string LLPrettyDecodedMessage::getString(LLTemplateMessageReader* readerp, const char* block_name, S32 block_num, const char* var_name, e_message_variable_type var_type, BOOL &returned_hex, BOOL summary_mode)
{
	returned_hex = FALSE;
	std::stringstream stream;
	char* value;
	U32 valueU32;
	U16 valueU16;
	LLVector3 valueVector3;
	LLVector3d valueVector3d;
	LLVector4 valueVector4;
	LLQuaternion valueQuaternion;
	LLUUID valueLLUUID;
	switch(var_type)
	{
	case MVT_U8:
		U8 valueU8;
		readerp->getU8(block_name, var_name, valueU8, block_num);
		stream << U32(valueU8);
		break;
	case MVT_U16:
		readerp->getU16(block_name, var_name, valueU16, block_num);
		stream << valueU16;
		break;
	case MVT_U32:
		readerp->getU32(block_name, var_name, valueU32, block_num);
		stream << valueU32;
		break;
	case MVT_U64:
		U64 valueU64;
		readerp->getU64(block_name, var_name, valueU64, block_num);
		stream << valueU64;
		break;
	case MVT_S8:
		S8 valueS8;
		readerp->getS8(block_name, var_name, valueS8, block_num);
		stream << S32(valueS8);
		break;
	case MVT_S16:
		S16 valueS16;
		readerp->getS16(block_name, var_name, valueS16, block_num);
		stream << valueS16;
		break;
	case MVT_S32:
		S32 valueS32;
		readerp->getS32(block_name, var_name, valueS32, block_num);
		stream << valueS32;
		break;
	/*case MVT_S64:
		S64 valueS64;
		readerp->getS64(block_name, var_name, valueS64, block_num);
		stream << valueS64;
		break;*/
	case MVT_F32:
		F32 valueF32;
		readerp->getF32(block_name, var_name, valueF32, block_num);
		stream << valueF32;
		break;
	case MVT_F64:
		F64 valueF64;
		readerp->getF64(block_name, var_name, valueF64, block_num);
		stream << valueF64;
		break;
	case MVT_LLVector3:
		readerp->getVector3(block_name, var_name, valueVector3, block_num);
		//stream << valueVector3;
		stream << "<" << valueVector3.mV[0] << ", " << valueVector3.mV[1] << ", " << valueVector3.mV[2] << ">";
		break;
	case MVT_LLVector3d:
		readerp->getVector3d(block_name, var_name, valueVector3d, block_num);
		//stream << valueVector3d;
		stream << "<" << valueVector3d.mdV[0] << ", " << valueVector3d.mdV[1] << ", " << valueVector3d.mdV[2] << ">";
		break;
	case MVT_LLVector4:
		readerp->getVector4(block_name, var_name, valueVector4, block_num);
		//stream << valueVector4;
		stream << "<" << valueVector4.mV[0] << ", " << valueVector4.mV[1] << ", " << valueVector4.mV[2] << ", " << valueVector4.mV[3] << ">";
		break;
	case MVT_LLQuaternion:
		readerp->getQuat(block_name, var_name, valueQuaternion, block_num);
		//stream << valueQuaternion;
		stream << "<" << valueQuaternion.mQ[0] << ", " << valueQuaternion.mQ[1] << ", " << valueQuaternion.mQ[2] << ", " << valueQuaternion.mQ[3] << ">";
		break;
	case MVT_LLUUID:
		readerp->getUUID(block_name, var_name, valueLLUUID, block_num);
		stream << valueLLUUID;
		break;
	case MVT_BOOL:
		BOOL valueBOOL;
		readerp->getBOOL(block_name, var_name, valueBOOL, block_num);
		stream << valueBOOL;
		break;
	case MVT_IP_ADDR:
		readerp->getIPAddr(block_name, var_name, valueU32, block_num);
		stream << LLHost(valueU32, 0).getIPString();
		break;
	case MVT_IP_PORT:
		readerp->getIPPort(block_name, var_name, valueU16, block_num);
		stream << valueU16;
	case MVT_VARIABLE:
	case MVT_FIXED:
	default:
		S32 size = readerp->getSize(block_name, block_num, var_name);
		if(size)
		{
			value = new char[size + 1];
			readerp->getBinaryData(block_name, var_name, value, size, block_num);
			value[size] = '\0';
			S32 readable = 0;
			S32 unreadable = 0;
			S32 end = (summary_mode && (size > 64)) ? 64 : size;
			for(S32 i = 0; i < end; i++)
			{
				if(!value[i])
				{
					if(i != (end - 1))
					{ // don't want null terminator hiding data
						unreadable = S32_MAX;
						break;
					}
				}
				else if(value[i] < 0x20 || value[i] >= 0x7F)
				{
					if(summary_mode)
						unreadable++;
					else
					{ // never want any wrong characters outside of summary mode
						unreadable = S32_MAX;
						break;
					}
				}
				else readable++;
			}
			if(readable >= unreadable)
			{
				if(summary_mode && (size > 64))
				{
					for(S32 i = 60; i < 63; i++)
						value[i] = '.';
					value[63] = '\0';
				}
				stream << value;

				delete[] value;
			}
			else
			{
				returned_hex = TRUE;
				S32 end = (summary_mode && (size > 8)) ? 8 : size;
				for(S32 i = 0; i < end; i++)
					//stream << std::uppercase << std::hex << U32(value[i]) << " ";
					stream << llformat("%02X ", (U8)value[i]);
				if(summary_mode && (size > 8))
					stream << " ... ";
			}
		}
		break;
	}

	return stream.str();
}

// </edit>
