#pragma once

#define USE_SERVER_KEY 1

#if USE_SERVER_KEY
#define CONTROL_USE_TLS 1
#define RESTREAMER_USE_TLS 1
#ifndef NDEBUG
#define DISABLE_VERIFY_CONTROL_SERVER 1
#define DISABLE_VERIFY_RESTREAM_SERVER 1
#endif
#endif

// #define USE_PG_CONFIG 1

enum {
    UPDATE_CERTIFICATE_TIMEOUT = 24 * 60, // minutes
};
