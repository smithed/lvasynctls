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


using namespace boost::asio;
using namespace lvasynctls;

class testCB : public lvasynctls::lvTlsCallback {
public:
	std::string pval;
	boost::lockfree::spsc_queue<std::string> * queue;
	
	testCB(std::string printer, boost::lockfree::spsc_queue<std::string> * q) :pval(printer), queue{ q }
	{
		return;
	}

	void execute() {
		queue->push(pval);
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
		emsg = message;
		printError();
	}

private:
	std::string emsg;
	int ec;
};


int main(int argc, char* argv[])
{
	std::cout << "hello\n";
	std::shared_ptr<lvAsyncEngine> engine { new lvasynctls::lvAsyncEngine() };
	std::cout << "new IO engine\n";
	std::shared_ptr<lvTlsClientConnector> client { new lvasynctls::lvTlsClientConnector(engine, 2) };
	engine->registerConnector(client);
	std::cout << "new client\n";	

	auto hostName ="www.google.com";
	auto port = "443";
	auto q = new boost::lockfree::spsc_queue<std::string>(50);

	std::string s;

	client->resolveAndConnect(hostName, port, 5000000, new testCB("connected", q));
	
	while (!(q->pop(s))) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::cout << s;

	auto conn = client->getNextConnection();

	if (conn.get()) {

		std::string req = "GET / HTTP/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n\0";
		auto vdata = new std::vector<unsigned char>{ req.begin(), req.end() };
		std::cout << "starting write\n";
		conn->startWrite(vdata, new testCB("sent", q));

		while (!(q->pop(s))) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << s << "\n";


		conn->startStreamReadUntilTermString("\r\n\r\n", 100000, new testCB("read done", q));

		while (!(q->pop(s))) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << s << "\n";

		auto bytesRead = conn->getInputStreamSize();
		
		std::cout << "bytes available: "<< bytesRead << "\n";

		auto readBuf = new char[bytesRead + 1];

		auto actualSize = conn->inputStreamReadN(readBuf, bytesRead);
		std::cout << "bytes read: " << actualSize << "\n";

		readBuf[actualSize] = 0;

		std::cout << "data:\n" << readBuf << "\nfin\n";

		delete[] readBuf;
		conn->shutdown();
		conn.reset();
	}
	else {
		std::cout << "conn failed\n";
	}
	
	client->shutdown();
	client.reset();
	std::cout << "dispose client\n";

	engine->shutdown();
	engine.reset();
	std::cout << "dispose eng\n";


	int blah;
	std::cin >> blah;

	return 0;
}

