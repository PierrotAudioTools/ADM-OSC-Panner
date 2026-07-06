#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>
#include <optional>
#include "PluginProcessor.h"

class PositionDisplay final : public juce::Component,
                              private juce::Timer
{
public:
    explicit PositionDisplay (ADM_OSC_Music_PannerAudioProcessor& processor);
    ~PositionDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;

    void updateFromParameters();
    void setParametersFromPoint (juce::Point<float> point);
    juce::Rectangle<float> getInteractiveBounds() const;

    ADM_OSC_Music_PannerAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    const std::atomic<float>* posXValue { nullptr };
    const std::atomic<float>* posYValue { nullptr };
    const std::atomic<float>* posZValue { nullptr };

    juce::AudioParameterFloat* posXParam { nullptr };
    juce::AudioParameterFloat* posYParam { nullptr };
    juce::AudioParameterFloat* posZParam { nullptr };

    float cachedX { 0.0f };
    float cachedY { 0.0f };
    float cachedZ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PositionDisplay)
};

class ElevationDisplay final : public juce::Component,
                               private juce::Timer
{
public:
    explicit ElevationDisplay (ADM_OSC_Music_PannerAudioProcessor& processor);
    ~ElevationDisplay() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;

    void updateFromParameters();
    void setParametersFromPoint (juce::Point<float> point);
    juce::Rectangle<float> getInteractiveBounds() const;

    ADM_OSC_Music_PannerAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    const std::atomic<float>* posXValue { nullptr };
    const std::atomic<float>* posZValue { nullptr };

    juce::AudioParameterFloat* posXParam { nullptr };
    juce::AudioParameterFloat* posZParam { nullptr };

    float cachedX { 0.0f };
    float cachedZ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElevationDisplay)
};

class CircleIconComponent final : public juce::Component
{
public:
    CircleIconComponent() = default;
    ~CircleIconComponent() override = default;

    void paint (juce::Graphics& g) override;
};

class ADM_OSC_Music_PannerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                       private juce::Timer
{
public:
    explicit ADM_OSC_Music_PannerAudioProcessorEditor (ADM_OSC_Music_PannerAudioProcessor&);
    ~ADM_OSC_Music_PannerAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    ADM_OSC_Music_PannerAudioProcessor& audioProcessor;

    // === 2D MAP ===
    PositionDisplay positionDisplay;
    ElevationDisplay elevationDisplay;
    std::vector<std::unique_ptr<juce::Label>> mapLabels;

    // === HEADER ===
    juce::Label betaLabel;
    juce::Label titleLabel;
    juce::Label companyLabel;
    juce::HyperlinkButton pierrotLink { "PierrotAudioTools", juce::URL ("https://github.com/PierrotAudioTools") };
    juce::Label tabOscLabel;
    juce::Label tabFxLabel;
    juce::Label tabSettingsLabel;

    // === OSC TAB ===
    juce::Label oscSectionTitle;
    juce::TextButton oscInToggle;
    juce::TextButton oscOutToggle;
    juce::Label receiveLabel;
    juce::Label sendLabel;
    juce::TextEditor receivePortEditor;
    juce::TextEditor sendPortEditor;
    juce::Label sendHostLabel;
    juce::TextEditor sendHostEditor;
    juce::Label oscFormatLabel;
    juce::ComboBox oscFormatBox;
    juce::Label oscInfoLabel;
    juce::Label receiveStatusLabel;
    juce::Label sendStatusLabel;
    juce::ToggleButton circleToggle;
    juce::Label circleSubdivisionLabel;
    juce::ComboBox circleSubdivisionBox;
    juce::Label circleRadiusLabel;
    juce::Slider circleRadiusSlider;
    CircleIconComponent circleIcon;
    juce::Label fxSectionLabel;
    juce::Label circleSectionLabel;
    juce::Label circleActivateLabel;
    juce::Label objectLabel;
    juce::TextEditor objectEditor;
    juce::Label mapsSectionLabel;

    class StatusLight;
    class ClickHotspot;
    std::unique_ptr<StatusLight> receiveStatusLight;
    std::unique_ptr<StatusLight> sendStatusLight;
    std::unique_ptr<ClickHotspot> receiveToggleHitbox;
    std::unique_ptr<ClickHotspot> sendPortToggleHitbox;
    std::unique_ptr<ClickHotspot> sendHostToggleHitbox;

    // === CONTROL ===
    juce::Label controlTitle;
    juce::Slider posXSlider;
    juce::Slider posYSlider;
    juce::Slider posZSlider;

    juce::Label posXLabel;
    juce::Label posYLabel;
    juce::Label posZLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SliderAttachment> posXAttachment;
    std::unique_ptr<SliderAttachment> posYAttachment;
    std::unique_ptr<SliderAttachment> posZAttachment;
    std::unique_ptr<ButtonAttachment> circleEnabledAttachment;
    std::unique_ptr<SliderAttachment> circleRadiusAttachment;
    std::unique_ptr<ComboAttachment> circleSubdivisionAttachment;

    juce::Rectangle<int> headerPanelBounds;
    juce::Rectangle<int> tabPanelBounds;
    juce::Rectangle<int> oscPanelBounds;
    juce::Rectangle<int> mapsPanelBounds;
    juce::Rectangle<int> controlPanelBounds;
    juce::Rectangle<int> fxPanelBounds;

    void initialiseSlider (juce::Slider& slider, bool isRotary, bool integer, int textBoxWidth, int textBoxHeight);
    void configureLabel (juce::Label& label, const juce::String& text);
    void configurePortEditor (juce::TextEditor& editor);
    void commitReceivePortFromEditor();
    void commitSendPortFromEditor();
    void commitSendHostFromEditor();
    void commitObjectNumberFromEditor();
    void commitOscFormatFromEditor();
    void refreshPortEditors();
    void refreshSendHostEditor();
    void refreshObjectEditor();
    void refreshOscFormatEditors();
    void updateOscEnablementUI();
    void dismissEditorsIfNeeded (juce::Component* clickedComponent);
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ADM_OSC_Music_PannerAudioProcessorEditor)
};
