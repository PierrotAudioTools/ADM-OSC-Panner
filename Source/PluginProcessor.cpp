#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr int minOscPort = 1024;
    constexpr int maxOscPort = 65535;
    constexpr int defaultReceivePort = 9000;
    constexpr int defaultSendPort    = 9001;
    constexpr float maxDistanceValue = 1.0f;
    constexpr float parameterWriteEpsilon = 1.0e-4f;

    template <typename FloatType>
    FloatType readOscNumeric (const juce::OSCArgument& arg, FloatType fallback) noexcept
    {
        if (arg.isFloat32())
        {
            const auto value = static_cast<FloatType> (arg.getFloat32());
            return std::isfinite (value) ? value : fallback;
        }

        if (arg.isInt32())
        {
            const auto value = static_cast<FloatType> (arg.getInt32());
            return std::isfinite (value) ? value : fallback;
        }

        return fallback;
    }

    void cartesianToPolar (float x, float y, float z, float& azimuthDeg, float& elevationDeg, float& distanceValue)
    {
        const float radial = std::sqrt (x * x + y * y + z * z);
        distanceValue = juce::jlimit (0.0f, maxDistanceValue, radial);

        if (radial <= 1.0e-6f)
        {
            azimuthDeg = 0.0f;
            elevationDeg = 0.0f;
            distanceValue = 0.0f;
            return;
        }

        // ADM/ADM-OSC convention:
        // +Y = front, +X = right, +Z = up, azimuth positive to the left (counter-clockwise).
        azimuthDeg = juce::radiansToDegrees (-std::atan2 (x, y));
        elevationDeg = juce::radiansToDegrees (std::atan2 (z, std::sqrt (x * x + y * y)));
    }

    void polarToCartesian (float azimuthDeg, float elevationDeg, float distanceValue, float& x, float& y, float& z)
    {
        distanceValue = juce::jlimit (0.0f, maxDistanceValue, distanceValue);

        const float azimuthRad = juce::degreesToRadians (azimuthDeg);
        const float elevationRad = juce::degreesToRadians (juce::jlimit (-90.0f, 90.0f, elevationDeg));
        const float cosElevation = std::cos (elevationRad);

        x = distanceValue * std::sin (-azimuthRad) * cosElevation;
        y = distanceValue * std::cos (-azimuthRad) * cosElevation;
        z = distanceValue * std::sin (elevationRad);
    }

    bool updateFloatParameterIfNeeded (juce::AudioProcessorValueTreeState& params,
                                       const juce::String& parameterID,
                                       float value,
                                       float normalizedEpsilon = parameterWriteEpsilon)
    {
        if (auto* param = dynamic_cast<juce::AudioParameterFloat*> (params.getParameter (parameterID)))
        {
            const auto norm = param->getNormalisableRange().convertTo0to1 (value);
            const auto currentNorm = param->getNormalisableRange().convertTo0to1 (param->get());

            if (std::abs (norm - currentNorm) > normalizedEpsilon)
            {
                param->setValueNotifyingHost (norm);
                return true;
            }
        }

        return false;
    }

    class SharedOscInputHub final : private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
    {
    public:
        struct InstanceStatus
        {
            int activeListenPort { defaultReceivePort };
            int configuredPort { defaultReceivePort };
            int activePortSubscriberCount { 0 };
            bool thisInstanceOnActivePort { false };
            bool thisInstanceExists { false };
            bool thisInstanceIsLeader { false };
        };

        static SharedOscInputHub& get()
        {
            static SharedOscInputHub instance;
            return instance;
        }

        void setListenPort (int newPort)
        {
            const int clampedPort = juce::jlimit (minOscPort, maxOscPort, newPort);
            const juce::ScopedLock sl (lock);

            if (listenPort == clampedPort)
                return;

            listenPort = clampedPort;
            reconnectLocked();
        }

        void registerOrUpdateRoute (void* key, int objectId, std::function<void (float, float, float, bool)> callback,
                                    int preferredListenPort)
        {
            const juce::ScopedLock sl (lock);
            const bool wasEmpty = subscriptions.empty();

            Route sub;
            sub.instanceKey = key;
            sub.objectId = objectId;
            sub.order = ++nextRegistrationOrder;
            sub.preferredListenPort = juce::jlimit (minOscPort, maxOscPort, preferredListenPort);
            sub.callback = std::move (callback);
            subscriptions[key] = std::move (sub);

            rebuildRoutesLocked();

            if (wasEmpty)
                listenPort = juce::jlimit (minOscPort, maxOscPort, preferredListenPort);

            if (! receiverConnected)
                reconnectLocked();
        }

        void unregisterRoute (void* key)
        {
            const juce::ScopedLock sl (lock);

            if (subscriptions.erase (key) == 0)
                return;

            rebuildRoutesLocked();

            if (routes.empty())
                disconnectLocked();
        }

        InstanceStatus getInstanceStatus (void* key, int configuredPort) const
        {
            InstanceStatus status;
            status.configuredPort = juce::jlimit (minOscPort, maxOscPort, configuredPort);

            const juce::ScopedLock sl (lock);
            status.activeListenPort = listenPort;

            uint64_t leaderOrder = std::numeric_limits<uint64_t>::max();
            uint64_t thisOrder = std::numeric_limits<uint64_t>::max();

            for (const auto& [instanceKey, sub] : subscriptions)
            {
                juce::ignoreUnused (instanceKey);
                if (sub.preferredListenPort == listenPort)
                {
                    ++status.activePortSubscriberCount;
                    leaderOrder = juce::jmin (leaderOrder, sub.order);
                }

                if (sub.instanceKey == key)
                {
                    status.thisInstanceExists = true;
                    thisOrder = sub.order;
                    status.thisInstanceOnActivePort = (sub.preferredListenPort == listenPort);
                }
            }

            status.thisInstanceIsLeader = status.thisInstanceOnActivePort && thisOrder == leaderOrder;
            return status;
        }

    private:
        struct Route
        {
            void* instanceKey {};
            int objectId { 1 };
            uint64_t order { 0 };
            int preferredListenPort { defaultReceivePort };
            std::function<void (float, float, float, bool)> callback;
        };

        struct ObjectState
        {
            float x { 0.0f };
            float y { 0.0f };
            float z { 0.0f };
            float azimuth { 0.0f };
            float elevation { 0.0f };
            float distance { 1.0f };
        };

        void oscMessageReceived (const juce::OSCMessage& message) override
        {
            const auto address = message.getAddressPattern().toString();
            static constexpr auto prefix = "/adm/obj/";
            if (! address.startsWith (prefix))
                return;

            auto remainder = address.substring (juce::String (prefix).length());
            const auto slashPos = remainder.indexOfChar ('/');
            if (slashPos <= 0)
                return;

            const auto objectText = remainder.substring (0, slashPos);
            const auto axis = remainder.substring (slashPos + 1);
            if (axis.isEmpty())
                return;

            const int objectId = objectText.getIntValue();
            if (objectId < 1 || objectId > 128)
                return;

            std::vector<std::function<void (float, float, float, bool)>> callbacks;
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            bool isPolar = false;
            {
                const juce::ScopedLock sl (lock);

                const auto routeIt = routes.find (objectId);
                if (routeIt == routes.end())
                    return;

                auto& state = objectStates[objectId];

                if (axis == "xyz")
                {
                    if (message.size() < 2)
                        return;

                    state.x = static_cast<float> (readOscNumeric (message[0], state.x));
                    state.y = static_cast<float> (readOscNumeric (message[1], state.y));
                    if (message.size() > 2)
                        state.z = static_cast<float> (readOscNumeric (message[2], state.z));
                }
                else if (axis == "aed")
                {
                    if (message.size() < 2)
                        return;

                    state.azimuth = static_cast<float> (readOscNumeric (message[0], state.azimuth));
                    state.elevation = static_cast<float> (readOscNumeric (message[1], state.elevation));
                    if (message.size() > 2)
                        state.distance = static_cast<float> (readOscNumeric (message[2], state.distance));

                    polarToCartesian (state.azimuth, state.elevation, state.distance, state.x, state.y, state.z);
                    isPolar = true;
                }
                else if (axis == "x")
                {
                    if (message.size() < 1)
                        return;
                    state.x = static_cast<float> (readOscNumeric (message[0], state.x));
                }
                else if (axis == "y")
                {
                    if (message.size() < 1)
                        return;
                    state.y = static_cast<float> (readOscNumeric (message[0], state.y));
                }
                else if (axis == "z")
                {
                    if (message.size() < 1)
                        return;
                    state.z = static_cast<float> (readOscNumeric (message[0], state.z));
                }
                else if (axis == "a" || axis == "azimuth" || axis == "azim")
                {
                    if (message.size() < 1)
                        return;
                    state.azimuth = static_cast<float> (readOscNumeric (message[0], state.azimuth));
                    polarToCartesian (state.azimuth, state.elevation, state.distance, state.x, state.y, state.z);
                    isPolar = true;
                }
                else if (axis == "e" || axis == "elevation")
                {
                    if (message.size() < 1)
                        return;
                    state.elevation = static_cast<float> (readOscNumeric (message[0], state.elevation));
                    polarToCartesian (state.azimuth, state.elevation, state.distance, state.x, state.y, state.z);
                    isPolar = true;
                }
                else if (axis == "d" || axis == "distance")
                {
                    if (message.size() < 1)
                        return;
                    state.distance = static_cast<float> (readOscNumeric (message[0], state.distance));
                    polarToCartesian (state.azimuth, state.elevation, state.distance, state.x, state.y, state.z);
                    isPolar = true;
                }
                else
                {
                    return;
                }

                if (axis == "xyz" || axis == "x" || axis == "y" || axis == "z")
                    cartesianToPolar (state.x, state.y, state.z, state.azimuth, state.elevation, state.distance);

                callbacks.reserve (routeIt->second.size());
                for (const auto& route : routeIt->second)
                    callbacks.push_back (route.callback);
                x = state.x;
                y = state.y;
                z = state.z;
            }

            for (const auto& callback : callbacks)
            {
                if (callback)
                    callback (x, y, z, isPolar);
            }
        }

        void reconnectLocked()
        {
            disconnectLocked();
            if (routes.empty())
                return;

            if (receiver.connect (listenPort))
            {
                receiver.addListener (this);
                receiverConnected = true;
            }
            else
            {
                DBG ("ADM_OSC_Music_Panner: Failed to bind shared OSC input on port " << listenPort);
            }
        }

        void disconnectLocked()
        {
            if (! receiverConnected)
                return;

            receiver.removeListener (this);
            receiver.disconnect();
            receiverConnected = false;
        }

        void rebuildRoutesLocked()
        {
            routes.clear();
            for (const auto& [instanceKey, sub] : subscriptions)
            {
                juce::ignoreUnused (instanceKey);
                routes[sub.objectId].push_back (sub);
            }
        }

        juce::CriticalSection lock;
        juce::OSCReceiver receiver;
        int listenPort { defaultReceivePort };
        bool receiverConnected { false };
        std::unordered_map<int, std::vector<Route>> routes;
        std::unordered_map<void*, Route> subscriptions;
        uint64_t nextRegistrationOrder { 0 };
        std::unordered_map<int, ObjectState> objectStates;
    };
}

