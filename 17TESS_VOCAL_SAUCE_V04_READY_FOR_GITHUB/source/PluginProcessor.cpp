#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <string>
#include "PluginProcessor.h"
#include "PluginEditor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
#if JUCE_WINDOWS
 #include <windows.h>
#endif

VocalSauceProcessor::VocalSauceProcessor()
    : juce::AudioProcessor(juce::AudioProcessor::BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "17TESS_PARAMETERS", makeLayout()) {
    for (size_t i = 0; i < rawParameters.size(); ++i)
        rawParameters[i] = parameters.getRawParameterValue(SauceDSP::ids[i]);
}

juce::AudioProcessorValueTreeState::ParameterLayout VocalSauceProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto make = [&](const char* id, const char* name, float min, float max, float step, float def, const char* unit) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ id, 1 }, name,
            juce::NormalisableRange<float>{ min, max, step }, def,
            juce::AudioParameterFloatAttributes().withLabel(unit)));
    };

    make("sat", "Saturation", 0, 100, 0.1f, 0, "%");
    make("dist", "Distortion", 0, 100, 0.1f, 0, "%");
    make("air", "Air", 0, 100, 0.1f, 0, "%");
    make("punch", "Punch", 0, 100, 0.1f, 0, "%");
    make("wet", "Wet", 0, 100, 0.1f, 100, "%");
    make("deess", "De-ess", 0, 100, 0.1f, 0, "%");
    make("color", "Color EQ", 0, 100, 0.1f, 0, "%");
    make("chorus", "Chorus", 0, 100, 0.1f, 0, "%");
    make("comp", "Compressor", 0, 100, 0.1f, 0, "%");

    make("sat_drive", "Sat Drive", 0, 100, 0.1f, 60, "%");
    make("sat_tone", "Sat Tone", 0, 100, 0.1f, 50, "%");
    make("sat_output", "Sat Output", -18, 18, 0.1f, 0, "dB");

    make("dist_drive", "Dist Drive", 0, 100, 0.1f, 60, "%");
    make("dist_hp", "Dist Low Cut", 20, 2200, 1, 65, "Hz");
    make("dist_lp", "Dist High Cut", 800, 18000, 1, 15000, "Hz");
    make("dist_output", "Dist Output", -18, 18, 0.1f, 0, "dB");

    make("air_freq", "Air Freq", 3000, 14000, 1, 4400, "Hz");
    make("air_boost", "Air Boost", 0, 100, 0.1f, 55, "%");

    make("punch_attack", "Punch Attack", 10, 300, 1, 90, "Hz");
    make("punch_release", "Punch Release", 2, 60, 0.1f, 9, "Hz");

    make("wet_output", "Output Trim", -18, 18, 0.1f, 0, "dB");

    make("deess_freq", "De-ess Frequency", 3500, 12000, 1, 6600, "Hz");
    make("deess_amount", "De-ess Depth", 0, 100, 0.1f, 50, "%");

    make("color_tilt", "Color Tilt", -100, 100, 0.1f, 0, "%");
    make("color_bias", "Color Bias", 0, 100, 0.1f, 50, "%");

    make("chorus_rate", "Chorus Rate", 0.05f, 8.f, 0.01f, 0.8f, "Hz");
    make("chorus_depth", "Chorus Depth", 0, 100, 0.1f, 40, "%");
    make("chorus_width", "Chorus Width", 0, 100, 0.1f, 50, "%");

    make("comp_threshold", "Comp Threshold", -40, 0, 0.1f, -18, "dB");
    make("comp_ratio", "Comp Ratio", 1, 20, 0.1f, 4, ":1");
    make("comp_attack", "Comp Attack", 1, 250, 0.1f, 12, "ms");
    make("comp_release", "Comp Release", 5, 500, 0.1f, 80, "ms");
    make("comp_makeup", "Comp Makeup", -6, 24, 0.1f, 0, "dB");

    for (int i = 1; i <= 6; ++i) {
        const auto id = std::string("eq_gain") + std::to_string(i);
        const auto nm = std::string("EQ Gain ") + std::to_string(i);
        make(id.c_str(), nm.c_str(), -36, 36, 0.1f, 0.0f, "dB");
    }
    for (int i = 1; i <= 6; ++i) {
        const auto id = std::string("eq_freq") + std::to_string(i);
        const auto nm = std::string("EQ Freq ") + std::to_string(i);
        make(id.c_str(), nm.c_str(), 30, 16500, 1, SauceDSP::defaults[(size_t)(SauceDSP::eqFreq1 + (i - 1))], "Hz");
    }
    for (int i = 1; i <= 6; ++i) {
        const auto id = std::string("eq_q") + std::to_string(i);
        const auto nm = std::string("EQ Width Q ") + std::to_string(i);
        make(id.c_str(), nm.c_str(), 0.25f, 10.0f, 0.01f, 1.0f, "");
    }
    return layout;
}

