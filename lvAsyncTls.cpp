#include <stdio.h>
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <unordered_set>
#include <mutex>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lockfree/queue.hpp>
#include <openssl/evp.h>
#include "lvAsyncTls.hpp"

using namespace boost::asio;

namespace lvasynctls {

	//main engine
	lvAsyncEngine::lvAsyncEngine() : socketSet(), connectorSet(),
		ioEngine { new boost::asio::io_service(std::thread::hardware_concurrency()) },
		engineWork(new io_service::work(*ioEngine))
	{
		OpenSSL_add_all_algorithms();
		OpenSSL_add_all_ciphers();
		OpenSSL_add_all_digests();
		PKCS5_PBE_add();
		initializeThreads(std::thread::hardware_concurrency());
	}

	lvAsyncEngine::lvAsyncEngine(std::size_t threadCount) : socketSet(), connectorSet(),
		ioEngine{ new boost::asio::io_service(threadCount) },
		engineWork(new io_service::work(*ioEngine))
	{
		OpenSSL_add_all_algorithms();
		OpenSSL_add_all_ciphers();
		OpenSSL_add_all_digests();
		PKCS5_PBE_add();
		initializeThreads(threadCount);
	}

	lvAsyncEngine::~lvAsyncEngine()
	{
		try {
			if (engineWork) {
				delete engineWork;
				engineWork = nullptr;
			}

			ioEngine->stop();
		}
		catch (...) {

		}


		try {


//lock		
			auto l = lock.try_lock();
			if (!l) {
				while (!l) {
					boost::this_thread::sleep(boost::posix_time::milliseconds(1));
					l = lock.try_lock();
				}
			}

			std::unordered_set<lvTlsSocketBase*> sockSnapshot(socketSet);
			std::unordered_set<lvTlsConnectionCreator*> connSnapshot(connectorSet);

			lock.unlock();
//unlock

			for (std::unordered_set<lvTlsSocketBase*>::iterator itr = sockSnapshot.begin(); itr != sockSnapshot.end(); ++itr) {
				try {
					auto val = *itr;
					delete val;
					val = nullptr;
				}
				catch (...) {

				}
			}
		
			for (std::unordered_set<lvTlsConnectionCreator*>::iterator itr = connSnapshot.begin(); itr != connSnapshot.end(); ++itr) {
				try {
					auto val = *itr;
					delete val;
					val = nullptr;
				}
				catch (...) {

				}
			}
		}
		catch (...) {

		}

		try {
			threadSet.interrupt_all();
			threadSet.join_all();
		}
		catch (...)
		{

		}
		

		ioEngine.reset();
	}

	boost::shared_ptr<boost::asio::io_service> lvAsyncEngine::getIOService()
	{
		return ioEngine;
	}

	void lvAsyncEngine::registerSocket(lvTlsSocketBase * tlsSock)
	{
		if (tlsSock) {
			std::lock_guard<std::mutex> lockg(lock);
			socketSet.insert(tlsSock);
		}

		return;
	}

	std::size_t lvAsyncEngine::unregisterSocket(lvTlsSocketBase * tlsSock)
	{
		std::size_t count;
		if (tlsSock) {
			std::lock_guard<std::mutex> lockg(lock);
			count = socketSet.erase(tlsSock);
		}

		return count;
	}

	void lvAsyncEngine::registerConnector(lvTlsConnectionCreator * connector)
	{
		if (connector) {
			std::lock_guard<std::mutex> lockg(lock);
			connectorSet.insert(connector);
		}
		
		return;
	}

	std::size_t lvAsyncEngine::unregisterConnector(lvTlsConnectionCreator * connector)
	{
		std::size_t count;
		if (connector) {
			std::lock_guard<std::mutex> lockg(lock);
			count = connectorSet.erase(connector);
		}

		return count;
	}

	void lvAsyncEngine::softShutdown()
	{
		if (engineWork) {
			delete engineWork;
			engineWork = nullptr;
		}
	}

	//adds workers to the thread pool
	void lvAsyncEngine::initializeThreads(std::size_t threadCount)
	{
		for (size_t i = 0; i < threadCount; i++)
		{
			threadSet.add_thread(new boost::thread{ boost::bind(&lvAsyncEngine::ioRunThread, this, ioEngine) });
		}
	}

