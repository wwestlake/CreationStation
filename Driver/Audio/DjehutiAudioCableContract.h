#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define DJEHUTI_ROUTER_SAMPLE_RATE 48000U
#define DJEHUTI_ROUTER_CHANNELS 2U
#define DJEHUTI_ROUTER_FRAMES_PER_BUFFER 480U
#define DJEHUTI_ROUTER_BUFFER_LATENCY_MS 10U

static const wchar_t* DJEHUTI_ROUTER_MONITOR_ENDPOINT = L"Djehuti Router Monitor";
static const wchar_t* DJEHUTI_ROUTER_CAPTURE_ENDPOINT = L"Djehuti Router Capture";

#ifdef __cplusplus
}
#endif

