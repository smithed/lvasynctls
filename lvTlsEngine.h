#pragma once

#include "lvTlsSocket.h"
#include "lvTlsConnectionCreator.h"
#include <unordered_map>
#include <mutex>
#include <memory>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>


namespace lvasynctls {
	class lvTlsConnectionCreator;
	class lvTlsSocketBase;

	class lvAsyncEngine  {
	public:
		explicit lvAsyncEngine();
		explicit lvAsyncEngine(std::size_t threadCount);
		void shutdown();
		~lvAsyncEngine();
		std::shared_ptr<boost::asio::io_service> getIOService();

		void registerSocket(std::shared_ptr<lvTlsSocketBase> tlsSock);
		std::size_t unregisterSocket(lvTlsSocketBase* tlsSock);

		void registerConnector(std::shared_ptr<lvTlsConnectionCreator> connector);
		std::size_t unregisterConnector(lvTlsConnectionCreator* connector);

	private:
		lvAsyncEngine(const lvAsyncEngine& that) = delete;
		const lvAsyncEngine& operator=(const lvAsyncEngine&) = delete;
		void initializeThreads(std::size_t threadCount);

		void ioRunThread(std::shared_ptr<boost::asio::io_service> ioengine);

		std::shared_ptr<boost::asio::io_service> ioEngine;
		boost::asio::io_service::work* engineWork;
		boost::thread_group threadSet;
		std::unordered_map<lvTlsSocketBase*, std::shared_ptr<lvTlsSocketBase>> socketSet;
		std::unordered_map<lvTlsConnectionCreator*, std::shared_ptr<lvTlsConnectionCreator>> connectorSet;
		std::mutex lock;
		std::atomic_bool dead;
	};
}

