#pragma once

#include "lvTlsSocket.h"
#include "lvTlsCallbackBase.h"
#include <extcode.h>
#include <boost/system/error_code.hpp>

//https://stackoverflow.com/questions/1505582/determining-32-vs-64-bit-in-c
#include <cstdint>
#if INTPTR_MAX == INT32_MAX
#define LVASYNCENV32
#elif INTPTR_MAX == INT64_MAX
#define LVASYNCENV64
#else
#error "Environment not 32 or 64-bit."
#endif

namespace lvasynctls {
	class lvTlsCallback;
}


namespace lvasyncapi {

	#include <lv_prolog.h>

		typedef struct lvCompleteEventType {
			uInt32 requestID;
			int32 errorCode;
			LStrHandle errorData;
		} lvCompleteEventType;

	#include <lv_epilog.h>

	class LVCompletionNotificationCallback : public lvasynctls::lvTlsCallback {
	public:
		LVCompletionNotificationCallback(LVUserEventRef * lvevent, uint32_t requestID);
		~LVCompletionNotificationCallback() override;
		void execute() override;
		//copy error information into class
		void setErrorCondition(int code, std::string message) override;

	private:
		LVUserEventRef * e;
		uint32_t id;
		int32_t errorCode;
		std::string errorMessage;
	};


}


