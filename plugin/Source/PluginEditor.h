#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// The editor hosts the same HTML interface as the web version inside a native WebView.
class ImpaktEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ImpaktEditor (ImpaktProcessor&);
    ~ImpaktEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    void sendLoad();
    void createWebView();
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    ImpaktProcessor& proc;
    int seenVersion = -1;
    int ramPkSeen[sk::NUM_TRACKS] { -1, -1, -1, -1, -1, -1, -1, -1 };   // RAM: peaks counter last sent per buffer
    int brkSent = -1;                                                    // BREAK: the slots' version last sent
    int cubeSent = -1;                                                   // CUBE: the banks' version last sent
    int radSent = -1;                                                    // RADIO: the receivers' version last sent
    int updSent = -1;                                                    // updates: the updater's version last sent
    bool pageReady = false;
    std::unique_ptr<juce::WebBrowserComponent> web;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImpaktEditor)
};
