#pragma once

#include "lvTlsCallbackBase.h"
#include "lvTlsEngine.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <atomic>
#include <queue>
#include <mutex>

namespace lvasynctls {
	class lvTlsSocketBase;
	class lvAsyncEngine;
	class lvTlsCallback;

	class lvTlsConnectionCreator {
	public:
		explicit lvTlsConnectionCreator(std::shared_ptr<lvAsyncEngine> engineContext, size_t connectionQueueSize);
		virtual void shutdown();
		virtual ~lvTlsConnectionCreator();
		

		void enablePasswordCallback(std::string pw);
		
		std::shared_ptr<lvTlsSocketBase> getNextConnection();

		boost::asio::ssl::context ctx;
	protected:
		void completeHandshake(std::shared_ptr<lvTlsSocketBase> newConnection, boost::asio::ssl::stream_base::handshake_type handType, lvasynctls::lvTlsCallback * callback);
		
		std::shared_ptr<lvAsyncEngine> engineOwner;

		std::mutex connQLock;
		std::queue<std::shared_ptr<lvTlsSocketBase>> connQ;

		std::atomic_bool dead;

	private:
		lvTlsConnectionCreator() = delete;
		lvTlsConnectionCreator(const lvTlsConnectionCreator& that) = delete;
		const lvTlsConnectionCreator& operator=(const lvTlsConnectionCreator&) = delete;


		std::string getPass();
		std::string cachePW;
	};

	
	class lvTlsClientConnector : public lvTlsConnectionCreator {
	public:
		explicit lvTlsClientConnector(std::shared_ptr<lvAsyncEngine> engineContext, size_t connectionQueueSize = 10);
		void shutdown() override;
		~lvTlsClientConnector() override;

		void resolveAndConnect(std::string host, std::string port, size_t streamSize, size_t outQueueSize, lvasynctls::lvTlsCallback * callback);

	private:
		lvTlsClientConnector() = delete;
		lvTlsClientConnector(const lvTlsClientConnector& that) = delete;
		const lvTlsClientConnector& operator=(const lvTlsClientConnector&) = delete;

		std::mutex connectLock;
		std::map<lvTlsSocketBase*, std::shared_ptr<lvTlsSocketBase>> pendingConnects;

		boost::asio::ip::tcp::resolver resolver;
	};

	
	class lvTlsServerAcceptor : public lvTlsConnectionCreator {
	public:
		explicit lvTlsServerAcceptor(std::shared_ptr<lvAsyncEngine> engineContext, boost::asio::ip::tcp::endpoint endpt, size_t connectionQueueSize = 10);
		void shutdown() override;
		~lvTlsServerAcceptor() override;

		void startAccept(size_t streamSize, size_t outQueueSize, lvasynctls::lvTlsCallback * callback);

	private:
		lvTlsServerAcceptor() = delete;
		lvTlsServerAcceptor(const lvTlsServerAcceptor& that) = delete;
		const lvTlsServerAcceptor& operator=(const lvTlsServerAcceptor&) = delete;

		typedef struct acceptArgs {
			lvasynctls::lvTlsCallback * callback;
			size_t streamSize;
			size_t outQueueSize;
		} acceptArgs;

		void doAccept();
		std::mutex acceptQueueLock;
		std::queue<acceptArgs> acceptQueue;
		std::atomic_bool acceptRunning;

		boost::asio::ip::tcp::acceptor serverAcceptor;
	};
}