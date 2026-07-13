#pragma once

#include "../../Audio/DjehutiAudioCableContract.h"

typedef struct _DJEHUTI_ROUTER_MINIPORT_STATE
{
    DJEHUTI_ROUTER_CABLE_SPEC cableSpec;
    unsigned long streamIndex;
} DJEHUTI_ROUTER_MINIPORT_STATE;

static __inline DJEHUTI_ROUTER_MINIPORT_STATE DjehutiRouterMakeMiniportState(void)
{
    DJEHUTI_ROUTER_MINIPORT_STATE state = { DjehutiRouterMakeCableSpec(), 0UL };
    return state;
}

