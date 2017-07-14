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
	void lvTlsIOengine_recordInstance(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine);
	void lvTlsIOengine_removeInstance(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine);

	//engine
	lvExport std::shared_ptr<lvasynctls::lvAsyncEngine>* lvtlsCreateIOEngine();
	lvExport std::shared_ptr<lvasynctls::lvAsyncEngine>* lvtlsCreateIOEngineNThreads(std::size_t threadCount);
	lvExport MgErr lvtlsDisposeIOEngine(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine);

	//client
	lvExport std::shared_ptr<lvasynctls::lvTlsClientConnector>* lvtlsCreateClientConnector(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine);
	lvExport MgErr lvtlsBeginClientConnect(std::shared_ptr<lvasynctls::lvTlsClientConnector>* client, LStrHandle host, LStrHandle port, size_t bufferMaxSize, size_t outputQueueSize, LVUserEventRef * e, uint32_t requestID);

	//server
	lvExport std::shared_ptr<lvasynctls::lvTlsServerAcceptor>* lvtlsCreateServerAcceptor(std::shared_ptr<lvasynctls::lvAsyncEngine>* engine, uint16_t port, uint8_t v6=0);
	lvExport MgErr lvtlsBeginServerAccept(std::shared_ptr<lvasynctls::lvTlsServerAcceptor>* acceptor, size_t bufferMaxSize, size_t outputQueueSize, LVUserEventRef * e, uint32_t requestID);
	
	//conn creator
	lvExport MgErr lvtlsDisposeConnectionCreator(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator);
	lvExport std::shared_ptr<lvasynctls::lvTlsSocketBase>* lvtlsPopAvailableConnection(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator);

	//ssl context
	lvExport MgErr lvtlsSetOptions(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, long options);
	lvExport MgErr lvtlsSetVerifymode(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, int v);
	lvExport MgErr lvtlsSetRfc2818Verification(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char* host);
	lvExport MgErr lvtlsLoadVerifyFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename);
	lvExport MgErr lvtlsAddCertificateauthority(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle ca);
	lvExport MgErr lvtlsSetDefaultVerifyPaths(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator);
	lvExport MgErr lvtlsAddVerifyPath(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  path);
	lvExport MgErr lvtlsUseCertificate(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle certificate, int format);
	lvExport MgErr lvtlsUseCertificateFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename, int format);
	lvExport MgErr lvtlsUseCertificateChain(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle chain);
	lvExport MgErr lvtlsUseCertificateChainFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename);
	lvExport MgErr lvtlsUsePrivateKey(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle privateKey, int format);
	lvExport MgErr lvtlsUsePrivateKeyFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename, int format);
	lvExport MgErr lvtlsUseRsaPrivateKey(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle privateKey, int format);
	lvExport MgErr lvtlsUseRsaPrivateKeyFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename, int format);
	lvExport MgErr lvtlsUseTmpDh(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, LStrHandle dh);
	lvExport MgErr lvtlsUseTmpDhFile(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char*  filename);
	lvExport MgErr lvtlsSetFilePassword(std::shared_ptr<lvasynctls::lvTlsConnectionCreator>* creator, const char* pass);

	//tls stream socket
	lvExport int64_t lvtlsBeginWrite(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LVUserEventRef * e, LStrHandle data, uint32_t requestID);
	lvExport MgErr lvtlsBeginStreamRead(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LVUserEventRef * e, size_t bytesToRead, uint32_t requestID);
	lvExport MgErr lvtlsBeginStreamReadUntilTerm(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LVUserEventRef * e, LStrHandle termination, size_t maxBytesToRead, uint32_t requestID);
	//internal data stream access
	lvExport size_t lvtlsGetInternalStreamSize(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s);
	lvExport MgErr lvtlsInputStreamReadSome(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LStrHandle buffer, size_t maxLen);
	lvExport MgErr lvtlsInputStreamReadN(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LStrHandle buffer, size_t len);
	lvExport MgErr lvtlsInputStreamReadLine(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s, LStrHandle buffer, size_t maxLen, char delimiter);
	//free
	lvExport MgErr lvtlsCloseConnection(std::shared_ptr<lvasynctls::lvTlsSocketBase>* s);

#ifdef __cplusplus
}
#endif
