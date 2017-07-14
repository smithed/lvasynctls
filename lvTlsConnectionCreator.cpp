#include "lvTlsConnectionCreator.h"
#include "lvTlsCallbackBase.h"
#include "lvTlsEngine.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <queue>
#include <mutex>


using namespace boost::asio;
namespace lvasynctls {

	//connection establisher
	lvasynctls::lvTlsConnectionCreator::lvTlsConnectionCreator(std::shared_ptr<lvAsyncEngine> engineContext, size_t connectionQueueSize) :
		engineOwner{ engineContext },
		ctx{ *(engineContext->getIOService()), ssl::context::sslv23 }, 
		connQ { }, 
		dead {false}
	{
	}

	void lvTlsConnectionCreator::shutdown()
	{
		dead = true;
		try {
			engineOwner->unregisterConnector(this);
		}
		catch (...) {
		}
	}

	lvTlsConnectionCreator::~lvTlsConnectionCreator()
	{
		shutdown();
		try {
			engineOwner->unregisterConnector(this);
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

	std::shared_ptr<lvTlsSocketBase> lvTlsConnectionCreator::getNextConnection()
	{
		std::lock_guard<std::mutex> lg(connQLock);
		if (connQ.empty()) {
			return std::shared_ptr<lvTlsSocketBase>{nullptr};
		}
		std::shared_ptr<lvTlsSocketBase> c = std::move(connQ.front());
		connQ.pop();
		return c;
	}

	void lvTlsConnectionCreator::completeHandshake(std::shared_ptr<lvTlsSocketBase> newConnection, boost::asio::ssl::stream_base::handshake_type handType, lvasynctls::lvTlsCallback * callback)
	{
		boost::system::error_code ecref;
		boost::system::error_code ecret;
		if (!dead) {
			ecret = newConnection->getStream().handshake(handType, ecref);
		}

		if (ecref || ecret || dead) {
			if (ecret && callback) errorCheck(ecref, callback);
			if (ecret && callback) errorCheck(ecret, callback);
			newConnection->shutdown();
			newConnection.reset();
		}
		else {
			engineOwner->registerSocket(newConnection);
			{
				std::lock_guard<std::mutex> lg(connQLock);
				connQ.push(newConnection);
			}
		}

		if (callback) {
			callback->execute();

			delete callback;
			callback = nullptr;
		}

		return;
	}

	std::string lvTlsConnectionCreator::getPass()
	{
		return cachePW;
	}
	


	//client
	lvTlsClientConnector::lvTlsClientConnector(std::shared_ptr<lvAsyncEngine> engineContext, size_t connectionQueueSize) :
		lvTlsConnectionCreator{ engineContext, connectionQueueSize },
		resolver(*(engineContext->getIOService())),
		pendingConnects{}
	{
		return;
	}

	void lvTlsClientConnector::shutdown()
	{
		if (!dead) {
			dead = true;
			try {
				resolver.cancel();
			}
			catch (...)
			{
			}

			//retry empty up to 10x unless already dead, then try once
			for (size_t i = 0; i < (dead ? 1 : 10); i++)
			{
				{
					std::map<lvTlsSocketBase*, std::shared_ptr<lvTlsSocketBase>> pendingCpy;
					{
						std::lock_guard<std::mutex> lg{ connectLock };
						pendingCpy = pendingConnects;
					}

					if (!pendingCpy.empty()) {
						for each (auto sockPair in pendingCpy)
						{
							auto ssp = sockPair.second;
							if (ssp) {
								boost::system::error_code ec;
								ssp->getStream().lowest_layer().cancel(ec);
							}
						}
					}
				}

				bool poll = true;
				//retry half second unless dead, then 50ms
				for (size_t j = 0; j < (dead ? 10 : 100); j++)
				{
					while (poll) {
						{
							std::lock_guard<std::mutex> lg{ connectLock };
							poll = (pendingConnects.size() > 0);
						}
						if (poll) {
							std::this_thread::sleep_for(std::chrono::milliseconds(5)); //yield
						}
						else {
							break;
						}
					}
					if (!poll) {
						break;
					}
				}
				if (!poll) {
					break;
				}
			}

			lvTlsConnectionCreator::shutdown();
		}
	}

	lvTlsClientConnector::~lvTlsClientConnector()
	{
		shutdown();
		return;
	}

	void lvTlsClientConnector::resolveAndConnect(std::string host, std::string port, size_t streamSize, size_t outQueueSize, lvasynctls::lvTlsCallback * callback)
	{
		if (!dead && !host.empty() && !port.empty()) {
			ip::tcp::resolver::query resolveQuery(host, port);
			
			std::shared_ptr<lvTlsSocketBase> newConnection{ new lvTlsSocketBase(engineOwner, ctx, streamSize, outQueueSize) };

			{
				std::lock_guard<std::mutex> lg{ connectLock };
				pendingConnects.insert({newConnection.get(),newConnection});
			}

			try
			{
				resolver.async_resolve(resolveQuery,
					[this, newConnection, callback](const boost::system::error_code& error, ip::tcp::resolver::iterator iterator) mutable {
					if (error || dead) {
						//exit early, error is not fatal so don't set dead

						{
							std::lock_guard<std::mutex> lg{ connectLock };
							pendingConnects.erase(newConnection.get());
						}

						if (callback) {
							errorCheck(error, callback);
							callback->execute();

							delete callback;
							callback = nullptr;
						}

						try {
							newConnection->shutdown();
							newConnection.reset();
						}
						catch (...) {
						}

						return;
					}

					else {
						async_connect((newConnection->getStream()).lowest_layer(), iterator,
							[this, newConnection, callback](const boost::system::error_code& error, ip::tcp::resolver::iterator iterator) mutable {
							if (error || dead) {
								//exit early, error is not fatal so don't set dead
								{
									std::lock_guard<std::mutex> lg{ connectLock };
									pendingConnects.erase(newConnection.get());
								}

								if (callback) {
									errorCheck(error, callback);
									callback->execute();

									delete callback;
									callback = nullptr;
								}
								try {
									newConnection->shutdown();
									newConnection.reset();
								}
								catch (...) {
								}

								return;
							}
							else {
								try
								{
									completeHandshake(newConnection, boost::asio::ssl::stream_base::client, callback);
								}
								catch (...)
								{
									newConnection->shutdown();
									newConnection.reset();
								}

								{
									std::lock_guard<std::mutex> lg{ connectLock };
									pendingConnects.erase(newConnection.get());
								}
							}
						}
						);
					}
				});
			}
			catch (...)
			{
				//if we can't start a resolve on a normal request (that didnt already cause an exception before the try), this is fatal
				dead = true;
				{
					std::lock_guard<std::mutex> lg{ connectLock };
					pendingConnects.erase(newConnection.get());
				}
			}
		}
	}


	//server acceptor
	lvTlsServerAcceptor::lvTlsServerAcceptor(std::shared_ptr<lvAsyncEngine> engineContext, boost::asio::ip::tcp::endpoint endpt, size_t connectionQueueSize) :
		lvTlsConnectionCreator{ engineContext , connectionQueueSize },
		serverAcceptor{ *(engineContext->getIOService()), endpt },
		acceptRunning{false}
	{
	}

	void lvTlsServerAcceptor::shutdown()
	{
		if (!dead) {
			dead = true;
			
			boost::system::error_code ec;
			serverAcceptor.cancel(ec);
			serverAcceptor.close(ec);

			{
				std::lock_guard<std::mutex> lg{ acceptQueueLock };
				while(!acceptQueue.empty()){
					auto farg = acceptQueue.front();
					acceptQueue.pop();
					delete farg.callback;
				}
			}

			//try for up to 2 sec
			for (size_t i = 0; i < 2000; i++)
			{
				if (acceptRunning) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1)); //yield
				}
				else {
					break;
				}
			}

			lvTlsConnectionCreator::shutdown();
		}
		
	}

	lvTlsServerAcceptor::~lvTlsServerAcceptor()
	{
		shutdown();
	}

	void lvTlsServerAcceptor::startAccept(size_t streamSize, size_t outQueueSize, lvasynctls::lvTlsCallback * callback)
	{
		if (!dead) {
			{
				std::lock_guard<std::mutex> lg{ acceptQueueLock };
				acceptArgs arg{ callback, streamSize, outQueueSize };
				acceptQueue.push(arg);
			}

			bool notRunning = false;
			bool isRunning = true;

			if (acceptRunning.compare_exchange_strong(notRunning, isRunning)) {
				(engineOwner->getIOService())->post([this]() mutable { doAccept(); });
			}
		}
		else {
			if (callback) {
				delete callback;
			}
		}
	}

	void lvTlsServerAcceptor::doAccept()
	{		
		acceptArgs arg{ nullptr, 0 , 0};
		bool run = !dead;
		{
			std::lock_guard<std::mutex> lg{ acceptQueueLock };
			run &= !acceptQueue.empty();
			if (run) {
				arg = acceptQueue.front();
				acceptQueue.pop();
			}
		}

		if (run) {
			std::shared_ptr<lvTlsSocketBase> newConnection{ new lvTlsSocketBase(engineOwner, ctx, arg.streamSize, arg.outQueueSize) };
			serverAcceptor.async_accept(newConnection->getStream().lowest_layer(),
				[this, newConnection, cb=arg.callback](const boost::system::error_code & error) mutable 
				{
					if (error || dead) {
						acceptRunning = false;

						if (cb) {
							errorCheck(error, cb);
							cb->execute();

							delete cb;
							cb = nullptr;
						}

						newConnection->shutdown();
						newConnection.reset();
					}
					else {
						try {
							completeHandshake(newConnection, boost::asio::ssl::stream_base::server, cb);
							(engineOwner->getIOService())->post([this]() mutable { doAccept(); });
						}
						catch (...)
						{
							acceptRunning = false;
						}
						return;
					}
				}
			);
		}
		else {
			acceptRunning = false;
			if (arg.callback) {
				delete arg.callback;
			}
		}
	}

}