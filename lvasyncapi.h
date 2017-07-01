#pragma once

#include <extcode.h>
#include <vector>
#include "lvTlsCallbackBase.h"
#include "lvapicallbacks.h"
#include "lvTlsConnectionCreator.h"
#include "lvTlsEngine.h"
#include "lvTlsSocket.h"



namespace lvasynctls {
	class lvAsyncEngine;
	class lvTlsSocketBase;
	class lvTlsConnectionCreator;
	class lvTlsClientConnector;
	class lvTlsServerAcceptor;
	class LVCompletionNotificationCallback;
}

#ifndef lvExport
#define lvExport __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

	//abort handler
	lvExport MgErr lvTlsIOengine_reserve(InstanceDataPtr *instance);
	lvExport MgErr lvTlsIOengine_unreserve(InstanceDataPtr *instance);
	lvExport MgErr lvTlsIOengine_abort(InstanceDataPtr *instance);
	void lvTlsIOengine_recordInstance(lvasynctls::lvAsyncEngine ** engine);
	void lvTlsIOengine_removeInstance(lvasynctls::lvAsyncEngine ** engine);

	//engine
	lvExport lvasynctls::lvAsyncEngine** lvtlsCreateIOEngine();
	lvExport lvasynctls::lvAsyncEngine** lvtlsCreateIOEngineNThreads(std::size_t threadCount);
	lvExport MgErr lvtlsDisposeIOEngine(lvasynctls::lvAsyncEngine** engine);

	//client
	lvExport lvasynctls::lvTlsClientConnector** lvtlsCreateClientConnector(lvasynctls::lvAsyncEngine** engine);
	lvExport MgErr lvtlsBeginClientConnect(lvasynctls::lvTlsClientConnector** client, LStrHandle host, LStrHandle port, LVUserEventRef * e, uint32_t requestID);

	//server
	lvExport lvasynctls::lvTlsServerAcceptor** lvtlsCreateServerAcceptor(lvasynctls::lvAsyncEngine** engine, uint16_t port);
	lvExport MgErr lvtlsBeginServerAccept(lvasynctls::lvTlsServerAcceptor** acceptor, LVUserEventRef * e, uint32_t requestID);
	
	//conn creator
	lvExport MgErr lvtlsDisposeConnectionCreator(lvasynctls::lvTlsConnectionCreator** creator);
	lvExport lvasynctls::lvTlsSocketBase** lvtlsPopAvailableConnection(lvasynctls::lvTlsConnectionCreator** creator);

	//ssl context
	lvExport MgErr lvtlsSetOptions(lvasynctls::lvTlsConnectionCreator** creator, long options);
	lvExport MgErr lvtlsSetVerifymode(lvasynctls::lvTlsConnectionCreator** creator, int v);
	lvExport MgErr lvtlsSetRfc2818Verification(lvasynctls::lvTlsConnectionCreator** creator, const char* host);
	lvExport MgErr lvtlsLoadVerifyFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename);
	lvExport MgErr lvtlsAddCertificateauthority(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle ca);
	lvExport MgErr lvtlsSetDefaultVerifyPaths(lvasynctls::lvTlsConnectionCreator** creator);
	lvExport MgErr lvtlsAddVerifyPath(lvasynctls::lvTlsConnectionCreator** creator, const char*  path);
	lvExport MgErr lvtlsUseCertificate(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle certificate, int format);
	lvExport MgErr lvtlsUseCertificateFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename, int format);
	lvExport MgErr lvtlsUseCertificateChain(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle chain);
	lvExport MgErr lvtlsUseCertificateChainFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename);
	lvExport MgErr lvtlsUsePrivateKey(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle privateKey, int format);
	lvExport MgErr lvtlsUsePrivateKeyFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename, int format);
	lvExport MgErr lvtlsUseRsaPrivateKey(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle privateKey, int format);
	lvExport MgErr lvtlsUseRsaPrivateKeyFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename, int format);
	lvExport MgErr lvtlsUseTmpDh(lvasynctls::lvTlsConnectionCreator** creator, LStrHandle dh);
	lvExport MgErr lvtlsUseTmpDhFile(lvasynctls::lvTlsConnectionCreator** creator, const char*  filename);
	lvExport MgErr lvtlsSetFilePassword(lvasynctls::lvTlsConnectionCreator** creator, const char* pass);

	//tls stream socket
	lvExport int64_t lvtlsBeginWrite(lvasynctls::lvTlsSocketBase** s, LVUserEventRef * e, LStrHandle data, uint32_t requestID);
	lvExport MgErr lvtlsBeginStreamRead(lvasynctls::lvTlsSocketBase** s, LVUserEventRef * e, size_t bytesToRead, uint32_t requestID);
	lvExport MgErr lvtlsBeginStreamReadUntilTerm(lvasynctls::lvTlsSocketBase** s, LVUserEventRef * e, LStrHandle termination, size_t maxBytesToRead, uint32_t requestID);
	//internal data stream access
	lvExport size_t lvtlsGetInternalStreamSize(lvasynctls::lvTlsSocketBase** s);
	lvExport MgErr lvtlsInputStreamReadSome(lvasynctls::lvTlsSocketBase** s, LStrHandle buffer, size_t maxLen);
	lvExport MgErr lvtlsInputStreamReadN(lvasynctls::lvTlsSocketBase** s, LStrHandle buffer, size_t len);
	lvExport MgErr lvtlsInputStreamReadLine(lvasynctls::lvTlsSocketBase** s, LStrHandle buffer, size_t maxLen, char delimiter);
	//free
	lvExport MgErr lvtlsCloseConnection(lvasynctls::lvTlsSocketBase** s);

#ifdef __cplusplus
}
#endif