ADM_OSC_Music_PannerAudioProcessor::ADM_OSC_Music_PannerAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    currentX.store (parameters.getRawParameterValue (ParameterIDs::posX)->load());
    currentY.store (parameters.getRawParameterValue (ParameterIDs::posY)->load());
    currentZ.store (parameters.getRawParameterValue (ParameterIDs::posZ)->load());
    circleEnabled.store (parameters.getRawParameterValue (ParameterIDs::circleEnabled)->load() >= 0.5f);
    circleRadius.store (parameters.getRawParameterValue (ParameterIDs::circleRadius)->load());
    circleSubdivisionIndex.store (juce::roundToInt (parameters.getRawParameterValue (ParameterIDs::circleSubdivision)->load()));
    objectNumber.store (juce::jlimit (1, 128, juce::roundToInt (parameters.getRawParameterValue (ParameterIDs::objectNumber)->load())));
    circleStartTimeSeconds.store (getSeconds());
    circlePhaseOffset.store (std::atan2 (currentY.load(), currentX.load()));

    auto& stateTree = parameters.state;

    const auto storedReceive = static_cast<int> (stateTree.getProperty (ParameterIDs::receivePort, defaultReceivePort));
    const auto storedSend    = static_cast<int> (stateTree.getProperty (ParameterIDs::sendPort, defaultSendPort));
    const bool storedInputEnabled  = static_cast<bool> (stateTree.getProperty (ParameterIDs::oscInputEnabled, true));
    const bool storedOutputEnabled = static_cast<bool> (stateTree.getProperty (ParameterIDs::oscOutputEnabled, true));
    const int storedInputFormat = static_cast<int> (stateTree.getProperty (ParameterIDs::oscInputFormat,
                                                                           static_cast<int> (OscCoordinateFormat::cartesian)));
    const int storedOutputFormat = static_cast<int> (stateTree.getProperty (ParameterIDs::oscOutputFormat,
                                                                            static_cast<int> (OscCoordinateFormat::cartesian)));

    currentReceivePort.store (juce::jlimit (minOscPort, maxOscPort, storedReceive));
    currentSendPort.store (juce::jlimit (minOscPort, maxOscPort, storedSend));
    oscInputEnabled.store (storedInputEnabled);
    oscOutputEnabled.store (storedOutputEnabled);
    oscInputFormat.store (juce::jlimit (0, 1, storedInputFormat));
    oscOutputFormat.store (juce::jlimit (0, 1, storedOutputFormat));

    {
        const juce::ScopedLock sl (hostLock);
        currentSendHost = stateTree.getProperty (ParameterIDs::sendHost, juce::var ("127.0.0.1")).toString();
    }

    stateTree.setProperty (ParameterIDs::receivePort, currentReceivePort.load(), nullptr);
    stateTree.setProperty (ParameterIDs::sendPort, currentSendPort.load(), nullptr);
    stateTree.setProperty (ParameterIDs::circleEnabled, circleEnabled.load(), nullptr);
    stateTree.setProperty (ParameterIDs::circleRadius, circleRadius.load(), nullptr);
    stateTree.setProperty (ParameterIDs::circleSubdivision, circleSubdivisionIndex.load(), nullptr);
    stateTree.setProperty (ParameterIDs::objectNumber, objectNumber.load(), nullptr);
    stateTree.setProperty (ParameterIDs::sendHost, currentSendHost, nullptr);
    stateTree.setProperty (ParameterIDs::oscInputEnabled, oscInputEnabled.load(), nullptr);
    stateTree.setProperty (ParameterIDs::oscOutputEnabled, oscOutputEnabled.load(), nullptr);
    stateTree.setProperty (ParameterIDs::oscInputFormat, oscInputFormat.load(), nullptr);
    stateTree.setProperty (ParameterIDs::oscOutputFormat, oscOutputFormat.load(), nullptr);

    for (auto& id : { ParameterIDs::posX, ParameterIDs::posY, ParameterIDs::posZ,
                      ParameterIDs::circleEnabled, ParameterIDs::circleRadius,
                      ParameterIDs::circleSubdivision, ParameterIDs::objectNumber })
    {
        parameters.addParameterListener (id, this);
    }

    syncPolarParametersFromCartesian();

    registerWithSharedOscHub();
    configureSender();

    startTimerHz (50);
}

