#include "PluginEditor.h"

namespace
{
    constexpr float circlePadding = 6.0f;
    constexpr const char* uiTypefaceName = "Avenir Next Condensed";
    constexpr float uiFontScale = 0.9f;
    constexpr float uiFontBoost = 6.0f;
    constexpr float uiLetterSpacing = 0.05f;
    constexpr float fs (float v) { return v * uiFontScale + uiFontBoost; }

    juce::Font uiFont (float baseSize, int style = juce::Font::plain)
    {
        auto f = juce::Font { juce::FontOptions (fs (baseSize), style) };
        f.setExtraKerningFactor (uiLetterSpacing);
        return f;
    }

    juce::Font uiMediumFont (float baseSize)
    {
        auto f = uiFont (baseSize, juce::Font::plain);
        f.setTypefaceStyle ("Demi Bold");
        if (! f.getTypefaceStyle().containsIgnoreCase ("demi")
            && ! f.getTypefaceStyle().containsIgnoreCase ("semi"))
            f.setTypefaceStyle ("Semibold");
        return f;
    }

    namespace Palette
    {
        const juce::Colour backgroundTop    (0xff141414);
        const juce::Colour backgroundBottom (0xff080808);
        const juce::Colour panelFill        (0xff1b1b1b);
        const juce::Colour panelHighlight   (0xff373737);
        const juce::Colour accent           (0xff9a9a9a);
        const juce::Colour accentSoft       (0xff6a6a6a);
        const juce::Colour textPrimary      (0xffefefef);
        const juce::Colour textSecondary    (0xffa0a0a0);
        const juce::Colour mutedFill        (0xff242424);
        const juce::Colour sliderTrack      (0xff343434);
        const juce::Colour sliderThumb      (0xffd6d6d6);
        const juce::Colour sliderX          (0xffffa24d);
        const juce::Colour sliderY          (0xff6cb6ff);
        const juce::Colour sliderZ          (0xff47d6b4);
        const juce::Colour panPoint         (0xff66d6ff);
        const juce::Colour panPointSoft     (0xff2f8aa5);
        const juce::Colour warning          (0xffffbe63);
    }

}

class ADM_OSC_Music_PannerAudioProcessorEditor::StatusLight final : public juce::Component
{
public:
    StatusLight (juce::Colour activeColourIn)
        : activeColour (activeColourIn) {}

    void setActive (bool shouldBeActive)
    {
        if (isActive != shouldBeActive)
        {
            isActive = shouldBeActive;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto glowBounds = bounds.reduced (0.5f);
        auto colour = isActive ? activeColour : activeColour.darker (0.7f).withAlpha (0.35f);

        if (isActive)
        {
            juce::ColourGradient glow (colour.withAlpha (0.25f), glowBounds.getCentre(),
                                       juce::Colours::transparentBlack, glowBounds.getCentre(), true);
            g.setGradientFill (glow);
            g.fillEllipse (glowBounds.expanded (3.5f));
        }

        g.setColour (colour.withAlpha (isActive ? 0.9f : 0.45f));
        g.fillEllipse (glowBounds.reduced (isActive ? 0.0f : 1.0f));

        g.setColour (colour.brighter (0.5f).withAlpha (isActive ? 0.9f : 0.5f));
        g.drawEllipse (glowBounds.reduced (isActive ? 0.2f : 1.2f), 1.2f);
    }

private:
    juce::Colour activeColour;
    bool isActive { false };
};

class CompactComboBoxLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return uiFont (12.0f, juce::Font::plain);
    }
};

class ADM_OSC_Music_PannerAudioProcessorEditor::ClickHotspot final : public juce::Component
{
public:
    std::function<void()> onClick;

    void paint (juce::Graphics&) override {}

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (event.mouseWasClicked() && onClick != nullptr)
            onClick();
    }
};

//==============================================================================
PositionDisplay::PositionDisplay (ADM_OSC_Music_PannerAudioProcessor& p)
    : processor (p), state (p.getValueTreeState())
{
    posXValue = state.getRawParameterValue (ParameterIDs::posX);
    posYValue = state.getRawParameterValue (ParameterIDs::posY);
    posZValue = state.getRawParameterValue (ParameterIDs::posZ);

    posXParam = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (ParameterIDs::posX));
    posYParam = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (ParameterIDs::posY));
    posZParam = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (ParameterIDs::posZ));

    jassert (posXValue != nullptr && posYValue != nullptr && posZValue != nullptr);
    jassert (posXParam != nullptr && posYParam != nullptr && posZParam != nullptr);

    updateFromParameters();

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (50);
}

PositionDisplay::~PositionDisplay()
{
    stopTimer();
}

void PositionDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto interactive = getInteractiveBounds();
    auto centre = interactive.getCentre();

    juce::ColourGradient background (Palette::panelFill.brighter (0.05f), bounds.getTopLeft(),
                                     Palette::panelFill.darker (0.2f), bounds.getBottomRight(), false);
    g.setGradientFill (background);
    g.fillRoundedRectangle (bounds, 10.0f);

    g.setColour (Palette::panelHighlight.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds, 10.0f, 1.4f);

    g.setColour (Palette::panelFill.darker (0.1f));
    g.fillRoundedRectangle (interactive, 8.0f);
    g.setColour (Palette::panelHighlight.withAlpha (0.35f));
    g.drawRoundedRectangle (interactive, 8.0f, 1.2f);

    auto drawAxis = [&g] (juce::Point<float> start, juce::Point<float> end)
    {
        g.drawLine (juce::Line<float> (start, end), 2.0f);
    };

    g.setColour (Palette::textSecondary.withAlpha (0.6f));
    drawAxis ({ interactive.getX(), centre.y }, { interactive.getRight(), centre.y });
    drawAxis ({ centre.x, interactive.getY() }, { centre.x, interactive.getBottom() });

    g.setColour (Palette::textSecondary.withAlpha (0.8f));
    g.drawFittedText ("Y", static_cast<int> (centre.x) - 6, static_cast<int> (interactive.getY()) - 18, 12, 14,
                      juce::Justification::centred, 1);
    g.drawFittedText ("X", static_cast<int> (interactive.getRight()) + 6, static_cast<int> (centre.y) - 7, 12, 14,
                      juce::Justification::centred, 1);

    const auto pointX = juce::jmap (cachedX, -1.0f, 1.0f, interactive.getX(), interactive.getRight());
    const auto pointY = juce::jmap (cachedY, -1.0f, 1.0f, interactive.getBottom(), interactive.getY());
    const auto zNormal = juce::jlimit (0.0f, 1.0f, juce::jmap (cachedZ, -1.0f, 1.0f, 0.0f, 1.0f));

    const float haloRadius = juce::jmap (zNormal, 0.0f, 1.0f, 10.0f, 18.0f);
    g.setColour (Palette::panPointSoft.withAlpha (0.3f + 0.2f * zNormal));
    g.fillEllipse (pointX - haloRadius, pointY - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);

    const float baseRadius = 8.0f;
    g.setColour (Palette::panPoint);
    g.fillEllipse (pointX - baseRadius, pointY - baseRadius, baseRadius * 2.0f, baseRadius * 2.0f);
}
void PositionDisplay::resized()
{
}

void PositionDisplay::timerCallback()
{
    updateFromParameters();
    repaint();
}

void PositionDisplay::mouseDown (const juce::MouseEvent& event)
{
    setParametersFromPoint (event.position);
}

void PositionDisplay::mouseDrag (const juce::MouseEvent& event)
{
    setParametersFromPoint (event.position);
}

void PositionDisplay::updateFromParameters()
{
    if (posXValue != nullptr)
        cachedX = posXValue->load();

    if (posYValue != nullptr)
        cachedY = posYValue->load();

    if (posZValue != nullptr)
        cachedZ = posZValue->load();
}

void PositionDisplay::setParametersFromPoint (juce::Point<float> point)
{
    auto interactive = getInteractiveBounds();

    point.x = juce::jlimit (interactive.getX(), interactive.getRight(), point.x);
    point.y = juce::jlimit (interactive.getY(), interactive.getBottom(), point.y);

    const auto normX = interactive.getWidth() > 0.0f ? (point.x - interactive.getX()) / interactive.getWidth() : 0.5f;
    const auto normY = interactive.getHeight() > 0.0f ? (point.y - interactive.getY()) / interactive.getHeight() : 0.5f;

    const auto worldX = juce::jmap (normX, 0.0f, 1.0f, -1.0f, 1.0f);
    const auto worldY = juce::jmap (normY, 0.0f, 1.0f, 1.0f, -1.0f);

    if (posXParam != nullptr)
        posXParam->setValueNotifyingHost (posXParam->getNormalisableRange().convertTo0to1 (worldX));

    if (posYParam != nullptr)
        posYParam->setValueNotifyingHost (posYParam->getNormalisableRange().convertTo0to1 (worldY));
}

juce::Rectangle<float> PositionDisplay::getInteractiveBounds() const
{
    auto area = getLocalBounds().toFloat().reduced (circlePadding);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    return { area.getCentreX() - size * 0.5f,
             area.getCentreY() - size * 0.5f,
             size, size };
}

//==============================================================================
ElevationDisplay::ElevationDisplay (ADM_OSC_Music_PannerAudioProcessor& p)
    : processor (p), state (p.getValueTreeState())
{
    posXValue = state.getRawParameterValue (ParameterIDs::posX);
    posZValue = state.getRawParameterValue (ParameterIDs::posZ);

    posXParam = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (ParameterIDs::posX));
    posZParam = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (ParameterIDs::posZ));

    jassert (posXValue != nullptr && posZValue != nullptr);
    jassert (posXParam != nullptr && posZParam != nullptr);

    updateFromParameters();

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (50);
}

ElevationDisplay::~ElevationDisplay()
{
    stopTimer();
}

void ElevationDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto interactive = getInteractiveBounds();
    auto centre = interactive.getCentre();

    juce::ColourGradient background (Palette::panelFill.brighter (0.04f), bounds.getTopLeft(),
                                     Palette::panelFill.darker (0.25f), bounds.getBottomRight(), false);
    g.setGradientFill (background);
    g.fillRoundedRectangle (bounds, 10.0f);

    g.setColour (Palette::panelHighlight.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds, 10.0f, 1.4f);

    g.setColour (Palette::panelFill.darker (0.12f));
    g.fillRoundedRectangle (interactive, 8.0f);
    g.setColour (Palette::panelHighlight.withAlpha (0.35f));
    g.drawRoundedRectangle (interactive, 8.0f, 1.2f);

    g.setColour (Palette::textSecondary.withAlpha (0.6f));
    g.drawLine (juce::Line<float> ({ interactive.getX(), centre.y }, { interactive.getRight(), centre.y }), 2.0f);
    g.drawLine (juce::Line<float> ({ centre.x, interactive.getY() }, { centre.x, interactive.getBottom() }), 2.0f);

    g.setColour (Palette::textSecondary.withAlpha (0.8f));
    g.drawFittedText ("Z", static_cast<int> (centre.x) - 6, static_cast<int> (interactive.getY()) - 18, 12, 14,
                      juce::Justification::centred, 1);
    g.drawFittedText ("X", static_cast<int> (interactive.getRight()) + 6, static_cast<int> (centre.y) - 7, 12, 14,
                      juce::Justification::centred, 1);

    const auto pointX = juce::jmap (cachedX, -1.0f, 1.0f, interactive.getX(), interactive.getRight());
    const auto pointY = juce::jmap (cachedZ, -1.0f, 1.0f, interactive.getBottom(), interactive.getY());
    const auto zNormal = juce::jlimit (0.0f, 1.0f, juce::jmap (cachedZ, -1.0f, 1.0f, 0.0f, 1.0f));

    const float haloRadius = juce::jmap (zNormal, 0.0f, 1.0f, 10.0f, 18.0f);
    g.setColour (Palette::sliderZ.darker (0.45f).withAlpha (0.3f + 0.2f * zNormal));
    g.fillEllipse (pointX - haloRadius, pointY - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);

    const float baseRadius = 8.0f;
    g.setColour (Palette::sliderZ);
    g.fillEllipse (pointX - baseRadius, pointY - baseRadius, baseRadius * 2.0f, baseRadius * 2.0f);
}
void ElevationDisplay::resized()
{
}

void ElevationDisplay::timerCallback()
{
    updateFromParameters();
    repaint();
}

void ElevationDisplay::mouseDown (const juce::MouseEvent& event)
{
    setParametersFromPoint (event.position);
}

void ElevationDisplay::mouseDrag (const juce::MouseEvent& event)
{
    setParametersFromPoint (event.position);
}

void ElevationDisplay::updateFromParameters()
{
    if (posXValue != nullptr)
        cachedX = posXValue->load();

    if (posZValue != nullptr)
        cachedZ = posZValue->load();
}

void ElevationDisplay::setParametersFromPoint (juce::Point<float> point)
{
    auto interactive = getInteractiveBounds();

    point.x = juce::jlimit (interactive.getX(), interactive.getRight(), point.x);
    point.y = juce::jlimit (interactive.getY(), interactive.getBottom(), point.y);

    const auto normX = interactive.getWidth() > 0.0f ? (point.x - interactive.getX()) / interactive.getWidth() : 0.5f;
    const auto normZ = interactive.getHeight() > 0.0f ? (point.y - interactive.getY()) / interactive.getHeight() : 0.5f;

    const auto worldX = juce::jmap (normX, 0.0f, 1.0f, -1.0f, 1.0f);
    const auto worldZ = juce::jmap (normZ, 0.0f, 1.0f, 1.0f, -1.0f);

    if (posXParam != nullptr)
        posXParam->setValueNotifyingHost (posXParam->getNormalisableRange().convertTo0to1 (worldX));

    if (posZParam != nullptr)
        posZParam->setValueNotifyingHost (posZParam->getNormalisableRange().convertTo0to1 (worldZ));
}

juce::Rectangle<float> ElevationDisplay::getInteractiveBounds() const
{
    auto area = getLocalBounds().toFloat().reduced (circlePadding);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    return { area.getCentreX() - size * 0.5f,
             area.getCentreY() - size * 0.5f,
             size, size };
}

