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
		lvTlsConnectionCreator{ engineContext },
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


}