void VocalSauceProcessor::prepareToPlay(double sampleRate, int) { engine.prepare(sampleRate, getTotalNumOutputChannels()); }
void VocalSauceProcessor::releaseResources() { engine.reset(); }

bool VocalSauceProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    auto mainIn = layout.getMainInputChannelSet();
    auto mainOut = layout.getMainOutputChannelSet();
    return (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo()) && mainIn == mainOut;
}

void VocalSauceProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int count = (std::min)(2, audio.getNumChannels());
    if (count <= 0 || audio.getNumSamples() == 0) return;
    std::array<float, SauceDSP::numParams> values{};
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = rawParameters[i]->load();
    auto* const* channels = audio.getArrayOfWritePointers();
    engine.process(channels, count, audio.getNumSamples(), values);
    for (int i = count; i < audio.getNumChannels(); ++i)
        audio.clear(i, 0, audio.getNumSamples());
}

juce::AudioProcessorEditor* VocalSauceProcessor::createEditor() { return new VocalSauceEditor(*this); }

void VocalSauceProcessor::getStateInformation(juce::MemoryBlock& block) {
    auto xml = parameters.copyState().createXml();
    if (xml) copyXmlToBinary(*xml, block);
}

void VocalSauceProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        if (xml->hasTagName(parameters.state.getType().toString()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::File VocalSauceProcessor::getPresetDirectory() const {
    auto fallback = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("17TESS").getChildFile("VocalSauce").getChildFile("Presets");
#if JUCE_WINDOWS
    HMODULE h = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(reinterpret_cast<const void*>(&createPluginFilter)), &h) && h) {
        wchar_t path[32768]{};
        DWORD count = GetModuleFileNameW(h, path, 32768);
        if (count > 0 && count < 32768) {
            auto module = juce::File(juce::String(path));
            auto dir = module.getParentDirectory();
            if (dir.getFileName().equalsIgnoreCase("x86_64-win")) {
                auto candidate = dir.getParentDirectory().getParentDirectory().getChildFile("Presets");
                if (candidate.createDirectory().wasOk()) {
                    auto probe = candidate.getChildFile(".17tess_write_test");
                    juce::FileOutputStream out(probe);
                    if (out.openedOk()) { out.writeByte('1'); out.flush(); probe.deleteFile(); return candidate; }
                }
            }
        }
    }
#endif
    fallback.createDirectory();
    return fallback;
}

bool VocalSauceProcessor::savePreset(const juce::File& file, juce::String& error) {
    if (!file.hasFileExtension("17tesspreset")) { error = "Use the .17tesspreset extension."; return false; }
    if (auto xml = parameters.copyState().createXml()) {
        if (xml->writeTo(file)) return true;
    }
    error = "Cannot save preset. Check folder permissions.";
    return false;
}

bool VocalSauceProcessor::loadPreset(const juce::File& file, juce::String& error) {
    auto xml = juce::XmlDocument::parse(file);
    if (!xml || !xml->hasTagName(parameters.state.getType().toString())) { error = "Invalid 17TESS preset."; return false; }
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) { error = "Preset format error."; return false; }
    parameters.replaceState(tree);
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VocalSauceProcessor(); }
