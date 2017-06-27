
//#include <stdio.h>
//#include <algorithm>
//#include <memory>
//#include <cstdlib>
//#include <iostream>
//#include <vector>
//#include <thread>
//#include <unordered_set>
//#include <mutex>
//#include <boost/bind.hpp>
//#include <boost/ref.hpp>
//#include <boost/asio.hpp>
//#include <boost/asio/ssl.hpp>
//#include <boost/thread.hpp>
//#include <boost/date_time/posix_time/posix_time.hpp>
//#include <boost/asio/ip/tcp.hpp>
//#include <boost/lockfree/queue.hpp>
//#include "lvAsyncTls.hpp"


#include "lvasyncapi.h"
#include <extcode.h>
#include <boost/asio.hpp>
#include <unordered_set>
#include <boost/assert.hpp>

#ifndef lvExport
#define lvExport __declspec(dllexport)
#endif

LStrHandle LHdlFromCstr(char * cstr);
LStrHandle LHdlFromCstr(char * cstr) {
	auto len = std::strlen(cstr);
	auto lstr = (LStrHandle)DSNewHandle(len + sizeof(int32));
	LStrLen(LHStrPtr(lstr)) = len;
	MoveBlock(cstr, LHStrBuf(lstr), len);
	return lstr;
}

std::string LstrToStdStr(LStrHandle s);
std::string LstrToStdStr(LStrHandle s) {
	return std::string((char*)LHStrBuf(s), LHStrLen(s));
}

std::vector<uint8_t> LStrToVector(LStrHandle s);
std::vector<uint8_t> LStrToVector(LStrHandle s) {
	std::vector<uint8_t> v(LHStrBuf(s), LHStrLen(s)+LHStrBuf(s));
	return v;
}

static std::unordered_set<lvasynctls::lvAsyncEngine*> engineSet{};
static std::mutex engineSetLock{};

