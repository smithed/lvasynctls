#pragma once

#include "lvTlsEngine.h"
#include "lvTlsCallbackBase.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <istream>
#include <ostream>

#ifndef LVSOCKETMAXREADSIZE
#define LVSOCKETMAXREADSIZE 102400
#endif


namespace lvasynctls {
	class lvTlsCallback;
	class lvAsyncEngine;


	class lvTlsSocketBase {
	public:
		explicit lvTlsSocketBase(lvAsyncEngine* engineContext, boost::asio::ssl::context& sslContext, size_t streamSize = LVSOCKETMAXREADSIZE);
		~lvTlsSocketBase();

		//writes are always direct to the stream
		int64_t startWrite(char * buffer, int64_t len, lvasynctls::lvTlsCallback * callback);
		//synchronous write, don't mix with async
		size_t lvTlsSocketBase::Write(std::vector<unsigned char> & data, boost::system::error_code & err);

		//synchronous reads, don't mix with async
		size_t lvTlsSocketBase::Read(std::vector<unsigned char> & data, boost::system::error_code & err);
		size_t lvTlsSocketBase::ReadUntil(const std::string & term, boost::system::error_code & err);

		//for stream reads, max size of a read is determined by LVSOCKETMAXREADSIZE
		//return -1 error directly if len is too long or 0 if we already have the selected data length in the buffer
		void startStreamReadUntilTermChar(unsigned char term, size_t max, lvasynctls::lvTlsCallback* callback);
		void startStreamReadUntilTermString(std::string term, size_t max, lvasynctls::lvTlsCallback* callback);
		void startStreamRead(size_t len, lvasynctls::lvTlsCallback* callback);


		//input stream
		//get size of underlying buffer in terms of bytes available
		int64_t getInputStreamSize();
		//read some avoids errors that might be cause by trying to read past the buffer
		int64_t inputStreamReadSome(char * buffer, int64_t len);
		int64_t inputStreamReadN(char * buffer, int64_t len);
		//returns as a cstr, so last char is null
		int64_t inputStreamGetLine(char * buffer, int64_t len, char delimiter = '\n');

		boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& getStream();

	protected:
		lvAsyncEngine* engineOwner;
		boost::asio::io_service::strand socketStrand;
		boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket;
		boost::asio::streambuf inputStreamBuffer;
		boost::asio::streambuf outputStreamBuffer;
		std::basic_istream<char> inputStream;
		std::basic_ostream<char> outputStream;

	private:
		lvTlsSocketBase() = delete;
		lvTlsSocketBase(const lvTlsSocketBase& that) = delete;
		const lvTlsSocketBase& operator=(const lvTlsSocketBase&) = delete;

		void CBWriteComplete(const boost::system::error_code& error, std::size_t bytes_transferred, lvasynctls::lvTlsCallback* callback);
		void CBReadStreamComplete(const boost::system::error_code& error, std::size_t bytes_transferred, lvasynctls::lvTlsCallback* callback);
	};
}