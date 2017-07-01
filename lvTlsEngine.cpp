#include "lvTlsEngine.h"
#include "lvTlsConnectionCreator.h"
#include <unordered_set>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <openssl/evp.h>



using namespace boost::asio;

namespace lvasynctls {
	//main engine
	lvAsyncEngine::lvAsyncEngine() : socketSet(), connectorSet(),
		ioEngine{ new boost::asio::io_service(std::thread::hardware_concurrency()) },
		engineWork(new io_service::work(*ioEngine))
	{
		OpenSSL_add_all_algorithms();
		OpenSSL_add_all_ciphers();
		OpenSSL_add_all_digests();
		PKCS5_PBE_add();
		lvAsyncEngine::initializeThreads(std::thread::hardware_concurrency());
	}

	lvAsyncEngine::lvAsyncEngine(std::size_t threadCount) : socketSet(), connectorSet(),
		ioEngine{ new boost::asio::io_service(threadCount) },
		engineWork(new io_service::work(*ioEngine))
	{
		OpenSSL_add_all_algorithms();
		OpenSSL_add_all_ciphers();
		OpenSSL_add_all_digests();
		PKCS5_PBE_add();
		lvAsyncEngine::initializeThreads(threadCount);
	}

	lvAsyncEngine::~lvAsyncEngine()
	{
		try {
			std::unordered_set<lvTlsSocketBase*> sockSnapshot;
			std::unordered_set<lvTlsConnectionCreator*> connSnapshot;
			{
				std::lock_guard<std::mutex> lg(lock);
				
				sockSnapshot = socketSet;
				connSnapshot = connectorSet;
				socketSet.empty();
				connectorSet.empty();
			}

			if (connSnapshot.size() > 0) {
				for (auto itr = connSnapshot.begin(); itr != connSnapshot.end(); ++itr) {
					try {
						auto val = *itr;
						if (val) {
							delete val;
							val = nullptr;
						}
					}
					catch (...) {
					}
				}
			}

			if (sockSnapshot.size() > 0) {
				for (auto itr = sockSnapshot.begin(); itr != sockSnapshot.end(); ++itr) {
					try {
						auto val = *itr;
						if (val) {
							delete val;
							val = nullptr;
						}
					}
					catch (...) {
					}
				}
			}
			
		}
		catch (...) {
		}

		//engine has to run to allow sockets to shutdown do we do that now.
		try {
			delete engineWork;
			engineWork = nullptr;

			ioEngine->stop();
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
			try {
				std::lock_guard<std::mutex> lockg(lock);
				socketSet.insert(tlsSock);
			}
			catch (...)	{

			}
		}

		return;
	}

	std::size_t lvAsyncEngine::unregisterSocket(lvTlsSocketBase * tlsSock)
	{
		std::size_t count = 0;
		if (tlsSock) {
			try {
				std::lock_guard<std::mutex> lockg(lock);
				count = socketSet.erase(tlsSock);
			}
			catch (...) {

			}
		}

		return count;
	}

	void lvAsyncEngine::registerConnector(lvTlsConnectionCreator * connector)
	{
		if (connector) {
			try {
				std::lock_guard<std::mutex> lockg(lock);
				connectorSet.insert(connector);
			}
			catch (...) {

			}
		}

		return;
	}

	std::size_t lvAsyncEngine::unregisterConnector(lvTlsConnectionCreator * connector)
	{
		std::size_t count = 0;
		if (connector) {
			try {
				std::lock_guard<std::mutex> lockg(lock);
				count = connectorSet.erase(connector);
			}
			catch (...) {

			}
		}

		return count;
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
			if (ioengine) {
				try
				{
					ioengine->run();
				}
				catch (...)
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
}