#ifdef __cplusplus
extern "C" {
#endif
	//abort handlers
	lvExport MgErr lvTlsIOengine_reserve(InstanceDataPtr * instance)
	{
		return mgNoErr;
	}

	void clearInstanceEngineSet();

	//todo: error
	lvExport MgErr lvTlsIOengine_unreserve(InstanceDataPtr * instance)
	{
		try {
			clearInstanceEngineSet();
		}
		catch (...) {
			return mgArgErr;
		}

		return mgNoErr;
	}

	lvExport MgErr lvTlsIOengine_abort(InstanceDataPtr * instance)
	{
		try {
			clearInstanceEngineSet();
		}
		catch (...) {
			return mgArgErr;
		}

		return mgNoErr;
	}

	void clearInstanceEngineSet() 
	{
		std::lock_guard<std::mutex> lockg(engineSetLock);
		try {
			for (auto itr = engineSet.begin(); itr != engineSet.end(); ++itr) {
				try {
					auto engPtr = *itr;
					if (engPtr) {
						delete engPtr;
					}
				}
				catch (...) {
				}
			}
		}
		catch (...)
		{

		}
		try {
			engineSet.clear();
		}
		catch (...)
		{

		}	
	}
	

	void lvTlsIOengine_recordInstance(lvasynctls::lvAsyncEngine * engine)
	{
		try {
			std::lock_guard<std::mutex> lockg(engineSetLock);
			if (engine) {
				engineSet.insert(engine);
			}
		}
		catch (...)
		{
			//ignore
		}
	}

	void lvTlsIOengine_removeInstance(lvasynctls::lvAsyncEngine * engine)
	{
		try {
			std::lock_guard<std::mutex> lockg(engineSetLock);
			if (engine) {
				engineSet.erase(engine);
			}
		}
		catch (...)
		{
			//ignore
		}
	}


	//engine
	lvExport lvasynctls::lvAsyncEngine * lvtlsCreateIOEngine()
	{
		lvasynctls::lvAsyncEngine * engine;
		try {
			engine = new lvasynctls::lvAsyncEngine();
			lvTlsIOengine_recordInstance(engine);
			return engine;
		}
		catch (...) {
			if (engine) {
				delete engine;
				engine = nullptr;
			}
			return nullptr;
		}	
	}

	lvExport MgErr lvtlsDisposeIOEngine(lvasynctls::lvAsyncEngine * engine)
	{
		try {
			if (engine) {
				engine->softShutdown();
				lvTlsIOengine_removeInstance(engine);
				delete engine;
				engine = nullptr;
				return mgNoErr;
			}
			else {
				return oleNullRefnumPassed;
			}
		}
		catch (...) {
			return 42;
		}
	}


	//client
	lvExport lvasynctls::lvTlsClientConnector * lvtlsCreateClientConnector(lvasynctls::lvAsyncEngine * engine)
	{
		if (engine) {
			lvasynctls::lvTlsClientConnector * c;
			try {
				c = new lvasynctls::lvTlsClientConnector(engine);
				return c;
			}
			catch (...) {
				if (c) {
					delete c;
					c = nullptr;
				}
				return nullptr;
			}
		}
		else {
			return nullptr;
		}
	}

	lvExport MgErr lvtlsDisposeClientConnector(lvasynctls::lvTlsClientConnector * client)
	{
		try {
			if (client) {
				delete client;
				client = nullptr;
				return mgNoErr;
			}
			else {
				return oleNullRefnumPassed;
			}
		}
		catch (...) {
			return 42;
		}
	}

	lvExport MgErr lvtlsBeginClientConnect(lvasynctls::lvTlsClientConnector * client, 
		LStrHandle host, LStrHandle port, LVUserEventRef * e, uint32_t requestID)
	{
		if (client && e != nullptr && *e != kNotAMagicCookie && LHStrPtr(host) && LHStrPtr(port) && LHStrBuf(host) && LHStrBuf(port)) {
			lvasyncapi::LVConnectionNotificationCallback * cb;
			try {
				auto cb = new lvasyncapi::LVConnectionNotificationCallback(e, lvasyncapi::lvRequestType::ClientConnect, requestID);
				client->resolveAndConnect(LstrToStdStr(host), LstrToStdStr(port), cb);
				return mgNoErr;
			}
			catch (...) {
				if (cb) {
					delete cb;
					cb = nullptr;
				}
				return 42;
			}
		}
		else {
			if (client) {
				return axEventQueueNotCreated;
			}
			else {
				return oleNullRefnumPassed;
			}
		}
	}

	//server
	lvExport lvasynctls::lvTlsServerAcceptor * lvtlsCreateServerAcceptor(lvasynctls::lvAsyncEngine * engine, uint16_t port)
	{
		if (engine) {
			lvasynctls::lvTlsServerAcceptor * a;
			try {
				a = new lvasynctls::lvTlsServerAcceptor(engine, port);
				return a;
			}
			catch (...) {
				if (a) {
					delete a;
					a = nullptr;
				}
				return nullptr;
			}
		}
		else {
			return nullptr;
		}
	}

	lvExport MgErr lvtlsDisposeServerAcceptor(lvasynctls::lvTlsServerAcceptor * acceptor)
	{
		try {
			if (acceptor) {
				delete acceptor;
				acceptor = nullptr;
				return mgNoErr;
			}
			else {
				return oleNullRefnumPassed;
			}
		}
		catch (...) {
			return 42;
		}
	}

	lvExport MgErr lvtlsBeginServerAccept(lvasynctls::lvTlsServerAcceptor * acceptor, LVUserEventRef * e, uint32_t requestID)
	{
		if (acceptor && e != nullptr && *e != kNotAMagicCookie) {
			lvasyncapi::LVConnectionNotificationCallback * cb;
			try {
				cb = new lvasyncapi::LVConnectionNotificationCallback(e, lvasyncapi::lvRequestType::AcceptConnection, requestID);
				acceptor->startAccept(cb);
				return mgNoErr;
			}
			catch (...) {
				if (cb) {
					delete cb;
					cb = nullptr;
				}
				return 42;
			}
		}
		else {
			if (acceptor) {
				return axEventQueueNotCreated;
			}
			else {
				return oleNullRefnumPassed;
			}
		}
	}

	//ssl context
	lvExport MgErr lvtlsSetOptions(lvasynctls::lvTlsConnectionCreator * creator, long options)
	{
		if (creator) {
			try {
				creator->ctx.set_options(options);
				return mgNoErr;
			}
			catch (...) 
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsSetVerifymode(lvasynctls::lvTlsConnectionCreator* creator, int v)
	{
		if (creator) {
			try {
				creator->ctx.set_verify_mode(v);
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsSetRfc2818Verification(lvasynctls::lvTlsConnectionCreator * creator, const char* host)
	{
		if (creator) {
			try {
				std::string h(host);
				creator->ctx.set_verify_callback(boost::asio::ssl::rfc2818_verification(h));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsLoadVerifyFile(lvasynctls::lvTlsConnectionCreator* creator, const char* filename)
	{
		if (creator) {
			try {
				std::string f(filename);
				creator->ctx.load_verify_file(f);
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsAddCertificateauthority(lvasynctls::lvTlsConnectionCreator* creator, LStrHandle ca)
	{
		if (creator) {
			try {
				creator->ctx.add_certificate_authority(boost::asio::const_buffer(LHStrBuf(ca), LHStrLen(ca)));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsSetDefaultVerifyPaths(lvasynctls::lvTlsConnectionCreator* creator)
	{
		if (creator) {
			try {
				creator->ctx.set_default_verify_paths();
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsAddVerifyPath(lvasynctls::lvTlsConnectionCreator* creator, const char* path)
	{
		if (creator) {
			try {
				std::string p(path);
				creator->ctx.add_verify_path(p);
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUseCertificate(lvasynctls::lvTlsConnectionCreator* creator, LStrHandle certificate, int format)
	{
		if (creator) {
			try {
				creator->ctx.use_certificate(boost::asio::const_buffer(LHStrBuf(certificate), LHStrLen(certificate)), static_cast<boost::asio::ssl::context_base::file_format>(format));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUseCertificateFile(lvasynctls::lvTlsConnectionCreator* creator, const char* filename, int format)
	{
		if (creator) {
			try {
				std::string f(filename);
				creator->ctx.use_certificate_file(f, static_cast<boost::asio::ssl::context_base::file_format>(format));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUseCertificateChain(lvasynctls::lvTlsConnectionCreator* creator, LStrHandle chain)
	{
		if (creator) {
			try {
				creator->ctx.use_certificate_chain(boost::asio::const_buffer(LHStrBuf(chain), LHStrLen(chain)));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUseCertificateChainFile(lvasynctls::lvTlsConnectionCreator* creator, const char*  filename)
	{
		if (creator) {
			try {
				std::string f(filename);
				creator->ctx.use_certificate_chain_file(f);
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUsePrivateKey(lvasynctls::lvTlsConnectionCreator* creator, LStrHandle privateKey, int format)
	{
		if (creator) {
			try {
				creator->ctx.use_private_key(boost::asio::const_buffer(LHStrBuf(privateKey), LHStrLen(privateKey)), static_cast<boost::asio::ssl::context_base::file_format>(format));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUsePrivateKeyFile(lvasynctls::lvTlsConnectionCreator* creator, const char*  filename, int format)
	{
		if (creator) {
			try {
				std::string f(filename);
				creator->ctx.use_private_key_file(f, static_cast<boost::asio::ssl::context_base::file_format>(format));
				return mgNoErr;
			}
			catch (boost::system::system_error e)
			{
				return e.code().value();
			}
			catch (...) 
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUseRsaPrivateKey(lvasynctls::lvTlsConnectionCreator* creator, LStrHandle privateKey, int format)
	{
		if (creator) {
			try {
				creator->ctx.use_rsa_private_key(boost::asio::const_buffer(LHStrBuf(privateKey), LHStrLen(privateKey)), static_cast<boost::asio::ssl::context_base::file_format>(format));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUseRsaPrivateKeyFile(lvasynctls::lvTlsConnectionCreator* creator, const char*  filename, int format)
	{
		if (creator) {
			try {
				std::string f(filename);
				creator->ctx.use_rsa_private_key_file(f, static_cast<boost::asio::ssl::context_base::file_format>(format));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUseTmpDh(lvasynctls::lvTlsConnectionCreator* creator, LStrHandle dh)
	{
		if (creator) {
			try {
				creator->ctx.use_tmp_dh(boost::asio::const_buffer(LHStrBuf(dh), LHStrLen(dh)));
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsUseTmpDhFile(lvasynctls::lvTlsConnectionCreator* creator, const char*  filename)
	{
		if (creator) {
			try {
				std::string f(filename);
				creator->ctx.use_tmp_dh_file(f);
				return mgNoErr;
			}
			catch (boost::system::system_error se)
			{
				return se.code().value();
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsSetFilePassword(lvasynctls::lvTlsConnectionCreator * creator, const char * pass)
	{
		if (creator) {
			try {
				std::string p(pass);
				creator->enablePasswordCallback(p);
				return mgNoErr;
			}
			catch (...)
			{
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}
	

	//socket stream
	lvExport MgErr lvtlsCloseConnection(lvasynctls::lvTlsSocketBase * s)
	{
		if (s) {
			delete s;
			s = nullptr;
			return mgNoErr;
		}
		else {
			return oleNullRefnumPassed;
		}
		return mgNoErr;
	}

	lvExport MgErr lvtlsBeginWrite(lvasynctls::lvTlsSocketBase * s, LVUserEventRef * e, LStrHandle data, uint32_t requestID)
	{
		if (s && e && (*e) != kNotAMagicCookie && LHStrPtr(data) && LHStrBuf(data) && (LHStrLen(data) > 0)) {
			lvasyncapi::LVNotificationCallback * cb = nullptr;
			std::vector<unsigned char>* writeData;
			try {
				cb = new lvasyncapi::LVNotificationCallback(e, lvasyncapi::lvRequestType::StreamWrite, requestID);
				writeData = new std::vector<unsigned char>();
				writeData->assign(LHStrBuf(data), LHStrBuf(data) + LHStrLen(data));
				//give data to write, which gives it to cb, which deallocs
				s->startWrite(writeData, cb);
				return mgNoErr;
			}
			catch (...) {
				if (cb) {
					delete cb;
					cb = nullptr;
				}
				if (writeData) {
					delete writeData;
					writeData = nullptr;
				}

				return 42;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsBeginDirectRead(lvasynctls::lvTlsSocketBase * s, LVUserEventRef * e, size_t bytesToRead, uint32_t requestID)
	{
		if (s && e && (*e) != kNotAMagicCookie) {
			lvasyncapi::LVNotificationCallback* cb = nullptr;
			try {
				auto cb = new lvasyncapi::LVNotificationCallback(e, lvasyncapi::lvRequestType::StreamRead, requestID);
				s->startDirectRead(bytesToRead, cb);
				return mgNoErr;
			}
			catch (...) {
				if (cb) {
					delete cb;
					cb = nullptr;
				}
				return 42;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsBeginDirectReadSome(lvasynctls::lvTlsSocketBase * s, LVUserEventRef * e, size_t maxBytesToRead, uint32_t requestID)
	{
		if (s && e && (*e) != kNotAMagicCookie) {
			lvasyncapi::LVNotificationCallback* cb = nullptr;
			try {
				auto cb = new lvasyncapi::LVNotificationCallback(e, lvasyncapi::lvRequestType::StreamRead, requestID);
				s->startDirectReadSome(maxBytesToRead, cb);
				return mgNoErr;
			}
			catch (...) {
				if (cb) {
					delete cb;
					cb = nullptr;
				}
				return 42;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsBeginDirectReadAtLeast(lvasynctls::lvTlsSocketBase * s, LVUserEventRef * e, size_t minBytesToRead, size_t maxBytesToRead, uint32_t requestID)
	{
		if (s && e && (*e) != kNotAMagicCookie) {
			lvasyncapi::LVNotificationCallback* cb = nullptr;
			try {
				auto cb = new lvasyncapi::LVNotificationCallback(e, lvasyncapi::lvRequestType::StreamRead, requestID);
				s->startDirectReadAtLeast(minBytesToRead, maxBytesToRead, cb);
				return mgNoErr;
			}
			catch (...) {
				if (cb) {
					delete cb;
					cb = nullptr;
				}
				return 42;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsBeginStreamRead(lvasynctls::lvTlsSocketBase * s, LVUserEventRef * e, size_t bytesToRead, uint32_t requestID)
	{
		if (s && e && (*e) != kNotAMagicCookie) {
			lvasyncapi::LVCompletionNotificationCallback* cb = nullptr;
			try {
				auto cb = new lvasyncapi::LVCompletionNotificationCallback(e, lvasyncapi::lvRequestType::StreamRead, requestID);
				s->startStreamRead(bytesToRead, cb);
				return mgNoErr;
			}
			catch (...) {
				if (cb) {
					delete cb;
					cb = nullptr;
				}
				return 42;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsBeginStreamReadAtLeast(lvasynctls::lvTlsSocketBase * s, LVUserEventRef * e, size_t minBytesToRead, size_t maxBytesToRead, uint32_t requestID)
	{
		if (s && e && (*e) != kNotAMagicCookie) {
			lvasyncapi::LVCompletionNotificationCallback* cb = nullptr;
			try {
				auto cb = new lvasyncapi::LVCompletionNotificationCallback(e, lvasyncapi::lvRequestType::StreamRead, requestID);
				s->startStreamReadAtLeast(minBytesToRead, maxBytesToRead, cb);
				return mgNoErr;
			}
			catch (...) {
				if (cb) {
					delete cb;
					cb = nullptr;
				}
				return 42;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsBeginStreamReadUntilTerm(lvasynctls::lvTlsSocketBase * s, LVUserEventRef * e, LStrHandle termination, size_t maxBytesToRead, uint32_t requestID)
	{
		if (s && e && (*e) != kNotAMagicCookie && termination && LHStrPtr(termination)) {
			lvasyncapi::LVCompletionNotificationCallback* cb = nullptr;
			size_t termLen = LHStrLen(termination);
			if (termLen > 0) {
				try {
					auto cb = new lvasyncapi::LVCompletionNotificationCallback(e, lvasyncapi::lvRequestType::StreamRead, requestID);

					if (termLen == 1) {
						//single character
						char termChar = LHStrBuf(termination)[0];
						s->startStreamReadUntilTermChar(termChar, maxBytesToRead, cb);
						return mgNoErr;
					}
					else if (termLen > 1) {
						//multi-character
						std::string termStr = LstrToStdStr(termination);
						s->startStreamReadUntilTermString(termStr, maxBytesToRead, cb);
						return mgNoErr;
					}
					else {
						//if we get to this point something bad happened
						if (cb) {
							delete cb;
							cb = nullptr;
						}
						return 42;
					}					
				}
				catch (...) {
					if (cb) {
						delete cb;
						cb = nullptr;
					}
					return 42;
				}
			}
			else {
				//string length is 0
				return mgArgErr;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport size_t lvtlsGetInternalStreamSize(lvasynctls::lvTlsSocketBase * s)
	{
		return s->getInputStreamSize();
	}

	lvExport MgErr lvtlsInputStreamReadSome(lvasynctls::lvTlsSocketBase * s, LStrHandle buffer, size_t maxLen)
	{
		if (s && buffer && maxLen > 0) {
			if (LHStrLen(buffer) != maxLen) {
				//make sure handle is big enough to hold data
				DSSetHandleSize(buffer, maxLen + sizeof(int32_t));
			}

			char* bufPtr = (char *)(LHStrBuf(buffer));
			try {
				auto trueLen = s->inputStreamReadSome(bufPtr, maxLen);
				DSSetHandleSize(buffer, trueLen + sizeof(int32_t));
				LStrLen(LHStrPtr(buffer)) = trueLen;
				return mgNoErr;
			}
			catch (...) {
				DSSetHSzClr(buffer, sizeof(int32)); //truncate handle size to just length=0
			}
		}
		else {
			return mgArgErr;
		}

		return MgErr();
	}


#ifdef __cplusplus
}
#endif