ADM_OSC_Music_PannerAudioProcessor::~ADM_OSC_Music_PannerAudioProcessor()
{
    stopTimer();

    unregisterFromSharedOscHub();
    oscSender.disconnect();

    for (auto& id : { ParameterIDs::posX, ParameterIDs::posY, ParameterIDs::posZ,
                      ParameterIDs::circleEnabled, ParameterIDs::circleRadius,
                      ParameterIDs::circleSubdivision, ParameterIDs::objectNumber })
    {
        parameters.removeParameterListener (id, this);
    }
}

//==============================================================================
const juce::String ADM_OSC_Music_PannerAudioProcessor::getName() const
{
    return "ADM OSC Music Panner";
}

bool ADM_OSC_Music_PannerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void ADM_OSC_Music_PannerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void ADM_OSC_Music_PannerAudioProcessor::releaseResources()
{
}

template <typename FloatType>
static void clearExtraOutputChannels (juce::AudioBuffer<FloatType>& buffer, int totalNumInputChannels)
{
    const auto numSamples = buffer.getNumSamples();

    for (int channel = totalNumInputChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);
}

void ADM_OSC_Music_PannerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    updateTransportInfo();
    clearExtraOutputChannels (buffer, getTotalNumInputChannels());
}

void ADM_OSC_Music_PannerAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer,
                                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    updateTransportInfo();
    clearExtraOutputChannels (buffer, getTotalNumInputChannels());
}