	//performs the actual work of running the IO engine
	void lvAsyncEngine::ioRunThread(boost::shared_ptr<boost::asio::io_service> ioengine)
	{
		do {
			if(ioengine) {
				try
				{
					ioengine->run();
				}
				catch (const std::exception&)
				{
					//ignore exceptions
					if (ioengine && !(ioengine->stopped() || boost::this_thread::interruption_enabled())) {
						//if we get into an exception loop lets at least not rail the CPU
						boost::this_thread::sleep(boost::posix_time::milliseconds(50));
					}
				}
			}
			else {
				break;
			}
		} while (ioengine && !(ioengine->stopped() || boost::this_thread::interruption_enabled()));
	}




//parent
	lvTlsSocketBase::lvTlsSocketBase(lvAsyncEngine* engineContext, boost::asio::ssl::context& sslContext) :
		engineOwner(engineContext), 
		socketStrand(*(engineContext->getIOService())),  
		socket(*(engineContext->getIOService()), sslContext), 
		inputStreamBuffer { LVSOCKETMAXREADSIZE }, 
		inputStream { &inputStreamBuffer }
	{
		if (engineContext) {
			engineContext->registerSocket(this);
		}
	}

	lvTlsSocketBase::~lvTlsSocketBase()
	{
		try {
			socket.shutdown();
		}
		catch (...) {

		}
		try {
			socket.lowest_layer().cancel();
			socket.lowest_layer().close();
		}
		catch (...) {

		}
		try {
			if (engineOwner) {
				engineOwner->unregisterSocket(this);
			}
		}
		catch (...) {

		}
		return;
	}

