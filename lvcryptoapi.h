#pragma once
#include <extcode.h>
#include <openssl\sha.h>



#ifndef lvExport
#define lvExport __declspec(dllexport)
#endif

namespace lvcryptowrapper {
	typedef enum { SHA512, SHA384, SHA256, SHA224 } SHAdepth;
}


#ifdef __cplusplus
extern "C" {
#endif

	
	MgErr lvcryptoSHA(LStrHandle data, LStrHandle digest, lvcryptowrapper::SHAdepth depth);








#ifdef __cplusplus
}
#endif