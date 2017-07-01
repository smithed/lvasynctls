#include "lvTlsSocket.h"
#include "lvTlsCallbackBase.h"
#include <vector>
#include <thread>
#include <chrono>
#include <istream>
#include <ostream>
#include <boost/lockfree/spsc_queue.hpp>


using namespace boost::asio;

namespace lvasynctls {

	class transferMinOrFull
	{
	public:
		explicit transferMinOrFull(size_t minimum, boost::asio::streambuf & s)
			: min(minimum), buf{ s }
		{
		}

		std::size_t operator()(const boost::system::error_code & err, std::size_t bytes_transferred)
		{
			if (err || (bytes_transferred > min) || buf.size() == buf.max_size()) {
				return 0;
			}
			else {
				auto remain = min - bytes_transferred;
				auto space = buf.max_size() - buf.size();
				return ((remain > space) ? space : remain);
			}
			return 0;
		}

	private:
		size_t min;
		boost::asio::streambuf & buf;
	};

	lvTlsSocketBase::lvTlsSocketBase(lvAsyncEngine* engineContext, boost::asio::ssl::context& sslContext, size_t streamSize) :
		engineOwner{ engineContext },
		socketStrand{ *(engineContext->getIOService()) },
		socket{ *(engineContext->getIOService()), sslContext },
		inputStreamBuffer{ streamSize },
		outputStreamBuffer{ streamSize }, 
		inputStream{ &inputStreamBuffer },
		outputStream{ &outputStreamBuffer }
	{
		if (engineContext) {
			engineContext->registerSocket(this);
		}
	}

	lvTlsSocketBase::~lvTlsSocketBase()
	{
		//we are somewhat protected by the mutex in lvland, but this seems to be whats needed to avoid crashes and (more commonly) hangs
		//https://stackoverflow.com/questions/32046034/what-is-the-proper-way-to-securely-disconnect-an-asio-ssl-socket
		//https://stackoverflow.com/questions/22575315/how-to-gracefully-shutdown-a-boost-asio-ssl-client
		//https://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error/25703699#25703699

		boost::system::error_code ec;
		try {
			//cancel existing operations
			socket.lowest_layer().cancel();
			socket.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_receive);
		}
		catch (...) {
		}

		try {
			boost::lockfree::spsc_queue<int> q(1);

			//start shutdown process
			socket.async_shutdown(socketStrand.wrap(
				[&q](const boost::system::error_code& ec) {
					//we want this shutdown process to still be synchronous, we just want to force it a bit
					//the queue lets us keep things synchronous
					q.push(ec.value());
				}
			));

			const char buffer[] = "";
			boost::system::error_code wec;
			//this write should immediately fail, allowing shutdown to complete
			async_write(socket, boost::asio::buffer(buffer), socketStrand.wrap(
				[](...) {
					return;
				}
			));

			//wait for shutdown to be complete
			while (!q.pop()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		}
		catch (...) {
		}

		try {
			socket.lowest_layer().close();
		}
		catch (...) {
		}

		try {
			if (engineOwner) {
				engineOwner->unregisterSocket(this);
			}
		}
		catch (...) {
		}

		return;
	}

	// WRITE

	//take ownership of data buffer and of callback, we will dispose
	int64_t lvTlsSocketBase::startWrite(char * buffer, int64_t len, lvasynctls::lvTlsCallback * callback)
	{
		auto sizeavailable = outputStreamBuffer.max_size() - outputStreamBuffer.size();
		auto writesize = (sizeavailable > len) ? len : sizeavailable;

		//apparently, insane as it may seem, there is no way to see how much was written?
		for (int i = 0; i < writesize; i++) {
			outputStream.put(buffer[i]);
			if (outputStream.fail() || outputStream.bad() || outputStream.eof()) {
				outputStream.clear();
				writesize = i;
				break;
			}
		}
		
		auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBWriteComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
		async_write(socket, outputStreamBuffer, transfer_at_least(writesize), func);

		return writesize;
	}

	//callback for completion of async write
	void lvTlsSocketBase::CBWriteComplete(const boost::system::error_code & error, std::size_t bytes_transferred, lvasynctls::lvTlsCallback* callback)
	{
		if (callback) {
			errorCheck(error, callback);
			callback->execute();

			delete callback;
			callback = nullptr;
		}
	}

	// READ
		
	void lvTlsSocketBase::startStreamReadUntilTermChar(unsigned char term, size_t max, lvasynctls::lvTlsCallback * callback)
	{
		auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
		async_read_until(socket, inputStreamBuffer, term, func);
	}

	void lvTlsSocketBase::startStreamReadUntilTermString(std::string term, size_t max, lvasynctls::lvTlsCallback * callback)
	{
		auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
		async_read_until(socket, inputStreamBuffer, term, func);
	}

	void lvTlsSocketBase::startStreamRead(size_t len, lvasynctls::lvTlsCallback * callback)
	{
		auto maxRead = inputStreamBuffer.max_size() - inputStreamBuffer.size();
		auto readsize = (len > maxRead) ? maxRead : len;

		auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
		async_read(socket, inputStreamBuffer, transferMinOrFull(readsize, inputStreamBuffer), func);
	}

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

	void lvTlsSocketBase::CBReadStreamComplete(const boost::system::error_code & error, std::size_t bytes_transferred, lvasynctls::lvTlsCallback * callback)
	{
		if (callback) {
			errorCheck(error, callback);

			callback->execute();

			delete callback;
			callback = nullptr;
		}
	}

	//synchronous, dont mix with async
	size_t lvTlsSocketBase::Write(std::vector<unsigned char> & data, boost::system::error_code & err)
	{
		return write(socket, boost::asio::buffer(data), err);
	}

	size_t lvTlsSocketBase::Read(std::vector<unsigned char> & data, boost::system::error_code & err)
	{
		return read(socket, boost::asio::buffer(data), err);
	}

	size_t lvTlsSocketBase::ReadUntil(const std::string & term, boost::system::error_code & err)
	{
		return read_until(socket, inputStreamBuffer, term, err);
	}
}