//==============================================================================
juce::AudioProcessorEditor* ADM_OSC_Music_PannerAudioProcessor::createEditor()
{
    return new ADM_OSC_Music_PannerAudioProcessorEditor (*this);
}

const juce::String ADM_OSC_Music_PannerAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return "Default";
}

void ADM_OSC_Music_PannerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void ADM_OSC_Music_PannerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void ADM_OSC_Music_PannerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        parameters.replaceState (juce::ValueTree::fromXml (*xml));

        currentX.store (parameters.getRawParameterValue (ParameterIDs::posX)->load());
        currentY.store (parameters.getRawParameterValue (ParameterIDs::posY)->load());
        currentZ.store (parameters.getRawParameterValue (ParameterIDs::posZ)->load());
        circleEnabled.store (parameters.getRawParameterValue (ParameterIDs::circleEnabled)->load() >= 0.5f);
        circleRadius.store (parameters.getRawParameterValue (ParameterIDs::circleRadius)->load());
        circleSubdivisionIndex.store (juce::roundToInt (parameters.getRawParameterValue (ParameterIDs::circleSubdivision)->load()));
        objectNumber.store (juce::jlimit (1, 128, juce::roundToInt (parameters.getRawParameterValue (ParameterIDs::objectNumber)->load())));
        {
            const juce::ScopedLock sl (hostLock);
            currentSendHost = parameters.state.getProperty (ParameterIDs::sendHost, juce::var ("127.0.0.1")).toString();
        }
        oscInputEnabled.store (static_cast<bool> (parameters.state.getProperty (ParameterIDs::oscInputEnabled, true)),
                               std::memory_order_release);
        oscOutputEnabled.store (static_cast<bool> (parameters.state.getProperty (ParameterIDs::oscOutputEnabled, true)),
                                std::memory_order_release);
        oscInputFormat.store (juce::jlimit (0, 1, static_cast<int> (parameters.state.getProperty (
                                                                        ParameterIDs::oscInputFormat,
                                                                        static_cast<int> (OscCoordinateFormat::cartesian)))));
        oscOutputFormat.store (juce::jlimit (0, 1, static_cast<int> (parameters.state.getProperty (
                                                                         ParameterIDs::oscOutputFormat,
                                                                         static_cast<int> (OscCoordinateFormat::cartesian)))));
        parameters.state.setProperty (ParameterIDs::oscInputEnabled, oscInputEnabled.load(), nullptr);
        parameters.state.setProperty (ParameterIDs::oscOutputEnabled, oscOutputEnabled.load(), nullptr);
        parameters.state.setProperty (ParameterIDs::oscInputFormat, oscInputFormat.load(), nullptr);
        parameters.state.setProperty (ParameterIDs::oscOutputFormat, oscOutputFormat.load(), nullptr);
        circleStartTimeSeconds.store (getSeconds());
        circlePhaseOffset.store (std::atan2 (currentY.load(), currentX.load()));

        syncPolarParametersFromCartesian();
        refreshSharedOscHubRoute();
        setReceivePort (static_cast<int> (parameters.state.getProperty (ParameterIDs::receivePort, defaultReceivePort)));
        setSendPort (static_cast<int> (parameters.state.getProperty (ParameterIDs::sendPort, defaultSendPort)));
        portsDirty.store (true);
        triggerAsyncUpdate();
    }
}