ADM_OSC_Music_PannerAudioProcessorEditor::ADM_OSC_Music_PannerAudioProcessorEditor (ADM_OSC_Music_PannerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      audioProcessor (p),
      positionDisplay (p),
      elevationDisplay (p)
{
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName (uiTypefaceName);

    auto& state = audioProcessor.getValueTreeState();

    // === HEADER ===
    const bool showBeta = ! juce::JUCEApplicationBase::isStandaloneApp();
    if (showBeta)
    {
        addAndMakeVisible (betaLabel);
        betaLabel.setText ("BETA", juce::dontSendNotification);
        betaLabel.setFont (uiFont (20.0f, juce::Font::bold));
        betaLabel.setJustificationType (juce::Justification::centredLeft);
        betaLabel.setColour (juce::Label::textColourId, juce::Colour (0xfffff15a));
    }
    else
    {
        betaLabel.setVisible (false);
    }

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("ADM-OSC PANNER", juce::dontSendNotification);
    titleLabel.setFont (uiFont (21.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, Palette::textPrimary);

    companyLabel.setVisible (false);

    addAndMakeVisible (pierrotLink);
    pierrotLink.setColour (juce::HyperlinkButton::textColourId, Palette::textSecondary);
    pierrotLink.setFont (uiFont (13.0f, juce::Font::plain), false, juce::Justification::centredRight);

    configureLabel (posXLabel, "X");
    configureLabel (posYLabel, "Y");
    configureLabel (posZLabel, "Z");
    posXLabel.setColour (juce::Label::textColourId, Palette::sliderX);
    posYLabel.setColour (juce::Label::textColourId, Palette::sliderY);
    posZLabel.setColour (juce::Label::textColourId, Palette::sliderZ);
    posXLabel.setJustificationType (juce::Justification::centred);
    posYLabel.setJustificationType (juce::Justification::centred);
    posZLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (oscSectionTitle);
    oscSectionTitle.setText ("OSC", juce::dontSendNotification);
    oscSectionTitle.setFont (uiFont (19.0f, juce::Font::bold));
    oscSectionTitle.setJustificationType (juce::Justification::centred);
    oscSectionTitle.setColour (juce::Label::textColourId, Palette::textPrimary);

    auto configureOscActionButton = [] (ClickHotspot& hotspot)
    {
        hotspot.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        hotspot.setWantsKeyboardFocus (false);
    };
    receiveToggleHitbox = std::make_unique<ClickHotspot>();
    sendPortToggleHitbox = std::make_unique<ClickHotspot>();
    sendHostToggleHitbox = std::make_unique<ClickHotspot>();
    configureOscActionButton (*receiveToggleHitbox);
    configureOscActionButton (*sendPortToggleHitbox);
    configureOscActionButton (*sendHostToggleHitbox);

    receiveToggleHitbox->onClick = [this]
    {
        audioProcessor.setOscInputEnabled (! audioProcessor.isOscInputEnabled());
        updateOscEnablementUI();
    };

    sendPortToggleHitbox->onClick = [this]
    {
        audioProcessor.setOscOutputEnabled (! audioProcessor.isOscOutputEnabled());
        updateOscEnablementUI();
    };

    sendHostToggleHitbox->onClick = [this]
    {
        audioProcessor.setOscOutputEnabled (! audioProcessor.isOscOutputEnabled());
        updateOscEnablementUI();
    };

    receiveLabel.setText ("OSC In (Port)", juce::dontSendNotification);
    receiveLabel.setFont (uiMediumFont (13.0f));
    receiveLabel.setJustificationType (juce::Justification::left);
    receiveLabel.setColour (juce::Label::textColourId, Palette::textSecondary);

    sendLabel.setText ("OSC Out (Port)", juce::dontSendNotification);
    sendLabel.setFont (uiMediumFont (13.0f));
    sendLabel.setJustificationType (juce::Justification::left);
    sendLabel.setColour (juce::Label::textColourId, Palette::textSecondary);

    // === OSC TAB ===
    configurePortEditor (receivePortEditor);
    receivePortEditor.onReturnKey = [this] { commitReceivePortFromEditor(); };
    receivePortEditor.onFocusLost = [this] { commitReceivePortFromEditor(); };
    receivePortEditor.onEscapeKey = [this] { refreshPortEditors(); };

    configurePortEditor (sendPortEditor);
    sendPortEditor.onReturnKey = [this] { commitSendPortFromEditor(); };
    sendPortEditor.onFocusLost = [this] { commitSendPortFromEditor(); };
    sendPortEditor.onEscapeKey = [this] { refreshPortEditors(); };

    receiveStatusLight = std::make_unique<StatusLight> (Palette::sliderThumb);
    sendStatusLight    = std::make_unique<StatusLight> (Palette::sliderThumb);

    sendHostLabel.setText ("OSC Out (IP)", juce::dontSendNotification);
    sendHostLabel.setFont (uiMediumFont (13.0f));
    sendHostLabel.setJustificationType (juce::Justification::left);
    sendHostLabel.setColour (juce::Label::textColourId, Palette::textSecondary);
    sendHostLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (sendHostLabel);

    sendHostEditor.setInputRestrictions (0, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:");
    sendHostEditor.setJustification (juce::Justification::centred);
    sendHostEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    sendHostEditor.setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    sendHostEditor.setColour (juce::TextEditor::backgroundColourId, Palette::mutedFill);
    sendHostEditor.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
    sendHostEditor.setColour (juce::TextEditor::highlightColourId, Palette::accentSoft.withAlpha (0.45f));
    sendHostEditor.setFont (uiMediumFont (12.0f));
    sendHostEditor.setSelectAllWhenFocused (true);
    sendHostEditor.onReturnKey = [this] { commitSendHostFromEditor(); };
    sendHostEditor.onFocusLost = [this] { commitSendHostFromEditor(); };
    sendHostEditor.onEscapeKey = [this] { refreshSendHostEditor(); };
    addAndMakeVisible (sendHostEditor);

    oscFormatLabel.setText ("FORMAT", juce::dontSendNotification);
    oscFormatLabel.setFont (uiMediumFont (13.0f));
    oscFormatLabel.setJustificationType (juce::Justification::left);
    oscFormatLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    oscFormatLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (oscFormatLabel);

    oscFormatBox.addItem ("Cartesian", 1);
    oscFormatBox.addItem ("Polar", 2);
    static CompactComboBoxLookAndFeel compactComboLookAndFeel;
    oscFormatBox.setLookAndFeel (&compactComboLookAndFeel);
    oscFormatBox.setJustificationType (juce::Justification::centred);
    oscFormatBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    oscFormatBox.setColour (juce::ComboBox::backgroundColourId, Palette::mutedFill);
    oscFormatBox.setColour (juce::ComboBox::textColourId, Palette::textPrimary);
    oscFormatBox.onChange = [this] { commitOscFormatFromEditor(); };
    addAndMakeVisible (oscFormatBox);

    addAndMakeVisible (receiveLabel);
    addAndMakeVisible (sendLabel);
    addAndMakeVisible (*receiveToggleHitbox);
    addAndMakeVisible (*sendPortToggleHitbox);
    addAndMakeVisible (*sendHostToggleHitbox);
    addAndMakeVisible (receivePortEditor);
    addAndMakeVisible (sendPortEditor);
    addAndMakeVisible (*receiveStatusLight);
    addAndMakeVisible (*sendStatusLight);

    objectLabel.setText ("OBJ", juce::dontSendNotification);
    objectLabel.setFont (uiMediumFont (14.0f));
    objectLabel.setJustificationType (juce::Justification::centred);
    objectLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    objectLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (objectLabel);

    objectEditor.setInputRestrictions (3, "0123456789");
    objectEditor.setJustification (juce::Justification::centred);
    objectEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    objectEditor.setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    objectEditor.setColour (juce::TextEditor::backgroundColourId, Palette::mutedFill);
    objectEditor.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
    objectEditor.setFont (uiMediumFont (14.0f));
    objectEditor.setSelectAllWhenFocused (true);
    objectEditor.onReturnKey = [this] { commitObjectNumberFromEditor(); };
    objectEditor.onFocusLost = [this] { commitObjectNumberFromEditor(); };
    objectEditor.onEscapeKey = [this] { refreshObjectEditor(); };
    addAndMakeVisible (objectEditor);

    addAndMakeVisible (mapsSectionLabel);
    mapsSectionLabel.setText ("PANNER", juce::dontSendNotification);
    mapsSectionLabel.setFont (uiFont (19.0f, juce::Font::bold));
    mapsSectionLabel.setJustificationType (juce::Justification::centred);
    mapsSectionLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    mapsSectionLabel.setInterceptsMouseClicks (false, false);

    // === 2D MAP ===
    addAndMakeVisible (positionDisplay);
    addAndMakeVisible (elevationDisplay);

    oscInfoLabel.setText ("", juce::dontSendNotification);
    oscInfoLabel.setFont (uiMediumFont (11.0f));
    oscInfoLabel.setJustificationType (juce::Justification::left);
    oscInfoLabel.setColour (juce::Label::textColourId, Palette::textSecondary);
    oscInfoLabel.setInterceptsMouseClicks (false, false);
    oscInfoLabel.setVisible (true);
    addAndMakeVisible (oscInfoLabel);

    // === CONTROL ===
    controlTitle.setText ("POSITION", juce::dontSendNotification);
    controlTitle.setFont (uiFont (19.0f, juce::Font::bold));
    controlTitle.setJustificationType (juce::Justification::centred);
    controlTitle.setColour (juce::Label::textColourId, Palette::textPrimary);
    addAndMakeVisible (controlTitle);

    initialiseSlider (posXSlider, false, false, 0, 0);
    initialiseSlider (posYSlider, false, false, 0, 0);
    initialiseSlider (posZSlider, false, false, 0, 0);

    posXSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    posYSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    posZSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    addAndMakeVisible (posXLabel);
    addAndMakeVisible (posYLabel);
    addAndMakeVisible (posZLabel);
    addAndMakeVisible (posXSlider);
    addAndMakeVisible (posYSlider);
    addAndMakeVisible (posZSlider);

    posXSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    posYSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    posZSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    posXSlider.setSliderStyle (juce::Slider::LinearVertical);
    posYSlider.setSliderStyle (juce::Slider::LinearVertical);
    posZSlider.setSliderStyle (juce::Slider::LinearVertical);

    posXSlider.setColour (juce::Slider::thumbColourId, Palette::sliderX);
    posXSlider.setColour (juce::Slider::rotarySliderFillColourId, Palette::sliderX.withAlpha (0.8f));
    posYSlider.setColour (juce::Slider::thumbColourId, Palette::sliderY);
    posYSlider.setColour (juce::Slider::rotarySliderFillColourId, Palette::sliderY.withAlpha (0.8f));
    posZSlider.setColour (juce::Slider::thumbColourId, Palette::sliderZ);
    posZSlider.setColour (juce::Slider::rotarySliderFillColourId, Palette::sliderZ.withAlpha (0.8f));

    posXAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::posX, posXSlider);
    posYAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::posY, posYSlider);
    posZAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::posZ, posZSlider);

    refreshPortEditors();
    refreshObjectEditor();
    updateOscEnablementUI();
    juce::Desktop::getInstance().addGlobalMouseListener (static_cast<juce::Component*> (this));
    startTimerHz (20);
    timerCallback();

    refreshOscFormatEditors();
    setSize (615, 615);
}

