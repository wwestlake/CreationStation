#pragma once

#include <ntddk.h>
#include <wdf.h>
#include "../Audio/DjehutiAudioCableContract.h"

typedef struct _DJEHUTI_ROUTER_CABLE_ENDPOINT
{
    const wchar_t* name;
    const wchar_t* role;
    const wchar_t* direction;
} DJEHUTI_ROUTER_CABLE_ENDPOINT;

typedef struct _DJEHUTI_ROUTER_CABLE_SPEC
{
    unsigned int sampleRate;
    unsigned int channelCount;
    unsigned int framesPerBuffer;
    unsigned int bufferLatencyMs;
    DJEHUTI_ROUTER_CABLE_ENDPOINT renderEndpoint;
    DJEHUTI_ROUTER_CABLE_ENDPOINT captureEndpoint;
} DJEHUTI_ROUTER_CABLE_SPEC;

static __inline DJEHUTI_ROUTER_CABLE_SPEC DjehutiRouterMakeCableSpec(void)
{
    DJEHUTI_ROUTER_CABLE_SPEC spec = {
        DJEHUTI_ROUTER_SAMPLE_RATE,
        DJEHUTI_ROUTER_CHANNELS,
        DJEHUTI_ROUTER_FRAMES_PER_BUFFER,
        DJEHUTI_ROUTER_BUFFER_LATENCY_MS,
        { L"Djehuti Router Monitor", L"monitor", L"render" },
        { L"Djehuti Router Capture", L"capture", L"capture" },
    };

    return spec;
}

EVT_WDF_DRIVER_DEVICE_ADD DriverEvtDeviceAdd;
