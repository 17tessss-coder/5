#pragma once
#include <JuceHeader.h>
#include "DSPCore.h"
#include <array>
#include <atomic>

class VocalSauceProcessor final : public juce::AudioProcessor {
public:
    VocalSauceProcessor();
    ~VocalSauceProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "17TESS VOCAL SAUCE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.12; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState parameters;
    juce::File getPresetDirectory() const;
    bool savePreset(const juce::File&, juce::String& error);
    bool loadPreset(const juce::File&, juce::String& error);
    float param(int id) const noexcept { return rawParameters[(size_t)id]->load(); }
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    std::array<std::atomic<float>*, SauceDSP::numParams> rawParameters{};
    SauceDSP::Engine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalSauceProcessor)
};
