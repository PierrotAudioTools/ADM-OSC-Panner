#include "PluginEditor.h"

namespace
{
    constexpr float circlePadding = 6.0f;

    namespace Palette
    {
        const juce::Colour backgroundTop    (0xff161b22);
        const juce::Colour backgroundBottom (0xff05070b);
        const juce::Colour panelFill        (0xff11161d);
        const juce::Colour panelHighlight   (0xff2b3440);
        const juce::Colour accent           (0xff4dd2ff);
        const juce::Colour accentSoft       (0xff2a8fb8);
        const juce::Colour textPrimary      (0xffecf1f6);
        const juce::Colour textSecondary    (0xff9ca8b5);
        const juce::Colour mutedFill        (0xff1a212b);
        const juce::Colour sliderTrack      (0xff273240);
        const juce::Colour sliderThumb      (0xffc8d2dd);
        const juce::Colour sliderX          (0xffffa24d);
        const juce::Colour sliderY          (0xff6cb6ff);
        const juce::Colour sliderZ          (0xff47d6b4);
        const juce::Colour warning          (0xffffbe63);
    }
}

void CircleIconComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    auto centre = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.45f;

    g.setColour (Palette::accent.withAlpha (0.18f));
    g.fillEllipse (bounds);

    g.setColour (Palette::accent);
    g.drawEllipse (bounds, 2.0f);

    juce::Line<float> horizontal (centre.x - radius, centre.y, centre.x + radius, centre.y);
    juce::Line<float> vertical   (centre.x, centre.y - radius, centre.x, centre.y + radius);
    g.setColour (Palette::accentSoft.withAlpha (0.65f));
    g.drawLine (horizontal, 1.6f);
    g.drawLine (vertical, 1.6f);

    auto drawNode = [&g] (juce::Point<float> point, juce::Colour colour)
    {
        const float nodeRadius = 6.0f;
        g.setColour (colour.withAlpha (0.18f));
        g.fillEllipse (point.x - nodeRadius, point.y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
        g.setColour (colour);
        g.drawEllipse (point.x - nodeRadius, point.y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f, 1.4f);
    };

    drawNode ({ centre.x, centre.y - radius * 0.75f }, Palette::sliderY);
    drawNode ({ centre.x, centre.y + radius * 0.75f }, Palette::sliderY);
    drawNode ({ centre.x - radius * 0.75f, centre.y }, Palette::sliderX);
    drawNode ({ centre.x + radius * 0.75f, centre.y }, Palette::sliderZ);

    const float orbitRadius = radius * 0.6f;
    g.setColour (Palette::accentSoft.withAlpha (0.35f));
    g.drawEllipse (centre.x - orbitRadius, centre.y - orbitRadius, orbitRadius * 2.0f, orbitRadius * 2.0f, 1.4f);
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
    g.fillRoundedRectangle (bounds, 22.0f);

    g.setColour (Palette::panelHighlight.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds, 22.0f, 1.4f);

    g.setColour (Palette::panelFill.darker (0.1f));
    g.fillRoundedRectangle (interactive, 18.0f);
    g.setColour (Palette::panelHighlight.withAlpha (0.35f));
    g.drawRoundedRectangle (interactive, 18.0f, 1.2f);

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
    g.setColour (Palette::accentSoft.withAlpha (0.3f + 0.2f * zNormal));
    g.fillEllipse (pointX - haloRadius, pointY - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);

    const float baseRadius = 8.0f;
    g.setColour (Palette::accent);
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
    g.fillRoundedRectangle (bounds, 20.0f);

    g.setColour (Palette::panelHighlight.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds, 20.0f, 1.4f);

    g.setColour (Palette::panelFill.darker (0.12f));
    g.fillRoundedRectangle (interactive, 16.0f);
    g.setColour (Palette::panelHighlight.withAlpha (0.35f));
    g.drawRoundedRectangle (interactive, 16.0f, 1.2f);

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
    g.setColour (Palette::accentSoft.withAlpha (0.3f + 0.2f * zNormal));
    g.fillEllipse (pointX - haloRadius, pointY - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);

    const float baseRadius = 8.0f;
    g.setColour (Palette::accent);
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
    auto& state = audioProcessor.getValueTreeState();

    // === HEADER ===
    addAndMakeVisible (titleLabel);
    titleLabel.setText ("ADM-OSC PANNER", juce::dontSendNotification);
    titleLabel.setFont (juce::Font { juce::FontOptions (18.0f, juce::Font::bold) });
    titleLabel.setJustificationType (juce::Justification::left);
    titleLabel.setColour (juce::Label::textColourId, Palette::textPrimary);

    auto configureTab = [] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font { juce::FontOptions (15.0f, juce::Font::bold) });
        label.setColour (juce::Label::textColourId, Palette::textSecondary);
        label.setInterceptsMouseClicks (false, false);
    };

    configureTab (tabOscLabel, "OSC");
    configureTab (tabFxLabel, "FX");
    configureTab (tabSettingsLabel, "SETTINGS");
    tabOscLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    tabOscLabel.setVisible (false);
    tabFxLabel.setVisible (false);
    tabSettingsLabel.setVisible (false);

    addAndMakeVisible (companyLabel);
    companyLabel.setText ("La Tool de Pierrot", juce::dontSendNotification);
    companyLabel.setFont (juce::Font { juce::FontOptions (14.0f, juce::Font::plain) });
    companyLabel.setJustificationType (juce::Justification::right);
    companyLabel.setColour (juce::Label::textColourId, Palette::textSecondary);

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
    oscSectionTitle.setFont (juce::Font { juce::FontOptions (16.0f, juce::Font::bold) });
    oscSectionTitle.setJustificationType (juce::Justification::left);
    oscSectionTitle.setColour (juce::Label::textColourId, Palette::textPrimary);

    auto configureOscToggle = [] (juce::TextButton& button, const juce::String& text)
    {
        button.setButtonText (text);
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        button.setColour (juce::TextButton::textColourOnId, Palette::textPrimary);
        button.setColour (juce::TextButton::textColourOffId, Palette::textSecondary.withAlpha (0.6f));
        button.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        button.setWantsKeyboardFocus (false);
    };

    configureOscToggle (oscInToggle, "IN");
    configureOscToggle (oscOutToggle, "OUT");

    oscInToggle.onClick = [this]
    {
        audioProcessor.setOscInputEnabled (oscInToggle.getToggleState());
        updateOscEnablementUI();
    };

    oscOutToggle.onClick = [this]
    {
        audioProcessor.setOscOutputEnabled (oscOutToggle.getToggleState());
        updateOscEnablementUI();
    };

    addAndMakeVisible (oscInToggle);
    addAndMakeVisible (oscOutToggle);

    receiveLabel.setText ("OSC In (Port)", juce::dontSendNotification);
    receiveLabel.setFont (juce::Font { juce::FontOptions (15.0f, juce::Font::bold) });
    receiveLabel.setJustificationType (juce::Justification::left);
    receiveLabel.setColour (juce::Label::textColourId, Palette::textSecondary);

    sendLabel.setText ("OSC Out (Port)", juce::dontSendNotification);
    sendLabel.setFont (juce::Font { juce::FontOptions (15.0f, juce::Font::bold) });
    sendLabel.setJustificationType (juce::Justification::left);
    sendLabel.setColour (juce::Label::textColourId, Palette::textSecondary);

    // === OSC TAB ===
    configurePortEditor (receivePortEditor);
    receivePortEditor.onReturnKey = [this] { commitReceivePortFromEditor(); };
    receivePortEditor.onFocusLost = [this] { refreshPortEditors(); };
    receivePortEditor.onEscapeKey = [this] { refreshPortEditors(); };

    configurePortEditor (sendPortEditor);
    sendPortEditor.onReturnKey = [this] { commitSendPortFromEditor(); };
    sendPortEditor.onFocusLost = [this] { refreshPortEditors(); };
    sendPortEditor.onEscapeKey = [this] { refreshPortEditors(); };

    receiveStatusLight = std::make_unique<StatusLight> (Palette::accent);
    sendStatusLight    = std::make_unique<StatusLight> (Palette::sliderZ);

    sendHostLabel.setText ("OSC Out (IP)", juce::dontSendNotification);
    sendHostLabel.setFont (juce::Font { juce::FontOptions (15.0f, juce::Font::bold) });
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
    sendHostEditor.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::bold) });
    sendHostEditor.setSelectAllWhenFocused (true);
    sendHostEditor.onReturnKey = [this] { commitSendHostFromEditor(); };
    sendHostEditor.onFocusLost = [this] { refreshSendHostEditor(); };
    sendHostEditor.onEscapeKey = [this] { refreshSendHostEditor(); };
    addAndMakeVisible (sendHostEditor);

    oscFormatLabel.setText ("FORMAT", juce::dontSendNotification);
    oscFormatLabel.setFont (juce::Font { juce::FontOptions (11.0f, juce::Font::bold) });
    oscFormatLabel.setJustificationType (juce::Justification::left);
    oscFormatLabel.setColour (juce::Label::textColourId, Palette::textSecondary);
    oscFormatLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (oscFormatLabel);

    oscFormatBox.addItem ("Cartesian", 1);
    oscFormatBox.addItem ("Polar", 2);
    oscFormatBox.setJustificationType (juce::Justification::centred);
    oscFormatBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    oscFormatBox.setColour (juce::ComboBox::backgroundColourId, Palette::mutedFill);
    oscFormatBox.setColour (juce::ComboBox::textColourId, Palette::textPrimary);
    oscFormatBox.onChange = [this] { commitOscFormatFromEditor(); };
    addAndMakeVisible (oscFormatBox);

    addAndMakeVisible (receiveLabel);
    addAndMakeVisible (sendLabel);
    addAndMakeVisible (receivePortEditor);
    addAndMakeVisible (sendPortEditor);
    addAndMakeVisible (receiveStatusLabel);
    addAndMakeVisible (sendStatusLabel);
    addAndMakeVisible (*receiveStatusLight);
    addAndMakeVisible (*sendStatusLight);

    circleToggle.setButtonText ("");
    circleToggle.setClickingTogglesState (true);
    circleToggle.setTooltip ("Enable an automatic circular path driven by the host tempo");
    circleToggle.setColour (juce::ToggleButton::textColourId, Palette::textPrimary);
    circleToggle.setColour (juce::ToggleButton::tickColourId, Palette::accent);
    circleToggle.setColour (juce::ToggleButton::tickDisabledColourId, Palette::textSecondary.withAlpha (0.4f));
    addAndMakeVisible (circleToggle);

    circleSubdivisionLabel.setText ("TIME", juce::dontSendNotification);
    circleSubdivisionLabel.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::bold) });
    circleSubdivisionLabel.setJustificationType (juce::Justification::left);
    circleSubdivisionLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.65f));
    circleSubdivisionLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (circleSubdivisionLabel);

    circleSubdivisionBox.addItemList (juce::StringArray { "1/1", "1/2", "1/4", "1/8" }, 1);
    circleSubdivisionBox.setJustificationType (juce::Justification::centred);
    circleSubdivisionBox.setSelectedId (3);
    circleSubdivisionBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    circleSubdivisionBox.setColour (juce::ComboBox::backgroundColourId, Palette::mutedFill);
    circleSubdivisionBox.setColour (juce::ComboBox::textColourId, Palette::textPrimary);
    circleSubdivisionBox.setScrollWheelEnabled (false);
    addAndMakeVisible (circleSubdivisionBox);

    circleRadiusLabel.setText ("RADIUS", juce::dontSendNotification);
    circleRadiusLabel.setFont (juce::Font { juce::FontOptions (13.0f, juce::Font::bold) });
    circleRadiusLabel.setJustificationType (juce::Justification::centred);
    circleRadiusLabel.setColour (juce::Label::textColourId, Palette::textSecondary);
    circleRadiusLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (circleRadiusLabel);

    initialiseSlider (circleRadiusSlider, true, false, 52, 18);
    circleRadiusSlider.setTextBoxStyle (juce::Slider::TextEntryBoxPosition::TextBoxBelow, false, 60, 20);
    circleRadiusSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    circleRadiusSlider.setColour (juce::Slider::textBoxTextColourId, Palette::textPrimary);
    circleRadiusSlider.setColour (juce::Slider::textBoxBackgroundColourId, Palette::mutedFill);
    addAndMakeVisible (circleRadiusSlider);

    addAndMakeVisible (circleIcon);

    fxSectionLabel.setText ("FX", juce::dontSendNotification);
    fxSectionLabel.setFont (juce::Font { juce::FontOptions (16.0f, juce::Font::bold) });
    fxSectionLabel.setJustificationType (juce::Justification::centredLeft);
    fxSectionLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    fxSectionLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (fxSectionLabel);

    circleSectionLabel.setText ("CIRCLE", juce::dontSendNotification);
    circleSectionLabel.setFont (juce::Font { juce::FontOptions (13.0f, juce::Font::bold) });
    circleSectionLabel.setJustificationType (juce::Justification::centredLeft);
    circleSectionLabel.setColour (juce::Label::textColourId, Palette::textSecondary);
    circleSectionLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (circleSectionLabel);

    circleActivateLabel.setText ("ACTIVATE", juce::dontSendNotification);
    circleActivateLabel.setFont (juce::Font { juce::FontOptions (13.0f, juce::Font::bold) });
    circleActivateLabel.setJustificationType (juce::Justification::left);
    circleActivateLabel.setColour (juce::Label::textColourId, Palette::textSecondary);
    circleActivateLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (circleActivateLabel);

    objectLabel.setText ("OBJECT", juce::dontSendNotification);
    objectLabel.setFont (juce::Font { juce::FontOptions (13.0f, juce::Font::bold) });
    objectLabel.setJustificationType (juce::Justification::centredRight);
    objectLabel.setColour (juce::Label::textColourId, Palette::textSecondary);
    objectLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (objectLabel);

    objectEditor.setInputRestrictions (3, "0123456789");
    objectEditor.setJustification (juce::Justification::centred);
    objectEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    objectEditor.setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    objectEditor.setColour (juce::TextEditor::backgroundColourId, Palette::mutedFill);
    objectEditor.setColour (juce::TextEditor::textColourId, Palette::textPrimary);
    objectEditor.setSelectAllWhenFocused (true);
    objectEditor.onReturnKey = [this] { commitObjectNumberFromEditor(); };
    objectEditor.onFocusLost = [this] { refreshObjectEditor(); };
    objectEditor.onEscapeKey = [this] { refreshObjectEditor(); };
    addAndMakeVisible (objectEditor);

    addAndMakeVisible (mapsSectionLabel);
    mapsSectionLabel.setText ("PANNER", juce::dontSendNotification);
    mapsSectionLabel.setFont (juce::Font { juce::FontOptions (16.0f, juce::Font::bold) });
    mapsSectionLabel.setJustificationType (juce::Justification::centred);
    mapsSectionLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    mapsSectionLabel.setInterceptsMouseClicks (false, false);

    // === 2D MAP ===
    addAndMakeVisible (positionDisplay);
    addAndMakeVisible (elevationDisplay);

    oscInfoLabel.setText ("", juce::dontSendNotification);
    oscInfoLabel.setFont (juce::Font { juce::FontOptions (11.0f, juce::Font::plain) });
    oscInfoLabel.setJustificationType (juce::Justification::left);
    oscInfoLabel.setColour (juce::Label::textColourId, Palette::textSecondary);
    oscInfoLabel.setInterceptsMouseClicks (false, false);
    oscInfoLabel.setVisible (true);
    addAndMakeVisible (oscInfoLabel);

    // === CONTROL ===
    controlTitle.setText ("Parameters", juce::dontSendNotification);
    controlTitle.setFont (juce::Font { juce::FontOptions (16.0f, juce::Font::bold) });
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
    circleEnabledAttachment = std::make_unique<ButtonAttachment> (state, ParameterIDs::circleEnabled, circleToggle);
    circleRadiusAttachment = std::make_unique<SliderAttachment> (state, ParameterIDs::circleRadius, circleRadiusSlider);
    circleSubdivisionAttachment = std::make_unique<ComboAttachment> (state, ParameterIDs::circleSubdivision, circleSubdivisionBox);

    refreshPortEditors();
    refreshObjectEditor();
    updateOscEnablementUI();
    startTimerHz (20);
    timerCallback();

    refreshOscFormatEditors();
    setSize (600, 750);
}

