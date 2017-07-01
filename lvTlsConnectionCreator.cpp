#include "lvTlsConnectionCreator.h"
#include "lvTlsCallbackBase.h"
#include "lvTlsEngine.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/ip/tcp.hpp>


using namespace boost::asio;
namespace lvasynctls {

	//connection establisher
	lvasynctls::lvTlsConnectionCreator::lvTlsConnectionCreator(lvAsyncEngine * engineContext, size_t connectionQueueSize) :
		engineOwner{ engineContext },
		ctx{ *(engineContext->getIOService()), ssl::context::sslv23 }, 
		ConnQ { connectionQueueSize }
	{
		if (engineContext)
		{
			engineContext->registerConnector(this);
		}
	}

	lvTlsConnectionCreator::~lvTlsConnectionCreator()
	{
		try {
			if (engineOwner) {
				engineOwner->unregisterConnector(this);
				engineOwner = nullptr;
			}
		}
		catch (...) {
		}
		
	}

	void lvTlsConnectionCreator::enablePasswordCallback(const std::string pw)
	{
		cachePW = pw;
		auto f = boost::bind(&lvTlsConnectionCreator::getPass, this);
		ctx.set_password_callback(f);
	}


	lvTlsSocketBase * lvTlsConnectionCreator::getNextConnection()
	{
		lvTlsSocketBase * c = nullptr;
		auto success = ConnQ.pop(c);
		if (success) {
			return c;
		}
		return nullptr;
	}

	void lvTlsConnectionCreator::completeHandshake(lvTlsSocketBase * newConnection, lvasynctls::lvTlsCallback * callback)
	{
		boost::system::error_code ecref;
		auto ecret = newConnection->getStream().handshake(boost::asio::ssl::stream_base::client, ecref);

		if (ecref || ecret) {
			if (ecret) errorCheck(ecref, callback);
			if (ecret) errorCheck(ecret, callback);
			delete newConnection;
		}
		else {
			ConnQ.push(newConnection);
		}

		callback->execute();

		delete callback;
		callback = nullptr;

		return;
	}

	std::string lvTlsConnectionCreator::getPass()
	{
		return cachePW;
	}



	//client
	lvTlsClientConnector::lvTlsClientConnector(lvAsyncEngine* engineContext, size_t connectionQueueSize) :
		lvTlsConnectionCreator{ engineContext, connectionQueueSize },
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

	void lvTlsClientConnector::resolveAndConnect(std::string host, std::string port, lvasynctls::lvTlsCallback * callback)
	{
		ip::tcp::resolver::query resolveQuery(host, port);
		auto newConnection = new lvTlsSocketBase(engineOwner, ctx);
		auto func = boost::bind(&lvTlsClientConnector::CBResolveToConnect,this, newConnection, placeholders::error, placeholders::iterator, callback);
		resolver.async_resolve(resolveQuery, func);
	}

	//connect 1 of 3: resolved host/port
	void lvTlsClientConnector::CBResolveToConnect(lvTlsSocketBase* newConnection,
		const boost::system::error_code& error, ip::tcp::resolver::iterator iterator,
		lvasynctls::lvTlsCallback* callback)
	{
		if (error) {
			if (callback) {
				errorCheck(error, callback);
				callback->execute();

				delete callback;
				callback = nullptr;
			}
			if (newConnection) {
				delete newConnection;
				newConnection = nullptr;
			}
			return;
		}
		auto func = boost::bind(&lvTlsClientConnector::CBConnectionEstablished, this, newConnection, boost::asio::placeholders::error, boost::asio::placeholders::iterator, callback);
		if (newConnection && callback) {
			async_connect((newConnection->getStream()).lowest_layer(), iterator, func);
		}
		else if (callback) {
				callback->execute();
				delete callback;
				callback = nullptr;
		}
		
	}

	//connect 2 of 3: connected to target, need to init tls (next step is handshake)
	void lvTlsClientConnector::CBConnectionEstablished(lvTlsSocketBase* newConnection, const boost::system::error_code& error, ip::tcp::resolver::iterator iterator, lvasynctls::lvTlsCallback* callback)
	{
		if (error || !newConnection) {
			if (callback) {
				errorCheck(error, callback);
				callback->execute();

				delete callback;
				callback = nullptr;
			}
			if (newConnection) {
				delete newConnection;
				newConnection = nullptr;
			}
			return;
		}
		else {
			completeHandshake(newConnection, callback);
			return;
		}
	}



	//server acceptor
	lvTlsServerAcceptor::lvTlsServerAcceptor(lvAsyncEngine* engineContext, unsigned short port, size_t connectionQueueSize) :
		lvTlsConnectionCreator{ engineContext , connectionQueueSize },
		serverAcceptor{ *(engineContext->getIOService()), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port) }
	{
	}

	lvTlsServerAcceptor::~lvTlsServerAcceptor()
	{
		try {
			serverAcceptor.cancel();
		}
		catch (...) {
		}
		try {
			serverAcceptor.close();
		}
		catch (...) {
		}
	}

	void lvTlsServerAcceptor::startAccept(lvasynctls::lvTlsCallback * callback)
	{
		auto newConnection = new lvTlsSocketBase(engineOwner, ctx);
		if (newConnection && callback) {
			serverAcceptor.async_accept(newConnection->getStream().lowest_layer(),
				boost::bind(&lvTlsServerAcceptor::CBConnectionAccepted, this, newConnection, boost::asio::placeholders::error, callback));
			return;
		}
		else if (callback) {
			delete callback;
			callback = nullptr;
		}
	}

	void lvTlsServerAcceptor::CBConnectionAccepted(lvTlsSocketBase * newConnection, const boost::system::error_code & error, lvasynctls::lvTlsCallback * callback)
	{
		if (error || !newConnection) {
			if (callback) {
				errorCheck(error, callback);
				callback->execute();

				delete callback;
				callback = nullptr;
			}
			if (newConnection) {
				delete newConnection;
				newConnection = nullptr;
			}
			return;
		}
		else {
			completeHandshake(newConnection, callback);
			return;
		}
	}

}