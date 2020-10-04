#ifndef SIM5320_TRACE_H
#define SIM5320_TRACE_H

// helper wrapper for 'mbed_trace'
#include "mbed_trace.h"
#ifdef TRACE_GROUP
#undef TRACE_GROUP
#endif
#define TRACE_GROUP "sim5"

#endif // SIM5320_TRACE_H
