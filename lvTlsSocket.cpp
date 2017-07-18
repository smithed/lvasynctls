#include "lvTlsSocket.h"
#include "lvTlsCallbackBase.h"
#include <vector>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <functional>
#include <istream>
#include <ostream>
#include <condition_variable>


using namespace boost::asio;

namespace lvasynctls {

	lvTlsSocketBase::lvTlsSocketBase(std::shared_ptr<lvAsyncEngine> engineContext, boost::asio::ssl::context& sslContext, size_t streamSize, size_t outputQueueSize) :
		engineOwner{ engineContext },
		socketStrand{ *(engineContext->getIOService()) },
		socket{ *(engineContext->getIOService()), sslContext },
		socketErr{ 0 },
		iSLock{},
		inputRunning{ false },
		inQueue{},
		inputStreamBuffer{ streamSize },
		inputStream{ &inputStreamBuffer },
		oQLock{},
		outputRunning{ false },
		outQueue{},
		oQMaxSize{ outputQueueSize }
	{
	}

	void lvTlsSocketBase::shutdown()
	{
		//we are somewhat protected by the mutex in lvland, but this seems to be whats needed to avoid crashes and (more commonly) hangs
		//https://stackoverflow.com/questions/32046034/what-is-the-proper-way-to-securely-disconnect-an-asio-ssl-socket
		//https://stackoverflow.com/questions/22575315/how-to-gracefully-shutdown-a-boost-asio-ssl-client
		//https://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error/25703699#25703699

		int noErr = 0;
		int abortSock = LVTLSSOCKETERRFLAGABORT;
		//set socket error flag if it wasn't set already
		if (socketErr.compare_exchange_strong(noErr, abortSock)) {
			//true means it was set to noErr before so we need to kill everything
			bool runningCpy = false;
			boost::system::error_code ec;
			int32_t iter = 0;
			//stop all ongoing write operations, retry up to 2 second (see wait at bottom)
			while (iter < 2000) {
				iter++;
				try {
					//cancel existing operations
					socket.lowest_layer().cancel(ec);
					socket.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
				}
				catch (...) {
					//assumption is that something in socket.lowest_layer is dead, so we should probably stop running.
					iter = INT32_MAX;
				}
				
				{
					std::lock_guard<std::mutex> lg(oQLock);
					runningCpy = outputRunning;
				}

				if (!runningCpy) {
					iter = INT32_MAX;
				}
				else {
					std::this_thread::sleep_for(std::chrono::milliseconds(1)); //yield
				}
			}

			iter = 0;
			
			//stop all ongoing read operations, retry up to 2 second (see wait at bottom)
			while (iter < 2000) {
				iter++;

				try {
					//cancel existing operations
					socket.lowest_layer().cancel(ec);
					socket.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
				}
				catch (...) {
					//assumption is that something in socket.lowest_layer is dead, so we should probably stop running.
					iter = INT32_MAX;
				}
				{
					std::lock_guard<std::mutex> lg(iSLock);
					runningCpy = inputRunning;
				}
				if (!inputRunning) {
					iter = INT32_MAX;
				}
				else {
					std::this_thread::sleep_for(std::chrono::milliseconds(1)); //yield
				}				
			}

			//clear queues
			{
				std::lock_guard<std::mutex> lg(oQLock);
				while (!outQueue.empty()) {
					//properly clean up queue elements
					auto temp = outQueue.front();

					delete temp.chunkdata;

					if (temp.optcallback) {
						temp.optcallback->setErrorCondition(1000, "socket shut down");
						temp.optcallback->execute();
						delete temp.optcallback;
					}

					outQueue.pop();
				}
			}
			{
				std::lock_guard<std::mutex> lg(iSLock);
				while (!inQueue.empty()) {
					//properly clean up queue elements
					auto temp = inQueue.front();

					if (temp.optcallback) {
						temp.optcallback->setErrorCondition(1000, "socket shut down");
						temp.optcallback->execute();
						delete temp.optcallback;
					}

					inQueue.pop();
				}
			}



			try {
				std::atomic_bool done{ false };

				//start shutdown process, this hack brought to you by stack overflow. Async shutdown blocks until data moves...
				socket.async_shutdown(socketStrand.wrap(
					[&done](const boost::system::error_code& ec) {
					//we want this shutdown process to still be synchronous, we just want to force it a bit
					//the queue lets us keep things synchronous
						done = true;
					}
				));

				//so then we move some data with an async write
				const char buffer[] = "\0";
				boost::system::error_code wec;
				//this write should immediately fail, allowing shutdown to complete
				async_write(socket, boost::asio::buffer(buffer), socketStrand.wrap(
					[](...) {
					//ignore anything that happens here
					return;
				}
				));

				for (size_t i = 0; i < 500; i++)
				{
					if (done) {
						break;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(5)); //yield
				}
			}
			catch (...) {
			}

			try {
				socket.lowest_layer().close();
			}
			catch (...) {
			}

		}
		
	}

	lvTlsSocketBase::~lvTlsSocketBase()
	{
		engineOwner->unregisterSocket(this);

		shutdown();

		return;
	}

