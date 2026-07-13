#pragma once

#include "../../Audio/DjehutiAudioCableContract.h"

typedef struct _DJEHUTI_ROUTER_TOPOLOGY_STATE
{
    DJEHUTI_ROUTER_CABLE_SPEC cableSpec;
    unsigned long nodeCount;
} DJEHUTI_ROUTER_TOPOLOGY_STATE;

static __inline DJEHUTI_ROUTER_TOPOLOGY_STATE DjehutiRouterMakeTopologyState(void)
{
    DJEHUTI_ROUTER_TOPOLOGY_STATE state = { DjehutiRouterMakeCableSpec(), 0UL };
    return state;
}

