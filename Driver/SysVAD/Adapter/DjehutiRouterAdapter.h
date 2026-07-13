#pragma once

#include "../../Audio/DjehutiAudioCableContract.h"

typedef struct _DJEHUTI_ROUTER_ADAPTER_STATE
{
    DJEHUTI_ROUTER_CABLE_SPEC cableSpec;
    unsigned long deviceInstanceId;
} DJEHUTI_ROUTER_ADAPTER_STATE;

static __inline DJEHUTI_ROUTER_ADAPTER_STATE DjehutiRouterMakeAdapterState(void)
{
    DJEHUTI_ROUTER_ADAPTER_STATE state = { DjehutiRouterMakeCableSpec(), 0UL };
    return state;
}

