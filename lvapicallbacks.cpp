#include "lvapicallbacks.h"
#include "lvTlsSocket.h"
#include <extcode.h>

namespace lvasyncapi {
	//op complete callback
	LVCompletionNotificationCallback::LVCompletionNotificationCallback(LVUserEventRef * lvevent, uint32_t requestID) : 
		lvasynctls::lvTlsCallback{}, id{ requestID }, e{ lvevent }, errorCode{ 0 }, errorMessage{ "" }
	{
	}

	LVCompletionNotificationCallback::~LVCompletionNotificationCallback()
	{
	}

	void LVCompletionNotificationCallback::execute()
	{
		if (e && *e != kNotAMagicCookie) {
			lvCompleteEventType eventData;
			eventData.requestID = id;
			eventData.errorCode = errorCode;
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

			DSDisposeHandle(eventData.errorData);
		}
	}

	void LVCompletionNotificationCallback::setErrorCondition(int code, std::string message)
	{
		if (code) {
			errorCode = code;
			errorMessage = message;
		}
	}
}
