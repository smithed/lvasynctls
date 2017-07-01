#pragma once

#include "lvTlsSocket.h"
#include <vector>
#include <boost/system/error_code.hpp>

namespace lvasynctls {
	class lvTlsSocketBase;

	class lvTlsCallback {
	public:
		virtual void execute();
		virtual ~lvTlsCallback();
		//copy error information into class
		virtual void setErrorCondition(int code, std::string message);
	};

	void errorCheck(const boost::system::error_code & error, lvasynctls::lvTlsCallback* callback);
}
