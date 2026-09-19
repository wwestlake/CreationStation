#pragma once

#include <JuceHeader.h>

// A long-running action with a window that shows a progress bar, a line saying what it is doing, and a
// Cancel button. The work runs on a background thread, so the window keeps painting and Cancel is answered.
//
//   auto task = std::make_unique<ProgressTask>("Importing video", [](ProgressTask& t)
//   {
//       t.report(0.25, "Copying clip.mp4 into the project...");
//       if (t.cancelRequested()) return;
//   }, [](bool cancelled) { /* back on the message thread */ });
//   task->start();
//
// The owner keeps the task alive until onFinished has run, then deletes it (not from inside onFinished -
// use MessageManager::callAsync).
class ProgressTask final : public juce::ThreadWithProgressWindow
{
public:
    using Work = std::function<void(ProgressTask&)>;
    using Finished = std::function<void(bool cancelled)>;

    ProgressTask(const juce::String& title, Work workToRun, Finished onDone)
        : juce::ThreadWithProgressWindow(title, true, true, 30000, "Cancel", nullptr),
          work(std::move(workToRun)),
          finished(std::move(onDone))
    {
        setStatusMessage("Starting...");
    }

    void start() { launchThread(); }

    // From the worker thread: how far along (0..1) and what is happening now.
    void report(double fraction, const juce::String& status)
    {
        setProgress(juce::jlimit(0.0, 1.0, fraction));
        setStatusMessage(status);
    }

    // From the worker thread: true once the user pressed Cancel.
    bool cancelRequested() const { return threadShouldExit(); }

private:
    void run() override
    {
        if (work)
            work(*this);
    }

    void threadComplete(bool userPressedCancel) override
    {
        if (finished)
            finished(userPressedCancel);
    }

    Work work;
    Finished finished;
};
