#pragma once

#include "../../Audio/DjehutiAudioCableContract.h"

typedef struct _DJEHUTI_ROUTER_RING_BUFFER_STATE
{
    DJEHUTI_ROUTER_CABLE_SPEC cableSpec;
    unsigned long frameCount;
    unsigned long writeCursor;
    unsigned long readCursor;
} DJEHUTI_ROUTER_RING_BUFFER_STATE;

static __inline DJEHUTI_ROUTER_RING_BUFFER_STATE DjehutiRouterMakeRingBufferState(void)
{
    DJEHUTI_ROUTER_RING_BUFFER_STATE state = { DjehutiRouterMakeCableSpec(), 0UL, 0UL, 0UL };
    return state;
}