ADM_OSC_Music_PannerAudioProcessorEditor::~ADM_OSC_Music_PannerAudioProcessorEditor()
{
    juce::Desktop::getInstance().removeGlobalMouseListener (static_cast<juce::Component*> (this));
    oscFormatBox.setLookAndFeel (nullptr);
    stopTimer();
}

void ADM_OSC_Music_PannerAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient background (Palette::backgroundTop, bounds.getTopLeft(),
                                     Palette::backgroundBottom, bounds.getBottomRight(), false);
    background.addColour (0.35f, Palette::backgroundTop.withAlpha (0.45f));
    background.addColour (0.85f, Palette::backgroundBottom.withAlpha (0.85f));

    g.setGradientFill (background);
    g.fillAll();

    auto drawPanel = [&g] (const juce::Rectangle<int>& area, float radius)
    {
        if (area.isEmpty())
            return;

        auto r = area.toFloat();
        juce::ColourGradient fill (Palette::panelFill.withAlpha (0.96f), r.getTopLeft(),
                                   Palette::panelFill.darker (0.35f).withAlpha (0.98f), r.getBottomRight(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (r, radius);

        g.setColour (Palette::panelHighlight.withAlpha (0.55f));
        g.drawRoundedRectangle (r, radius, 1.2f);
    };

    if (! headerPanelBounds.isEmpty())
    {
        auto header = headerPanelBounds.toFloat();
        juce::ColourGradient headerGradient (Palette::panelFill.brighter (0.03f), header.getTopLeft(),
                                             Palette::panelFill.darker (0.08f), header.getBottomLeft(), false);
        g.setGradientFill (headerGradient);
        g.fillRoundedRectangle (header, 9.0f);
        g.setColour (Palette::panelHighlight.withAlpha (0.6f));
        g.drawRoundedRectangle (header, 9.0f, 1.0f);
    }

    drawPanel (oscPanelBounds, 9.0f);
    drawPanel (mapsPanelBounds, 9.0f);
    drawPanel (controlPanelBounds, 9.0f);
}

void ADM_OSC_Music_PannerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const int outerMargin = 16;
    const int topGap = 7;
    const int panelGap = 14;

    auto content = bounds.reduced (outerMargin, 10);

    headerPanelBounds = content.removeFromTop (48);
    if (betaLabel.isVisible())
    {
        const int betaW = 118;
        const int betaH = 34;
        betaLabel.setBounds (headerPanelBounds.getX() + 10,
                             headerPanelBounds.getY() + (headerPanelBounds.getHeight() - betaH) / 2,
                             betaW, betaH);
    }
    titleLabel.setBounds (headerPanelBounds.getX(), headerPanelBounds.getY(), headerPanelBounds.getWidth(), headerPanelBounds.getHeight());
    pierrotLink.setBounds (headerPanelBounds.getRight() - 170, headerPanelBounds.getY() + 11, 150, 22);

    content.removeFromTop (topGap);
    auto topRow = content.removeFromTop (190);
    content.removeFromTop (panelGap - 5);
    auto bottomRow = content;

    const int minControlWidth = 275;
    const int targetOscWidth = 292;
    const int maxOscWidth = juce::jmax (260, topRow.getWidth() - panelGap - minControlWidth);
    const int oscWidth = juce::jlimit (260, maxOscWidth, targetOscWidth);

    oscPanelBounds = topRow.removeFromLeft (oscWidth);
    topRow.removeFromLeft (panelGap);
    controlPanelBounds = topRow;
    mapsPanelBounds = bottomRow.withTrimmedBottom (5);

    oscSectionTitle.setBounds (oscPanelBounds.getX(), oscPanelBounds.getY() + 11, oscPanelBounds.getWidth(), 24);
    oscSectionTitle.setJustificationType (juce::Justification::centred);

    const int left = oscPanelBounds.getX() + 20;
    const int labelW = 130;
    const int valueX = left + labelW - 5;
    const int rowH = 30;
    const int startY = oscPanelBounds.getY() + 51;

    receiveLabel.setBounds (left, startY, labelW, 24);
    sendLabel.setBounds (left, startY + rowH, labelW, 24);
    sendHostLabel.setBounds (left, startY + rowH * 2, labelW, 24);
    if (receiveToggleHitbox != nullptr)
        receiveToggleHitbox->setBounds (receiveLabel.getBounds());
    if (sendPortToggleHitbox != nullptr)
        sendPortToggleHitbox->setBounds (sendLabel.getBounds());
    if (sendHostToggleHitbox != nullptr)
        sendHostToggleHitbox->setBounds (sendHostLabel.getBounds());
    oscFormatLabel.setBounds (left, startY + rowH * 3, labelW, 24);

    receivePortEditor.setBounds (valueX, startY, 116, 24);
    sendPortEditor.setBounds (valueX, startY + rowH, 116, 24);
    sendHostEditor.setBounds (valueX, startY + rowH * 2, 136, 24);
    oscFormatBox.setBounds (valueX, startY + rowH * 3, 136, 24);

    if (receiveStatusLight != nullptr)
        receiveStatusLight->setBounds (valueX + 124, startY + 6, 12, 12);
    if (sendStatusLight != nullptr)
        sendStatusLight->setBounds (valueX + 124, startY + rowH + 6, 12, 12);

    oscInfoLabel.setBounds (left, oscPanelBounds.getBottom() - 18, oscPanelBounds.getWidth() - 32, 14);

    const int controlTopY = controlPanelBounds.getY() + 11;
    const int objectColumnX = controlPanelBounds.getX() + 24;
    const int objectLabelW = 55;
    const int objectEditorW = 49;
    const int objectLabelH = 21;
    const int objectEditorH = 27;
    const int objectBlockGap = 2;
    const int objectBlockH = objectLabelH + objectBlockGap + objectEditorH;
    const int objectBlockY = controlPanelBounds.getY() + (controlPanelBounds.getHeight() - objectBlockH) / 2;
    objectLabel.setBounds (objectColumnX, objectBlockY, objectLabelW, objectLabelH);
    objectEditor.setBounds (objectColumnX + (objectLabelW - objectEditorW) / 2, objectBlockY + objectLabelH + objectBlockGap, objectEditorW, objectEditorH);

    const int titleX = controlPanelBounds.getX() + 12;
    const int titleW = controlPanelBounds.getWidth() - 24;
    controlTitle.setBounds (titleX, controlTopY, titleW, 24);
    controlTitle.setJustificationType (juce::Justification::centred);

    const int sliderTop = controlPanelBounds.getY() + 58;
    const int sliderH = 98;
    const int sliderW = 24;
    const int sliderAreaLeft = objectColumnX + objectLabelW + 14;
    const int sliderAreaRight = controlPanelBounds.getRight() - 12;
    const int sliderAreaWidth = juce::jmax (sliderW * 3 + 24, sliderAreaRight - sliderAreaLeft);
    const int spacing = juce::jlimit (12, 28, (sliderAreaWidth - sliderW * 3) / 2);
    const int firstX = sliderAreaLeft + (sliderAreaWidth - (sliderW * 3 + spacing * 2)) / 2;

    posXSlider.setBounds (firstX + 7, sliderTop, sliderW, sliderH);
    posYSlider.setBounds (firstX + 7 + sliderW + spacing, sliderTop, sliderW, sliderH);
    posZSlider.setBounds (firstX + 7 + (sliderW + spacing) * 2, sliderTop, sliderW, sliderH);

    posXLabel.setBounds (posXSlider.getX() - 2, posXSlider.getBottom() + 4, 34, 18);
    posYLabel.setBounds (posYSlider.getX() - 2, posYSlider.getBottom() + 4, 34, 18);
    posZLabel.setBounds (posZSlider.getX() - 2, posZSlider.getBottom() + 4, 34, 18);

    const int mapPadding = 18;
    const int mapTop = mapsPanelBounds.getY() + 51;
    const int mapHeight = mapsPanelBounds.getHeight() - 66;
    const int mapSize = juce::jmin (mapHeight, (mapsPanelBounds.getWidth() - mapPadding * 3) / 2);
    const int leftMapX = mapsPanelBounds.getX() + (mapsPanelBounds.getWidth() - (mapSize * 2 + mapPadding)) / 2;
    const int rightMapX = leftMapX + mapSize + mapPadding;

    positionDisplay.setBounds (leftMapX, mapTop, mapSize, mapSize);
    elevationDisplay.setBounds (rightMapX, mapTop, mapSize, mapSize);

    if (mapLabels.empty())
    {
        mapLabels.emplace_back (std::make_unique<juce::Label>());
        mapLabels.emplace_back (std::make_unique<juce::Label>());
        for (auto& label : mapLabels)
        {
            label->setJustificationType (juce::Justification::centred);
            label->setFont (uiFont (12.0f, juce::Font::plain));
            label->setColour (juce::Label::textColourId, Palette::textSecondary);
            label->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*label);
        }
    }

    mapLabels[0]->setText ("TOP VIEW", juce::dontSendNotification);
    mapLabels[1]->setText ("REAR VIEW", juce::dontSendNotification);
    mapLabels[0]->setBounds (positionDisplay.getX(), mapsPanelBounds.getY() + 24, positionDisplay.getWidth(), 18);
    mapLabels[1]->setBounds (elevationDisplay.getX(), mapsPanelBounds.getY() + 24, elevationDisplay.getWidth(), 18);

    mapsSectionLabel.setVisible (true);
    mapsSectionLabel.setBounds (mapsPanelBounds.getCentreX() - 60, mapsPanelBounds.getY() + 7, 120, 20);
}