ADM_OSC_Music_PannerAudioProcessorEditor::~ADM_OSC_Music_PannerAudioProcessorEditor()
{
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
        juce::ColourGradient headerGradient (Palette::backgroundTop.brighter (0.2f), header.getTopLeft(),
                                             Palette::backgroundTop.darker (0.2f), header.getBottomLeft(), false);
        g.setGradientFill (headerGradient);
        g.fillRoundedRectangle (header, 18.0f);
        g.setColour (Palette::panelHighlight.withAlpha (0.6f));
        g.drawRoundedRectangle (header, 18.0f, 1.0f);
    }

    if (! tabPanelBounds.isEmpty())
    {
        auto tabs = tabPanelBounds.toFloat();
        g.setColour (Palette::panelFill.withAlpha (0.95f));
        g.fillRoundedRectangle (tabs, 14.0f);
        g.setColour (Palette::panelHighlight.withAlpha (0.45f));
        g.drawRoundedRectangle (tabs, 14.0f, 1.0f);

        auto activeBounds = tabOscLabel.getBounds();
        if (! activeBounds.isEmpty())
        {
            auto active = activeBounds.toFloat()
                                .withY (tabs.getY())
                                .withHeight (tabs.getHeight())
                                .expanded (6.0f, 6.0f)
                                .getIntersection (tabs);

            g.setColour (Palette::accent.withAlpha (0.14f));
            g.fillRoundedRectangle (active, 12.0f);

            juce::Line<float> underline (juce::Point<float> (static_cast<float> (activeBounds.getX()),
                                                             static_cast<float> (activeBounds.getBottom() + 6)),
                                         juce::Point<float> (static_cast<float> (activeBounds.getRight()),
                                                             static_cast<float> (activeBounds.getBottom() + 6)));
            g.setColour (Palette::accent);
            g.drawLine (underline, 2.0f);
        }
    }

    drawPanel (oscPanelBounds, 16.0f);
    drawPanel (mapsPanelBounds, 16.0f);
    drawPanel (controlPanelBounds, 16.0f);
    drawPanel (fxPanelBounds, 16.0f);
}

