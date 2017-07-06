#pragma once

#include "lvTlsCallbackBase.h"
#include "lvTlsEngine.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lockfree/queue.hpp>


namespace lvasynctls {
	class lvTlsSocketBase;
	class lvAsyncEngine;
	class lvTlsCallback;

	class lvTlsConnectionCreator {
	public:
		explicit lvTlsConnectionCreator(lvAsyncEngine* engineContext, size_t connectionQueueSize);
		virtual ~lvTlsConnectionCreator();
		
		void enablePasswordCallback(std::string pw);
		
		
		lvTlsSocketBase* getNextConnection();

		boost::asio::ssl::context ctx;
	protected:
		void completeHandshake(lvTlsSocketBase * newConnection, boost::asio::ssl::stream_base::handshake_type handType, lvasynctls::lvTlsCallback * callback);
		
		lvAsyncEngine* engineOwner;
		boost::lockfree::queue<lvTlsSocketBase*, boost::lockfree::fixed_sized<true>> ConnQ;

	private:
		lvTlsConnectionCreator() = delete;
		lvTlsConnectionCreator(const lvTlsConnectionCreator& that) = delete;
		const lvTlsConnectionCreator& operator=(const lvTlsConnectionCreator&) = delete;

		

		std::string getPass();
		std::string cachePW;
	};

	
	class lvTlsClientConnector : public lvTlsConnectionCreator {
	public:
		explicit lvTlsClientConnector(lvAsyncEngine* engineContext, size_t connectionQueueSize = 10);
		~lvTlsClientConnector() override;

		void resolveAndConnect(std::string host, std::string port, size_t streamSize, lvasynctls::lvTlsCallback * callback);

	private:
		lvTlsClientConnector() = delete;
		lvTlsClientConnector(const lvTlsClientConnector& that) = delete;
		const lvTlsClientConnector& operator=(const lvTlsClientConnector&) = delete;

		boost::asio::ip::tcp::resolver resolver;
	};

	
	class lvTlsServerAcceptor : public lvTlsConnectionCreator {
	public:
		explicit lvTlsServerAcceptor(lvAsyncEngine* engineContext, unsigned short port, size_t connectionQueueSize = 10);
		~lvTlsServerAcceptor() override;

		void startAccept(size_t streamSize, lvasynctls::lvTlsCallback * callback);

	private:
		lvTlsServerAcceptor() = delete;
		lvTlsServerAcceptor(const lvTlsServerAcceptor& that) = delete;
		const lvTlsServerAcceptor& operator=(const lvTlsServerAcceptor&) = delete;

		boost::asio::ip::tcp::acceptor serverAcceptor;
	};
}