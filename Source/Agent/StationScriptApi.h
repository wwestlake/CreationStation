#pragma once

#include "StationAgentHost.h"

#include <creation/frust/ScriptRunner.h>

#include <string>

// Station's API for FRust scripts: the functions declared in StationAgentApi.frust, carried out through a
// StationAgentHost. The running script and its limits belong to the shared creation::frust::ScriptRunner;
// this is only what Station offers it.
//
// `declarations` is the text of StationAgentApi.frust. The host travels with the API (ScriptApi::userData), so
// each run reaches its own host and several can exist side by side; the host must outlive its runs.
namespace station_script
{
creation::frust::ScriptApi makeApi(StationAgentHost& host, std::string declarations);
}
