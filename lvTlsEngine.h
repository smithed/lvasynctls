#pragma once

#include "lvTlsSocket.h"
#include "lvTlsConnectionCreator.h"
#include <unordered_set>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>


namespace lvasynctls {
	class lvTlsConnectionCreator;
	class lvTlsSocketBase;

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
}

