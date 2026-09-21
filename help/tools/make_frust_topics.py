#!/usr/bin/env python3
"""Writes Station's FRust help topics (help/topics/en-US/djehuti.station.frust-*.json).

These topics serve two readers, like every help topic: people learning to script Station, and the Virtual Engineer, which
looks them up when it writes FRust. The language topics come from the grammar (third_party/FrustLang .../grammar/frust.y
and frust.l); every code example was compiled against the real compiler before it was written here. Run from the Station
folder:  python help/tools/make_frust_topics.py
"""

import json
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "topics" / "en-US"


def topic(topic_id, kind, title, summary, keywords, questions, blocks, related=()):
    return {
        "schemaVersion": "1.0",
        "id": topic_id,
        "locale": "en-US",
        "type": kind,
        "title": title,
        "summary": summary,
        "product": "Djehuti Station",
        "status": "draft",
        "audiences": ["user", "support", "virtual-engineer"],
        "owners": ["Djehuti Station"],
        "appliesTo": {"since": "0.9.1-beta6", "platforms": ["windows"]},
        "contexts": [],
        "keywords": list(keywords),
        "alternateQueries": list(questions),
        "relations": [{"type": "next", "topicId": r} for r in related],
        "blocks": [{"id": f"b{i}", "kind": k, "text": t} if k in ("paragraph", "note", "warning") else dict(t, id=f"b{i}", kind=k)
                   for i, (k, t) in enumerate(blocks, start=1)],
        "contentRevision": 1,
        "reviewedAt": "2026-09-20",
        "reviewedBy": "Claude (from the FRust grammar and the compiler, not yet reviewed by a person)",
    }


def para(text): return ("paragraph", text)
def note(text): return ("note", text)
def warn(text): return ("warning", text)
def define(term, meaning): return ("definition", {"term": term, "definition": meaning})
def trouble(symptom, cause, fix): return ("troubleshooting", {"symptoms": [symptom], "cause": cause, "resolution": fix})


