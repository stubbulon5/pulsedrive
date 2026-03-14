/*  pd_audio_capture_stub.cpp  --  Fallback for unsupported platforms
 */
#include "pd_internal.h"

#if !defined(__APPLE__) && !defined(_WIN32)

#include <stdio.h>

bool pd_capture_start(PdState *state)
{
    (void)state;
    fprintf(stderr, "[pulsedrive] Audio capture not supported on this platform\n");
    return false;
}

void pd_capture_stop(PdState *state)
{
    (void)state;
}

#endif
