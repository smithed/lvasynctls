// testApp.cpp : Defines the entry point for the console application.
//


#include <stdio.h>
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <streambuf>
#include <thread>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <chrono>
#include <lvAsyncTls.hpp>




using namespace boost::asio;
using namespace lvasynctls;

class testCB : public lvasynctls::lvTlsDataReadCallback {
public:
	std::string pval;
	boost::lockfree::spsc_queue<std::string> * queue;

	testCB(std::string printer, boost::lockfree::spsc_queue<std::string> * q) :pval(printer), queue{ q }
	{
		return;
	}
	void execute() {
		queue->push(pval);
		if (mem) {
			std::string pd{ mem->begin(), mem->end() };
			queue->push(pd);
		}
		else {
			queue->push("");
		}
	}
	virtual void setErrorCondition(const boost::system::error_code& error) override {
		if (mem) {
			delete mem;
			mem = nullptr;
		}
		auto em = error.message();
		mem = new std::vector<uint8_t>(em.length() + 1);
		std::copy(em.begin(), em.end(), std::back_inserter(*mem));
	}
};

class testCompleteCB : public lvasynctls::lvTlsCompletionCallback {
public:
	boost::lockfree::spsc_queue<size_t> * queue;
	std::string pval;

	testCompleteCB(boost::lockfree::spsc_queue<size_t> * q, std::string msg) : queue{ q }, pval{ msg } {
		return;
	}
	~testCompleteCB() {
		return;
	}

	void execute() {
		std::cout << pval << "\n";
		queue->push(dataLength);
		return;
	}

	virtual void setErrorCondition(const boost::system::error_code& error) override {
		std::cout << "error condition" << error.value() << "\n";
	}

};

class testConnCB : public lvasynctls::lvTlsNewConnectionCallback {
public:
	std::string pval;
	boost::lockfree::spsc_queue<lvTlsSocketBase*> * queue;

	testConnCB(std::string printer, boost::lockfree::spsc_queue<lvTlsSocketBase*> * q) :pval(printer), queue{ q }
	{
		return;
	}
	void execute() {
		std::cout << pval << "\n";
		queue->push(s);
	}
	virtual void setErrorCondition(const boost::system::error_code& error) override {
		if (error) {
			std::string err = error.message();
			if (error.category() == boost::asio::error::get_ssl_category()) {
				err = std::string(" (")
					+ boost::lexical_cast<std::string>(ERR_GET_LIB(error.value())) + ","
					+ boost::lexical_cast<std::string>(ERR_GET_FUNC(error.value())) + ","
					+ boost::lexical_cast<std::string>(ERR_GET_REASON(error.value())) + ") "
					;
				//ERR_PACK /* crypto/err/err.h */
				char buf[128];
				::ERR_error_string_n(error.value(), buf, sizeof(buf));
				err += buf;
			}

			std::cout << "error occurred: " << error.value() << "\n" << "error message: " << err << "\n";
		}
	}
};


int main(int argc, char* argv[])
{
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	PKCS5_PBE_add();

	std::cout << "hello\n";
	auto engine = new lvasynctls::lvAsyncEngine();
	std::cout << "new IO engine\n";


	auto sAcceptor = new lvasynctls::lvTlsServerAcceptor(engine, 5555);
	std::cout << "new acceptor\n";

	sAcceptor->ctx.set_options(
		boost::asio::ssl::context::default_workarounds
		| boost::asio::ssl::context::no_sslv2
		| boost::asio::ssl::context::single_dh_use);
	lvasynctls::lvTlsConnectionCreator * cc = (lvasynctls::lvTlsConnectionCreator *) sAcceptor;
	cc->enablePasswordCallback("test");
	sAcceptor->ctx.use_certificate_chain_file("server.pem");



	//std::ifstream t("server.pem");
	//std::string str;

	//t.seekg(0, std::ios::end);
	//str.reserve(t.tellg());
	//t.seekg(0, std::ios::beg);

	//str.assign((std::istreambuf_iterator<char>(t)),	std::istreambuf_iterator<char>());
	//t.close();
	//sAcceptor->ctx.use_private_key(buffer(str), boost::asio::ssl::context::pem);
	sAcceptor->ctx.use_private_key_file("server.pem", boost::asio::ssl::context::pem);
	sAcceptor->ctx.use_tmp_dh_file("dh2048.pem");
	std::cout << "config ssl context\n";


	auto cq = new boost::lockfree::spsc_queue<lvTlsSocketBase*>(5);

	sAcceptor->startAccept(new testConnCB("connected", cq));

	lvTlsSocketBase* conn;
	while (!(cq->pop(conn))) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	if (conn) {
		auto q = new boost::lockfree::spsc_queue<std::string>(50);
		auto lq = new boost::lockfree::spsc_queue<size_t>(50);
		std::string s;
		size_t readlen;
		
		conn->startStreamReadUntilTermChar('\n', 10240, new testCompleteCB(lq, "read done"));

		while (!(lq->pop(readlen))) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << readlen << " read from ssl\n";

		char * buffer = new char[readlen];

		auto ret = conn->inputStreamReadSome(buffer, readlen);
		std::cout << ret << " read from internal stream\n";

		//conn->startDirectRead(10, new testCB("read", q));

		//for (size_t i = 0; i < 2; i++)
		//{
		//	while (!(q->pop(s))) {
		//		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		//	}
		//	std::cout << s << "\n";
		//}
		std::cout << "Buffer:\n";
		for (int i = 0; i < ret; i++) {
			std::cout << buffer[i];
		}
		std::cout << "\nEoBuffer\n";

		//auto tx = new std::vector<uint8_t>(s.begin(), s.end());
		auto tx = new std::vector<uint8_t>(buffer, buffer + ret);
		conn->startWrite(tx, new testCB("echo", q));

		for (size_t i = 0; i < 2; i++)
		{
			while (!(q->pop(s))) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			std::cout << s << "\n";
		}

		delete[] buffer;
		delete conn;
	}
	else {
		std::cout << "conn failed\n";
	}


	try {
		delete sAcceptor;
	}
	catch (const std::exception&)
	{
		std::cout << "exception during shutdown \n";
	}
	std::cout << "dispose acceptor\n";


	engine->softShutdown();

	try {
		delete engine;
	}
	catch (const std::exception&)
	{
		std::cout << "exception during shutdown \n";
	}
	std::cout << "dispose eng\n";


	int blah;
	std::cin >> blah;

	return 0;
}