void ADM_OSC_Music_PannerAudioProcessorEditor::initialiseSlider (juce::Slider& slider,
                                                                 bool isRotary,
                                                                 bool integer,
                                                                 int textBoxWidth,
                                                                 int textBoxHeight)
{
    if (isRotary)
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    else
        slider.setSliderStyle (juce::Slider::LinearHorizontal);

    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, textBoxWidth, textBoxHeight);
    slider.setColour (juce::Slider::rotarySliderFillColourId, Palette::sliderThumb.withAlpha (0.85f));
    slider.setColour (juce::Slider::thumbColourId, Palette::sliderThumb);
    slider.setColour (juce::Slider::trackColourId, Palette::sliderTrack);
    slider.setColour (juce::Slider::backgroundColourId, Palette::mutedFill);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::textPrimary);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, Palette::mutedFill);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    if (integer)
        slider.setNumDecimalPlacesToDisplay (0);
    else
        slider.setNumDecimalPlacesToDisplay (2);
}

void ADM_OSC_Music_PannerAudioProcessorEditor::configureLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (uiFont (17.0f, juce::Font::plain));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, Palette::textPrimary);
}

void ADM_OSC_Music_PannerAudioProcessorEditor::configurePortEditor (juce::TextEditor& editor)
{
    editor.setInputRestrictions (5, "0123456789");
    editor.setJustification (juce::Justification::centred);
    editor.setColour (juce::TextEditor::backgroundColourId, Palette::mutedFill);
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    editor.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
    editor.setColour (juce::TextEditor::highlightColourId, Palette::accentSoft.withAlpha (0.45f));
    editor.setFont (uiMediumFont (12.0f));
}

