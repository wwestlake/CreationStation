#!/usr/bin/env python3
"""Writes Station's basic music-theory help topics (help/topics/en-US/djehuti.station.theory-*.json).

Foundations for the Producer role (and for anyone using Station): notes, intervals and scales, keys and the circle of fifths,
chords and harmony, rhythm and meter, tuning systems and intonation, and song structure and vocabulary. Run from the Station
folder:  python help/tools/make_theory_topics.py
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
        "reviewedBy": "Claude (from standard music theory, not yet reviewed by a person)",
    }


def para(text): return ("paragraph", text)
def note(text): return ("note", text)
def warn(text): return ("warning", text)
def define(term, meaning): return ("definition", {"term": term, "definition": meaning})


TOPICS = [
    topic(
        "djehuti.station.theory-notes-intervals-scales",
        "reference",
        "Music theory: notes, intervals and scales",
        "Note names, semitones and tones, intervals and how they sound, the major and minor scales, modes, pentatonic and blues scales, and how notes map to MIDI numbers and frequencies.",
        ["theory", "note", "notes", "interval", "semitone", "tone", "scale", "major", "minor", "mode", "pentatonic", "blues", "chromatic",
         "octave", "midi", "frequency", "A440", "enharmonic", "sharp", "flat", "fifth", "third", "degree"],
        ["What is a semitone?", "What notes are in a major scale?", "What is the minor pentatonic scale?", "What is an interval?", "What MIDI number is middle C?"],
        [
            define("Notes and the octave", "There are twelve notes in an octave: C, C#/Db, D, D#/Eb, E, F, F#/Gb, G, G#/Ab, A, A#/Bb, B, then C again an octave higher, at twice the frequency. "
                                           "The step between neighbouring notes is a semitone (a half step); two semitones make a tone (a whole step). C# and Db are the same pitch with different names (enharmonic)."),
            define("Frequencies and MIDI numbers", "Concert pitch puts A4 at 440 Hz, which is MIDI note 69. Each semitone is a factor of about 1.0595 in frequency, and 100 cents; twelve semitones "
                                                   "double the frequency. Middle C (C4) is MIDI 60, about 261.6 Hz."),
            define("Intervals", "The distance between two notes, counted in semitones. 1 minor second (tense, half step); 2 major second; 3 minor third (sad, dark); 4 major third (bright, happy); "
                                "5 perfect fourth; 6 tritone (the most unstable, augmented fourth or diminished fifth); 7 perfect fifth (the strongest, most stable); 8 minor sixth; 9 major sixth; "
                                "10 minor seventh (bluesy, leads onward); 11 major seventh (dreamy, wants to resolve up); 12 octave."),
            define("Major scale", "Seven notes in the pattern whole, whole, half, whole, whole, whole, half. From C: C D E F G A B. Its notes are called degrees: 1 (tonic, home), 2, 3, 4, 5 (dominant), 6, 7 (leading "
                                  "note, a half step under the tonic and wanting to rise to it). Bright and settled."),
            define("Natural minor scale", "Whole, half, whole, whole, half, whole, whole. From A: A B C D E F G. It has a lowered third, sixth and seventh compared with the major scale on the same note, "
                                          "and sounds darker. Harmonic minor raises the seventh (leading note) and melodic minor raises the sixth and seventh going up."),
            define("Modes", "Scales built by starting a major scale on a different degree: Ionian (the major scale itself), Dorian (minor with a raised sixth; jazz, funk, folk), Phrygian (minor with a lowered "
                            "second; Spanish, metal), Lydian (major with a raised fourth; dreamy, film), Mixolydian (major with a lowered seventh; rock, blues, folk), Aeolian (natural minor), "
                            "Locrian (rare, unstable)."),
            define("Pentatonic and blues scales", "The major pentatonic drops the fourth and seventh: 1 2 3 5 6 (open, folk and country). The minor pentatonic is 1 b3 4 5 b7, the backbone of rock and blues guitar. "
                                                  "Adding the flat fifth (a 'blue note') makes the blues scale: 1 b3 4 b5 5 b7."),
            define("Chromatic scale", "All twelve semitones in a row; notes outside the key are heard as colour or tension."),
        ],
        related=("djehuti.station.theory-keys-circle-of-fifths", "djehuti.station.theory-chords-harmony"),
    ),
    topic(
        "djehuti.station.theory-keys-circle-of-fifths",
        "reference",
        "Music theory: keys and the circle of fifths",
        "Keys and key signatures, the circle of fifths, relative and parallel keys, which keys are close together, and the chords that belong to a key.",
        ["circle of fifths", "key", "key signature", "sharps", "flats", "relative minor", "parallel", "modulation", "transpose", "closely related",
         "diatonic", "tonic", "dominant", "subdominant", "capo", "fifth"],
        ["What is the circle of fifths?", "How many sharps are in the key of D?", "What is the relative minor of C?", "What chords are in the key of G?",
         "How do I change key?", "What keys sound close together?"],
        [
            para("A key is a home note (the tonic) and the scale of notes that belong with it. A song 'in C major' mostly uses the notes of the C major scale and feels settled when it lands on C."),
            define("The circle of fifths", "Arrange the twelve keys so each step clockwise goes up a perfect fifth (seven semitones): C, G, D, A, E, B, F#/Gb, Db, Ab, Eb, Bb, F, and back to C. "
                                           "Each step clockwise adds one sharp to the key signature; each step counter-clockwise adds one flat."),
            define("Key signatures", "C major has no sharps or flats. Clockwise: G has 1 sharp (F#), D 2 (F#, C#), A 3, E 4, B 5, F#/Gb 6. Counter-clockwise: F has 1 flat (Bb), Bb 2 (Bb, Eb), Eb 3, Ab 4, Db 5, Gb 6. "
                                     "The sharps appear in the order F C G D A E B; the flats in the reverse order B E A D G C F."),
            define("Relative minor", "Every major key shares its notes with a minor key that starts on its sixth degree, three semitones below: C major and A minor, G and E minor, D and B minor, A and F# minor, "
                                     "E and C# minor, F and D minor, Bb and G minor, Eb and C minor. Same key signature, different home note. The parallel minor shares the same tonic (C major and C minor) "
                                     "and has three more flats."),
            define("Close and distant keys", "Keys next to each other on the circle share six of their seven notes, so moving to a neighbour (a fifth up or down) is the smoothest change of key: from C to G "
                                            "(dominant) or to F (subdominant), or to the relative minor. Keys far across the circle (C and F#) share few notes and a change between them is dramatic."),
            define("Chords of a key", "Build a triad on each degree of the major scale: I major, ii minor, iii minor, IV major, V major, vi minor, vii diminished. In C: C, Dm, Em, F, G, Am, Bdim. "
                                      "In G: G, Am, Bm, C, D, Em, F#dim. The three major chords (I, IV, V) sit side by side on the circle: F, C and G in the key of C. "
                                      "The most common minor chord in a major key is vi."),
            define("Using the circle for movement", "Chord roots moving down a fifth (or up a fourth) are the strongest, most natural motion: V to I, ii to V, vi to ii. This is why so many "
                                                    "progressions travel counter-clockwise round the circle."),
            define("Transposing and capos", "To move a song to another key, shift every note by the same number of semitones. A capo on fret N raises the pitch of open strings by N semitones, "
                                            "so the chord shapes stay the same but the key rises: capo 2 with G shapes sounds in A."),
        ],
        related=("djehuti.station.theory-chords-harmony", "djehuti.station.theory-notes-intervals-scales"),
    ),
    topic(
        "djehuti.station.theory-chords-harmony",
        "reference",
        "Music theory: chords and harmony",
        "Triads and seventh chords, chord function, common progressions, cadences, tension and release.",
        ["chord", "chords", "triad", "seventh", "major", "minor", "diminished", "augmented", "sus", "inversion", "progression", "cadence",
         "tonic", "dominant", "subdominant", "12 bar blues", "ii V I", "harmony", "tension", "resolution", "voice leading"],
        ["What is a ii V I?", "What is a cadence?", "What are the chords in a 12 bar blues?", "What makes a chord major or minor?", "What is a dominant seventh?"],
        [
            define("Triads", "Three notes stacked in thirds. Major: root, major third, fifth (0-4-7 semitones; bright). Minor: root, minor third, fifth (0-3-7; darker). Diminished: 0-3-6 (tense). "
                              "Augmented: 0-4-8 (unresolved, floating). Suspended chords replace the third with a second or fourth (sus2 0-2-7, sus4 0-5-7) and want to resolve."),
            define("Seventh chords", "Add another third on top. Major seventh 0-4-7-11 (smooth, dreamy); dominant seventh 0-4-7-10 (bluesy, pulls toward the chord a fifth below); minor seventh 0-3-7-10 (mellow); "
                                     "half-diminished 0-3-6-10; diminished seventh 0-3-6-9. Extensions (9th, 11th, 13th) add colour above."),
            define("Inversions", "The same chord with a different note in the bass: root position has the root lowest; first inversion has the third lowest; second inversion the fifth. Written C/E means C major with E in the bass."),
            define("Function", "Chords do jobs. Tonic (I, vi) is home and rest. Subdominant (IV, ii) leaves home, a step of tension. Dominant (V, vii diminished) creates the strongest pull back to tonic. "
                               "A progression is a journey: rest, departure, tension, resolution."),
            define("Cadences", "Chord endings that punctuate a phrase. Authentic V to I: the strongest full stop. Plagal IV to I: the 'amen', gentler. Half cadence ending on V: a comma, unresolved. "
                               "Deceptive V to vi: the expected home is replaced by its relative minor, a surprise."),
            define("Common progressions", "I-V-vi-IV (pop, endlessly used). I-vi-IV-V (fifties and doo-wop). ii-V-I (the backbone of jazz; in C: Dm7 G7 Cmaj7). I-IV-V (rock, country, folk). "
                                          "vi-IV-I-V. The twelve-bar blues: I I I I, IV IV I I, V IV I I, usually with dominant sevenths."),
            define("Tension and release", "Dissonant intervals (minor second, major seventh, tritone) create tension that is released by resolving to consonant ones (thirds, fifths, sixths, the octave). "
                                          "The leading note resolves up to the tonic; the seventh of a dominant chord resolves down. A phrase ending on tension feels open; on the tonic, closed."),
            define("Voice leading", "Smooth harmony moves each note of a chord by the smallest step to the next chord, keeping common notes. It is what makes a progression sound connected instead of jumpy."),
        ],
        related=("djehuti.station.theory-keys-circle-of-fifths", "djehuti.station.theory-tuning-intonation"),
    ),
    topic(
        "djehuti.station.theory-rhythm-meter",
        "reference",
        "Music theory: rhythm, meter and tempo",
        "Note values, time signatures, simple and compound meter, syncopation, backbeat, tuplets and tempo terms.",
        ["rhythm", "meter", "time signature", "4/4", "3/4", "6/8", "12/8", "waltz", "compound", "simple", "syncopation", "backbeat", "polyrhythm",
         "tuplet", "triplet", "note value", "quarter note", "eighth note", "downbeat", "bar", "measure", "tempo", "allegro", "andante", "swing"],
        ["What does 6/8 mean?", "What is a backbeat?", "What is the difference between 3/4 and 6/8?", "What is syncopation?", "What tempo is allegro?"],
        [
            define("Note values", "A whole note lasts four beats (in 4/4), a half note two, a quarter note one, an eighth half a beat, a sixteenth a quarter of a beat. A dot adds half again. "
                                  "A triplet fits three notes in the time of two."),
            define("Time signatures", "The top number is how many beats in a bar; the bottom says which note value is one beat. 4/4 (common time): four quarter-note beats, the default of pop, rock and most "
                                      "dance music. 3/4: three beats, a waltz. 2/4: a march. 6/8, 9/8 and 12/8 (compound): beats divide into three, so 6/8 is two big beats of three eighths each, "
                                      "with a rolling, swaying feel. 5/4 and 7/8 (odd meters): uneven groupings, often felt as 3+2 or 2+2+3."),
            define("Downbeat, backbeat, off-beat", "The downbeat is beat one, the anchor. In 4/4 the backbeat is the snare on beats two and four, the heart of rock and pop. The off-beat sits between the "
                                                   "beats (the 'and' of a count), giving reggae and funk their lift."),
            define("Syncopation and polyrhythm", "Syncopation accents weak beats or off-beats, or leaves an expected beat silent, creating forward push. Polyrhythm plays two different groupings at once, such as three "
                                                 "against four; a hemiola makes a bar of 6 feel like 3 groups of 2 then 2 groups of 3."),
            define("Swing and shuffle", "Pairs of eighth notes played long-short instead of even, near a triplet ratio of about 2 to 1. Jazz, blues and shuffle grooves are built on it."),
            define("Tempo terms", "Beats per minute: largo about 40 to 60, adagio 66 to 76, andante 76 to 108, moderato 108 to 120, allegro 120 to 156, vivace 156 to 176, presto 168 and up. "
                                  "Pop is commonly 90 to 130, house about 120 to 130, drum and bass about 170 to 175. Half time or double time counts the same music at half or twice the rate."),
        ],
        related=("djehuti.station.music-rhythm-interpretation",),
    ),
    topic(
        "djehuti.station.theory-tuning-intonation",
        "explanation",
        "Music theory: tuning systems and what 'in tune' means",
        "Equal temperament versus just intonation, why a well-sung interval can measure a few cents off the piano, and what that means for judging tuning.",
        ["tuning", "intonation", "equal temperament", "just intonation", "temperament", "pythagorean", "cents", "in tune", "out of tune", "third", "fifth",
         "reference pitch", "432", "440", "guitar tuning", "capo", "beats", "harmonic"],
        ["What does in tune mean?", "Why is my third flat against the piano?", "What is just intonation?", "Is A432 better?", "Why is a guitar never perfectly in tune?"],
        [
            para("Pitch is measured in cents, 100 to a semitone, against equal temperament (ET): the tuning of pianos, keyboards and most software, where every semitone is exactly 100 cents. "
                 "But ET is a compromise. Its intervals are not the purest ones, and voices, strings and horns, which can bend pitch freely, often tune to purer ones by ear."),
            define("Just intonation", "Intervals tuned as simple frequency ratios, which sound smoothest because their overtones line up and beating disappears. A just major third (5:4) is about 386 cents, "
                                      "so 14 cents flatter than the 400 of ET. A just minor third (6:5) is about 316 cents, 16 cents sharper than ET's 300. A just perfect fifth (3:2) is 702 cents, "
                                      "2 cents wider than ET. A just harmonic seventh (7:4) is about 969 cents, 31 cents flatter than the ET minor seventh."),
            define("Why it matters when measuring", "A singer or string player holding the third of a major chord in pure tune will measure roughly 14 cents flat against ET. That is not a mistake: "
                                                     "against the other notes of the chord it is in tune, and can sound better than the ET value. Judge a held chord note against the chord, not only against the piano."),
            define("Melodic tuning", "Melody wants the opposite: leading notes and the tops of ascending lines are often sung a little sharp (Pythagorean thirds are about 408 cents, 8 sharp of ET) for brightness "
                                     "and pull toward the next note. So a slightly sharp leading note and a slightly flat chord third can both be musical, the same singer doing two right things."),
            define("Equal temperament in practice", "Fixed-pitch instruments (piano, marimba, fretted guitar, MIDI) cannot bend to purity, so they are tuned to ET with every interval slightly impure. "
                                                    "A guitar adds its own errors: pressing a string hard sharpens it, the nut and frets compensate imperfectly, and chords tuned pure at one position "
                                                    "sound slightly off at another. Guitarists nudge chord shapes by ear for this reason."),
            define("Reference pitch", "Concert pitch is A4 = 440 Hz. Some music is tuned to 415 (baroque), 432 or 442 (some orchestras). If every note in a take measures consistently sharp or flat "
                                      "by the same amount, the take (or an instrument) is tuned to a different reference, not the player, and the analysis reference can be set to match. Nothing about "
                                      "432 Hz makes the music better in any way that has been measured; it is simply a different reference."),
            warn("So a measurement in cents against ET is a good first look, not a verdict. Weigh it with what the harmony is doing (a chord third), whether the note is melodic or harmonic, and the instrument."),
        ],
        related=("djehuti.station.music-pitch-interpretation", "djehuti.station.theory-chords-harmony"),
    ),
    topic(
        "djehuti.station.theory-form-and-vocabulary",
        "reference",
        "Music theory: song form, dynamics and the words producers use",
        "Song sections, arrangement and texture, dynamics and articulation terms, and common production vocabulary such as pocket, headroom and tension.",
        ["song form", "structure", "verse", "chorus", "bridge", "intro", "outro", "pre-chorus", "hook", "arrangement", "texture", "dynamics", "forte",
         "piano", "crescendo", "staccato", "legato", "accent", "pocket", "groove", "headroom", "mix", "range", "register"],
        ["What is a bridge?", "What does legato mean?", "What is the pocket?", "What are the parts of a song?", "What does forte mean?"],
        [
            define("Song sections", "Intro sets the scene. Verse carries the story and changes its words each time. Pre-chorus builds toward the chorus. Chorus is the repeated, memorable core, the hook. "
                                    "Bridge (middle eight) is a contrast, usually once, in different harmony or feel, before returning. Outro ends the song. A common form: intro, verse, chorus, verse, chorus, bridge, chorus, outro."),
            define("Arrangement and texture", "Arrangement is which instruments play what, and when. Adding instruments raises energy, taking them away creates space. Texture describes the layering: a single line (monophonic), "
                                              "melody over chords (homophonic), or several independent lines (polyphonic). Contrast between sections is what keeps a song moving."),
            define("Dynamics", "Loudness, in Italian marks: pp very soft, p soft, mp medium soft, mf medium loud, f loud, ff very loud; crescendo growing louder, diminuendo growing softer. "
                                "In production, dynamics also means the difference between quiet and loud within a part, which compression reduces."),
            define("Articulation", "How each note is shaped. Staccato is short and detached; legato smooth and connected; tenuto held its full length; an accent stresses a note; a slur joins notes in one motion; "
                                   "pizzicato is plucked, and palm-muted guitar is dampened and percussive."),
            define("Range and register", "The span from a voice's or instrument's lowest to highest note. Registers colour the sound: low is weighty, middle is natural, high is bright and exposed. Notes at the edge of a range "
                                          "are often where tuning and timing suffer, and where emotion peaks."),
            define("Production vocabulary", "The pocket is the groove locked in place between the players, tight without stiffness. Feel is how the timing and dynamics breathe. Headroom is the space left below full "
                                            "level before clipping. Tension and release is the rise and settling of energy, harmonic or dynamic. Space is deliberate silence or sparse playing that lets other parts speak. "
                                            "Hook is the part a listener remembers."),
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
