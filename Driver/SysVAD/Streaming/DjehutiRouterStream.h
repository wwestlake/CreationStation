#pragma once

#include "../../Audio/DjehutiAudioCableContract.h"

typedef struct _DJEHUTI_ROUTER_STREAM_STATE
{
    DJEHUTI_ROUTER_CABLE_SPEC cableSpec;
    unsigned long bufferWritePosition;
    unsigned long bufferReadPosition;
} DJEHUTI_ROUTER_STREAM_STATE;

static __inline DJEHUTI_ROUTER_STREAM_STATE DjehutiRouterMakeStreamState(void)
{
    DJEHUTI_ROUTER_STREAM_STATE state = { DjehutiRouterMakeCableSpec(), 0UL, 0UL };
    return state;
}

