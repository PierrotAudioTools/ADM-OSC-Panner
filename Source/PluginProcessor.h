#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_osc/juce_osc.h>
#include <atomic>

namespace ParameterIDs
{
    static constexpr auto posX        = "posX";
    static constexpr auto posY        = "posY";
    static constexpr auto posZ        = "posZ";
    static constexpr auto azimuth     = "azimuth";
    static constexpr auto elevation   = "elevation";
    static constexpr auto distance    = "distance";
    static constexpr auto receivePort       = "receivePort";
    static constexpr auto sendPort          = "sendPort";
    static constexpr auto objectNumber      = "objectNumber";
    static constexpr auto sendHost          = "sendHost";
    static constexpr auto oscInputEnabled   = "oscInputEnabled";
    static constexpr auto oscOutputEnabled  = "oscOutputEnabled";
    static constexpr auto oscInputFormat    = "oscInputFormat";
    static constexpr auto oscOutputFormat   = "oscOutputFormat";
} // namespace ParameterIDs

enum class OscCoordinateFormat
{
    cartesian = 0,
    polar = 1
};

class ADM_OSC_Music_PannerAudioProcessor final : public juce::AudioProcessor,
                                                 private juce::AudioProcessorValueTreeState::Listener,
                                                 private juce::Timer,
                                                 private juce::AsyncUpdater
{
public:
    ADM_OSC_Music_PannerAudioProcessor();
    ~ADM_OSC_Music_PannerAudioProcessor() override;

    // AudioProcessor overrides
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override { juce::ignoreUnused (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

    std::atomic<float>& getCurrentX() noexcept { return currentX; }
    std::atomic<float>& getCurrentY() noexcept { return currentY; }
    std::atomic<float>& getCurrentZ() noexcept { return currentZ; }

    bool isReceivingActive() const;
    bool isSendingActive() const;
    bool isOscInputEnabled() const noexcept;
    bool isOscOutputEnabled() const noexcept;
    OscCoordinateFormat getOscInputFormat() const noexcept;
    OscCoordinateFormat getOscOutputFormat() const noexcept;
    juce::String getOscInputHubStatusText() const;
    int getObjectNumber() const noexcept { return objectNumber.load (std::memory_order_acquire); }
    juce::String getSendHost() const;
    void setSendHost (juce::String newHost);
    void setOscInputEnabled (bool shouldEnable);
    void setOscOutputEnabled (bool shouldEnable);
    void setOscInputFormat (OscCoordinateFormat format);
    void setOscOutputFormat (OscCoordinateFormat format);

    void setReceivePort (int newPort);
    void setSendPort (int newPort);

    int getReceivePort() const noexcept { return currentReceivePort.load(); }
    int getSendPort() const noexcept    { return currentSendPort.load(); }

private:
    // juce::Timer
    void timerCallback() override;

    // juce::AsyncUpdater
    void handleAsyncUpdate() override;

    // AudioProcessorValueTreeState listener
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void registerWithSharedOscHub();
    void unregisterFromSharedOscHub();
    void refreshSharedOscHubRoute();
    void configureSender();
    void sendPositionOscIfChanged();
    void requestRemotePositionSnapshot();
    static double getSeconds() noexcept;
    void updatePositionFromOSC (float x, float y, float z);
    void syncPolarParametersFromCartesian();

    juce::AudioProcessorValueTreeState parameters;

    juce::OSCSender   oscSender;

    std::atomic<float> currentX { 0.0f };
    std::atomic<float> currentY { 0.0f };
    std::atomic<float> currentZ { 0.0f };

    std::atomic<int> currentReceivePort { 4001 };
    std::atomic<int> currentSendPort    { 4001 };

    std::atomic<bool> portsDirty { false };
    std::atomic<bool> positionDirty { true };
    std::atomic<double> lastReceiveTimeSeconds { 0.0 };
    std::atomic<double> lastSendTimeSeconds { 0.0 };
    std::atomic<bool> oscInputEnabled { true };
    std::atomic<bool> oscOutputEnabled { true };
    std::atomic<int> oscInputFormat { static_cast<int> (OscCoordinateFormat::cartesian) };
    std::atomic<int> oscOutputFormat { static_cast<int> (OscCoordinateFormat::cartesian) };
    std::atomic<int>    objectNumber { 1 };
    mutable juce::CriticalSection hostLock;
    juce::String currentSendHost { "127.0.0.1" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ADM_OSC_Music_PannerAudioProcessor)
};
