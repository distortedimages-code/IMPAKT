#include "PluginProcessor.h"
#include <cstdio>

// Hosts the real editor in a window and pulls audio from a timer (simulated audio thread).
class TestApp : public juce::JUCEApplication, private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "impakt_gui_test"; }
    const juce::String getApplicationVersion() override { return "1"; }

    void initialise (const juce::String&) override
    {
        proc = std::make_unique<ImpaktProcessor>();
        proc->setRateAndBufferSizeDetails (48000.0, 480);
        proc->prepareToPlay (48000.0, 480);
        window = std::make_unique<juce::DocumentWindow> ("IMPAKT", juce::Colours::black, 0);
        window->setUsingNativeTitleBar (false);
        window->setContentOwned (proc->createEditorIfNeeded(), true);
        window->setTopLeftPosition (0, 0);
        window->setVisible (true);
        startTimer (10);
    }
    void shutdown() override { stopTimer(); window = nullptr; proc = nullptr; }

private:
    void timerCallback() override
    {
        juce::AudioBuffer<float> buf (2, 480); juce::MidiBuffer midi;
        proc->processBlock (buf, midi);
        if (++ticks % 100 == 0)
            std::printf ("t=%ds stateJson=%d chars running=%d step0=%d hits0=%d\n", ticks / 100,
                         proc->getStateJson().length(), proc->pos.running.load(), proc->pos.steps[0].load(), proc->pos.hits[0].load());
        std::fflush (stdout);
    }
    std::unique_ptr<ImpaktProcessor> proc;
    std::unique_ptr<juce::DocumentWindow> window;
    int ticks = 0;
};

START_JUCE_APPLICATION (TestApp)
