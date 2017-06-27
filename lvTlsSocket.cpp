#include "lvTlsSocket.h"
#include "lvTlsCallbackBase.h"
#include <vector>

#ifndef LVSOCKETMAXREADSIZE
#define LVSOCKETMAXREADSIZE 102400
#endif

using namespace boost::asio;

namespace lvasynctls {

	lvTlsSocketBase::lvTlsSocketBase(lvAsyncEngine* engineContext, boost::asio::ssl::context& sslContext) :
		engineOwner(engineContext),
		socketStrand(*(engineContext->getIOService())),
		socket(*(engineContext->getIOService()), sslContext),
		inputStreamBuffer{ LVSOCKETMAXREADSIZE },
		inputStream{ &inputStreamBuffer }
	{
		if (engineContext) {
			engineContext->registerSocket(this);
		}
	}

	lvTlsSocketBase::~lvTlsSocketBase()
	{
		try {
			if (engineOwner) {
				engineOwner->unregisterSocket(this);
			}
		}
		catch (...) {

		}
		try {
			socket.lowest_layer().cancel();
		}
		catch (...) {

		}
		try {
			socket.shutdown();
		}
		catch (...) {

		}

		return;
	}

	//take ownership of data buffer and of callback, we will dispose
	void lvTlsSocketBase::startWrite(std::vector<unsigned char> * data, lvasynctls::lvTlsDataReadCallback * callback)
	{
		auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBWriteComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, data, callback));
		async_write(socket, boost::asio::buffer(*data), func);
	}

	//take ownership of callback, we will dispose
	void lvTlsSocketBase::startDirectRead(size_t len, lvasynctls::lvTlsDataReadCallback* callback)
	{
		if (callback) {
			auto mem = new std::vector<unsigned char>(len);
			callback->giveDataResult(mem);
			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read(socket, boost::asio::buffer(*mem), transfer_all(), func);
		}
	}

	void lvTlsSocketBase::startDirectReadSome(size_t len, lvasynctls::lvTlsDataReadCallback * callback)
	{
		if (callback) {
			auto mem = new std::vector<unsigned char>(len);
			callback->giveDataResult(mem);
			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			socket.async_read_some(boost::asio::buffer(*mem), func);
		}
	}

	void lvTlsSocketBase::startDirectReadAtLeast(size_t min, size_t max, lvasynctls::lvTlsDataReadCallback * callback)
	{
		if (callback) {
			auto mem = new std::vector<unsigned char>(max);
			callback->giveDataResult(mem);
			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read(socket, boost::asio::buffer(*mem), transfer_at_least(min), func);
		}
	}


	int32_t lvTlsSocketBase::startStreamReadUntilTermChar(unsigned char term, size_t max, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			if (max > LVSOCKETMAXREADSIZE) {
				return -1;
			}
			if (max <= inputStreamBuffer.size()) {
				callback->setDataSize(max);
				callback->execute();
				return 0;
			}

			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read_until(socket, inputStreamBuffer, term, func);
		}
		return -2;
	}

	int32_t lvTlsSocketBase::startStreamReadUntilTermString(std::string term, size_t max, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			if (max > LVSOCKETMAXREADSIZE) {
				return -1;
			}
			if (max <= inputStreamBuffer.size()) {
				callback->setDataSize(max);
				callback->execute();
				return 0;
			}

			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read_until(socket, inputStreamBuffer, term, func);
		}
		return -2;
	}

	int32_t lvTlsSocketBase::startStreamRead(size_t len, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			if (len > LVSOCKETMAXREADSIZE) {
				return -1;
			}
			if (len <= inputStreamBuffer.size()) {
				callback->setDataSize(len);
				callback->execute();
				return 0;
			}

			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read(socket, inputStreamBuffer, transfer_exactly(len - inputStreamBuffer.size()), func);
		}
		return -2;
	}


	int32_t lvTlsSocketBase::startStreamReadAtLeast(size_t min, size_t max, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			if (min > LVSOCKETMAXREADSIZE) {
				return -1;
			}
			if (min <= inputStreamBuffer.size()) {
				callback->setDataSize(min);
				callback->execute();
				return 0;
			}

			auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBReadStreamComplete, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, callback));
			async_read(socket, inputStreamBuffer, transfer_at_least(min - inputStreamBuffer.size()), func);
		}
		return -2;
	}

	void lvTlsSocketBase::startHandshake(lvasynctls::lvTlsNewConnectionCallback * callback, boost::asio::ssl::stream_base::handshake_type handType)
	{
		auto func = socketStrand.wrap(boost::bind(&lvTlsSocketBase::CBHandShakeComplete, this, boost::asio::placeholders::error, callback));
		socket.async_handshake(handType, func);
	}

	size_t lvTlsSocketBase::getInputStreamSize()
	{
		return inputStreamBuffer.size();
	}

	int64_t lvTlsSocketBase::inputStreamReadSome(char* buffer, int64_t len)
	{
		return inputStream.readsome(buffer, len);
	}

	boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& lvTlsSocketBase::getStream()
	{
		return socket;
	}


	void lvTlsSocketBase::CBWriteComplete(const boost::system::error_code & error, std::size_t bytes_transferred, std::vector<unsigned char> * data, lvasynctls::lvTlsDataReadCallback* callback)
	{
		//if this callback runs data has been transmitted, so free it
		if (data) {
			delete data;
			data = nullptr;
		}

		if (callback) {
			errorCheck(error, callback);
			callback->execute();

			delete callback;
			callback = nullptr;
		}
	}

	void lvTlsSocketBase::CBReadComplete(const boost::system::error_code & error, std::size_t bytes_transferred, lvasynctls::lvTlsDataReadCallback* callback)
	{
		if (callback) {
			callback->resizeDataResult(bytes_transferred);

			errorCheck(error, callback);

			//because we gave data ptr to callback earlier, we don't have to do anything else
			callback->execute();

			//also frees data ptr
			delete callback;
			callback = nullptr;
		}
	}

	void lvTlsSocketBase::CBReadStreamComplete(const boost::system::error_code & error, std::size_t bytes_transferred, lvasynctls::lvTlsCompletionCallback * callback)
	{
		if (callback) {
			callback->setDataSize(bytes_transferred);

			errorCheck(error, callback);

			callback->execute();

			//also frees data ptr
			delete callback;
			callback = nullptr;
		}
	}

	void lvTlsSocketBase::CBHandShakeComplete(const boost::system::error_code & error, lvasynctls::lvTlsNewConnectionCallback* callback)
	{
		if (callback) {
			if (!error) {
				callback->cacheSocket(this);
			}

			errorCheck(error, callback);
			callback->execute();

			delete callback;
			callback = nullptr;
			if (error) {
				delete this;
			}
		}
	}

}

