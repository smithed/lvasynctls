#include "lvapicallbacks.h"
#include <extcode.h>

namespace lvasyncapi {

	//direct operation complete callback
	lvasyncapi::LVNotificationCallback::LVNotificationCallback(LVUserEventRef * lvevent, lvasyncapi::lvRequestType requestType, uint32_t requestID) : lvasynctls::lvTlsDataReadCallback{}
	{
		id = requestID;
		reqtype = requestType;
		e = lvevent;
		errorCode = 0;
		errorMessage = "";
	}

	lvasyncapi::LVNotificationCallback::~LVNotificationCallback()
	{
	}

	//posts an event associated with cached values (inc. error and data)
	void lvasyncapi::LVNotificationCallback::execute()
	{
		lvDataEventType eventData;
		eventData.requestID = id;
		eventData.requestType = (uint8_t)reqtype;
		eventData.errorCode = errorCode;
		if (errorCode == 0) {
			//no error
			if (mem) {
				if (mem->size() > 0) {
					auto truesize = (mem->size()) * (sizeof((*mem)[0]));
					auto vecstart = &((*mem)[0]);
					eventData.data = (LStrHandle)DSNewHandle(truesize + sizeof(int32_t));
					LStrLen(LHStrPtr(eventData.data)) = truesize;
					MoveBlock(vecstart, LHStrBuf(eventData.data), truesize);
				}
				else {
					eventData.data = (LStrHandle)DSNewHClr(sizeof(int32_t)); //also sets string size to 0
				}
			}
			else {
				//no message
				eventData.data = (LStrHandle)DSNewHClr(sizeof(int32_t)); //also sets string size to 0
			}
		}
		else {
			//error condition
			eventData.data = (LStrHandle)DSNewHandle(errorMessage.size() + sizeof(int32_t));
			LStrLen(LHStrPtr(eventData.data)) = errorMessage.size();
			MoveBlock(errorMessage.data(), LHStrBuf(eventData.data), errorMessage.size());
		}
		PostLVUserEvent(*e, (void *)(&eventData));
	}

	void lvasyncapi::LVNotificationCallback::setErrorCondition(const boost::system::error_code & error)
	{
		if (error) {
			auto ec = error.value();
			errorCode = ec;
			errorMessage = error.message();
		}
	}


	//new conn callback
	LVConnectionNotificationCallback::LVConnectionNotificationCallback(LVUserEventRef * lvevent, lvasyncapi::lvRequestType requestType, uint32_t requestID) : lvasynctls::lvTlsNewConnectionCallback{}
	{
		id = requestID;
		reqtype = requestType;
		e = lvevent;
		errorCode = 0;
		errorMessage = "";
	}

	LVConnectionNotificationCallback::~LVConnectionNotificationCallback()
	{
	}

	void LVConnectionNotificationCallback::execute()
	{
		lvNewConnectionType eventData;
		eventData.requestID = id;
		eventData.requestType = (uint8_t)reqtype;
		eventData.errorCode = errorCode;
#ifdef LVASYNCENV32
		eventData.padding = 0xABADCAFE;
		eventData.padding = 0x32323232;
#endif
		if (errorCode == 0) {
			//no error
			eventData.connection = s;
			//no message
			eventData.errorString = (LStrHandle)DSNewHClr(sizeof(int32_t)); //also sets string size to 0
		}
		else {
			//error condition
			eventData.errorString = (LStrHandle)DSNewHandle(errorMessage.size() + sizeof(int32_t));
			LStrLen(LHStrPtr(eventData.errorString)) = errorMessage.size();
			MoveBlock(errorMessage.data(), LHStrBuf(eventData.errorString), errorMessage.size());
		}
		auto err = PostLVUserEvent(*e, (void *)(&eventData));
		if (err != mgNoErr) {
			delete s;
			s = nullptr;
		}
	}

	void LVConnectionNotificationCallback::setErrorCondition(const boost::system::error_code & error)
	{
		if (error) {
			auto ec = error.value();
			errorCode = ec;
			errorMessage = error.message();
		}
	}


	//op complete callback
	LVCompletionNotificationCallback::LVCompletionNotificationCallback(LVUserEventRef * lvevent, lvasyncapi::lvRequestType requestType, uint32_t requestID) : lvasynctls::lvTlsCompletionCallback{}
	{
		id = requestID;
		reqtype = requestType;
		e = lvevent;
		errorCode = 0;
		errorMessage = "";
	}

	LVCompletionNotificationCallback::~LVCompletionNotificationCallback()
	{
	}

	void LVCompletionNotificationCallback::execute()
	{
		lvCompleteEventType eventData;
		eventData.requestID = id;
		eventData.requestType = (uint8_t)reqtype;
		eventData.errorCode = errorCode;
		eventData.dataLen = dataLength;
		if (errorCode != 0) {
			//error condition
			eventData.errorData = (LStrHandle)DSNewHandle(errorMessage.size() + sizeof(int32_t));
			LStrLen(LHStrPtr(eventData.errorData)) = errorMessage.size();
			MoveBlock(errorMessage.data(), LHStrBuf(eventData.errorData), errorMessage.size());
		}
		else {
			eventData.errorData = (LStrHandle)DSNewHClr(sizeof(int32_t)); //size 0
		}

		auto err = PostLVUserEvent(*e, (void *)(&eventData));
	}

	void LVCompletionNotificationCallback::setErrorCondition(const boost::system::error_code & error)
	{
		if (error) {
			auto ec = error.value();
			errorCode = ec;
			errorMessage = error.message();
		}
	}
}
