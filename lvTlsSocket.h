#pragma once

#include "lvTlsEngine.h"
#include "lvTlsCallbackBase.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <istream>
#include <queue>
#include <atomic>
#include <mutex>

#ifndef LVSOCKETMAXREADSIZE
#define LVSOCKETMAXREADSIZE 5242880 //5 MB
#endif

#ifndef LVSOCKETMAXOUTPUTCHUNKS
#define LVSOCKETMAXOUTPUTCHUNKS 5000
#endif

#define LVTLSSOCKETERRFLAGABORT 1000


namespace lvasynctls {
	class lvTlsCallback;
	class lvAsyncEngine;


	class lvTlsSocketBase {
	public:

		explicit lvTlsSocketBase(std::shared_ptr<lvAsyncEngine> engineContext, boost::asio::ssl::context& sslContext, size_t streamSize = LVSOCKETMAXREADSIZE, size_t outputQueueSize = LVSOCKETMAXOUTPUTCHUNKS);
		void shutdown();
		~lvTlsSocketBase();

		//writes are always direct to the stream
		int startWrite(std::vector<unsigned char> * data, lvasynctls::lvTlsCallback * callback);

		//for stream reads, max size of a read is determined by LVSOCKETMAXREADSIZE
		//return -1 error directly if len is too long or 0 if we already have the selected data length in the buffer
		int startStreamReadUntilTermChar(unsigned char term, size_t max, lvasynctls::lvTlsCallback* callback);
		int startStreamReadUntilTermString(std::string term, size_t max, lvasynctls::lvTlsCallback* callback);
		int startStreamRead(size_t len, lvasynctls::lvTlsCallback* callback);


		//input stream
		//get size of underlying buffer in terms of bytes available
		int64_t getInputStreamSize();
		//read some avoids errors that might be caused by trying to read past the buffer
		int64_t inputStreamReadSome(char * buffer, int64_t len);
		int64_t inputStreamReadN(char * buffer, int64_t len);
		//returns as a cstr, so last char is null
		int64_t inputStreamGetLine(char * buffer, int64_t len, char delimiter = '\n');

		boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& getStream();
		int getError();

	protected:


	private:
		lvTlsSocketBase() = delete;
		lvTlsSocketBase(const lvTlsSocketBase& that) = delete;
		const lvTlsSocketBase& operator=(const lvTlsSocketBase&) = delete;
				
		std::shared_ptr<lvAsyncEngine> engineOwner;
		boost::asio::io_service::strand socketStrand;
		boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket;
		
		//indicates if the socket is dead, will usually be read inside of iSLock and iQlock but is atomic for the write side.
		std::atomic_int socketErr;

		//per asio docs, multiple outstanding writes or reads are not permitted, we must serialize them
		//strand does not seem to help with this, because it actually calls 'read some' under the hood
		typedef std::function<void(const boost::system::error_code & error, std::size_t bytes_transferred)> readOperationFinalize;
		typedef std::function<void(readOperationFinalize finalizer)> readOperationCallback;
		struct inputChunk {
			readOperationCallback op;
			lvasynctls::lvTlsCallback* optcallback = nullptr;
		};

		//common code for all reads
		int startReadCore(inputChunk inChunk);
		//just a wrapper for internal function on self
		void readAction();

		std::mutex iSLock;
		//begin--locked by iSLock
			bool inputRunning;
			std::queue<inputChunk> inQueue;
		//end--locked by iSLock
		boost::asio::streambuf inputStreamBuffer;
		std::basic_istream<char> inputStream;
		
		struct outputChunk {
			std::vector<unsigned char> * chunkdata = nullptr;
			lvasynctls::lvTlsCallback* optcallback = nullptr;
		};

		//just a wrapper for internal function on self
		void writeAction();
		void writeCB(const boost::system::error_code & error, std::size_t bytes_transferred);

		std::mutex oQLock;
		//begin--locked by oQLock
			bool outputRunning;
			std::queue<outputChunk> outQueue;
		//end--locked by oQLock
		size_t oQMaxSize;
	};

}