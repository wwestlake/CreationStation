// Headless check of linked clips (a video and its sound): edits on one apply to its partner, splits and
// duplicates make separate groups, and link state survives a save/restore. Exit code 0 = pass.
#include <JuceHeader.h>

#include "Timeline/TimelineModel.h"

#include <cstdio>

namespace cs
{
juce::String toStorageToken(creation::assets::AssetReferenceMode) { return "exact"; }
creation::assets::AssetReferenceMode assetReferenceModeFromStorageToken(const juce::String&)
{
    return creation::assets::AssetReferenceMode::exact;
}
}

namespace
{
int failures = 0;
void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}
bool near(double a, double b) { return std::abs(a - b) < 1.0e-6; }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    cs::TimelineModel model;

    // Two linked clips on different tracks (a "picture" a and its "sound" b), plus a loner.
    auto fresh = [](cs::TimelineModel& m, int& a, int& b, int& loner)
    {
        m.clear();
        m.setTrackCount(3);
        for (int t = 0; t < 3; ++t)
            m.setTrackKind(t, cs::TrackKind::signal); // signal tracks take clips that need no file
        juce::String err;
        a = m.addClip(cs::ClipKind::signal, 0, "picture", {}, {}, {}, 10.0, 8.0, err);
        b = m.addClip(cs::ClipKind::signal, 1, "sound", {}, {}, {}, 10.0, 8.0, err);
        loner = m.addClip(cs::ClipKind::signal, 2, "loner", {}, {}, {}, 30.0, 4.0, err);
        m.linkClips(a, b);
    };

    int a = 0, b = 0, loner = 0;
    fresh(model, a, b, loner);
    check(a >= 0 && b >= 0 && loner >= 0, "test clips were created");
    if (a < 0 || b < 0 || loner < 0)
        return 2; // setup failed; nothing below is meaningful
    check(model.isClipLinked(a) && model.isClipLinked(b) && ! model.isClipLinked(loner), "link marks exactly the two clips");

    // Move: the partner follows by the same amount and keeps its own track.
    model.moveClip(a, 0, 15.0);
    check(near(model.getClips()[(size_t) a].startSeconds, 15.0) && near(model.getClips()[(size_t) b].startSeconds, 15.0), "moving one moves the other");
    check(model.getClips()[(size_t) b].trackIndex == 1, "the partner keeps its own track");
    check(near(model.getClips()[(size_t) loner].startSeconds, 30.0), "an unlinked clip does not move");

    // Move before zero: both stop together (no drift between them).
    model.moveClip(a, 0, -5.0);
    check(near(model.getClips()[(size_t) a].startSeconds, 0.0) && near(model.getClips()[(size_t) b].startSeconds, 0.0), "both clamp at zero together");

    // Trim start / end.
    fresh(model, a, b, loner);
    model.trimClipStart(a, 12.0);
    check(near(model.getClips()[(size_t) b].startSeconds, 12.0) && near(model.getClips()[(size_t) b].durationSeconds, 6.0), "trimming the start trims the partner too");
    fresh(model, a, b, loner);
    model.trimClipEnd(b, 15.0);
    check(near(model.getClips()[(size_t) a].durationSeconds, 5.0), "trimming the end trims the partner too");

    // Split: left halves stay linked, right halves form their own group.
    fresh(model, a, b, loner);
    check(model.splitClip(a, 14.0), "split succeeds");
    int linkedCount = 0;
    juce::StringArray groups;
    for (const auto& clip : model.getClips())
        if (clip.linkGroupId.isNotEmpty())
        {
            ++linkedCount;
            groups.addIfNotAlreadyThere(clip.linkGroupId);
        }
    check(model.getClips().size() == 5 && linkedCount == 4 && groups.size() == 2, "split makes two linked pairs (left halves, right halves)");

    // Duplicate: the copies are linked to each other, not to the originals.
    fresh(model, a, b, loner);
    check(model.duplicateClip(a), "duplicate succeeds");
    check(model.getClips().size() == 5, "duplicating a linked clip copies its partner too");
    groups.clear();
    for (const auto& clip : model.getClips())
        if (clip.linkGroupId.isNotEmpty())
            groups.addIfNotAlreadyThere(clip.linkGroupId);
    check(groups.size() == 2, "the copies form a group of their own");

    // Delete removes both; unlink frees them.
    fresh(model, a, b, loner);
    model.deleteClip(a);
    check(model.getClips().size() == 1 && model.getClips()[0].displayName == "loner", "deleting one deletes its partner");
    fresh(model, a, b, loner);
    model.unlinkClip(a);
    model.moveClip(a, 0, 20.0);
    check(near(model.getClips()[(size_t) b].startSeconds, 10.0), "after unlinking, clips move independently");
    check(model.findSoundCounterpart(a) < 0, "unrelated clips are not offered as a sound counterpart");

    // Save / restore keeps the link and the detached flag.
    fresh(model, a, b, loner);
    model.setClipSoundDetached(a, true);
    const auto state = model.createState();
    cs::TimelineModel restored;
    restored.restoreState(state);
    int restoredLinked = 0;
    bool detached = false;
    for (const auto& clip : restored.getClips())
    {
        if (clip.linkGroupId.isNotEmpty())
            ++restoredLinked;
        if (clip.soundDetached)
            detached = true;
    }
    check(restoredLinked == 2 && detached, "link and detached-sound survive save and restore");

    // A video clip's effect and layout settings survive save and restore (and copy along with a duplicated clip).
    fresh(model, a, b, loner);
    juce::NamedValueSet params;
    params.set("key.enabled", true);
    params.set("key.tolerance", 0.42);
    params.set("xf.scale", 0.5);
    params.set("xf.x", 0.25);
    model.setClipVideoParams(loner, params);
    cs::TimelineModel withParams;
    withParams.restoreState(model.createState());
    bool paramsBack = false;
    for (const auto& clip : withParams.getClips())
        if (clip.displayName == "loner")
            paramsBack = (bool) clip.videoParams.getWithDefault("key.enabled", false)
                      && near((double) clip.videoParams.getWithDefault("key.tolerance", 0.0), 0.42)
                      && near((double) clip.videoParams.getWithDefault("xf.scale", 0.0), 0.5)
                      && near((double) clip.videoParams.getWithDefault("xf.x", 0.0), 0.25);
    check(paramsBack, "a video clip's effect and layout settings survive save and restore");

    model.duplicateClip(loner);
    bool copyHasParams = false;
    for (const auto& clip : model.getClips())
        if (clip.displayName.endsWith("copy"))
            copyHasParams = near((double) clip.videoParams.getWithDefault("xf.scale", 0.0), 0.5);
    check(copyHasParams, "a duplicated clip keeps its settings");

    std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
