#pragma once

#include "Config.h"

extern const char* TmpDH2048;

#if USE_SERVER_KEY
extern const char* ServerCertificate;
extern const char* ServerKey;
extern const char* ServerChain;
extern const char* ServerFullChain;
#endif

extern const char* TestClientCertificate;
extern const char* TestClientKey;
