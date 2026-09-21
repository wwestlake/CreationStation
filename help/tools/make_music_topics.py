#!/usr/bin/env python3
"""Writes Station's music-interpretation help topics (help/topics/en-US/djehuti.station.music-*.json).

They serve the Producer role of the Virtual Engineer: it measures a track (pitch, tempo, beat) and these cards tell it how to
READ the numbers, so it can tell a mistake from a deliberate expressive choice (a bend, a slide, a triplet feel, a laid-back
groove) instead of calling everything that is not exactly on the grid or on pitch an error. They are also readable by a person
who wants to understand the report. Run from the Station folder:  python help/tools/make_music_topics.py
"""

import json
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "topics" / "en-US"


def topic(topic_id, kind, title, summary, keywords, questions, blocks, related=()):
    return {
        "schemaVersion": "1.0", "id": topic_id, "locale": "en-US", "type": kind, "title": title, "summary": summary,
        "product": "Djehuti Station", "status": "draft", "audiences": ["user", "support", "virtual-engineer"],
        "owners": ["Djehuti Station"], "appliesTo": {"since": "0.9.1-beta6", "platforms": ["windows"]}, "contexts": [],
        "keywords": list(keywords), "alternateQueries": list(questions),
        "relations": [{"type": "next", "topicId": r} for r in related],
        "blocks": [{"id": f"b{i}", "kind": k, "text": t} if k in ("paragraph", "note", "warning") else dict(t, id=f"b{i}", kind=k)
                   for i, (k, t) in enumerate(blocks, start=1)],
        "contentRevision": 1, "reviewedAt": "2026-09-20",
        "reviewedBy": "Claude (from general music practice, not yet reviewed by a person)",
    }


def para(text): return ("paragraph", text)
def note(text): return ("note", text)
def warn(text): return ("warning", text)
def define(term, meaning): return ("definition", {"term": term, "definition": meaning})