TOPICS = [
    topic(
        "djehuti.station.frust-language",
        "reference",
        "FRust: the language, for writing scripts",
        "How to write FRust: script shape, blocks and semicolons, values, variables, operators, if, loops, functions, structs, enums and match, and what FRust does not have.",
        ["frust", "script", "syntax", "language", "semicolon", "if", "while", "for", "loop", "function", "fn", "let", "struct", "enum", "match",
         "operator", "string", "text", "i64", "f64", "bool", "return", "impl", "&&", "||", "format", "to_string"],
        ["How do I write a FRust script?", "What is the syntax of FRust?", "Does FRust have &&?", "How do I loop in FRust?",
         "How do I define a function in FRust?", "How do I use match in FRust?", "Why does FRust need a semicolon after if?"],
        [
            para("FRust looks like Rust but is a different, smaller language. Do not assume Rust rules; use what is written here. Every example on this page was compiled against the real compiler."),
            para("A script defines `pub fn run() -> String`. What it returns is reported back. A function is `fn name(a: i64, b: f64) -> i64 = { ... }`, with an `=` before the body.\n\n"
                 "```frust\nfn twice(a: i64) -> i64 = {\n    a * 2\n}\n\npub fn run() -> String = {\n    station_log_i64(\"twice 4\", twice(4));\n    \"done\"\n}\n```"),
            para("Blocks and semicolons. Inside `{ ... }`, statements are separated by `;`. The last expression, with no `;`, is the value of the block. This includes `if`, `else`, `while`, `for` and `match` blocks: when something follows them, the closing brace needs a `;`.\n\n"
                 "```frust\npub fn run() -> String = {\n    let track = station_track_add(\"Bass\");\n    if (track == 0) {\n        return \"could not add the track\";\n    };\n    \"Added Bass\"\n}\n```\n\n"
                 "Without the `};` after the `if`, the compiler reports \"unexpected ... expecting }\" on the NEXT line, so the real mistake is on the line before the one reported."),
            para("An `if`/`else` as the last thing in a function is its value, so it has no `;` and each branch gives a value of the same kind.\n\n"
                 "```frust\npub fn run() -> String = {\n    let ok = station_track_set_volume_db(1, -6.0);\n    if (ok == 0) {\n        station_last_error()\n    } else {\n        \"Set the volume\"\n    }\n}\n```"),
            define("Values", "Whole numbers are i64 (3, -2). Decimals are f64 and are written with a point (-6.0, 0.5). Text is String, in double quotes, with \\n \\t \\\\ and \\\" as escapes. Truth values are bool (true, false). A list is [1, 2, 3], all items the same kind."),
            define("No text operations", "FRust has NO string concatenation, NO formatting, NO macros such as format! or println!, and NO methods on values (no .to_string(), .len()). To report a number call station_log_i64 or station_log_f64; to report text call station_log or return it."),
            para("Variables. `let` makes a variable; only one made with `let mut` can be assigned again with `=`. Convert a whole number to a decimal with `as`: `n as f64`.\n\n"
                 "```frust\npub fn run() -> String = {\n    let count = station_track_count();\n    let mut total = 0;\n    total = total + count;\n    station_log_i64(\"total\", total);\n    \"done\"\n}\n```"),
            define("Comparing text", "NEVER compare text with == or !=: that compares where the texts are stored, not their characters, so it is always wrong and the compiler rejects it. "
                                      "Use text_equals(a, b), text_equals_ignore_case(a, b), text_starts_with(a, b), text_ends_with(a, b) and text_contains(a, b), which give true or false; "
                                      "text_compare(a, b), which gives -1, 0 or 1; and text_length(a). Example: if (text_equals(station_track_name(track), \"Bass\")) { ... }. "
                                      "Comparison is by character; the ignore-case form treats A-Z and a-z as the same."),
            define("Operators","Arithmetic + - * / %; comparison == != < > <= >=; bitwise & | ^ << >>; negation -x; not !x; cast x as f64. There is NO && and NO ||. To test two things, nest the if blocks, or combine parenthesized comparisons with & (and) or | (or)."),
            para("```frust\npub fn run() -> String = {\n    let a = station_track_count();\n    if ((a > 0) & (a < 5)) {\n        \"between one and four tracks\"\n    } else {\n        \"some other number\"\n    }\n}\n```"),
            warn("Always put the condition of `if` and `while` in parentheses. A bare name before `{` is read as the start of a struct value and fails to compile: write `if (ok) { ... }`, never `if ok { ... }`."),
            para("Chained conditions and loops. A loop is followed by `;` when more code comes after it.\n\n"
                 "```frust\npub fn run() -> String = {\n    let count = station_track_count();\n    if (count == 0) {\n        \"no tracks\"\n    } else if (count == 1) {\n        \"one track\"\n    } else {\n        \"several tracks\"\n    }\n}\n```\n\n"
                 "```frust\npub fn run() -> String = {\n    let mut track = 1;\n    while (track <= station_track_count()) {\n        station_track_set_muted(track, 1);\n        track = track + 1;\n    };\n    for number in 1..3 {\n        station_log_i64(\"number\", number);\n    };\n    \"muted every track\"\n}\n```\n\n"
                 "`for name in a..b { ... }` counts from a up to but not including b. `loop { ... }` repeats until `break`; `continue` skips to the next round; `return value;` leaves the function early."),
            para("Structs, enums and match.\n\n"
                 "```frust\nstruct Point { x: f64, y: f64 }\n\npub fn run() -> String = {\n    let p = Point { x: 1.0, y: 2.0 };\n    let total = p.x + p.y;\n    \"ok\"\n}\n```\n\n"
                 "```frust\nenum Shape { Circle(f64), Empty }\n\nfn area(s: Shape) -> f64 = {\n    match (s) {\n        Shape::Circle(r) => r * r * 3.0;\n        Shape::Empty => 0.0\n    }\n}\n\npub fn run() -> String = {\n    let a = area(Shape::Circle(2.0));\n    \"ok\"\n}\n```"),
            warn("match arms are NOT separated by commas. Each arm is `pattern => expression`, optionally followed by `;`, and the last arm has none. The value being matched is in parentheses: `match (n) { ... }`. Every arm must give the same kind of value."),
            para("```frust\npub fn run() -> String = {\n    let n = 2;\n    match (n) {\n        1 => \"one\";\n        2 => \"two\";\n        _ => \"many\"\n    }\n}\n```\n\n"
                 "Methods: `impl Name { fn method(self) -> i64 = { self.field + 1 } }` adds methods to a struct, called as `value.method()`.\n\n"
                 "```frust\nstruct Counter { n: i64 }\n\nimpl Counter {\n    fn bump(self) -> i64 = { self.n + 1 }\n}\n\npub fn run() -> String = {\n    let c = Counter { n: 1 };\n    let k = c.bump();\n    \"ok\"\n}\n```"),
            note("Comments start with // and run to the end of the line, or sit between /* and */. FRust also has effects, smart pointers (own, shared, weak, raw*), refinement types and quote/unquote; scripts for Station rarely need them."),
        ],
        related=("djehuti.station.frust-compile-errors", "djehuti.station.frust-station-api"),
    ),
    topic(
        "djehuti.station.frust-compile-errors",
        "troubleshooting",
        "FRust compile errors and how to fix them",
        "What each common FRust compile error means and how to fix it.",
        ["frust", "error", "compile", "syntax error", "unexpected", "expecting", "fix", "semicolon", "operator", "condition", "return value"],
        ["Why does my FRust script not compile?", "What does 'unexpected STRING_LITERAL, expecting }' mean?", "What does 'has no return value' mean?"],
        [
            para("A compile error names a line and column in your script. The cause is often on the line BEFORE the one named."),
            trouble("unexpected STRING_LITERAL, expecting }  (or unexpected IDENT, expecting })",
                    "A block statement (`if`, `else`, `while`, `for`, `match`) is followed by more code but its closing brace has no `;`. Or an `if`/`while` condition is a bare name before `{`, which is read as a struct value.",
                    "Write `};` after the block, and put every condition in parentheses: `if (ok) { ... };`."),
            trouble("unexpected !, expecting }",
                    "A macro such as format!(...) or println!(...) was used. FRust has no macros.",
                    "Return a plain string literal, and report numbers with station_log_i64 or station_log_f64."),
            trouble("cannot call a method on an expression of unknown struct type",
                    "A method was called on a value that has none, such as \"x\".to_string() or n.abs().",
                    "Values have no methods. Use plain functions from the API; only a struct with an `impl` block has methods."),
            trouble("unexpected &  (from &&)",
                    "FRust has no && or ||.",
                    "Nest the if blocks, or write `if ((a == 1) & (b == 2)) { ... }`; use | for or."),
            trouble("the operator '+' does not work on text",
                    "Text was added, subtracted or otherwise combined with an operator. There is no string concatenation.",
                    "Report separate pieces with separate station_log calls."),
            trouble("function 'run' has no return value",
                    "The last line of the function ends with `;` or is not an expression.",
                    "End the function with an expression and no `;`, for example the text to report."),
            trouble("the condition of 'if' must be a true/false value or a number, not text",
                    "A String was used as a condition.",
                    "Compare it first, or use a number or a true/false value."),
            trouble("the two branches of this 'if' give different kinds of value",
                    "One branch gives a number and the other text.",
                    "Make both branches give the same kind of value."),
            trouble("the script has no `pub fn run() -> String`",
                    "The entry function is missing or named differently.",
                    "Define `pub fn run() -> String = { ... }`."),
            trouble("argument N of 'f' must be ..., or 'f' takes N argument(s)",
                    "A call passes the wrong number or kind of arguments.",
                    "Check the function's declaration in the Station script API topic."),
        ],
        related=("djehuti.station.frust-language",),
    ),
    topic(
        "djehuti.station.frust-station-api",
        "reference",
        "Station's script API: every function a script can call",
        "The functions Station gives a FRust script: reading the project, changing tracks, the transport, and reporting results.",
        ["station api", "script", "frust", "track", "volume", "pan", "mute", "solo", "transport", "station_", "add track", "log"],
        ["What can a FRust script do in Station?", "How do I add a track from a script?", "How do I set a track's volume from a script?",
         "How do I report a number from a script?"],
        [
            para("Tracks are numbered from 1, as you see them. A function that changes something returns 1 when it worked and 0 when it did not; call station_last_error() after a 0 to learn why. Volumes are in decibels from -60.0 up to 0.0 (0 is the top of the fader; there is no boost)."),
            define("Reading the project", "station_project_summary() -> String; station_track_count() -> i64; station_track_name(track: i64) -> String; station_track_volume_db(track: i64) -> f64; station_track_pan(track: i64) -> f64 (-1.0 left to 1.0 right); station_track_muted(track: i64) -> i64 (1 or 0); station_track_soloed(track: i64) -> i64 (1 or 0)."),
            define("Changing tracks", "station_track_add(name: String) -> i64 (the new track's number, or 0); station_track_set_name(track: i64, name: String) -> i64; station_track_set_volume_db(track: i64, db: f64) -> i64; station_track_set_pan(track: i64, pan: f64) -> i64; station_track_set_muted(track: i64, muted: i64) -> i64; station_track_set_soloed(track: i64, soloed: i64) -> i64."),
            define("The rest of a track's controls", "station_track_kind(track: i64) -> String (audio, midi, automation, signal, foley, video, folder or marker); station_track_set_kind(track: i64, kind: String) -> i64; "
                                                     "station_track_armed / station_track_monitored / station_track_stereo(track: i64) -> i64 (1 or 0); station_track_set_armed / set_monitored / set_stereo(track: i64, on: i64) -> i64; "
                                                     "station_track_move(track: i64, destination: i64) -> i64."),
            define("Automation", "An automation track draws how one control of another track changes over time. Setup is three steps: station_track_add(name) to make a track, station_track_set_kind(track, \"automation\") to make it an automation track, "
                                 "station_automation_set_target(automation_track: i64, target_track: i64, control: String) -> i64 with control \"volume\" or \"pan\". Points are optional: "
                                 "station_automation_add_point(automation_track: i64, seconds: f64, value: f64) -> i64 (value in dB, -60.0 to 0.0, for volume; -1.0 to 1.0 for pan); station_automation_clear(automation_track: i64) -> i64 removes all points."),
            define("Transport", "station_transport_play() -> i64; station_transport_stop() -> i64."),
            define("Reporting", "station_log(text: String) -> i64 adds a line to the result; station_log_i64(label: String, value: i64) -> i64 and station_log_f64(label: String, value: f64) -> i64 add the line \"label: value\"; station_last_error() -> String says why the last call that returned 0 failed."),
            para("Recipes, each compiled against the real compiler.\n\nAdd two tracks and set them up:\n\n"
                 "```frust\npub fn run() -> String = {\n    let drums = station_track_add(\"Drums\");\n    let bass = station_track_add(\"Bass\");\n    station_track_set_volume_db(drums, -8.0);\n    station_track_set_volume_db(bass, -6.0);\n    station_track_set_pan(bass, 0.3);\n    \"Added Drums and Bass\"\n}\n```\n\n"
                 "Report every track:\n\n"
                 "```frust\npub fn run() -> String = {\n    let count = station_track_count();\n    station_log_i64(\"tracks\", count);\n    for track in 1..(count + 1) {\n        station_log(station_track_name(track));\n        station_log_f64(\"volume dB\", station_track_volume_db(track));\n    };\n    \"done\"\n}\n```\n\n"
                 "Set every track to -12 dB:\n\n"
                 "```frust\npub fn run() -> String = {\n    let mut track = 1;\n    while (track <= station_track_count()) {\n        station_track_set_volume_db(track, -12.0);\n        track = track + 1;\n    };\n    \"All tracks set to -12 dB\"\n}\n```\n\n"
                 "Rename and pan track 1:\n\n"
                 "```frust\npub fn run() -> String = {\n    station_track_set_name(1, \"Lead\");\n    station_track_set_pan(1, -0.5);\n    \"Track 1 is now Lead, panned left\"\n}\n```\n\n"
                 "List the muted tracks:\n\n"
                 "```frust\npub fn run() -> String = {\n    let count = station_track_count();\n    if (count == 0) {\n        return \"The project has no tracks\";\n    };\n    let mut track = 1;\n    while (track <= count) {\n        if (station_track_muted(track) == 1) {\n            station_log(station_track_name(track));\n        };\n        track = track + 1;\n    };\n    \"Listed the muted tracks above\"\n}\n```"),
        ],
        related=("djehuti.station.frust-language", "djehuti.station.assistant-writes-scripts"),
    ),
    topic(
        "djehuti.station.assistant-writes-scripts",
        "explanation",
        "How the Virtual Engineer writes and runs scripts",
        "How the Virtual Engineer uses FRust to change your project: the tools it has, how a run works, what you see, and its limits.",
        ["virtual engineer", "assistant", "script", "frust", "run_frust", "check_frust", "tools", "stop", "limits", "scripts run", "coder"],
        ["How does the Virtual Engineer change my project?", "What tools does the Virtual Engineer have?", "How do I stop the Virtual Engineer?",
         "What are Scripts run?"],
        [
            para("The Virtual Engineer changes your project by writing a short FRust script and running it. A script can only do what Station's script API offers (see the Station script API topic): it cannot reach your files, the network or other programs."),
            define("Tools", "run_frust compiles and runs a script and reports what it returned and logged, or the compile errors with line and column. check_frust compiles a script without running it. frust_api_reference shows Station's script API. frust_lookup searches Station's help (the FRust language, the API, how the tools work) when the assistant is unsure."),
            para("A request runs like this: the assistant writes a script, runs it, reads the result (or the compile errors), and fixes and retries until it works, then tells you what it did. It works on its own thread, so Station stays usable. The arrow at the end of the message box becomes Stop while it works; Stop ends the request at the script's next call into Station."),
            para("Every script it ran is shown under \"Scripts run\" in its reply, with whether it ran or the error it failed with, so nothing it did is hidden."),
            note("Limits: a request has a step limit, a time limit and a limit on repeating the same failing call. A script that never calls back into Station cannot be interrupted until it returns."),
        ],
        related=("djehuti.station.frust-station-api", "djehuti.station.frust-language"),
    ),
]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for t in TOPICS:
        path = OUT / f"{t['id']}.json"
        with open(path, "w", encoding="utf-8", newline="\n") as out:
            out.write(json.dumps(t, indent=2, ensure_ascii=False) + "\n")
        print("wrote", path.name)


if __name__ == "__main__":
    main()
