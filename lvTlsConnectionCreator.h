#pragma once

#include "lvTlsCallbackBase.h"
#include "lvTlsEngine.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/ip/tcp.hpp>


namespace lvasynctls {
	class lvTlsSocketBase;
	class lvAsyncEngine;
	class lvTlsNewConnectionCallback;

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
}