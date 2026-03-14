/*  pd_audio_capture_wasapi.cpp  --  Windows WASAPI loopback capture
 *
 *  Captures system audio output using WASAPI loopback mode.
 *  No special permissions needed on Windows.
 */
#include "pd_internal.h"

#ifdef _WIN32

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>

struct WasapiContext {
    IMMDevice         *device;
    IAudioClient      *client;
    IAudioCaptureClient *capture;
    HANDLE             thread;
    bool               running;
    PdState           *state;
};

static DWORD WINAPI capture_thread(LPVOID param)
{
    WasapiContext *ctx = (WasapiContext *)param;
    PdState *state = ctx->state;

    while (ctx->running) {
        UINT32 packet_length = 0;
        ctx->capture->GetNextPacketSize(&packet_length);

        while (packet_length > 0 && ctx->running) {
            BYTE  *data;
            UINT32 frames;
            DWORD  flags;

            HRESULT hr = ctx->capture->GetBuffer(&data, &frames, &flags, NULL, NULL);
            if (FAILED(hr)) break;

            float *samples = (float *)data;
            /* Downmix stereo to mono */
            for (UINT32 i = 0; i < frames; i++) {
                float mono = (samples[i * 2] + samples[i * 2 + 1]) * 0.5f;
                int pos = state->ring.write_pos;
                state->ring.samples[pos] = mono;
                state->ring.write_pos = (pos + 1) & PD_RING_MASK;
            }

            ctx->capture->ReleaseBuffer(frames);
            ctx->capture->GetNextPacketSize(&packet_length);
        }

        Sleep(5);  /* ~200Hz poll rate */
    }
    return 0;
}

bool pd_capture_start(PdState *state)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    IMMDeviceEnumerator *enumerator = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  (void **)&enumerator);
    if (FAILED(hr)) {
        fprintf(stderr, "[pulsedrive] Failed to create device enumerator\n");
        return false;
    }

    WasapiContext *ctx = (WasapiContext *)calloc(1, sizeof(WasapiContext));
    ctx->state = state;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &ctx->device);
    enumerator->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[pulsedrive] Failed to get default audio device\n");
        free(ctx);
        return false;
    }

    hr = ctx->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                                NULL, (void **)&ctx->client);
    if (FAILED(hr)) {
        ctx->device->Release();
        free(ctx);
        return false;
    }

    WAVEFORMATEX *fmt;
    ctx->client->GetMixFormat(&fmt);

    hr = ctx->client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                  AUDCLNT_STREAMFLAGS_LOOPBACK,
                                  200000, 0, fmt, NULL);
    CoTaskMemFree(fmt);
    if (FAILED(hr)) {
        fprintf(stderr, "[pulsedrive] WASAPI Initialize failed: 0x%x\n", (unsigned)hr);
        ctx->client->Release();
        ctx->device->Release();
        free(ctx);
        return false;
    }

    hr = ctx->client->GetService(__uuidof(IAudioCaptureClient),
                                  (void **)&ctx->capture);
    if (FAILED(hr)) {
        ctx->client->Release();
        ctx->device->Release();
        free(ctx);
        return false;
    }

    ctx->client->Start();
    ctx->running = true;
    ctx->thread = CreateThread(NULL, 0, capture_thread, ctx, 0, NULL);

    state->capture_handle = ctx;
    fprintf(stderr, "[pulsedrive] Audio capture started (WASAPI loopback)\n");
    return true;
}

void pd_capture_stop(PdState *state)
{
    WasapiContext *ctx = (WasapiContext *)state->capture_handle;
    if (!ctx) return;

    ctx->running = false;
    WaitForSingleObject(ctx->thread, 1000);
    CloseHandle(ctx->thread);

    ctx->client->Stop();
    ctx->capture->Release();
    ctx->client->Release();
    ctx->device->Release();
    free(ctx);
    state->capture_handle = NULL;

    fprintf(stderr, "[pulsedrive] Audio capture stopped\n");
}

#endif /* _WIN32 */
