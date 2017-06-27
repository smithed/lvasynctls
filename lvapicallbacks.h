#pragma once

#include <extcode.h>
#include <boost/system/error_code.hpp>
#include "lvTlsSocket.h"
#include "lvTlsCallbackBase.h"

namespace lvasynctls {
	class lvTlsCallback;
	class lvTlsDataReadCallback;
	class lvTlsCompletionCallback;
	class lvTlsNewConnectionCallback;
}


namespace lvasyncapi {

	#include <lv_prolog.h>
		typedef struct lvDataEventType {
			uInt8 requestType;
			uInt32 requestID;
			int32 errorCode;
			LStrHandle data;
		} lvDataEventType;

		typedef struct lvCompleteEventType {
			uInt8 requestType;
			uInt32 requestID;
			int32 errorCode;
			LStrHandle errorData;
			uInt32 dataLen;
		} lvCompleteEventType;

		typedef struct lvNewConnectionType {
			uInt32 requestType;
			uInt32 requestID;
			int32 errorCode;
			LStrHandle errorString;
			lvasynctls::lvTlsSocketBase* connection;
	#ifdef LVASYNCENV32
			uInt32 padding;
	#endif
		} lvNewConnectionType;
	#include <lv_epilog.h>


	typedef enum lvRequestType { ClientConnect, StreamRead, StreamWrite, AcceptConnection } lvRequestType;

	class LVNotificationCallback : public lvasynctls::lvTlsDataReadCallback {
	public:
		LVNotificationCallback(LVUserEventRef * lvevent, lvasyncapi::lvRequestType requestType, uint32_t requestID);
		~LVNotificationCallback() override;
		void execute() override;
		//copy error information into class
		void setErrorCondition(const boost::system::error_code& error) override;

	private:
		LVUserEventRef * e;
		lvasyncapi::lvRequestType reqtype;
		uint32_t id;
		int32_t errorCode;
		std::string errorMessage;
	};

	class LVCompletionNotificationCallback : public lvasynctls::lvTlsCompletionCallback {
	public:
		LVCompletionNotificationCallback(LVUserEventRef * lvevent, lvasyncapi::lvRequestType requestType, uint32_t requestID);
		~LVCompletionNotificationCallback() override;
		void execute() override;
		//copy error information into class
		void setErrorCondition(const boost::system::error_code& error) override;

	private:
		LVUserEventRef * e;
		lvasyncapi::lvRequestType reqtype;
		uint32_t id;
		int32_t errorCode;
		std::string errorMessage;
	};


	class LVConnectionNotificationCallback : public lvasynctls::lvTlsNewConnectionCallback {
	public:
		LVConnectionNotificationCallback(LVUserEventRef * lvevent, lvasyncapi::lvRequestType requestType, uint32_t requestID);
		~LVConnectionNotificationCallback() override;
		void execute() override;
		//copy error information into class
		void setErrorCondition(const boost::system::error_code& error) override;

	private:
		LVUserEventRef * e;
		lvasyncapi::lvRequestType reqtype;
		uint32_t id;
		int32_t errorCode;
		std::string errorMessage;
	};
}


