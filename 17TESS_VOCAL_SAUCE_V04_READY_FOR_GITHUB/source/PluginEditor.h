#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class HotspotButton : public juce::TextButton {
public:
    HotspotButton() : juce::TextButton("") { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paintButton(juce::Graphics&, bool, bool) override {}
};

class SauceLook final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider::SliderStyle,juce::Slider&) override;
};

class SauceEQGraph final : public juce::Component {
public:
    explicit SauceEQGraph(VocalSauceProcessor& p) : processor(p) { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
private:
    juce::Point<float> point(int band) const;
    void movePoint(const juce::MouseEvent&);
    VocalSauceProcessor& processor;
    int selected = -1;
};

class VocalSauceEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit VocalSauceEditor(VocalSauceProcessor&);
    ~VocalSauceEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using Attachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    struct Effect {
        juce::Slider knob;
        HotspotButton expand;
        std::unique_ptr<Attachment> attachment;
    };
    struct Detail {
        juce::Slider slider;
        juce::Label name;
        juce::Label value;
        int owner=-1;
        std::unique_ptr<Attachment> attachment;
    };
    static constexpr int W=1206,H=1072;
    VocalSauceProcessor& processor;
    SauceLook look;
    SauceEQGraph graph;
    juce::Image background, compressorCard;
    std::array<Effect,9> effects;
    std::array<juce::Slider,6> eqKnobs;
    std::array<std::unique_ptr<Attachment>,6> eqAttachments;
    std::array<Detail,30> details;
    HotspotButton eqExpand, detailClose, saveButton, loadButton;
    std::unique_ptr<juce::FileChooser> chooser;
    int expanded=-1; // 0-8 effects, 9 EQ, -1 compressor card/default
    void setExpanded(int);
    void showError(const juce::String&);
    void savePreset();
    void loadPreset();
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocalSauceEditor)
};