//==============================================================================
void ADM_OSC_Music_PannerAudioProcessor::timerCallback()
{
    updateCircularMotion();
    sendPositionOscIfChanged();
}

void ADM_OSC_Music_PannerAudioProcessor::handleAsyncUpdate()
{
    if (portsDirty.exchange (false))
        configureSender();
}

void ADM_OSC_Music_PannerAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == ParameterIDs::posX)
    {
        currentX.store (newValue);
        positionDirty.store (true, std::memory_order_release);
        syncPolarParametersFromCartesian();
    }
    else if (parameterID == ParameterIDs::posY)
    {
        currentY.store (newValue);
        positionDirty.store (true, std::memory_order_release);
        syncPolarParametersFromCartesian();
    }
    else if (parameterID == ParameterIDs::posZ)
    {
        currentZ.store (newValue);
        positionDirty.store (true, std::memory_order_release);
        syncPolarParametersFromCartesian();
    }
    else if (parameterID == ParameterIDs::circleEnabled)
    {
        circleEnabled.store (newValue >= 0.5f, std::memory_order_release);
        circleStartTimeSeconds.store (getSeconds(), std::memory_order_release);
        circlePhaseOffset.store (std::atan2 (currentY.load(), currentX.load()), std::memory_order_release);
    }
    else if (parameterID == ParameterIDs::circleRadius)
    {
        circleRadius.store (newValue, std::memory_order_release);
    }
    else if (parameterID == ParameterIDs::circleSubdivision)
    {
        circleSubdivisionIndex.store (juce::roundToInt (newValue), std::memory_order_release);
        circleStartTimeSeconds.store (getSeconds(), std::memory_order_release);
    }
    else if (parameterID == ParameterIDs::objectNumber)
    {
        objectNumber.store (juce::jlimit (1, 128, juce::roundToInt (newValue)), std::memory_order_release);
        refreshSharedOscHubRoute();
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ADM_OSC_Music_PannerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::posX, "X",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::posY, "Y",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::posZ, "Z",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::azimuth, "Azimuth",
        juce::NormalisableRange<float> (-180.0f, 180.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::elevation, "Elevation",
        juce::NormalisableRange<float> (-90.0f, 90.0f, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::distance, "Distance",
        juce::NormalisableRange<float> (0.0f, maxDistanceValue, 0.001f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::circleEnabled, "Circle Enabled", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParameterIDs::circleRadius, "Circle Radius",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::circleSubdivision, "Circle Subdivision",
        juce::StringArray { "1/1", "1/2", "1/4", "1/8" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        ParameterIDs::objectNumber, "Object Number", 1, 128, 1));

    return { params.begin(), params.end() };
}

void ADM_OSC_Music_PannerAudioProcessor::registerWithSharedOscHub()
{
    refreshSharedOscHubRoute();
}

void ADM_OSC_Music_PannerAudioProcessor::unregisterFromSharedOscHub()
{
    SharedOscInputHub::get().unregisterRoute (this);
}

void ADM_OSC_Music_PannerAudioProcessor::refreshSharedOscHubRoute()
{
    const auto objectId = juce::jlimit (1, 128, objectNumber.load (std::memory_order_acquire));

    SharedOscInputHub::get().registerOrUpdateRoute (
        this, objectId,
        [this] (float x, float y, float z, bool isPolar)
        {
            if (! oscInputEnabled.load (std::memory_order_acquire))
                return;

            const auto inputFormat = getOscInputFormat();
            if (inputFormat == OscCoordinateFormat::cartesian && isPolar)
                return;
            if (inputFormat == OscCoordinateFormat::polar && ! isPolar)
                return;

            lastReceiveTimeSeconds.store (getSeconds(), std::memory_order_release);
            juce::ignoreUnused (isPolar);
            updatePositionFromOSC (x, y, z);
        },
        currentReceivePort.load (std::memory_order_acquire));
}

void ADM_OSC_Music_PannerAudioProcessor::configureSender()
{
    oscSender.disconnect();

    if (! oscOutputEnabled.load (std::memory_order_acquire))
        return;

    const auto port = currentSendPort.load();

    if (port <= 0)
        return;

    juce::String hostCopy;
    {
        const juce::ScopedLock sl (hostLock);
        hostCopy = currentSendHost;
    }

    if (! oscSender.connect (hostCopy, port))
        DBG ("ADM_OSC_Music_Panner: Failed to connect OSC sender on port " << port);
}

void ADM_OSC_Music_PannerAudioProcessor::sendPositionOscIfChanged()
{
    if (! positionDirty.exchange (false, std::memory_order_acq_rel))
        return;

    if (! oscOutputEnabled.load (std::memory_order_acquire))
        return;

    const auto now = getSeconds();

    auto sanitize = [] (float value) noexcept
    {
        return std::abs (value) < 1.0e-6f ? 0.0f : value;
    };

    const auto x = sanitize (currentX.load());
    const auto y = sanitize (currentY.load());
    const auto z = sanitize (currentZ.load());
    const auto objectId = juce::jlimit (1, 128, objectNumber.load());

    bool sent = false;
    if (getOscOutputFormat() == OscCoordinateFormat::polar)
    {
        float az = 0.0f, el = 0.0f, dist = 0.0f;
        cartesianToPolar (x, y, z, az, el, dist);
        auto address = "/adm/obj/" + juce::String (objectId) + "/aed";
        sent = oscSender.send (address.toRawUTF8(), sanitize (az), sanitize (el), sanitize (dist));
    }
    else
    {
        auto address = "/adm/obj/" + juce::String (objectId) + "/xyz";
        sent = oscSender.send (address.toRawUTF8(), x, y, z);
    }

    if (sent)
    {
        lastSendTimeSeconds.store (now, std::memory_order_release);
    }
    else
    {
        // Silently ignore send failures to avoid flooding the console.
    }
}

void ADM_OSC_Music_PannerAudioProcessor::updatePositionFromOSC (float x, float y, float z)
{
    x = juce::jlimit (-1.0f, 1.0f, x);
    y = juce::jlimit (-1.0f, 1.0f, y);
    z = juce::jlimit (-1.0f, 1.0f, z);

    bool changed = false;
    changed |= updateFloatParameterIfNeeded (parameters, ParameterIDs::posX, x);
    changed |= updateFloatParameterIfNeeded (parameters, ParameterIDs::posY, y);
    changed |= updateFloatParameterIfNeeded (parameters, ParameterIDs::posZ, z);

    if (changed)
        positionDirty.store (true, std::memory_order_release);
}

void ADM_OSC_Music_PannerAudioProcessor::syncPolarParametersFromCartesian()
{
    const float x = currentX.load (std::memory_order_acquire);
    const float y = currentY.load (std::memory_order_acquire);
    const float z = currentZ.load (std::memory_order_acquire);

    float az = 0.0f, el = 0.0f, dist = 0.0f;
    cartesianToPolar (x, y, z, az, el, dist);
    juce::ignoreUnused (az, el, dist);
}

void ADM_OSC_Music_PannerAudioProcessor::updateCircularMotion()
{
    if (! circleEnabled.load (std::memory_order_acquire))
        return;

    const auto bpmValue = getCurrentBpm().value_or (120.0);
    const auto now = getSeconds();
    const auto start = circleStartTimeSeconds.load (std::memory_order_acquire);
    const auto angleOffset = circlePhaseOffset.load (std::memory_order_acquire);
    const auto elapsedSeconds = juce::jmax (0.0, now - start);

    const double beats = elapsedSeconds * bpmValue / 60.0;
    const int subdivisionIndex = juce::jlimit (0, 3, circleSubdivisionIndex.load (std::memory_order_acquire));
    const double denom = subdivisionIndex == 0 ? 1.0 : (subdivisionIndex == 1 ? 2.0 : (subdivisionIndex == 2 ? 3.0 : 6.0));
    const double rotations = beats / juce::jmax (0.001, denom);
    const double angle = angleOffset + juce::MathConstants<double>::twoPi * rotations;

    const float radius = juce::jlimit (0.0f, 1.0f, circleRadius.load (std::memory_order_acquire));
    const float newX = radius * static_cast<float> (std::cos (angle));
    const float newY = radius * static_cast<float> (std::sin (angle));

    updateFloatParameterIfNeeded (parameters, ParameterIDs::posX, newX);
    updateFloatParameterIfNeeded (parameters, ParameterIDs::posY, newY);
}

std::optional<double> ADM_OSC_Music_PannerAudioProcessor::getCurrentBpm() const
{
    if (bpmValid.load (std::memory_order_acquire))
        return currentBpm.load (std::memory_order_acquire);

    return std::nullopt;
}

juce::String ADM_OSC_Music_PannerAudioProcessor::getSendHost() const
{
    const juce::ScopedLock sl (hostLock);
    return currentSendHost;
}

void ADM_OSC_Music_PannerAudioProcessor::setSendHost (juce::String newHost)
{
    newHost = newHost.trim();
    if (newHost.isEmpty())
        newHost = "127.0.0.1";

    bool changed = false;
    {
        const juce::ScopedLock sl (hostLock);
        if (currentSendHost != newHost)
        {
            currentSendHost = newHost;
            changed = true;
        }
    }

    if (changed)
    {
        parameters.state.setProperty (ParameterIDs::sendHost, currentSendHost, nullptr);
        portsDirty.store (true);
        triggerAsyncUpdate();
    }
}

void ADM_OSC_Music_PannerAudioProcessor::setOscInputEnabled (bool shouldEnable)
{
    const bool previous = oscInputEnabled.exchange (shouldEnable, std::memory_order_acq_rel);
    if (previous != shouldEnable)
    {
        parameters.state.setProperty (ParameterIDs::oscInputEnabled, shouldEnable, nullptr);
        if (! shouldEnable)
            lastReceiveTimeSeconds.store (0.0, std::memory_order_release);
    }
}

void ADM_OSC_Music_PannerAudioProcessor::setOscOutputEnabled (bool shouldEnable)
{
    const bool previous = oscOutputEnabled.exchange (shouldEnable, std::memory_order_acq_rel);
    if (previous != shouldEnable)
    {
        parameters.state.setProperty (ParameterIDs::oscOutputEnabled, shouldEnable, nullptr);
        if (! shouldEnable)
            lastSendTimeSeconds.store (0.0, std::memory_order_release);
        portsDirty.store (true);
        triggerAsyncUpdate();
    }
}

juce::String ADM_OSC_Music_PannerAudioProcessor::getOscInputHubStatusText() const
{
    const auto configuredPort = currentReceivePort.load (std::memory_order_acquire);
    const auto status = SharedOscInputHub::get().getInstanceStatus (const_cast<void*> (static_cast<const void*> (this)),
                                                                     configuredPort);

    if (! status.thisInstanceExists)
        return "OSC Hub: waiting";

    if (status.activeListenPort != status.configuredPort)
        return "OSC Hub active on " + juce::String (status.activeListenPort)
             + " (this instance configured: " + juce::String (status.configuredPort) + ")";

    if (status.activePortSubscriberCount <= 1)
        return "OSC Hub: direct listen on " + juce::String (status.activeListenPort);

    if (status.thisInstanceIsLeader)
        return "OSC Hub: main (" + juce::String (status.activePortSubscriberCount) + " linked instances)";

    return "OSC Hub: linked to main (" + juce::String (status.activePortSubscriberCount) + " instances)";
}

void ADM_OSC_Music_PannerAudioProcessor::updateTransportInfo()
{
    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 0.0)
            {
                currentBpm.store (*bpm, std::memory_order_release);
                bpmValid.store (true, std::memory_order_release);
                return;
            }
        }
    }

    bpmValid.store (false, std::memory_order_release);
}