TOPICS = [
    topic(
        "djehuti.station.music-pitch-interpretation",
        "explanation",
        "Reading pitch measurements: out of tune, or expressive?",
        "How to interpret a pitch report: what bends, slides, scoops, vibrato, blue notes and real tuning problems look like in the numbers.",
        ["pitch", "tuning", "cents", "sharp", "flat", "bend", "slide", "glissando", "portamento", "scoop", "fall", "vibrato", "blue note",
         "intonation", "out of tune", "wobble", "guitar", "vocal", "expressive", "intent", "motion", "rises", "falls"],
        ["Is this note out of tune or a bend?", "How do I tell vibrato from wobble?", "What does 'rises 100 cents across the note' mean?",
         "Is a slide a mistake?", "What is a blue note?", "How sharp is too sharp?"],
        [
            para("A pitch report gives, for each note, how many cents sharp (+) or flat (-) its steady middle sits, how much it wobbles, and, when they "
                 "happen, how the pitch moves across the note and whether it has vibrato. A cent is a hundredth of a semitone. As a first guide: under about "
                 "10 cents most listeners cannot hear; 10 to 25 a trained ear notices; over about 25 is clearly out. But the number alone never says whether "
                 "it is a mistake. Read it together with the motion, the vibrato, and the context (see the topic on judging intent)."),
            define("Bend", "The pitch rises (or falls) through the note toward a target, typical of guitar. In the report: the note \"rises N cents across the note\". "
                           "A clean bend covers a musical step: about 100 cents is a semitone, 200 a whole tone, 50 a quarter-tone bend. If the motion is close to a "
                           "multiple of 100 and the note then holds steady, the bend is probably deliberate and on target. The note's own cents figure describes its "
                           "steady middle, so for a note that moves by 50 cents or more do not judge its tuning from that figure alone; judge where it ends up."),
            define("Slide, glissando, portamento", "A smooth move from one note to another. It shows as a note with a large rise or fall across it, or as a short "
                                                    "glide between two steady notes. If the glide is smooth in one direction and arrives at the next note steady, it is expressive, not a wrong note."),
            define("Scoop and fall-off", "A scoop starts below the pitch and swoops up to it (common in singing, blues, country); a fall-off drops away at the end "
                                         "of a note. In the numbers: a rise across the note that lands close to the note's centre (a scoop), or a fall at the end. If it lands on pitch, it is "
                                         "style. If a note starts on pitch and simply sags flat at the end of every long note, that is more likely breath or support than style."),
            define("Vibrato", "Regular oscillation of the pitch. Typical: about 5 to 7 cycles a second, roughly 20 to 100 cents either side (singers and string players "
                              "often more, guitarists a bit less). It is expressive when it is regular, starts after the note settles, and is centred on the target pitch. "
                              "Slow (under about 4 Hz), very wide, or uneven oscillation is more likely a wobble. Judge the tuning of a note with vibrato by its centre, "
                              "which is the cents figure reported, not by the extremes."),
            define("Wobble versus vibrato", "Wobble is irregular and unintended: no steady rate, often growing toward the end of long notes or when tired. "
                                            "No vibrato is reported for it. If the report shows a large spread and no vibrato, treat it as unsteadiness."),
            define("Blue notes and expressive intonation", "Some styles deliberately place certain notes flat or sharp, for example the flat third and seventh in blues. "
                                                            "The mark of intent is consistency: the same scale degree is off in the same direction every time it appears."),
            define("Common tendencies", "Singers and players often go sharp when pushing, excited or loud, and flat when tired, quiet or short of breath. Guitarists "
                                        "fretting hard tend sharp. A steady overall lean in one direction (the report's overall figure) points at tuning of the instrument "
                                        "or of the whole take rather than at individual notes."),
            warn("Limits. The pitch tool measures one melodic line at a time. Chords, double stops, heavy distortion, reverb tails and other instruments bleeding "
                 "in can give wrong or missing notes; a note may be read an octave away. A low confidence or few notes found means the measurement is unreliable, "
                 "not that the playing is fine. Check the tuning reference (A = 440 Hz by default) if everything reads consistently sharp or flat."),
        ],
        related=("djehuti.station.music-judging-intent", "djehuti.station.music-rhythm-interpretation"),
    ),
    topic(
        "djehuti.station.music-rhythm-interpretation",
        "explanation",
        "Reading tempo and beat measurements: sloppy, or a feel?",
        "How to interpret a tempo and beat report: swing, triplets, pushing and laying back, rushing, rubato, ghost notes and real timing problems.",
        ["tempo", "beat", "rhythm", "timing", "swing", "shuffle", "triplet", "straight", "sixteenth", "eighth", "groove", "push", "pull", "laid back",
         "rushing", "dragging", "rubato", "drift", "ghost note", "syncopation", "polyrhythm", "tight", "loose", "humanize", "feel", "pocket"],
        ["Is my timing bad or is it swing?", "What does laid back mean?", "Am I rushing?", "What is a ghost note?", "How tight should I be?",
         "What is a triplet feel?"],
        [
            para("A tempo report gives the BPM and how sure it is, how steady the beat is, how tightly the hits sit on the beat grid (in milliseconds), whether they lean "
                 "ahead of or behind the beat, which grid they fit best, and the rhythm pattern bar by bar. Tightness is judged against the grid the playing actually "
                 "uses, so a swung or triplet groove is not called loose for not being straight."),
            define("Grid and feel", "Straight eighths or sixteenths put hits at even fractions of the beat. Triplets or swing put them on thirds of the beat: swing plays "
                                    "pairs of eighth notes long-short (about 2:1, the first and the third of a triplet). Sextuplets are six to a beat. If the report says the hits "
                                    "sit best on triplets or swing, the player is swinging on purpose, and the offsets from a straight grid are the swing, not error."),
            define("Tightness numbers", "Average distance from the grid: under about 10 ms is very tight (studio or programmed feel), 10 to 25 ms is a natural human feel, "
                                        "over about 30 ms is audibly loose. Spread says how consistent that distance is: a small spread with a large average distance means "
                                        "consistently early or late (a feel), a large spread means uneven timing."),
            define("Pushing and laying back", "The lean figure is the average distance ahead (-) or behind (+) of the beat. Leaning ahead 5 to 15 ms feels urgent and "
                                              "driving; leaning behind 10 to 30 ms feels relaxed and heavy (a laid-back snare or bass). A steady lean is a choice of feel. "
                                              "It is only a problem if it changes over the take or fights the other instruments."),
            define("Rushing and dragging", "Rushing is speeding up unintentionally; it shows as the tempo drifting upward across the section (positive drift in BPM) and "
                                           "the hits moving ahead of the original grid. Dragging is the opposite. Small drift in a groove-based style is usually a fault; "
                                           "in a ballad or a rubato passage it can be breathing."),
            define("Rubato", "Deliberate flexing of the tempo for expression: speeding into a phrase and easing at the end, in both directions, often at phrase boundaries. "
                             "It fits slower, expressive music. In dance, funk, rock and anything played to a click it is not expected."),
            define("Ghost notes and syncopation", "Ghost notes are very quiet hits between the main ones; they show as lighter marks (x) in the pattern and give a groove its texture. "
                                                  "Syncopation puts accents on the off-beats or leaves the expected beat empty; it shows as strong marks (X) between the beats or gaps on the beats. "
                                                  "Neither is a timing error."),
            define("Slip versus habit", "One hit far off (over about 40 ms) while the rest are tight is a slip. The same offset repeating at the same place in every bar "
                                        "(a late backbeat, an early first note) is a habit or a feel. Check whether the deviation repeats before calling it an error."),
            define("Fills and pickups", "A fast fill or pickup can look off-grid because it sits on a finer grid than the groove (thirty-second notes, sextuplets). "
                                        "Do not judge fills against the main grid."),
            define("MIDI drum tracks and humanization", "A MIDI drum track is played by the computer from notes, and Station's Humanize edit moves each note's timing, "
                                                         "velocity and length by an amount the producer chose. After it is applied, the variation is simply in the notes; there is no "
                                                         "setting left to read. So timing deviation on a MIDI drum track is deliberate by design and must not be called sloppy: the "
                                                         "question is whether the amount suits the style (a few milliseconds is subtle and tight; 10 to 25 ms is a natural human feel; "
                                                         "beyond about 30 ms on a tight genre sounds careless). Measuring the rendered audio of such a track only measures the humanization "
                                                         "itself, and the notes are the more exact source. Advice worth giving: real drummers do not shake randomly; they lean "
                                                         "consistently, for example a snare slightly behind the beat and hi-hats slightly ahead, and they play louder on strong beats. "
                                                         "Uniform random jitter on every hit, and random velocity, sounds like a machine imitating a person. A consistent offset per drum, "
                                                         "with accents on the beat, usually sounds more human than more randomness."),
            warn("Limits. Tempo comes from the attacks in the sound, so it suits drums and plucked or struck instruments; smooth vocals and pads give a low confidence. "
                 "A strummed chord spreads over 20 to 40 ms. Bars are counted as four beats from the first beat found, which may not be the downbeat, and other "
                 "meters are not detected. A reading of double or half the project tempo usually means the same rhythm counted at twice or half the rate."),
        ],
        related=("djehuti.station.music-judging-intent", "djehuti.station.music-pitch-interpretation"),
    ),
    topic(
        "djehuti.station.music-judging-intent",
        "explanation",
        "Judging intent: mistake, or a clever emotional choice?",
        "How to decide from measured data whether a pitch or timing deviation is a mistake or expression, and how to say it to an artist.",
        ["intent", "mistake", "expressive", "deliberate", "feedback", "producer", "critique", "artistic", "emotional", "judge", "consistency",
         "context", "tuning", "timing", "feel", "phrase"],
        ["How do I know if it is a mistake or on purpose?", "How should I give feedback on tuning?", "What makes a deviation expressive?",
         "How do I tell the artist their timing is off?"],
        [
            para("The numbers say what happened, never why. The same 30 cents flat can be a sagging note at the end of a breath or a deliberate blue note; the same 20 "
                 "milliseconds late can be dragging or a relaxed backbeat. Decide by asking these questions of the data, and when they do not settle it, say so and ask."),
            define("1. Is it consistent?", "A deviation that repeats in the same way in the same musical place (the same scale degree, the same beat of the bar, every "
                                          "phrase ending) is almost always a choice or a habit. A deviation that appears once, or varies wildly, is more likely a slip."),
            define("2. Is it in a meaningful place?", "Expression tends to sit at phrase peaks and endings, on held or emphasised notes, and at emotional turns. Errors are "
                                                        "scattered, or cluster where the player is under strain: fast passages, high notes, the ends of long breaths, the start of a take."),
            define("3. Does it resolve?", "A bend, scoop or slide that arrives on a pitch and settles there was aimed at it. A move that never lands, or lands between "
                                          "notes, was not. Likewise a push or lag that the band settles back from is a gesture; one that keeps growing is drift."),
            define("4. What is its size and direction?", "Small and steady is feel. Very large deviations, or deviations that move away from the pitch or beat rather than "
                                                         "toward a target, are more likely faults. Slow, deep vibrato and very wide wobble are less likely to be intended."),
            define("5. What does the genre expect?", "Blues, country, gospel and jazz welcome bent and scooped notes, swing, and laid-back timing. Pop vocals and tightly "
                                                     "programmed dance music expect close tuning and tight timing. Judge against what the artist is going for; ask if it is unknown."),
            define("6. Does the rest of the take agree?", "Compare with other notes, sections and takes. If the whole part leans one way, the reference or instrument tuning is "
                                                          "the likelier cause. If one note is off in an otherwise steady line, that is a slip worth fixing."),
            para("How to say it. Describe what was measured with specifics: which track, which moment, the numbers (\"the held note at 0:12 is about 30 cents flat and "
                 "stays there\"). Offer both readings honestly: \"if you were aiming for a bluesy flat third, it works; if not, it will sound like tuning\". Say how sure "
                 "you are, and when the measurement is unreliable, say that instead of judging. Ask what the artist intended, because only they know. Suggest a way to "
                 "test it: listen with and without the deviation, or compare with another take. Be constructive, and never claim a problem you did not measure."),
            note("Rules of thumb are not laws. A rule that is broken consistently and on purpose is a style. When the data cannot separate an error from an expressive choice, "
                 "the honest answer is \"this could be either; which did you mean?\""),
        ],
        related=("djehuti.station.music-pitch-interpretation", "djehuti.station.music-rhythm-interpretation", "djehuti.station.producer-tools-guide"),
    ),
    topic(
        "djehuti.station.producer-tools-guide",
        "reference",
        "The Producer's measuring tools and what their numbers mean",
        "What analyze_pitch, analyze_tempo and project_overview report, how to read each figure, how to combine them, and where they fall short.",
        ["producer", "analyze_pitch", "analyze_tempo", "project_overview", "cents", "bpm", "confidence", "measure", "tools", "report",
         "wobble", "vibrato", "motion", "tightness", "steadiness", "pattern"],
        ["What does the pitch report mean?", "What does the tempo report mean?", "What can the Producer measure?", "How sure is the measurement?"],
        [
            define("project_overview", "Lists the tracks with name, volume, pan, mute and solo, and whether the transport is playing. Use it first, to know what is in the session."),
            define("analyze_pitch", "For one track (a single melodic line): each note's time range, name, cents from exact pitch (+ sharp, - flat), wobble in cents, level, "
                                    "and, where they occur, \"rises/falls N cents across the note\" (a bend, scoop or slide) and \"vibrato R Hz +/-D cents\". The summary gives the "
                                    "average distance from pitch and whether the take leans sharp or flat. Tuning is judged from the steady middle of each note."),
            define("analyze_midi_drums", "For a MIDI drum track, read straight from the notes so it is exact. Per drum (kick, snare, hi-hats...): number of hits, average "
                                         "distance from the beat grid in ms, lean (negative ahead of the beat, positive behind), spread, and velocity (average, range, variation). "
                                         "Hits exactly on the grid with identical velocity are quantized and unhumanized; a steady lean on one drum is a chosen feel; "
                                         "varied velocity in a pattern is accents and ghost notes."),
            define("analyze_tempo", "For a track or the whole mix: the BPM with a confidence, how steady the beat is (jitter in ms, drift in BPM), tightness (average distance from "
                                    "the grid in ms, spread, lean ahead or behind), which grid the playing sits on (eighths, triplets or swing, sixteenths, sextuplets), and "
                                    "the rhythm pattern bar by bar. It compares the tempo with the project's, noting half or double time."),
            para("Combining them. Timing and tuning often move together: a singer who rushes may also go sharp as excitement builds, and both drifting late in a take suggests "
                 "fatigue. Look for the moments where several measurements change at once. A part that is loose in timing but consistent in feel (a steady lean, a swing grid) is "
                 "a groove; one that is loose and uneven is unsteady."),
            warn("Reliability. Each report is a measurement of one section of one track, not a verdict. Low confidence, few notes, or a note about chords or noise means the tool "
                 "could not measure well. Never report a figure the tool did not give, and say when something was not measured."),
        ],
        related=("djehuti.station.music-judging-intent",),
    ),
]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for t in TOPICS:
        with open(OUT / f"{t['id']}.json", "w", encoding="utf-8", newline="\n") as out:
            out.write(json.dumps(t, indent=2, ensure_ascii=False) + "\n")
        print("wrote", t["id"])


if __name__ == "__main__":
    main()
