// testApp.cpp : Defines the entry point for the console application.
//

#include <lvTlsCallbackBase.h>
#include <lvTlsEngine.h>
#include <lvTlsSocket.h>
#include <lvTlsConnectionCreator.h>


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





using namespace boost::asio;
using namespace lvasynctls;

class testCB : public lvasynctls::lvTlsCallback {
public:
	std::string pval;
	boost::lockfree::spsc_queue<std::string> * queue;
	boost::lockfree::spsc_queue<int> * errQ;

	testCB(std::string printer, boost::lockfree::spsc_queue<std::string> * q = nullptr, boost::lockfree::spsc_queue<int> *eq = nullptr) :
		pval(printer), queue{ q }, errQ{ eq }, ec{ 0 }, emsg {""}
	{
		return;
	}

	void execute() {
		if (queue) {
			queue->push(pval);
		}
		if (ec && errQ) {
			errQ->push(ec);
		}
	}

	void printError() {
		if (ec) {
			std::cout << "error occurred: " << ec << "\n" << "error message: " << emsg << "\n";
		}
		else {
			std::cout << "no error but callback called anyway\n";
		}
	}

	virtual void setErrorCondition(int code, std::string message) override {
		ec = code;
		emsg = message + std::string(" (")
			+ boost::lexical_cast<std::string>(ERR_GET_LIB(code)) + ","
			+ boost::lexical_cast<std::string>(ERR_GET_FUNC(code)) + ","
			+ boost::lexical_cast<std::string>(ERR_GET_REASON(code)) + ") "
			;
		printError();
	}

private:
	std::string emsg;
	int ec;
};


int main(int argc, char* argv[])
{
	auto q = new boost::lockfree::spsc_queue<std::string>(500);
	auto eq = new boost::lockfree::spsc_queue<int>(500);
	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 5555);
	std::string s;


	std::shared_ptr<lvAsyncEngine> engine { new lvasynctls::lvAsyncEngine(2) };
	
	std::shared_ptr<lvTlsServerAcceptor> sAcceptor{ new lvasynctls::lvTlsServerAcceptor(engine, endpoint, 2) };
	engine->registerConnector(sAcceptor);
	std::cout << "new acceptor\n";	

	sAcceptor->ctx.set_options(
		boost::asio::ssl::context::default_workarounds
		| boost::asio::ssl::context::no_sslv2
		| boost::asio::ssl::context::single_dh_use);
	sAcceptor->enablePasswordCallback("test");
	sAcceptor->ctx.use_certificate_chain_file("server.pem");
	sAcceptor->ctx.use_private_key_file("server.pem", boost::asio::ssl::context::pem);
	sAcceptor->ctx.use_tmp_dh_file("dh2048.pem");
	std::cout << "config ssl context\n";
	
	sAcceptor->startAccept(100000, new testCB("accepted\n", q));
	sAcceptor->startAccept(100000, new testCB("accepted\n", q));
	sAcceptor->startAccept(100000, new testCB("accepted\n", q));
	sAcceptor->startAccept(100000, new testCB("accepted\n", q));

	auto t = new boost::thread{ 
	[&engine, &sAcceptor]() mutable 
		{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		sAcceptor->shutdown();
		sAcceptor.reset();
		engine->shutdown();
		engine.reset();
		}
	};

	t->join();

	

	//while (!(q->pop(s))) {
	//	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//}

	//std::cout << s;

	//auto conn = sAcceptor->getNextConnection();

	//if (conn) {		
	//	int err = 0;

	//	//while (true) {
	//	//	for (size_t i = 0; i < 26; i++)
	//	//	{
	//	//		auto vec = new std::vector<unsigned char>(100000, 97 + i);
	//	//		auto cb = new testCB("responded\n", q, eq);
	//	//		auto ret = conn->startWrite(vec, cb);
	//	//		if (ret < 1) {
	//	//			//didnt enqueue
	//	//			delete vec;
	//	//			delete cb;
	//	//			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	//	//		}
	//	//		if (eq->pop(err)) {
	//	//			std::cout << "err: " << err << "\n";
	//	//			break;
	//	//		}
	//	//	}

	//	//	if(err){
	//	//		std::cout << "err: " << err << "\n";
	//	//		break;
	//	//	}
	//	//}

	//	

	//	std::cout << "starting read\n";
	//	conn->startStreamReadUntilTermString("\n", 10240, new testCB("read done", q));

	//	while (!(q->pop(s))) {
	//		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//	}
	//	std::cout << s << "\n";

	//	auto bytesRead = conn->getInputStreamSize();

	//	std::cout << "bytes available: " << bytesRead << "\n";

	//	auto readBuf = new char[bytesRead + 1];

	//	auto actualSize = conn->inputStreamReadN(readBuf, bytesRead);
	//	std::cout << "bytes read: " << actualSize << "\n";

	//	readBuf[actualSize] = 0;
	//	std::cout << "data:\n" << readBuf << "\nfin\n";

	//	auto outVec = new std::vector<unsigned char>{ readBuf, readBuf + actualSize };

	//	
	//	conn->startWrite(outVec, new testCB("responded", q));


	//	while (!(q->pop(s))) {
	//		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//	}
	//	std::cout << s << "\n";

	//	delete[] readBuf;

	//	conn->shutdown();
	//	conn.reset();
	//}
	//else {
	//	std::cout << "conn failed\n";
	//}

	//sAcceptor->shutdown();
	//sAcceptor.reset();
	//std::cout << "dispose acceptor\n";

	//engine->shutdown();
	//engine.reset();
	//std::cout << "dispose eng\n";

	delete q;
	int blah;
	std::cin >> blah;

	return 0;
}

