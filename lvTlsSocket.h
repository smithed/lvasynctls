#pragma once

#include "lvTlsEngine.h"
#include "lvTlsCallbackBase.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>


namespace lvasynctls {
	class lvTlsCallback;
	class lvTlsDataReadCallback;
	class lvTlsCompletionCallback;
	class lvTlsNewConnectionCallback;
	
	class lvAsyncEngine;

	class lvTlsSocketBase {
	public:
		lvTlsSocketBase(lvAsyncEngine* engineContext, boost::asio::ssl::context& sslContext);
		~lvTlsSocketBase();
		void startHandshake(lvasynctls::lvTlsNewConnectionCallback * callback, boost::asio::ssl::stream_base::handshake_type handType);

		//writes are always direct to the stream
		void startWrite(std::vector<unsigned char> * data, lvasynctls::lvTlsDataReadCallback * callback);

		//can read directly from the ssl layer into a buffer
		void startDirectRead(size_t len, lvasynctls::lvTlsDataReadCallback* callback);
		void startDirectReadSome(size_t len, lvasynctls::lvTlsDataReadCallback* callback);
		void startDirectReadAtLeast(size_t min, size_t max, lvasynctls::lvTlsDataReadCallback* callback);

		//for stream reads, max size of a read is determined by LVSOCKETMAXREADSIZE (vs unlimited for direct reads)
		//return -1 error directly if len is too long or 0 if we already have the selected data length in the buffer
		//to repeat, these functions will not read anything new if the buffer already has data in it
		//this is because the stream reads are not guaranteed to read a specific size but instead read at least N
		//so if you for example read until next crlf there may ne N bytes left over in the buffer already
		//read until term requires a streambuffer implementation
		int32_t startStreamReadUntilTermChar(unsigned char term, size_t max, lvasynctls::lvTlsCompletionCallback* callback);
		int32_t startStreamReadUntilTermString(std::string term, size_t max, lvasynctls::lvTlsCompletionCallback* callback);
		//to add matching implementation for other functions
		int32_t startStreamRead(size_t len, lvasynctls::lvTlsCompletionCallback* callback);
		int32_t startStreamReadAtLeast(size_t min, size_t max, lvasynctls::lvTlsCompletionCallback* callback);
		//get size of underlying buffer in terms of bytes available
		size_t getInputStreamSize();
		//read some avoids errors that might be cause by trying to read past the buffer
		int64_t inputStreamReadSome(char * buffer, int64_t len);

		boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& getStream();

	protected:
		lvAsyncEngine* engineOwner;
		boost::asio::io_service::strand socketStrand;
		boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket;
		boost::asio::streambuf inputStreamBuffer;
		std::basic_istream<char> inputStream;

	private:
		lvTlsSocketBase() = delete;
		lvTlsSocketBase(const lvTlsSocketBase& that) = delete;
		const lvTlsSocketBase& operator=(const lvTlsSocketBase&) = delete;

		void CBWriteComplete(const boost::system::error_code& error, std::size_t bytes_transferred, std::vector<unsigned char> * data, lvasynctls::lvTlsDataReadCallback* callback);
		void CBReadComplete(const boost::system::error_code& error, std::size_t bytes_transferred, lvasynctls::lvTlsDataReadCallback* callback);
		void CBReadStreamComplete(const boost::system::error_code& error, std::size_t bytes_transferred, lvasynctls::lvTlsCompletionCallback* callback);
		void CBHandShakeComplete(const boost::system::error_code& error, lvasynctls::lvTlsNewConnectionCallback* callback);

	};
}