void ADM_OSC_Music_PannerAudioProcessorEditor::commitReceivePortFromEditor()
{
    const auto text = receivePortEditor.getText().trim();
    if (text.isNotEmpty())
        audioProcessor.setReceivePort (text.getIntValue());
    refreshPortEditors();
}

void ADM_OSC_Music_PannerAudioProcessorEditor::commitSendPortFromEditor()
{
    const auto text = sendPortEditor.getText().trim();
    if (text.isNotEmpty())
        audioProcessor.setSendPort (text.getIntValue());
    refreshPortEditors();
}

void ADM_OSC_Music_PannerAudioProcessorEditor::commitSendHostFromEditor()
{
    const auto text = sendHostEditor.getText().trim();
    audioProcessor.setSendHost (text);
    refreshSendHostEditor();
}

void ADM_OSC_Music_PannerAudioProcessorEditor::commitObjectNumberFromEditor()
{
    const auto text = objectEditor.getText().trim();
    if (text.isEmpty())
    {
        refreshObjectEditor();
        return;
    }

    int value = text.getIntValue();
    value = juce::jlimit (1, 128, value);

    if (auto* param = dynamic_cast<juce::AudioParameterInt*> (audioProcessor.getValueTreeState().getParameter (ParameterIDs::objectNumber)))
        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (value)));

    refreshObjectEditor();
}

