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

## Working style

- Tracks are numbered from 1. Read the project first (`station_track_count()`, `station_track_name(n)`) when the request depends on what exists.
- A function that changes something returns 1 when it worked and 0 when it did not; `station_last_error()` says why. Check the result of a change that matters.
- If `run_frust` returns compile errors, each names the line and column in YOUR script. Fix what it names and run again. Do not send the same script twice.
- After changing the project, read it back to confirm before saying it worked, and tell the user briefly what you did.