	//take ownership of data buffer and of callback, we will dispose
	void lvTlsSocketBase::startWrite(std::vector<unsigned char> * data, lvasynctls::lvTlsDataReadCallback * callback)
	{
		auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBWriteComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, data, callback));
		async_write(socket, boost::asio::buffer(*data), func);
	}

	//take ownership of callback, we will dispose
	void lvTlsSocketBase::startDirectRead(size_t len, lvasynctls::lvTlsDataReadCallback* callback)
	{
		if (callback) {
			auto mem = new std::vector<unsigned char>(len);
			callback->giveDataResult(mem);
			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read(socket, boost::asio::buffer(*mem),transfer_all(), func);
		}
	}

	void lvTlsSocketBase::startDirectReadSome(size_t len, lvasynctls::lvTlsDataReadCallback * callback)
	{
		if (callback) {
			auto mem = new std::vector<unsigned char>(len);
			callback->giveDataResult(mem);
			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			socket.async_read_some(boost::asio::buffer(*mem), func);
		}
	}

	void lvTlsSocketBase::startDirectReadAtLeast(size_t min, size_t max, lvasynctls::lvTlsDataReadCallback * callback)
	{
		if (callback) {
			auto mem = new std::vector<unsigned char>(max);
			callback->giveDataResult(mem);
			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read(socket, boost::asio::buffer(*mem), transfer_at_least(min), func);
		}
	}


	int32_t lvTlsSocketBase::startStreamReadUntilTermChar(unsigned char term, size_t max, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			if (max > LVSOCKETMAXREADSIZE) {
				return -1;
			}
			if (max <= inputStreamBuffer.size()) {
				callback->setDataSize(max);
				callback->execute();
				return 0;
			}

			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read_until(socket, inputStreamBuffer, term, func);
		}
		return -2;
	}

	int32_t lvTlsSocketBase::startStreamReadUntilTermString(std::string term, size_t max, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			if (max > LVSOCKETMAXREADSIZE) {
				return -1;
			}
			if (max <= inputStreamBuffer.size()) {
				callback->setDataSize(max);
				callback->execute();
				return 0;
			}

			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read_until(socket, inputStreamBuffer, term, func);
		}
		return -2;
	}

	int32_t lvTlsSocketBase::startStreamRead(size_t len, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			if (len > LVSOCKETMAXREADSIZE) {
				return -1;
			}
			if (len <= inputStreamBuffer.size()) {
				callback->setDataSize(len);
				callback->execute();
				return 0;
			}

			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read(socket, inputStreamBuffer, transfer_exactly(len-inputStreamBuffer.size()), func);
		}
		return -2;
	}


	int32_t lvTlsSocketBase::startStreamReadAtLeast(size_t min, size_t max, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			if (min > LVSOCKETMAXREADSIZE) {
				return -1;
			}
			if (min <= inputStreamBuffer.size()) {
				callback->setDataSize(min);
				callback->execute();
				return 0;
			}
			
			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read(socket, inputStreamBuffer, transfer_at_least(min - inputStreamBuffer.size()), func);
		}
		return -2;
	}

	void lvTlsSocketBase::startHandshake(lvasynctls::lvTlsNewConnectionCallback * callback, boost::asio::ssl::stream_base::handshake_type handType)
	{
		auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBHandShakeComplete, this, boost::asio::placeholders::error, callback));
		socket.async_handshake(handType, func);
	}

	size_t lvTlsSocketBase::getInputStreamSize()
	{
		return inputStreamBuffer.size();
	}

	int64_t lvTlsSocketBase::inputStreamReadSome(char* buffer, int64_t len)
	{
		return inputStream.readsome(buffer, len);
	}

	boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& lvTlsSocketBase::getStream()
	{
		return socket;
	}


	void lvTlsSocketBase::CBWriteComplete(const boost::system::error_code & error, std::size_t bytes_transferred, std::vector<unsigned char> * data, lvasynctls::lvTlsDataReadCallback* callback)
	{
		//if this callback runs data has been transmitted, so free it
		if (data) {
			delete data;
			data = nullptr;
		}

		if (callback) {
			errorCheck(error, callback);
			callback->execute();

			delete callback;
			callback = nullptr;
		}
	}

	void lvTlsSocketBase::CBReadComplete(const boost::system::error_code & error, std::size_t bytes_transferred, lvasynctls::lvTlsDataReadCallback* callback)
	{
		if (callback) {
			callback->resizeDataResult(bytes_transferred);

			errorCheck(error, callback);

			//because we gave data ptr to callback earlier, we don't have to do anything else
			callback->execute();

			//also frees data ptr
			delete callback;
			callback = nullptr;
		}
	}

	void lvTlsSocketBase::CBReadStreamComplete(const boost::system::error_code & error, std::size_t bytes_transferred, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			callback->setDataSize(bytes_transferred);

			errorCheck(error, callback);

			callback->execute();

			//also frees data ptr
			delete callback;
			callback = nullptr;
		}
	}

	void lvTlsSocketBase::CBHandShakeComplete(const boost::system::error_code & error, lvasynctls::lvTlsNewConnectionCallback* callback)
	{
		if (callback) {
			if (!error) {
				callback->cacheSocket(this);
			}

			errorCheck(error, callback);
			callback->execute();

			delete callback;
			callback = nullptr;
			if (error) {
				delete this;
			}
		}
	}


//connection establisher
	lvasynctls::lvTlsConnectionCreator::lvTlsConnectionCreator(lvAsyncEngine * engineContext) :
		engineOwner{ engineContext },
		ctx{ *(engineContext->getIOService()), ssl::context::sslv23 }
	{
		if (engineContext) {
			engineContext->registerConnector(this);
		}
	}

	lvTlsConnectionCreator::~lvTlsConnectionCreator()
	{
		try {
			if (engineOwner) {
				engineOwner->unregisterConnector(this);
			}
		}
		catch (...) {

		}
	}

	void lvTlsConnectionCreator::enablePasswordCallback(std::string pw)
	{
		cachePW = pw;
		auto f = boost::bind(&lvTlsConnectionCreator::getPass, this);
		ctx.set_password_callback(f);
	}

	std::string lvTlsConnectionCreator::getPass()
	{
		return cachePW;
	}


