#include "lvcryptoapi.h"

MgErr lvcryptoSHA(LStrHandle data, LStrHandle digest, lvcryptowrapper::SHAdepth depth)
{
	if (data && digest && LHStrBuf(data) && LHStrLen(data) > 0) {
		int digestLen = 0;
		switch (depth) {
			case lvcryptowrapper::SHAdepth::SHA512:
				digestLen = SHA512_DIGEST_LENGTH;
				break;
			case lvcryptowrapper::SHAdepth::SHA384:
				digestLen = SHA384_DIGEST_LENGTH;
				break;
			case lvcryptowrapper::SHAdepth::SHA256:
				digestLen = SHA256_DIGEST_LENGTH;
				break;
			case lvcryptowrapper::SHAdepth::SHA224:
				digestLen = SHA224_DIGEST_LENGTH;
				break;
			default:
				break;
		}

		if (0==digestLen) {
			return mgArgErr;
		}
		
		DSSetHSzClr(digest, digestLen+sizeof(LStr::cnt));
		
		switch (depth) {
			case lvcryptowrapper::SHAdepth::SHA512:
				SHA512(LHStrBuf(data), LHStrLen(data), LHStrBuf(digest));
				break;
			case lvcryptowrapper::SHAdepth::SHA384:
				SHA512(LHStrBuf(data), LHStrLen(data), LHStrBuf(digest));
				break;
			case lvcryptowrapper::SHAdepth::SHA256:
				SHA512(LHStrBuf(data), LHStrLen(data), LHStrBuf(digest));
				break;
			case lvcryptowrapper::SHAdepth::SHA224:
				SHA512(LHStrBuf(data), LHStrLen(data), LHStrBuf(digest));
				break;
			default:
				break;
		}

		LStrLen(LHStrPtr(digest)) = digestLen;

		return mgNoErr;
	}
	else {
		return mgArgErr;
	}

}