bool ADM_OSC_Music_PannerAudioProcessor::isReceivingActive() const
{
    if (! oscInputEnabled.load (std::memory_order_acquire))
        return false;

    return (getSeconds() - lastReceiveTimeSeconds.load (std::memory_order_acquire)) < 0.5;
}

bool ADM_OSC_Music_PannerAudioProcessor::isSendingActive() const
{
    if (! oscOutputEnabled.load (std::memory_order_acquire))
        return false;

    return (getSeconds() - lastSendTimeSeconds.load (std::memory_order_acquire)) < 0.5;
}

bool ADM_OSC_Music_PannerAudioProcessor::isOscInputEnabled() const noexcept
{
    return oscInputEnabled.load (std::memory_order_acquire);
}

bool ADM_OSC_Music_PannerAudioProcessor::isOscOutputEnabled() const noexcept
{
    return oscOutputEnabled.load (std::memory_order_acquire);
}

OscCoordinateFormat ADM_OSC_Music_PannerAudioProcessor::getOscInputFormat() const noexcept
{
    return static_cast<OscCoordinateFormat> (juce::jlimit (0, 1, oscInputFormat.load (std::memory_order_acquire)));
}

OscCoordinateFormat ADM_OSC_Music_PannerAudioProcessor::getOscOutputFormat() const noexcept
{
    return static_cast<OscCoordinateFormat> (juce::jlimit (0, 1, oscOutputFormat.load (std::memory_order_acquire)));
}

