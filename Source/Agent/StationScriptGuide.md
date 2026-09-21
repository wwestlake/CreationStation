FRust for Station scripts. FRust looks like Rust but is a different, smaller language. Do not assume Rust rules: use exactly what is written here. Every example marked `frust` is compiled in a test, so it is valid.

## A script

A script defines `pub fn run() -> String`. Its value is reported back to the user. Everything a script does to Station goes through the API functions listed at the end.

```frust
pub fn run() -> String = {
    let track = station_track_add("Bass");
    station_track_set_volume_db(track, -6.0);
    "Added Bass"
}
```

A function is `fn name(a: i64, b: f64) -> i64 = { ... }`. Note the `=` before the body. Add helper functions next to `run`:

```frust
fn twice(a: i64) -> i64 = {
    a * 2
}

pub fn run() -> String = {
    station_log_i64("twice 4", twice(4));
    "done"
}
```

## Semicolons: the rule that causes most errors

Inside `{ ... }`, statements are separated by `;`. The last expression, with no `;`, is the value of the block. This applies to `if`, `else` and `while` blocks too: when something follows them, the closing brace needs a `;`.

```frust
pub fn run() -> String = {
    let track = station_track_add("Bass");
    if (track == 0) {
        return "could not add the track";
    };
    let ok = station_track_set_volume_db(track, -6.0);
    if (ok == 0) {
        return station_last_error();
    };
    "Added Bass at -6 dB"
}
```

The `};` after each `if` above is required. Without it the compiler reports "unexpected ... expecting }" on the NEXT line.

An `if`/`else` as the last thing in a function is its value, so it has no `;` and each branch gives a value:

```frust
pub fn run() -> String = {
    let ok = station_track_set_volume_db(1, -6.0);
    if (ok == 0) {
        station_last_error()
    } else {
        "Set the volume"
    }
}
```

## Values and types

- Whole numbers are `i64`: `3`, `-2`. Decimals are `f64` and are written with a point: `-6.0`, `0.5`. Text is `String`: `"Bass"`. Truth values are `bool`: `true`, `false`.
- Strings are written in double quotes and can contain `\n`, `\t`, `\\` and `\"`. There is NO string concatenation, NO formatting and NO `format!`, `println!` or any `name!` macro. Values have NO methods: no `.to_string()`, `.len()` and so on.
- To compare text, NEVER use `==` or `!=` (they do not compare the characters and the compiler rejects them). Use `text_equals(a, b)`, `text_equals_ignore_case(a, b)`, `text_starts_with(a, b)`, `text_ends_with(a, b)` and `text_contains(a, b)`, which give true or false, `text_compare(a, b)`, which gives -1, 0 or 1, and `text_length(a)`. For example, to find a track by name:

```frust
pub fn run() -> String = {
    let count = station_track_count();
    let mut track = 1;
    while (track <= count) {
        if (text_equals(station_track_name(track), "Bass")) {
            let ok = station_track_set_name(track, "Bass Scratch");
            if (ok == 0) {
                return station_last_error();
            };
            return "Renamed Bass to Bass Scratch";
        };
        track = track + 1;
    };
    "There is no track called Bass"
}
```
- To report a number, call `station_log_i64("label", value)` or `station_log_f64("label", value)`. To report text, call `station_log("text")` or return it from `run`.
- Convert a whole number to a decimal with `as`: `n as f64`.

## Variables

```frust
pub fn run() -> String = {
    let count = station_track_count();
    let mut total = 0;
    total = total + count;
    station_log_i64("total", total);
    "done"
}
```

`let` makes a variable. Only a variable made with `let mut` can be assigned again with `=`.

## Operators

Arithmetic `+ - * / %`, comparison `== != < > <= >=`, bitwise `& | ^ << >>`, negation `-x`, not `!x`, cast `x as f64`. There is NO `&&` and NO `||`. To test two things, nest the `if` blocks, or combine parenthesized comparisons with `&` (and) or `|` (or):

```frust
pub fn run() -> String = {
    let a = station_track_count();
    if ((a > 0) & (a < 5)) {
        "between one and four tracks"
    } else {
        "some other number"
    }
}
```

## Conditions and loops

Always put the condition of `if` and `while` in parentheses. A bare name before `{` is read as the start of a struct value and fails to compile: write `if (ok) { ... }`, never `if ok { ... }`.

```frust
pub fn run() -> String = {
    let count = station_track_count();
    if (count == 0) {
        "no tracks"
    } else if (count == 1) {
        "one track"
    } else {
        "several tracks"
    }
}
```

Loops (each is followed by `;` when more code comes after it):

```frust
pub fn run() -> String = {
    let mut track = 1;
    while (track <= station_track_count()) {
        station_track_set_muted(track, 1);
        track = track + 1;
    };
    for number in 1..3 {
        station_log_i64("number", number);
    };
    "muted every track"
}
```

`for name in a..b { ... }` counts from `a` up to but not including `b`. `loop { ... }` repeats until `break`; `continue` skips to the next round. `return value;` leaves the function early.

## Automation

An automation track draws how one control of ANOTHER track (its volume or pan) changes over time. Setting one up is three steps (add a track, make it an automation track, say what it controls); adding points is a fourth, optional step, and without points the lane just exists. A volume point's value is in dB (-60.0 to 0.0), a pan point's is -1.0 to 1.0, and its time is in seconds on the timeline. This fades the track called Clap in from -12 dB to 0 dB over ten seconds:

```frust
pub fn run() -> String = {
    let count = station_track_count();
    let mut clap = 0;
    let mut i = 1;
    while (i <= count) {
        if (text_equals(station_track_name(i), "Clap")) {
            clap = i;
        };
        i = i + 1;
    };
    if (clap == 0) {
        return "There is no track called Clap";
    };
    let lane = station_track_add("Clap volume");
    if (lane == 0) {
        return station_last_error();
    };
    if (station_track_set_kind(lane, "automation") == 0) {
        return station_last_error();
    };
    if (station_automation_set_target(lane, clap, "volume") == 0) {
        return station_last_error();
    };
    let a = station_automation_add_point(lane, 0.0, -12.0);
    let b = station_automation_add_point(lane, 10.0, 0.0);
    if ((a == 0) | (b == 0)) {
        return station_last_error();
    };
    "Added an automation track that fades the clap in over ten seconds"
}
```

Other track controls: `station_track_kind(n)` and `station_track_set_kind(n, "audio")` (audio, midi, automation, signal, foley, video, folder, marker); `station_track_set_armed`, `station_track_set_monitored`, `station_track_set_stereo` (1 or 0); `station_track_move(n, position)`; `station_automation_clear(lane)`.

## Working style

- Tracks are numbered from 1. Read the project first (`station_track_count()`, `station_track_name(n)`) when the request depends on what exists.
- A function that changes something returns 1 when it worked and 0 when it did not; `station_last_error()` says why. Check the result of a change that matters.
- If `run_frust` returns compile errors, each names the line and column in YOUR script. Fix what it names and run again. Do not send the same script twice.
- After changing the project, read it back to confirm before saying it worked, and tell the user briefly what you did.
- If you are unsure how to write something in FRust, what an error means, or what an API function does, call `frust_lookup` with a specific question instead of guessing. The help covers the whole language (structs, enums, `match`, methods), every compile error, and recipes.