//client
	lvTlsClientConnector::lvTlsClientConnector(lvAsyncEngine* engineContext) : 
		lvTlsConnectionCreator { engineContext },
		resolver(*(engineContext->getIOService()))
	{
		return;
	}

	lvTlsClientConnector::~lvTlsClientConnector()
	{
		try {
			resolver.cancel();
		}
		catch (...)
		{
		}
		return;
	}

	void lvTlsClientConnector::resolveAndConnect(std::string host, std::string port, lvasynctls::lvTlsNewConnectionCallback * callback)
	{
		ip::tcp::resolver::query resolveQuery(host, port);
		auto newConnection = new lvTlsSocketBase(engineOwner, ctx);
		auto func = boost::bind(&lvTlsClientConnector::CBResolveToConnect, 
			this, newConnection, placeholders::error, placeholders::iterator, callback);
		resolver.async_resolve(resolveQuery, func);
	}

	//connect 1 of 3: resolved host/port
	void lvTlsClientConnector::CBResolveToConnect(lvTlsSocketBase* newConnection, 
		const boost::system::error_code& error, ip::tcp::resolver::iterator iterator, 
		lvasynctls::lvTlsNewConnectionCallback* callback)
	{
		if (error) {
			errorCheck(error, callback);
			callback->execute();

			delete callback;
			callback = nullptr;

			delete newConnection;
			newConnection = nullptr;

			return;
		}
		auto func = boost::bind(&lvTlsClientConnector::CBConnectionEstablished, this, newConnection, boost::asio::placeholders::error, boost::asio::placeholders::iterator, callback);
		async_connect((newConnection->getStream()).lowest_layer(), iterator, func);
	}

	//connect 2 of 3: connected to target, need to init tls (next step is handshake)
	void lvTlsClientConnector::CBConnectionEstablished(lvTlsSocketBase* newConnection, const boost::system::error_code& error, ip::tcp::resolver::iterator iterator, lvasynctls::lvTlsNewConnectionCallback* callback)
	{
		if (error) {
			errorCheck(error, callback);
			callback->execute();

			delete callback;
			callback = nullptr;

			delete newConnection;
			newConnection = nullptr;

			return;
		}
		newConnection->startHandshake(callback, boost::asio::ssl::stream_base::client);
	}





//server acceptor
	lvTlsServerAcceptor::lvTlsServerAcceptor(lvAsyncEngine* engineContext, unsigned short port) :
		lvTlsConnectionCreator{ engineContext },
		serverAcceptor{ *(engineContext->getIOService()), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port) }
	{
	}

	lvTlsServerAcceptor::~lvTlsServerAcceptor()
	{
		try {
			serverAcceptor.cancel();
			serverAcceptor.close();
		}
		catch (...) {

		}
	}

	void lvTlsServerAcceptor::startAccept(lvasynctls::lvTlsNewConnectionCallback * callback)
	{
		auto newConnection = new lvTlsSocketBase(engineOwner, ctx);
		serverAcceptor.async_accept(newConnection->getStream().lowest_layer(),
			boost::bind(&lvTlsServerAcceptor::CBConnectionAccepted, this, newConnection, boost::asio::placeholders::error, callback));
	}

	void lvTlsServerAcceptor::CBConnectionAccepted(lvTlsSocketBase * newConnection, const boost::system::error_code & error, lvasynctls::lvTlsNewConnectionCallback * callback)
	{
		if (!error && newConnection) {	
			newConnection->startHandshake(callback, boost::asio::ssl::stream_base::server);
		}
		else {
			if (newConnection) {
				delete newConnection;
				newConnection = nullptr;
			}
		}
	}

	inline void errorCheck(const boost::system::error_code & error, lvasynctls::lvTlsCallback* callback) {
		if (callback) {
			if (error) {
				callback->setErrorCondition(error);
			}
		}
	}

	std::size_t isReadComplete(const boost::system::error_code& error, std::size_t bytes_transferred, std::size_t minBytesDesired) {
		if (error) {
			return 0;
		}
		if (bytes_transferred >= minBytesDesired) {
			return 0;
		}
		return minBytesDesired - bytes_transferred;
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

	lvTlsDataReadCallback::lvTlsDataReadCallback() : mem { nullptr }
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

	lvTlsCompletionCallback::lvTlsCompletionCallback(): dataLength{ 0 }
	{
	}

	lvTlsCompletionCallback::~lvTlsCompletionCallback()
	{
	}

}