double ADM_OSC_Music_PannerAudioProcessor::getSeconds() noexcept
{
    return juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void ADM_OSC_Music_PannerAudioProcessor::setReceivePort (int newPort)
{
    newPort = juce::jlimit (minOscPort, maxOscPort, newPort);
    const auto previous = currentReceivePort.exchange (newPort);
    parameters.state.setProperty (ParameterIDs::receivePort, newPort, nullptr);

    if (previous != newPort)
        SharedOscInputHub::get().setListenPort (newPort);
}

void ADM_OSC_Music_PannerAudioProcessor::setOscInputFormat (OscCoordinateFormat format)
{
    const int value = juce::jlimit (0, 1, static_cast<int> (format));
    const int previous = oscInputFormat.exchange (value, std::memory_order_acq_rel);
    if (previous != value)
        parameters.state.setProperty (ParameterIDs::oscInputFormat, value, nullptr);
}

void ADM_OSC_Music_PannerAudioProcessor::setOscOutputFormat (OscCoordinateFormat format)
{
    const int value = juce::jlimit (0, 1, static_cast<int> (format));
    const int previous = oscOutputFormat.exchange (value, std::memory_order_acq_rel);
    if (previous != value)
        parameters.state.setProperty (ParameterIDs::oscOutputFormat, value, nullptr);
}

void ADM_OSC_Music_PannerAudioProcessor::setSendPort (int newPort)
{
    newPort = juce::jlimit (minOscPort, maxOscPort, newPort);
    const auto previous = currentSendPort.exchange (newPort);
    parameters.state.setProperty (ParameterIDs::sendPort, newPort, nullptr);

    if (previous != newPort)
    {
        portsDirty.store (true);
        triggerAsyncUpdate();
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ADM_OSC_Music_PannerAudioProcessor();
}
