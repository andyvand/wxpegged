/* trace_retro68.h -- startup tracing for testing the classic Mac OS build
   in an emulator. Only compiled in with -DWXPEGGED_TRACE=ON (CMake); see
   retro68/trace_retro68.cpp. */

#ifndef TRACE_RETRO68_H
#define TRACE_RETRO68_H

#ifdef WXPEGGED_TRACE
void PegTrace(const char *fmt, ...);
void PegTraceInit();
void PegTraceAlive(class wxFrame *frame);
#  define MTRACE(...) PegTrace(__VA_ARGS__)
#else
#  define MTRACE(...) ((void)0)
#endif

#endif
