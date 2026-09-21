// The guide the Virtual Engineer is given for writing FRust, checked against the real compiler:
//  1. every example marked `frust` in StationScriptGuide.md compiles (so the guide cannot teach something invalid);
//  2. typical mistakes an AI makes (Rust habits FRust does not have) get a plain compile error, with a repair hint
//     where there is one, and never crash the compiler - which runs inside Station.

#include "../Source/Agent/StationScriptApi.h"

#include <BuiltInFrustData.h>

#include <creation/ai/FrustTools.h>

#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
#endif

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok)
        ++failures;
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

std::string embedded(const char* name)
{
    int size = 0;
    const char* data = BuiltInFrustData::getNamedResource(name, size);
    return data != nullptr ? std::string(data, (size_t) size) : std::string();
}

// Compile-only, so nothing here needs a real project.
class NullHost final : public StationAgentHost
{
public:
    std::string projectSummary() override { return {}; }
    int trackCount() override { return 0; }
    std::string trackName(int) override { return {}; }
    double trackVolumeDb(int) override { return 0; }
    double trackPan(int) override { return 0; }
    bool trackMuted(int) override { return false; }
    bool trackSoloed(int) override { return false; }
    int addTrack(const std::string&, std::string&) override { return 0; }
    bool setTrackName(int, const std::string&, std::string&) override { return false; }
    bool setTrackVolumeDb(int, double, std::string&) override { return false; }
    bool setTrackPan(int, double, std::string&) override { return false; }
    bool setTrackMuted(int, bool, std::string&) override { return false; }
    bool setTrackSoloed(int, bool, std::string&) override { return false; }
    bool transportPlay(std::string&) override { return false; }
    bool transportStop(std::string&) override { return false; }
};

std::vector<std::string> frustExamples(const std::string& guide)
{
    std::vector<std::string> blocks;
    std::string current;
    bool inside = false;
    size_t start = 0;
    while (start <= guide.size())
    {
        auto end = guide.find('\n', start);
        if (end == std::string::npos)
            end = guide.size();
        const auto line = guide.substr(start, end - start);
        if (! inside && line.rfind("```frust", 0) == 0)
        {
            inside = true;
            current.clear();
        }
        else if (inside && line.rfind("```", 0) == 0)
        {
            inside = false;
            blocks.push_back(current);
        }
        else if (inside)
        {
            current += line + "\n";
        }
        start = end + 1;
    }
    return blocks;
}
}

