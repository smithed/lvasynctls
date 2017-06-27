#include "lvTlsCallbackBase.h"
#include <vector>



namespace lvasynctls {

	void errorCheck(const boost::system::error_code & error, lvasynctls::lvTlsCallback* callback) {
		if (callback) {
			if (error) {
				callback->setErrorCondition(error);
			}
		}
	}


	//completion callbacks
	void lvTlsCallback::execute()
	{
		//virtual
	}

	void lvTlsCallback::setErrorCondition(const boost::system::error_code& error)
	{
	}


	void lvTlsNewConnectionCallback::cacheSocket(lvTlsSocketBase* socket)
	{
		s = socket;
	}

	lvTlsNewConnectionCallback::lvTlsNewConnectionCallback()
	{
		s = nullptr;
	}

	lvTlsNewConnectionCallback::~lvTlsNewConnectionCallback()
	{
		//do not destroy s, we don't own it
	}




	//callback for returning data to lv
	void lvTlsDataReadCallback::giveDataResult(std::vector<unsigned char> * data)
	{
		if (mem) {
			delete mem;
			mem = nullptr;
		}
		mem = data;
	}

	void lvTlsDataReadCallback::resizeDataResult(size_t len)
	{
		if (mem) {
			mem->resize(len);
		}
	}

	lvTlsDataReadCallback::lvTlsDataReadCallback() : mem{ nullptr }
	{
	}

	lvTlsDataReadCallback::~lvTlsDataReadCallback()
	{
		if (mem) {
			delete mem;
			mem = nullptr;
		}
	}

	//callback for indicating data has been read
	void lvTlsCompletionCallback::setDataSize(size_t len)
	{
		dataLength = len;
	}

	lvTlsCompletionCallback::lvTlsCompletionCallback() : dataLength{ 0 }
	{
	}

	lvTlsCompletionCallback::~lvTlsCompletionCallback()
	{
	}
}