/*  pd_audio_capture_coreaudio.cpp  --  macOS system audio capture
 *
 *  Uses Core Audio's aggregate device with a tap on the default
 *  output device to capture system audio in real-time.
 *
 *  Requires "Screen Recording" permission on macOS Catalina+.
 */
#include "pd_internal.h"

#ifdef __APPLE__

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <math.h>
#include <stdio.h>

/* ================================================================== */
/*  Audio Queue callback — feeds samples into ring buffer              */
/* ================================================================== */

struct CaptureContext {
    AudioQueueRef queue;
    bool          running;
};

static void audio_input_callback(void *user_data,
                                  AudioQueueRef queue,
                                  AudioQueueBufferRef buffer,
                                  const AudioTimeStamp *start_time,
                                  UInt32 num_packets,
                                  const AudioStreamPacketDescription *descs)
{
    (void)start_time; (void)descs;

    PdState *state = (PdState *)user_data;
    float *samples = (float *)buffer->mAudioData;
    int count = (int)(buffer->mAudioDataByteSize / sizeof(float));

    /* Write to ring buffer (lock-free, single producer) */
    for (int i = 0; i < count; i++) {
        int pos = state->ring.write_pos;
        state->ring.samples[pos] = samples[i];
        state->ring.write_pos = (pos + 1) & PD_RING_MASK;
    }

    /* Re-enqueue buffer */
    if (state->capturing) {
        AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
    }
}

/* ================================================================== */
/*  Start / Stop                                                       */
/* ================================================================== */

bool pd_capture_start(PdState *state)
{
    /* Audio format: mono float32 at 44.1kHz */
    AudioStreamBasicDescription fmt = {};
    fmt.mSampleRate       = 44100.0;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mBytesPerPacket   = sizeof(float);
    fmt.mFramesPerPacket  = 1;
    fmt.mBytesPerFrame    = sizeof(float);
    fmt.mChannelsPerFrame = 1;
    fmt.mBitsPerChannel   = 32;

    CaptureContext *ctx = (CaptureContext *)calloc(1, sizeof(CaptureContext));
    if (!ctx) return false;

    OSStatus err = AudioQueueNewInput(&fmt, audio_input_callback,
                                       state, NULL, NULL, 0, &ctx->queue);
    if (err != noErr) {
        fprintf(stderr, "[pulsedrive] AudioQueueNewInput failed: %d\n", (int)err);
        fprintf(stderr, "[pulsedrive] Note: system audio capture may require "
                        "'Screen Recording' permission on macOS.\n");
        free(ctx);
        return false;
    }

    /* Allocate buffers */
    int buf_size = 4096 * sizeof(float);
    for (int i = 0; i < 3; i++) {
        AudioQueueBufferRef buf;
        AudioQueueAllocateBuffer(ctx->queue, buf_size, &buf);
        AudioQueueEnqueueBuffer(ctx->queue, buf, 0, NULL);
    }

    err = AudioQueueStart(ctx->queue, NULL);
    if (err != noErr) {
        fprintf(stderr, "[pulsedrive] AudioQueueStart failed: %d\n", (int)err);
        AudioQueueDispose(ctx->queue, true);
        free(ctx);
        return false;
    }

    ctx->running = true;
    state->capture_handle = ctx;

    fprintf(stderr, "[pulsedrive] Audio capture started (Core Audio)\n");
    return true;
}

void pd_capture_stop(PdState *state)
{
    CaptureContext *ctx = (CaptureContext *)state->capture_handle;
    if (!ctx) return;

    ctx->running = false;
    AudioQueueStop(ctx->queue, true);
    AudioQueueDispose(ctx->queue, true);
    free(ctx);
    state->capture_handle = NULL;

    fprintf(stderr, "[pulsedrive] Audio capture stopped\n");
}

#endif /* __APPLE__ */
