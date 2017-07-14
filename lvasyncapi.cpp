
#include "lvasyncapi.h"
#include <extcode.h>
#include <vector>
#include <unordered_set>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>



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

static std::unordered_set<std::shared_ptr<lvasynctls::lvAsyncEngine>*> engineSet{};
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
		}
		return mgNoErr;
	}

	lvExport MgErr lvTlsIOengine_abort(InstanceDataPtr * instance)
	{
		try {
			clearInstanceEngineSet();
		}
		catch (...) {
		}
		return mgNoErr;
	}

	void clearInstanceEngineSet() 
	{
		std::unordered_set<std::shared_ptr<lvasynctls::lvAsyncEngine>*> setCpy{};
		try {
			std::lock_guard<std::mutex> lockg(engineSetLock);
			setCpy = engineSet;
			engineSet.clear();
		}
		catch (...) {
		}
		try {
			if (setCpy.size() > 0) {
				for (auto itr = setCpy.begin(); itr != setCpy.end(); ++itr) {
					try {
						auto engPtr = *itr;
						if (engPtr) {
							(*engPtr)->shutdown();
							engPtr->reset();
						}
					}
					catch (...) {
					}
				}
			}
		}
		catch (...)
		{
		}
	}
	
	void lvTlsIOengine_recordInstance(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine)
	{
		try {
			std::lock_guard<std::mutex> lockg(engineSetLock);
			engineSet.insert(engine);
		}
		catch (...)
		{
			//ignore
		}
	}

	void lvTlsIOengine_removeInstance(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine)
	{
		try {
			std::lock_guard<std::mutex> lockg(engineSetLock);
			engineSet.erase(engine);
		}
		catch (...)
		{
			//ignore
		}
	}


	//engine
	lvExport std::shared_ptr<lvasynctls::lvAsyncEngine>* lvtlsCreateIOEngine()
	{
		try {
			auto engine = new std::shared_ptr<lvasynctls::lvAsyncEngine>{ new lvasynctls::lvAsyncEngine() };
			lvTlsIOengine_recordInstance(engine);
			return engine;
		}
		catch (...) {
			return nullptr;
		}	
	}

	//engine
	lvExport std::shared_ptr<lvasynctls::lvAsyncEngine>* lvtlsCreateIOEngineNThreads(std::size_t threadCount)
	{
		try {
			auto engine = new std::shared_ptr<lvasynctls::lvAsyncEngine>{ new lvasynctls::lvAsyncEngine(threadCount) };
			lvTlsIOengine_recordInstance(engine);
			return engine;
		}
		catch (...) {
			return nullptr;
		}
	}

	lvExport MgErr lvtlsDisposeIOEngine(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine)
	{
		try {
			if (engine && *engine) {
				lvTlsIOengine_removeInstance(engine);
				
				(*engine)->shutdown();
				engine->reset();
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
	lvExport std::shared_ptr<lvasynctls::lvTlsClientConnector>* lvtlsCreateClientConnector(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine)
	{
		if (engine && *engine) {
			auto esp = *engine;
			auto c = new std::shared_ptr<lvasynctls::lvTlsClientConnector>{ new lvasynctls::lvTlsClientConnector{esp, 100} };
			esp->registerConnector(*c);
			return c;
		}
		else {
			return nullptr;
		}
	}

	lvExport MgErr lvtlsBeginClientConnect(std::shared_ptr<lvasynctls::lvTlsClientConnector>* client,
		LStrHandle host, LStrHandle port, size_t bufferMaxSize, size_t outputQueueSize,
		LVUserEventRef * e, uint32_t requestID)
	{
		if (client && *client && LHStrPtr(host) && LHStrPtr(port) && LHStrBuf(host) && LHStrBuf(port)) {
			lvasyncapi::LVCompletionNotificationCallback * cb = nullptr;
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				(*client)->resolveAndConnect(LstrToStdStr(host), LstrToStdStr(port), bufferMaxSize, outputQueueSize, cb);
				return mgNoErr;
			}
			catch (...) {
				delete cb;
				cb = nullptr;
				
				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	//server
	lvExport std::shared_ptr<lvasynctls::lvTlsServerAcceptor>* lvtlsCreateServerAcceptor(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine, uint16_t port, uint8_t v6)
	{
		if (engine && *engine) {
			boost::asio::ip::tcp::endpoint endpoint(((v6>0) ? boost::asio::ip::tcp::v6() : boost::asio::ip::tcp::v4()), port);
			auto esp = *engine;
			auto a = new std::shared_ptr<lvasynctls::lvTlsServerAcceptor>{ new lvasynctls::lvTlsServerAcceptor{ esp, endpoint} };
			esp->registerConnector(*a);
			return a;
		}
		else {
			return nullptr;
		}
	}


	lvExport MgErr lvtlsBeginServerAccept(
		std::shared_ptr<lvasynctls::lvTlsServerAcceptor>* acceptor, size_t bufferMaxSize, size_t outputQueueSize,
		LVUserEventRef * e, uint32_t requestID)
	{
		if (acceptor && *acceptor) {
			lvasyncapi::LVCompletionNotificationCallback * cb = nullptr;
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				(*acceptor)->startAccept(bufferMaxSize, outputQueueSize, cb);
				return mgNoErr;
			}
			catch (...) {
				delete cb;
				cb = nullptr;

				return 42;
			}
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	lvExport MgErr lvtlsDisposeConnectionCreator(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator)
	{
		try {
			if (creator && *creator) {
				(*creator)->shutdown();
				creator->reset();
				creator = nullptr;

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

	lvExport std::shared_ptr<lvasynctls::lvTlsSocketBase>* lvtlsPopAvailableConnection(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator)
	{
		if (creator && *creator) {
			auto snext = (*creator)->getNextConnection();
			if (snext.get()) {
				auto ssp = new std::shared_ptr<lvasynctls::lvTlsSocketBase>;
				ssp->swap(snext);
				return ssp;
			}
			else {
				return nullptr;
			}
		}
		return nullptr;
	}

	//ssl context
	lvExport MgErr lvtlsSetOptions(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, long options)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.set_options(options);
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

	lvExport MgErr lvtlsSetVerifymode(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, int v)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.set_verify_mode(v);
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

	lvExport MgErr lvtlsSetRfc2818Verification(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char* host)
	{
		if (creator && *creator) {
			try {
				std::string h(host);
				(*creator)->ctx.set_verify_callback(boost::asio::ssl::rfc2818_verification(h));
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

	lvExport MgErr lvtlsLoadVerifyFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char* filename)
	{
		if (creator && *creator) {
			try {
				std::string f(filename);
				(*creator)->ctx.load_verify_file(f);
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

	lvExport MgErr lvtlsAddCertificateauthority(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle ca)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.add_certificate_authority(boost::asio::const_buffer(LHStrBuf(ca), LHStrLen(ca)));
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

	lvExport MgErr lvtlsSetDefaultVerifyPaths(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.set_default_verify_paths();
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

	lvExport MgErr lvtlsAddVerifyPath(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char* path)
	{
		if (creator && *creator) {
			try {
				std::string p(path);
				(*creator)->ctx.add_verify_path(p);
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

	lvExport MgErr lvtlsUseCertificate(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle certificate, int format)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.use_certificate(boost::asio::const_buffer(LHStrBuf(certificate), LHStrLen(certificate)), static_cast<boost::asio::ssl::context_base::file_format>(format));
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

	lvExport MgErr lvtlsUseCertificateFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char* filename, int format)
	{
		if (creator && *creator) {
			try {
				std::string f(filename);
				(*creator)->ctx.use_certificate_file(f, static_cast<boost::asio::ssl::context_base::file_format>(format));
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

	lvExport MgErr lvtlsUseCertificateChain(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle chain)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.use_certificate_chain(boost::asio::const_buffer(LHStrBuf(chain), LHStrLen(chain)));
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

	lvExport MgErr lvtlsUseCertificateChainFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename)
	{
		if (creator && *creator) {
			try {
				std::string f(filename);
				(*creator)->ctx.use_certificate_chain_file(f);
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

	lvExport MgErr lvtlsUsePrivateKey(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle privateKey, int format)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.use_private_key(boost::asio::const_buffer(LHStrBuf(privateKey), LHStrLen(privateKey)), static_cast<boost::asio::ssl::context_base::file_format>(format));
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

	lvExport MgErr lvtlsUsePrivateKeyFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename, int format)
	{
		if (creator && *creator) {
			try {
				std::string f(filename);
				(*creator)->ctx.use_private_key_file(f, static_cast<boost::asio::ssl::context_base::file_format>(format));
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

	lvExport MgErr lvtlsUseRsaPrivateKey(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle privateKey, int format)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.use_rsa_private_key(boost::asio::const_buffer(LHStrBuf(privateKey), LHStrLen(privateKey)), static_cast<boost::asio::ssl::context_base::file_format>(format));
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

	lvExport MgErr lvtlsUseRsaPrivateKeyFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename, int format)
	{
		if (creator && *creator) {
			try {
				std::string f(filename);
				(*creator)->ctx.use_rsa_private_key_file(f, static_cast<boost::asio::ssl::context_base::file_format>(format));
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

	lvExport MgErr lvtlsUseTmpDh(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle dh)
	{
		if (creator && *creator) {
			try {
				(*creator)->ctx.use_tmp_dh(boost::asio::const_buffer(LHStrBuf(dh), LHStrLen(dh)));
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

	lvExport MgErr lvtlsUseTmpDhFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename)
	{
		if (creator && *creator) {
			try {
				std::string f(filename);
				(*creator)->ctx.use_tmp_dh_file(f);
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

	lvExport MgErr lvtlsSetFilePassword(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char * pass)
	{
		if (creator && *creator) {
			try {
				std::string p(pass);
				(*creator)->enablePasswordCallback(p);
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
	lvExport MgErr lvtlsCloseConnection(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s)
	{
		if (s && *s) {
			(*s)->shutdown();

			s->reset();

			delete s;
			s = nullptr;
			return mgNoErr;
		}
		else {
			return oleNullRefnumPassed;
		}
		return mgNoErr;
	}

	lvExport int64_t lvtlsBeginWrite(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LVUserEventRef * e, LStrHandle data, uint32_t requestID)
	{
		if (s && *s && LHStrPtr(data) && LHStrBuf(data) && (LHStrLen(data) > 0)) {
			auto dataVec = new std::vector<unsigned char>(LHStrBuf(data), LHStrBuf(data) + LHStrLen(data));
			//mem cpy
			//dataVec->assign(LHStrBuf(data), LHStrBuf(data) + LHStrLen(data));

			lvasyncapi::LVCompletionNotificationCallback * cb = nullptr;
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				auto copied = (*s)->startWrite(dataVec, cb);

				if (copied < 1) {
					delete dataVec;
					delete cb;
				}

				return copied;
			}
			catch (...) {
			}
			//only reach this point on exception

			delete cb;
			cb = nullptr;

			return -2;

		}
		else {
			return -1;
		}
	}

	lvExport MgErr lvtlsBeginStreamRead(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LVUserEventRef * e, size_t bytesToRead, uint32_t requestID)
	{
		if (s && *s) {
			lvasyncapi::LVCompletionNotificationCallback* cb = nullptr;
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				auto ret = (*s)->startStreamRead(bytesToRead, cb);
				return ret;
			}
			catch (...) {
				delete cb;
				cb = nullptr;

				return -2;
			}
		}
		else {
			return -1;
		}
	}

	lvExport MgErr lvtlsBeginStreamReadUntilTerm(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LVUserEventRef * e, LStrHandle termination, size_t maxBytesToRead, uint32_t requestID)
	{
		if (s && *s && termination && LHStrPtr(termination) && (LHStrLen(termination) > 0)) {
			lvasyncapi::LVCompletionNotificationCallback* cb = nullptr;
			size_t termLen = LHStrLen(termination);
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				if (termLen == 1) {
					//single character
					char termChar = LHStrBuf(termination)[0];
					auto ret = (*s)->startStreamReadUntilTermChar(termChar, maxBytesToRead, cb);
					return ret;
				}
				else if (termLen > 1) {
					//multi-character
					std::string termStr = LstrToStdStr(termination);
					auto ret = (*s)->startStreamReadUntilTermString(termStr, maxBytesToRead, cb);
					return ret;
				}
				else {
					//if we get to this point something bad happened
					delete cb;
					cb = nullptr;

					return -3;
				}			
			}
			catch (...) {
				delete cb;
				cb = nullptr;

				return -2;
			}
		}
		else {
			return -1;
		}
	}

	lvExport size_t lvtlsGetInternalStreamSize(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s)
	{
		if (s && *s) {
			return (*s)->getInputStreamSize();
		}
		else {
			return 0;
		}
	}

	lvExport MgErr lvtlsInputStreamReadSome(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LStrHandle buffer, size_t maxLen)
	{
		if (s && *s && buffer && maxLen > 0) {
			if (LHStrLen(buffer) != maxLen) {
				//make sure handle is big enough to hold data
				DSSetHandleSize(buffer, maxLen + sizeof(int32_t));
			}

			char* bufPtr = (char *)(LHStrBuf(buffer));
			try {
				auto trueLen = (*s)->inputStreamReadSome(bufPtr, maxLen);
				DSSetHandleSize(buffer, trueLen + sizeof(int32_t));
				LStrLen(LHStrPtr(buffer)) = trueLen;
				return mgNoErr;
			}
			catch (...) {
				DSSetHSzClr(buffer, sizeof(int32)); //truncate handle size to just length=0
				return fIOErr;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsInputStreamReadN(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LStrHandle buffer, size_t len)
	{
		if (s && *s && buffer && len > 0) {
			if (LHStrLen(buffer) != len) {
				//make sure handle is big enough to hold data
				DSSetHandleSize(buffer, len + sizeof(int32_t));
			}

			try {
				auto trueLen = (*s)->inputStreamReadN((char *)(LHStrBuf(buffer)), len);
				DSSetHandleSize(buffer, trueLen + sizeof(int32_t));
				LStrLen(LHStrPtr(buffer)) = trueLen;
				return mgNoErr;
			}
			catch (...) {
				DSSetHSzClr(buffer, sizeof(int32)); //truncate handle size to just length=0
				return fIOErr;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsInputStreamReadLine(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LStrHandle buffer, size_t maxLen, char delimiter)
	{
		if (s && *s && buffer && maxLen > 0) {
			if (LHStrLen(buffer) != maxLen) {
				//make sure handle is big enough to hold data
				DSSetHandleSize(buffer, maxLen + sizeof(int32_t));
			}
			try {
				auto trueLen = (*s)->inputStreamGetLine((char *)(LHStrBuf(buffer)), maxLen, delimiter);
				DSSetHandleSize(buffer, trueLen + sizeof(int32_t));
				LStrLen(LHStrPtr(buffer)) = trueLen;
				return mgNoErr;
			}
			catch (...) {
				DSSetHSzClr(buffer, sizeof(int32)); //truncate handle size to just length=0
				return fIOErr;
			}
		}
		else {
			return mgArgErr;
		}
	}


#ifdef __cplusplus
}
#endif
