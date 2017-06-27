#pragma once

#include "lvTlsSocket.h"
#include <vector>
#include <boost/system/error_code.hpp>

namespace lvasynctls {
	class lvTlsSocketBase;

	class lvTlsCallback {
	public:
		virtual void execute();
		//copy error information into class
		virtual void setErrorCondition(const boost::system::error_code& error);
	};

	class lvTlsNewConnectionCallback : public lvasynctls::lvTlsCallback {
	public:
		virtual void cacheSocket(lvTlsSocketBase* socket);
		lvTlsNewConnectionCallback();
		virtual ~lvTlsNewConnectionCallback();
	protected:
		lvTlsSocketBase* s;
	};


	class lvTlsDataReadCallback : public lvasynctls::lvTlsCallback {
	public:

		//take ownership of data
		void giveDataResult(std::vector<unsigned char> * data);
		void resizeDataResult(size_t len);

		lvTlsDataReadCallback();
		virtual ~lvTlsDataReadCallback();

	protected:
		std::vector<unsigned char> * mem;
	private:
		const lvTlsDataReadCallback& operator=(const lvTlsDataReadCallback&) = delete;
	};

	class lvTlsCompletionCallback : public lvasynctls::lvTlsCallback {
	public:

		void setDataSize(size_t len);

		lvTlsCompletionCallback();
		virtual ~lvTlsCompletionCallback();

	protected:
		size_t dataLength;
	private:
		const lvTlsCompletionCallback& operator=(const lvTlsCompletionCallback&) = delete;
	};



	void errorCheck(const boost::system::error_code & error, lvasynctls::lvTlsCallback* callback);
}