void ADM_OSC_Music_PannerAudioProcessorEditor::commitOscFormatFromEditor()
{
    const auto selected = oscFormatBox.getSelectedId();
    const auto format = selected == 2 ? OscCoordinateFormat::polar : OscCoordinateFormat::cartesian;
    audioProcessor.setOscInputFormat (format);
    audioProcessor.setOscOutputFormat (format);
    refreshOscFormatEditors();
}

void ADM_OSC_Music_PannerAudioProcessorEditor::refreshPortEditors()
{
    receivePortEditor.setText (juce::String (audioProcessor.getReceivePort()), juce::dontSendNotification);
    sendPortEditor.setText (juce::String (audioProcessor.getSendPort()), juce::dontSendNotification);
    refreshSendHostEditor();
    refreshObjectEditor();
}

void ADM_OSC_Music_PannerAudioProcessorEditor::refreshSendHostEditor()
{
    if (! sendHostEditor.hasKeyboardFocus (true))
        sendHostEditor.setText (audioProcessor.getSendHost(), juce::dontSendNotification);
}

void ADM_OSC_Music_PannerAudioProcessorEditor::refreshObjectEditor()
{
    if (! objectEditor.hasKeyboardFocus (true))
        objectEditor.setText (juce::String (audioProcessor.getObjectNumber()), juce::dontSendNotification);
}

void ADM_OSC_Music_PannerAudioProcessorEditor::refreshOscFormatEditors()
{
    const auto selectedId = audioProcessor.getOscInputFormat() == OscCoordinateFormat::polar ? 2 : 1;
    oscFormatBox.setSelectedId (selectedId, juce::dontSendNotification);
}

void ADM_OSC_Music_PannerAudioProcessorEditor::updateOscEnablementUI()
{
    const bool inputEnabled = audioProcessor.isOscInputEnabled();
    const bool outputEnabled = audioProcessor.isOscOutputEnabled();

    receiveLabel.setColour (juce::Label::textColourId, inputEnabled ? Palette::textPrimary : Palette::textSecondary);
    receivePortEditor.setEnabled (inputEnabled);
    if (receiveStatusLight != nullptr)
    {
        receiveStatusLight->setAlpha (inputEnabled ? 1.0f : 0.35f);
        if (! inputEnabled)
            receiveStatusLight->setActive (false);
    }

    sendLabel.setColour (juce::Label::textColourId, outputEnabled ? Palette::textPrimary : Palette::textSecondary);
    sendPortEditor.setEnabled (outputEnabled);
    sendHostLabel.setColour (juce::Label::textColourId, outputEnabled ? Palette::textPrimary : Palette::textSecondary);
    sendHostEditor.setEnabled (outputEnabled);
    oscFormatLabel.setEnabled (inputEnabled || outputEnabled);
    oscFormatBox.setEnabled (inputEnabled || outputEnabled);
    if (sendStatusLight != nullptr)
    {
        sendStatusLight->setAlpha (outputEnabled ? 1.0f : 0.35f);
        if (! outputEnabled)
            sendStatusLight->setActive (false);
    }
}

void ADM_OSC_Music_PannerAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    dismissEditorsIfNeeded (event.originalComponent);
}

void ADM_OSC_Music_PannerAudioProcessorEditor::dismissEditorsIfNeeded (juce::Component* clickedComponent)
{
    auto shouldDismiss = [clickedComponent] (juce::TextEditor& editor)
    {
        if (! editor.hasKeyboardFocus (true))
            return false;

        return clickedComponent != &editor && ! editor.isParentOf (clickedComponent);
    };

    if (shouldDismiss (receivePortEditor) || shouldDismiss (sendPortEditor) || shouldDismiss (sendHostEditor) || shouldDismiss (objectEditor))
        juce::Component::unfocusAllComponents();
}

void ADM_OSC_Music_PannerAudioProcessorEditor::timerCallback()
{
    updateOscEnablementUI();

    if (betaLabel.isVisible())
    {
        const bool betaVisible = (static_cast<int> (juce::Time::getMillisecondCounterHiRes() / 320.0) % 2) == 0;
        betaLabel.setAlpha (betaVisible ? 1.0f : 0.08f);
        betaLabel.setColour (juce::Label::textColourId, betaVisible ? juce::Colour (0xfffff15a) : juce::Colour (0xffff8a00));
    }

    const bool receivingActive = audioProcessor.isReceivingActive();
    const bool sendingActive   = audioProcessor.isSendingActive();

    if (receiveStatusLight != nullptr)
        receiveStatusLight->setActive (receivingActive);

    if (sendStatusLight != nullptr)
        sendStatusLight->setActive (sendingActive);

    refreshSendHostEditor();
    refreshObjectEditor();
    refreshOscFormatEditors();
    oscInfoLabel.setText (audioProcessor.getOscInputHubStatusText(), juce::dontSendNotification);
}