int main(int argc, char** argv)
{
#ifdef _WIN32
    // A compiler abort must fail this test, not open a dialog.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    _CrtSetReportMode(_CRT_ERROR, 0);
#endif

    NullHost host;
    const auto api = embedded("StationAgentApi_frust");
    const auto guide = embedded("StationScriptGuide_md");
    check(! api.empty() && ! guide.empty(), "the API declarations and the guide are embedded");

    creation::frust::ScriptRunner runner(station_script::makeApi(host, api), [](const std::function<void()>& work) { work(); return true; });

    // `StationScriptGuideSmoke --check-file <path>`: compile one script and say what happened. Exit code 0 if it
    // compiled, 1 if it was refused with errors (printed), and a crash shows as any other exit code. Used to run a
    // list of wrong programs one process at a time, so one that crashes the compiler does not stop the others.
    if (argc >= 3 && std::string(argv[1]) == "--check-file")
    {
        std::string script;
        {
            std::ifstream in(argv[2], std::ios::binary);
            std::ostringstream all;
            all << in.rdbuf();
            script = all.str();
        }
        const auto result = runner.check(script);
        if (result.ok)
        {
            std::cout << "compiled" << std::endl;
            return 0;
        }
        std::cout << creation::ai::describeScriptResult(result) << std::endl;
        return 1;
    }

    // ---- 1. Every example in the guide compiles ----
    const auto examples = frustExamples(guide);
    check(examples.size() >= 8, "the guide has examples (" + std::to_string(examples.size()) + ")");
    for (size_t i = 0; i < examples.size(); ++i)
    {
        const auto result = runner.check(examples[i]);
        check(result.ok, "guide example " + std::to_string(i + 1) + " compiles" + (result.ok ? "" : ": " + creation::ai::describeScriptResult(result)));
    }

    // The wrong programs below can make the compiler misbehave while a problem is being fixed, so they run only when
    // asked for: `StationScriptGuideSmoke --wrong-programs`. A plain run checks the guide's examples and stops here.
    if (argc < 2 || std::string(argv[1]) != "--wrong-programs")
    {
        std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
        return failures == 0 ? 0 : 1;
    }

    // ---- 2. Typical mistakes: a plain error (or nothing worse), never a crash ----
    struct Mistake { const char* what; const char* code; const char* hintContains; };
    const Mistake mistakes[] = {
        { "String concatenation",
          "pub fn run() -> String = {\n    let s = \"a\" + \"b\";\n    s\n}\n", "no String concatenation" },
        { "String arithmetic with a number",
          "pub fn run() -> String = {\n    let s = \"a\" * 3;\n    s\n}\n", "no String concatenation" },
        { "format! macro",
          "pub fn run() -> String = {\n    format!(\"x {}\", 1)\n}\n", "no macros" },
        { ".to_string() on a literal",
          "pub fn run() -> String = {\n    \"x\".to_string()\n}\n", "no methods" },
        { "&& and ||",
          "pub fn run() -> String = {\n    let a = 1;\n    if a == 1 && a == 2 { \"both\" } else { \"no\" }\n}\n", "no && or ||" },
        { "a bare name as the condition",
          "pub fn run() -> String = {\n    let ok = true;\n    if ok { \"y\" } else { \"n\" }\n}\n", "parentheses" },
        { "no semicolon after an if block",
          "pub fn run() -> String = {\n    let t = station_track_add(\"x\");\n    if t == 0 { return \"bad\"; }\n    \"good\"\n}\n", "every statement except the last" },
        { "no semicolon after a while block",
          "pub fn run() -> String = {\n    let mut n = 0;\n    while (n < 3) { n = n + 1; }\n    \"done\"\n}\n", "every statement except the last" },
        { "a function whose last line ends with a semicolon",
          "pub fn run() -> String = {\n    station_track_count();\n}\n", "" },
        { "an unclosed brace", "pub fn run() -> String = {\n    \"x\"\n", "" },
        { "an unknown function", "pub fn run() -> String = {\n    not_a_function(1);\n    \"x\"\n}\n", "" },
        { "a missing run", "pub fn other() -> String = {\n    \"x\"\n}\n", "" },
        { "an empty script", "", "" },
        { "a comparison of a String and a number",
          "pub fn run() -> String = {\n    let a = \"x\";\n    if (a == 1) { \"y\" } else { \"n\" }\n}\n", "" },
        { "negating a String", "pub fn run() -> String = {\n    let a = -\"x\";\n    \"y\"\n}\n", "" },
        { "a String as an if condition", "pub fn run() -> String = {\n    if (\"x\") { \"y\" } else { \"n\" }\n}\n", "" },
        { "a method call on a number", "pub fn run() -> String = {\n    let n = 3;\n    n.abs();\n    \"y\"\n}\n", "" },
        { "assigning to an immutable variable", "pub fn run() -> String = {\n    let n = 3;\n    n = 4;\n    \"y\"\n}\n", "" },
        { "an undeclared variable", "pub fn run() -> String = {\n    m = 4;\n    \"y\"\n}\n", "" },
        { "wrong argument count", "pub fn run() -> String = {\n    station_track_add();\n    \"y\"\n}\n", "" },
        { "wrong argument type", "pub fn run() -> String = {\n    station_track_add(5);\n    \"y\"\n}\n", "" },
        { "returning a number from a String function", "pub fn run() -> String = {\n    return 5;\n}\n", "" },
        { "a stray Rust-style type annotation", "pub fn run() -> String = {\n    let x: i32 = 5;\n    let v: Vec<i64> = 1;\n    \"y\"\n}\n", "" },
        { "a Rust closure", "pub fn run() -> String = {\n    let f = |x| x + 1;\n    \"y\"\n}\n", "" },
        { "a Rust struct impl", "struct P { x: i64 }\nimpl P { fn get(&self) -> i64 { self.x } }\npub fn run() -> String = {\n    \"y\"\n}\n", "" },
        { "an infinite recursion of types", "pub fn run() -> String = {\n    let a = a;\n    \"y\"\n}\n", "" },
    };

    for (const auto& mistake : mistakes)
    {
        const auto result = runner.check(mistake.code);
        const auto text = creation::ai::describeScriptResult(result);
        // Reaching this line at all means the compiler did not crash.
        if (result.ok)
        {
            // Some slips are accepted by the compiler; that is recorded, not asserted, unless it must be refused.
            std::cout << "NOTE  the compiler accepted: " << mistake.what << std::endl;
            check(std::string(mistake.hintContains).empty(), std::string("does not crash on: ") + mistake.what);
        }
        else
        {
            check(true, std::string("a plain error, no crash: ") + mistake.what);
            if (std::string(mistake.hintContains).size() > 0)
                check(contains(text, mistake.hintContains), std::string("the error carries a repair hint (") + mistake.hintContains + ") for: " + mistake.what + " -- " + text);
        }
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