	// WRITE


	void lvTlsSocketBase::writeCB(const boost::system::error_code & error, std::size_t bytes_transferred) {
		std::vector<unsigned char> * lastData = nullptr;
		lvasynctls::lvTlsCallback * lastCallback = nullptr;
		outputChunk next{ nullptr, nullptr };
		size_t sz = 0;
		int err = socketErr;
		bool more = false;
		{
			//ok, now we've started the write action lets pop
			std::lock_guard<std::mutex> lg(oQLock);
			sz = outQueue.size();
			if (sz > 0) {
				auto wd = outQueue.front();
				lastData = wd.chunkdata;
				lastCallback = wd.optcallback;
				outQueue.pop();
			}
			if ((sz > 1) && !error && !err && engineOwner) {
				//more data to write
				next = outQueue.front();
				more = true;
			}
			else {
				//no more data, set output running back to false
				outputRunning = false;
			}
		}

		delete lastData;
		if (lastCallback) {
			errorCheck(error, lastCallback);
			lastCallback->execute();

			delete lastCallback;
			lastCallback = nullptr;
		}



		if (more) {
			//socket is still OK, this request succeeded, and there is more work to do
			try
			{
				async_write(socket, buffer(*(next.chunkdata)), socketStrand.wrap(boost::bind(&lvTlsSocketBase::writeCB, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)));
			}
			catch (...)
			{
				int noErr = 0;
				int eVal = 42;
				socketErr.compare_exchange_strong(noErr, eVal);
				{	
					std::lock_guard<std::mutex> lg(oQLock); 
					outputRunning = false;
				}				
			}

		}
		else {
			{
				std::lock_guard<std::mutex> lg(oQLock);
				outputRunning = false;
			}
			if (error) {
				int noErr = 0;
				int code = error.value();
				//set socket error flag, eventually killing all operations on the socket.
				socketErr.compare_exchange_strong(noErr, code);
			}
		}
	}

	void lvTlsSocketBase::writeAction() {
		outputChunk next{ nullptr, nullptr };
		bool ready = false;
		{
			std::lock_guard<std::mutex> lg(oQLock);
			if ((0 == socketErr) && (outQueue.size() >= 1)) {
				//preview the queue, this lets any other enqueuers know they don't need to start a write action
				next = outQueue.front();
				ready = true;
			}
		}

		if (ready) {
			async_write(socket, buffer(*(next.chunkdata)), socketStrand.wrap(boost::bind(&lvTlsSocketBase::writeCB, this, boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred)));
		}
		else {
			{
				std::lock_guard<std::mutex> lg(oQLock);
				outputRunning = false;
			}
		}
		return;
	}

	//take ownership of data buffer if return > 0 and of callback, we will dispose
	//return: (-2, socket dead), (-1, arg err), (0, no space), (1, success, is enqueued), (2, success, will run next)
	int lvTlsSocketBase::startWrite(std::vector<unsigned char> * data, lvasynctls::lvTlsCallback * callback)
	{
		//general gist of problem and solution is here: https://stackoverflow.com/questions/7754695/boost-asio-async-write-how-to-not-interleaving-async-write-calls
		if (data) {
			outputChunk wd{ data, callback };
			bool start = false;
			int err = 0;
			{
				std::lock_guard<std::mutex> lg(oQLock);
				err = socketErr;

				if ((outQueue.size() >= oQMaxSize) && outputRunning) {
					//too many elements, dont' enqueue; if not running, go ahead and add but start the run process.
					return 0;
				}
				if (err != 0) {
					//socket should be dead
					return -2;
				}
				outQueue.push(wd);

				if (!outputRunning) {
					start = true;
					outputRunning = true;
				}
			}

			if (start) {
				try
				{
					(engineOwner->getIOService())->post([this]() mutable { writeAction(); });
				}
				catch (const std::exception&)
				{
					int noErr = 0;
					int eVal = 42;
					socketErr.compare_exchange_strong(noErr, eVal);
					{
						std::lock_guard<std::mutex> lg(oQLock);
						outputRunning = false;
					}
					
					return -2;
				}

				return 2;
			}
			else if (0 == err) {
				//someone else is taking care of it
				return 1;
			}
			else {
				return -2;
			}

		}
		//only get here if data was null
		return -1;
	}

	// READ
		
	int lvTlsSocketBase::startStreamReadUntilTermChar(unsigned char term, size_t max, lvasynctls::lvTlsCallback * callback)
	{
		inputChunk ind { 
			//read operation lambda
			[this, term, max, callback](readOperationFinalize finalizer) {
				async_read_until(socket, inputStreamBuffer, term, finalizer);
			}, 
			callback 
		};
		return startReadCore(ind);
	}

	int lvTlsSocketBase::startStreamReadUntilTermString(std::string term, size_t max, lvasynctls::lvTlsCallback * callback)
	{
		inputChunk ind{ 
			//read operation lambda
			[this, term, max, callback](readOperationFinalize finalizer) {
				async_read_until(socket, inputStreamBuffer, term, finalizer);
			}, 
			callback 
		};
		return startReadCore(ind);
	}

