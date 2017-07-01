
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

static std::unordered_set<lvasynctls::lvAsyncEngine**> engineSet{};
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
		return mgNoErr;
	}

	lvExport MgErr lvTlsIOengine_abort(InstanceDataPtr * instance)
	{
		try {
			clearInstanceEngineSet();
			return mgNoErr;
		}
		catch (...) {
		}
		return mgNoErr;
	}

	void clearInstanceEngineSet() 
	{
		std::unordered_set<lvasynctls::lvAsyncEngine**> setCpy;
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
						delete (*engPtr);
						*engPtr = nullptr;
						//don't delete engPtr here, new pattern is ONLY delete the ptr if the appropriate shutdown is called.
						//out of band failures will leave the handle intact but pointing to null
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
	
	void lvTlsIOengine_recordInstance(lvasynctls::lvAsyncEngine ** engine)
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

	void lvTlsIOengine_removeInstance(lvasynctls::lvAsyncEngine ** engine)
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
	lvExport lvasynctls::lvAsyncEngine ** lvtlsCreateIOEngine()
	{
		auto engine = new (lvasynctls::lvAsyncEngine *);
		try {
			(*engine) = new lvasynctls::lvAsyncEngine();
			lvTlsIOengine_recordInstance(engine);
			return engine;
		}
		catch (...) {
			delete (*engine);
			delete engine;

			return nullptr;
		}	
	}

	//engine
	lvExport lvasynctls::lvAsyncEngine ** lvtlsCreateIOEngineNThreads(std::size_t threadCount)
	{
		auto engine = new (lvasynctls::lvAsyncEngine *);
		try {
			(*engine) = new lvasynctls::lvAsyncEngine(threadCount);
			lvTlsIOengine_recordInstance(engine);
			return engine;
		}
		catch (...) {
			delete (*engine);
			delete engine;

			return nullptr;
		}
	}

	lvExport MgErr lvtlsDisposeIOEngine(lvasynctls::lvAsyncEngine ** engine)
	{
		try {
			if (engine) {
				lvTlsIOengine_removeInstance(engine);

				delete (*engine);
				*engine = nullptr;

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
	lvExport lvasynctls::lvTlsClientConnector ** lvtlsCreateClientConnector(lvasynctls::lvAsyncEngine ** engine)
	{
		if (engine && *engine) {
			auto c = new (lvasynctls::lvTlsClientConnector *);
			try {
				(*c) = new lvasynctls::lvTlsClientConnector(*engine);
				return c;
			}
			catch (...) {
				delete (*c);
				*c = nullptr;
				delete c;
				c = nullptr;
				
				return nullptr;
			}
		}
		else {
			return nullptr;
		}
	}

	lvExport MgErr lvtlsBeginClientConnect(lvasynctls::lvTlsClientConnector ** client,
		LStrHandle host, LStrHandle port, LVUserEventRef * e, uint32_t requestID)
	{
		if (client && *client && LHStrPtr(host) && LHStrPtr(port) && LHStrBuf(host) && LHStrBuf(port)) {
			lvasyncapi::LVCompletionNotificationCallback * cb = nullptr;
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				(*client)->resolveAndConnect(LstrToStdStr(host), LstrToStdStr(port), cb);
				return mgNoErr;
			}
			catch (...) {
				delete cb;
				cb = nullptr;
				
				return 42;
			}
		}
		else if (client && *client) {
			return axEventQueueNotCreated;
		}
		else {
			return oleNullRefnumPassed;
		}
	}

	//server
	lvExport lvasynctls::lvTlsServerAcceptor ** lvtlsCreateServerAcceptor(lvasynctls::lvAsyncEngine ** engine, uint16_t port)
	{
		if (engine && *engine) {
			auto a = new (lvasynctls::lvTlsServerAcceptor *);
			try {
				(*a) = new lvasynctls::lvTlsServerAcceptor(*engine, port);
				return a;
			}
			catch (...) {
				if (a) {
					delete (*a);
					*a = nullptr;

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


	lvExport MgErr lvtlsBeginServerAccept(lvasynctls::lvTlsServerAcceptor ** acceptor, LVUserEventRef * e, uint32_t requestID)
	{
		if (acceptor && *acceptor) {
			lvasyncapi::LVCompletionNotificationCallback * cb = nullptr;
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				(*acceptor)->startAccept(cb);
				return mgNoErr;
			}
			catch (...) {
				delete cb;
				cb = nullptr;

				return 42;
			}
		}
		else {
			if (acceptor && *acceptor) {
				return axEventQueueNotCreated;
			}
			else {
				return oleNullRefnumPassed;
			}
		}
	}

	lvExport MgErr lvtlsDisposeConnectionCreator(lvasynctls::lvTlsConnectionCreator ** creator)
	{
		try {
			if (creator) {
				delete (*creator);
				*creator = nullptr;

				delete creator;
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

	lvExport lvasynctls::lvTlsSocketBase ** lvtlsPopAvailableConnection(lvasynctls::lvTlsConnectionCreator ** creator)
	{
		if (creator && *creator) {
			auto s = (*creator)->getNextConnection();
			if (s) {
				return new (lvasynctls::lvTlsSocketBase *)(s);
			}
			else {
				return nullptr;
			}
		}
		return nullptr;
	}

	//ssl context
	lvExport MgErr lvtlsSetOptions(lvasynctls::lvTlsConnectionCreator ** creator, long options)
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

	lvExport MgErr lvtlsSetVerifymode(lvasynctls::lvTlsConnectionCreator** creator, int v)
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

	lvExport MgErr lvtlsSetRfc2818Verification(lvasynctls::lvTlsConnectionCreator** creator, const char* host)
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

	lvExport MgErr lvtlsLoadVerifyFile(lvasynctls::lvTlsConnectionCreator** creator, const char* filename)
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

	lvExport MgErr lvtlsAddCertificateauthority(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle ca)
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

	lvExport MgErr lvtlsSetDefaultVerifyPaths(lvasynctls::lvTlsConnectionCreator** creator)
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

	lvExport MgErr lvtlsAddVerifyPath(lvasynctls::lvTlsConnectionCreator** creator, const char* path)
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

	lvExport MgErr lvtlsUseCertificate(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle certificate, int format)
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

	lvExport MgErr lvtlsUseCertificateFile(lvasynctls::lvTlsConnectionCreator** creator, const char* filename, int format)
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

	lvExport MgErr lvtlsUseCertificateChain(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle chain)
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

	lvExport MgErr lvtlsUseCertificateChainFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename)
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

	lvExport MgErr lvtlsUsePrivateKey(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle privateKey, int format)
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

	lvExport MgErr lvtlsUsePrivateKeyFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename, int format)
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

	lvExport MgErr lvtlsUseRsaPrivateKey(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle privateKey, int format)
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

	lvExport MgErr lvtlsUseRsaPrivateKeyFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename, int format)
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

	lvExport MgErr lvtlsUseTmpDh(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle dh)
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

	lvExport MgErr lvtlsUseTmpDhFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename)
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

	lvExport MgErr lvtlsSetFilePassword(lvasynctls::lvTlsConnectionCreator** creator, const char * pass)
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
	lvExport MgErr lvtlsCloseConnection(lvasynctls::lvTlsSocketBase** s)
	{
		if (s) {
			delete (*s);
			*s = nullptr;

			delete s;
			s = nullptr;
			return mgNoErr;
		}
		else {
			return oleNullRefnumPassed;
		}
		return mgNoErr;
	}

	lvExport int64_t lvtlsBeginWrite(lvasynctls::lvTlsSocketBase ** s, LVUserEventRef * e, LStrHandle data, uint32_t requestID)
	{
		if (s && *s && LHStrPtr(data) && LHStrBuf(data) && (LHStrLen(data) > 0)) {
			lvasyncapi::LVCompletionNotificationCallback * cb = nullptr;
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				//give data to write, which copies it
				auto copied = (*s)->startWrite((char*)LHStrBuf(data), LHStrLen(data), cb);
				
				return copied;
			}
			catch (...) {
			}
			//only reach this point on exception
			try {
				delete cb;
				cb = nullptr;

				return -2;
			}
			catch (...) {
				return -3;
			}

		}
		else {
			return -1;
		}
	}

	lvExport MgErr lvtlsBeginStreamRead(lvasynctls::lvTlsSocketBase ** s, LVUserEventRef * e, size_t bytesToRead, uint32_t requestID)
	{
		if (s && *s) {
			lvasyncapi::LVCompletionNotificationCallback* cb = nullptr;
			try {
				if (e && *e != kNotAMagicCookie) {
					cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
				}
				(*s)->startStreamRead(bytesToRead, cb);
				return mgNoErr;
			}
			catch (...) {
				delete cb;
				cb = nullptr;

				return 42;
			}
		}
		else {
			return mgArgErr;
		}
	}

	lvExport MgErr lvtlsBeginStreamReadUntilTerm(lvasynctls::lvTlsSocketBase ** s, LVUserEventRef * e, LStrHandle termination, size_t maxBytesToRead, uint32_t requestID)
	{
		if (s && *s && termination && LHStrPtr(termination)) {
			lvasyncapi::LVCompletionNotificationCallback* cb = nullptr;
			size_t termLen = LHStrLen(termination);
			if (termLen > 0) {
				try {
					if (e && *e != kNotAMagicCookie) {
						cb = new lvasyncapi::LVCompletionNotificationCallback(e, requestID);
					}

					if (termLen == 1) {
						//single character
						char termChar = LHStrBuf(termination)[0];
						(*s)->startStreamReadUntilTermChar(termChar, maxBytesToRead, cb);
						return mgNoErr;
					}
					else if (termLen > 1) {
						//multi-character
						std::string termStr = LstrToStdStr(termination);
						(*s)->startStreamReadUntilTermString(termStr, maxBytesToRead, cb);
						return mgNoErr;
					}
					else {
						//if we get to this point something bad happened
						delete cb;
						cb = nullptr;

						return 42;
					}					
				}
				catch (...) {
					delete cb;
					cb = nullptr;

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

	lvExport size_t lvtlsGetInternalStreamSize(lvasynctls::lvTlsSocketBase ** s)
	{
		if (s && *s) {
			return (*s)->getInputStreamSize();
		}
		else {
			return 0;
		}
	}

	lvExport MgErr lvtlsInputStreamReadSome(lvasynctls::lvTlsSocketBase ** s, LStrHandle buffer, size_t maxLen)
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

	lvExport MgErr lvtlsInputStreamReadN(lvasynctls::lvTlsSocketBase ** s, LStrHandle buffer, size_t len)
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

	lvExport MgErr lvtlsInputStreamReadLine(lvasynctls::lvTlsSocketBase ** s, LStrHandle buffer, size_t maxLen, char delimiter)
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
