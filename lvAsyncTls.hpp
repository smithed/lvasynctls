#pragma once

#ifndef _lvasynctls_h
#define _lvasynctls_h


#include <unordered_set>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/thread.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

#ifndef LVSOCKETMAXREADSIZE
#define LVSOCKETMAXREADSIZE 102400
#endif


namespace lvasynctls {
	class lvAsyncEngine;
	class lvTlsSocketBase;
	class lvTlsConnectionCreator;
	class lvTlsClientConnector;
	class lvTlsServerAcceptor;
	class lvTlsCallback;
	class lvTlsNewConnectionCallback;
	class lvTlsDataReadCallback;
	class lvTlsCompletionCallback;


	inline void errorCheck(const boost::system::error_code & error, lvasynctls::lvTlsCallback* callback);

	//base context
	class lvAsyncEngine {
	public:
		lvAsyncEngine();
		lvAsyncEngine(std::size_t threadCount);
		~lvAsyncEngine();
		boost::shared_ptr<boost::asio::io_service> getIOService();

		void registerSocket(lvTlsSocketBase* tlsSock);
		std::size_t unregisterSocket(lvTlsSocketBase* tlsSock);

		void registerConnector(lvTlsConnectionCreator* connector);
		std::size_t unregisterConnector(lvTlsConnectionCreator* connector);

		void softShutdown();

	private:
		lvAsyncEngine(const lvAsyncEngine& that) = delete;
		const lvAsyncEngine& operator=(const lvAsyncEngine&) = delete;
		void initializeThreads(std::size_t threadCount);

		void ioRunThread(boost::shared_ptr<boost::asio::io_service> ioengine);

		boost::shared_ptr<boost::asio::io_service> ioEngine;
		boost::asio::io_service::work* engineWork;
		boost::thread_group threadSet;
		std::unordered_set<lvTlsSocketBase*> socketSet;
		std::unordered_set<lvTlsConnectionCreator*> connectorSet;
		std::mutex lock;
	};


	//parent for client/server socket to hold common fields
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


	//connection establishing parent
	class lvTlsConnectionCreator {
	public:
		lvTlsConnectionCreator(lvAsyncEngine* engineContext);
		virtual ~lvTlsConnectionCreator();
		void enablePasswordCallback(std::string pw);

		boost::asio::ssl::context ctx;
	protected:
		lvAsyncEngine* engineOwner;
		
	private: 
		lvTlsConnectionCreator() = delete;
		lvTlsConnectionCreator(const lvTlsConnectionCreator& that) = delete;
		const lvTlsConnectionCreator& operator=(const lvTlsConnectionCreator&) = delete;

		std::string getPass();
		std::string cachePW;
	};



	//connects a single stream
	class lvTlsClientConnector : public lvTlsConnectionCreator {
	public:
		lvTlsClientConnector(lvAsyncEngine* engineContext);
		~lvTlsClientConnector() override;

		void resolveAndConnect(std::string host, std::string port, lvasynctls::lvTlsNewConnectionCallback * callback);

	private:
		lvTlsClientConnector() = delete;
		lvTlsClientConnector(const lvTlsClientConnector& that) = delete;
		const lvTlsClientConnector& operator=(const lvTlsClientConnector&) = delete;

		void CBResolveToConnect(lvTlsSocketBase* newConnection, const boost::system::error_code& error, boost::asio::ip::tcp::resolver::iterator iterator, lvasynctls::lvTlsNewConnectionCallback* callback);
		void CBConnectionEstablished(lvTlsSocketBase* newConnection, const boost::system::error_code& error, boost::asio::ip::tcp::resolver::iterator iterator, lvasynctls::lvTlsNewConnectionCallback* callback);

		boost::asio::ip::tcp::resolver resolver;
	};

	//manages a server acceptor
	class lvTlsServerAcceptor : public lvTlsConnectionCreator {
	public:
		lvTlsServerAcceptor(lvAsyncEngine* engineContext, unsigned short port);
		~lvTlsServerAcceptor() override;

		void startAccept(lvasynctls::lvTlsNewConnectionCallback * callback);

	private:
		lvTlsServerAcceptor() = delete;
		lvTlsServerAcceptor(const lvTlsServerAcceptor& that) = delete;
		const lvTlsServerAcceptor& operator=(const lvTlsServerAcceptor&) = delete;

		void CBConnectionAccepted(lvTlsSocketBase* newConnection, const boost::system::error_code& error, lvasynctls::lvTlsNewConnectionCallback * callback);


		boost::asio::ip::tcp::acceptor serverAcceptor;
	};

	
	std::size_t isReadComplete(const boost::system::error_code& error, std::size_t bytes_transferred, std::size_t minBytesDesired);


	class lvTlsCallback{
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

}

#endif // !_l
//vasynctls_h_