	int lvTlsSocketBase::startStreamRead(size_t len, lvasynctls::lvTlsCallback * callback)
	{
		inputChunk ind{ 
			//read operation lambda
			[this, len, callback](readOperationFinalize finalizer) {
				auto maxRead = inputStreamBuffer.max_size() - inputStreamBuffer.size();
				auto readsize = (len > maxRead) ? maxRead : len;
				async_read(socket, inputStreamBuffer, 
					[this, readsize](const boost::system::error_code & err, std::size_t bytes_transferred) -> std::size_t
					{
						if (err || (bytes_transferred > readsize) || inputStreamBuffer.size() == inputStreamBuffer.max_size()) {
							return 0;
						}
						else {
							auto remain = readsize - bytes_transferred;
							auto space = inputStreamBuffer.max_size() - inputStreamBuffer.size();
							return ((remain > space) ? space : remain);
						}
						return 0;
					},
					finalizer);
			},
			callback 
		};
		return startReadCore(ind);
	}

	void lvasynctls::lvTlsSocketBase::readAction()
	{
		bool success = false;
		inputChunk ind{ nullptr, nullptr };
		{
			std::lock_guard<std::mutex> lg(iSLock);
			if ((0 == socketErr) && (inQueue.size() >= 1)) {
				//preview the queue to make sure something is avail
				ind = inQueue.front();
				success = true;
			}
		}

		if (success) {
			ind.op(
				[this](const boost::system::error_code & error, std::size_t bytes_transferred) mutable
				{
					lvasynctls::lvTlsCallback* cb = nullptr;
					size_t sz = 0;
					{
						//ok, now we've finished the read action lets pop
						std::lock_guard<std::mutex> lg(iSLock);
						sz = inQueue.size();
						if (sz > 0) {
							auto ind = inQueue.front();
							cb = ind.optcallback;
							inQueue.pop();
						}


						if (!error && (0 == socketErr) && (sz > 1)) {
							//if there is still work to do and an error did not occur at this time
							try
							{
								(engineOwner->getIOService())->post([this]() mutable { readAction(); });
							}
							catch (...)
							{
								int noErr = 0;
								int eVal = 42;
								socketErr.compare_exchange_strong(noErr, eVal);
								inputRunning = false;
							}
						}
						else {
							//we're going to stop working, so set inputRunning to false to signal that the next read should start it up
							inputRunning = false;
							if (error) {
								//set error flag so other operations quit
								int noErr = 0;
								int eVal = error.value();
								socketErr.compare_exchange_strong(noErr, eVal);
							}
						}
					}

					//run final user callback if it wasn't already killed by socket closing
					if (cb) {
						errorCheck(error, cb);
						cb->execute();

						delete cb;
						cb = nullptr;
					}

				}
			);
		}
		else {
			inputRunning = false;
		}
		return;
	}

	//core function for all reads, although considering how many lambdas are involved it might not be worth it...
	//return: (-2, socket dead), (-1, no data-arg err), (0, no space), (1, success, is enqueued), (2, success, will run next)
	int lvasynctls::lvTlsSocketBase::startReadCore(inputChunk inChunk)
	{
		//general gist of problem and solution is here: https://stackoverflow.com/questions/7754695/boost-asio-async-write-how-to-not-interleaving-async-write-calls
		{
			std::lock_guard<std::mutex> lg(iSLock);

			if (0 == socketErr) {

				inQueue.push(inChunk);
			
				if (!inputRunning) {
					
					//when we got the lock the input wasnt running, so we need to start the read operation
					inputRunning = true;

					try
					{
						(engineOwner->getIOService())->post([this]() mutable { readAction(); });
					}
					catch (...)
					{
						int noErr = 0;
						int eVal = 42;
						socketErr.compare_exchange_strong(noErr, eVal);
						inputRunning = false;
						return -2;
					}
					return 2;
				}
				else {
					//someone else is taking care of it
					return 1;
				}
			}
			else {
				return -2;
			}
		}
		return 0;
	}

	//streams

	int64_t lvTlsSocketBase::getInputStreamSize()
	{
		return inputStreamBuffer.size();
	}

	int64_t lvTlsSocketBase::inputStreamReadSome(char* buffer, int64_t len)
	{
		return inputStream.readsome(buffer, len);
	}

	int64_t lvTlsSocketBase::inputStreamReadN(char * buffer, int64_t len)
	{
		inputStream.read(buffer, len);
		if (inputStream.fail() || inputStream.bad() || inputStream.eof()) {
			inputStream.clear();
			return inputStream.gcount();
		}
		else {
			return len;
		}
	}

	int64_t lvTlsSocketBase::inputStreamGetLine(char * buffer, int64_t len, char delimiter)
	{
		inputStream.getline(buffer, len, delimiter);
		if (inputStream.fail() || inputStream.bad() || inputStream.eof()) {
			inputStream.clear();
			return inputStream.gcount();
		}
		else {
			return len;
		}
	}

	boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& lvTlsSocketBase::getStream()
	{
		return socket;
	}

	int lvTlsSocketBase::getError()
	{
		int err = socketErr;
		return err;
	}
}
