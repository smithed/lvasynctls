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
			if (ioengine) {
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
}