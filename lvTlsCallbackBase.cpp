#include "lvTlsCallbackBase.h"
#include <vector>



namespace lvasynctls {

	void errorCheck(const boost::system::error_code & error, lvasynctls::lvTlsCallback* callback) {
		if (callback && error) {
			callback->setErrorCondition(error.value(), error.message());
		}
	}


	//completion callbacks
	void lvTlsCallback::execute()
	{
		//virtual
	}

	lvTlsCallback::~lvTlsCallback()
	{
	}

	void lvTlsCallback::setErrorCondition(int code, std::string message)
	{
	}
}