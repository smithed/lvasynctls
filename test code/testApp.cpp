// testApp.cpp : Defines the entry point for the console application.
//


#include <stdio.h>
#include <algorithm>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <vector>
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
#include <chrono>
#include <lvAsyncTls.hpp>

using namespace boost::asio;
using namespace lvasynctls;

class testCB : public lvasynctls::lvTlsDataReadCallback {
public:
	std::string pval;
	boost::lockfree::spsc_queue<std::string> * queue;
	
	testCB(std::string printer, boost::lockfree::spsc_queue<std::string> * q);
	void execute();
	virtual void setErrorCondition(const boost::system::error_code& error) override;
};


void testCB::execute()
{
	queue->push(pval);
	if (mem) {
		std::string pd{ mem->begin(), mem->end() };
		queue->push(pd);
	}
	else {
		queue->push("");
	}
}

testCB::testCB(std::string printer, boost::lockfree::spsc_queue<std::string> * q) :pval(printer), queue{ q }
{
	return;
}

void testCB::setErrorCondition(const boost::system::error_code & error)
{
	if (mem) {
		delete mem;
		mem = nullptr;
	}
	auto em = error.message();
	mem = new std::vector<uint8_t>(em.length() + 1);
	std::copy(em.begin(), em.end(), std::back_inserter(*mem));
}


class testConnCB : public lvasynctls::lvTlsNewConnectionCallback {
public:
	std::string pval;
	boost::lockfree::spsc_queue<lvTlsSocketBase*> * queue;

	testConnCB(std::string printer, boost::lockfree::spsc_queue<lvTlsSocketBase*> * q);
	void execute();
	virtual void setErrorCondition(const boost::system::error_code& error) override;
};

testConnCB::testConnCB(std::string printer, boost::lockfree::spsc_queue<lvTlsSocketBase*> * q) :pval(printer), queue{ q }
{
	return;
}

void testConnCB::execute()
{
	std::cout << pval << "\n";
	queue->push(s);
}

void testConnCB::setErrorCondition(const boost::system::error_code & error)
{
	if (error) {
		std::cout << "error occurred: " << error.value() << "\n" << "error message: " << error.message() << "\n";
	}
}


int main(int argc, char* argv[])
{
	std::cout << "hello\n";
	auto engine = new lvasynctls::lvAsyncEngine();
	std::cout << "new IO engine\n";
	auto client = new lvasynctls::lvTlsClientConnector(engine);
	std::cout << "new client\n";	

	auto hostName ="www.google.com";
	auto port = "443";
	auto cq = new boost::lockfree::spsc_queue<lvTlsSocketBase*>(5);
	client->resolveAndConnect(hostName, port, new testConnCB("connected", cq));
	
	lvTlsSocketBase* conn;
	while (!(cq->pop(conn))) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	if (conn) {
		auto q = new boost::lockfree::spsc_queue<std::string>(50);
		testCB * cb;
		std::string s;

		std::string req = "GET / HTTP/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n\0";
		auto reqdata = new std::vector<unsigned char>(req.begin(), req.end());
		cb = new testCB("sending", q);
		conn->startWrite(reqdata, cb);

		while (!(q->pop(s))) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << s << "\n";
		while (!(q->pop(s))) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << s << "\n";



		cb = new testCB("read done", q);
		conn->startDirectRead(2000, cb);

		while (!(q->pop(s))) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << s << "\n";
		while (!(q->pop(s))) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << s << "\n";
		try {
			delete conn;
		}
		catch (...) {

		}
	}
	else {
		std::cout << "conn failed\n";
	}
	
	
	try {
		delete client;
	}
	catch (const std::exception&)
	{
		std::cout << "exception during shutdown \n";
	}
	std::cout << "dispose client\n";

	
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