void ADM_OSC_Music_PannerAudioProcessorEditor::resized()
{
    headerPanelBounds = juce::Rectangle<int> (10, 10, 575, 50);
    titleLabel.setBounds (headerPanelBounds.getX() + 16, headerPanelBounds.getY() + 14, 220, 20);
    companyLabel.setBounds (headerPanelBounds.getRight() - 165, headerPanelBounds.getY() + 14, 155, 20);

    tabPanelBounds = {};
    tabOscLabel.setBounds (juce::Rectangle<int>());
    tabFxLabel.setBounds (juce::Rectangle<int>());
    tabSettingsLabel.setBounds (juce::Rectangle<int>());

    oscPanelBounds = juce::Rectangle<int> (10, 70, 270, 170);
    controlPanelBounds = juce::Rectangle<int> (310, 70, 280, 170);

    oscSectionTitle.setBounds (oscPanelBounds.getX() + (oscPanelBounds.getWidth() - 120) / 2,
                               oscPanelBounds.getY() + 10, 120, 20);
    oscSectionTitle.setFont (juce::Font { juce::FontOptions (16.0f, juce::Font::bold) });
    oscSectionTitle.setJustificationType (juce::Justification::centred);

    const int toggleHeight = oscSectionTitle.getHeight();
    const int toggleY = oscSectionTitle.getY();
    const int toggleSpacing = 6;
    const int toggleBaseX = oscSectionTitle.getRight() - 22;
    const int inToggleWidth = 34;
    oscInToggle.setBounds (toggleBaseX, toggleY - 1, inToggleWidth, toggleHeight + 2);
    oscOutToggle.setBounds (toggleBaseX + inToggleWidth + toggleSpacing, toggleY - 1, 44, toggleHeight + 2);

    receiveLabel.setBounds (27, 114, 120, 20);
    sendLabel.setBounds (27, 143, 120, 20);
    sendHostLabel.setBounds (27, 172, 120, 20);
    receiveLabel.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::plain) });
    sendLabel.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::plain) });
    sendHostLabel.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::plain) });

    receivePortEditor.setBounds (157, 110, 90, 24);
    sendPortEditor.setBounds (157, 139, 90, 24);
    sendHostEditor.setBounds (157, 168, 90, 24);
    if (receiveStatusLight != nullptr)
    {
        receiveStatusLight->setVisible (true);
        receiveStatusLight->setBounds (257, 116, 12, 12);
    }
    if (sendStatusLight != nullptr)
    {
        sendStatusLight->setVisible (true);
        sendStatusLight->setBounds (257, 145, 12, 12);
    }
    receiveStatusLabel.setVisible (false);
    sendStatusLabel.setVisible (false);
    oscFormatLabel.setBounds (27, 198, 120, 20);
    oscFormatLabel.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::plain) });
    oscFormatBox.setBounds (157, 195, 90, 24);
    objectLabel.setVisible (true);
    objectLabel.setText ("OBJECT", juce::dontSendNotification);
    objectLabel.setFont (juce::Font { juce::FontOptions (14.0f, juce::Font::bold) });
    objectLabel.setColour (juce::Label::textColourId, Palette::textPrimary);
    objectLabel.setBounds (314, 132, 80, 20);

    objectEditor.setVisible (true);
    objectEditor.setBounds (345, 156, 40, 22);

    controlTitle.setBounds (controlPanelBounds.getX() + (controlPanelBounds.getWidth() - 140) / 2,
                            controlPanelBounds.getY() + 10, 140, 20);
    controlTitle.setFont (juce::Font { juce::FontOptions (16.0f, juce::Font::bold) });
    controlTitle.setJustificationType (juce::Justification::centred);

    posXSlider.setBounds (430, 105, 24, 75);
    posYSlider.setBounds (487, 105, 24, 75);
    posZSlider.setBounds (544, 105, 24, 75);

    posXLabel.setBounds (428, 185, 30, 20);
    posYLabel.setBounds (485, 185, 30, 20);
    posZLabel.setBounds (542, 185, 30, 20);
    posXLabel.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::plain) });
    posYLabel.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::plain) });
    posZLabel.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::plain) });

    const int mapSize = 240;
    const int mapSpacing = 40;
    const int mapTop = 310;

    juce::Rectangle<int> topViewBounds (32, mapTop, mapSize, mapSize);
    juce::Rectangle<int> rearViewBounds (323, mapTop - 3, mapSize, mapSize);
    positionDisplay.setBounds (topViewBounds);
    elevationDisplay.setBounds (rearViewBounds);

    if (mapLabels.empty())
    {
        mapLabels.emplace_back (std::make_unique<juce::Label>());
        mapLabels.emplace_back (std::make_unique<juce::Label>());
        for (auto& label : mapLabels)
        {
            label->setJustificationType (juce::Justification::centred);
            label->setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::bold) });
            label->setColour (juce::Label::textColourId, Palette::textSecondary);
            label->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*label);
        }
    }

    mapLabels[0]->setText ("TOP VIEW", juce::dontSendNotification);
    mapLabels[1]->setText ("REAR VIEW", juce::dontSendNotification);
    mapLabels[0]->setBounds (102, 255, 100, 18);
    mapLabels[1]->setBounds (393, 255, 100, 18);

    mapsSectionLabel.setVisible (true);
    mapsSectionLabel.setBounds (235, 256, 120, 20);

    mapsPanelBounds = juce::Rectangle<int> (11, 251, 575, 310);

    fxPanelBounds = juce::Rectangle<int> (11, 573, 575, 163);

    fxSectionLabel.setBounds (284, 584, 80, 20);
    const int fxRowY = 640;
    circleSectionLabel.setBounds (85, fxRowY, 80, 20);
    circleToggle.setBounds (163, fxRowY, 18, 18);
    circleActivateLabel.setBounds (195, fxRowY, 80, 18);
    circleRadiusLabel.setBounds (283, fxRowY, 80, 18);
    circleRadiusSlider.setBounds (356, fxRowY - 16, 60, 60);
    circleSubdivisionLabel.setText ("TIME", juce::dontSendNotification);
    circleSubdivisionLabel.setBounds (426, fxRowY, 60, 18);
    circleSubdivisionBox.setBounds (500, fxRowY - 6, 60, 24);
    circleIcon.setBounds (22, fxRowY - 16, 40, 40);
    oscInfoLabel.setBounds (27, 222, 242, 14);
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
    label.setFont (juce::Font { juce::FontOptions (17.0f, juce::Font::bold) });
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
    editor.setFont (juce::Font { juce::FontOptions (12.0f, juce::Font::bold) });
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

    oscInToggle.setToggleState (inputEnabled, juce::dontSendNotification);
    oscOutToggle.setToggleState (outputEnabled, juce::dontSendNotification);

    receiveLabel.setEnabled (inputEnabled);
    receivePortEditor.setEnabled (inputEnabled);
    if (receiveStatusLight != nullptr)
    {
        receiveStatusLight->setAlpha (inputEnabled ? 1.0f : 0.35f);
        if (! inputEnabled)
            receiveStatusLight->setActive (false);
    }

    sendLabel.setEnabled (outputEnabled);
    sendPortEditor.setEnabled (outputEnabled);
    sendHostLabel.setEnabled (outputEnabled);
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

void ADM_OSC_Music_PannerAudioProcessorEditor::timerCallback()
{
    updateOscEnablementUI();

    const bool receivingActive = audioProcessor.isReceivingActive();
    const bool sendingActive   = audioProcessor.isSendingActive();

    if (receiveStatusLight != nullptr)
        receiveStatusLight->setActive (receivingActive);

    if (sendStatusLight != nullptr)
        sendStatusLight->setActive (sendingActive);

    receiveStatusLabel.setColour (juce::Label::textColourId,
                                  Palette::textPrimary.withAlpha (receivingActive ? 0.95f : 0.4f));
    sendStatusLabel.setColour (juce::Label::textColourId,
                               Palette::textPrimary.withAlpha (sendingActive ? 0.95f : 0.4f));

    refreshSendHostEditor();
    refreshObjectEditor();
    refreshOscFormatEditors();
    oscInfoLabel.setText (audioProcessor.getOscInputHubStatusText(), juce::dontSendNotification);
}
