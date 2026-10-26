#pragma once
#include <JuceHeader.h>
#include "FeedbackEngine.h"

class HowlShieldAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxChannels = 8;

    HowlShieldAudioProcessor();
    ~HowlShieldAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Howl Shield"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- สำหรับ UI (แยกตามช่อง 0–7) ----
    int   getNumActiveChannels() const { return juce::jlimit (1, maxChannels, getTotalNumInputChannels()); }
    int   getActiveFilterCount (int ch) const { return engines[(size_t) ch].getActiveCount(); }
    float getSpectrumDb (int ch, int i) const { return engines[(size_t) ch].getSpectrumDb (i); }
    float getSlotFreq   (int ch, int i) const { return engines[(size_t) ch].getSlotFreq (i); }
    float getSlotGainDb (int ch, int i) const { return engines[(size_t) ch].getSlotGainDb (i); }
    float getInLevelDb  (int ch) const { return inLevelDb[(size_t) ch].load(); }
    float getOutLevelDb (int ch) const { return outLevelDb[(size_t) ch].load(); }
    float getMix        (int ch) const { return mixP[(size_t) ch]->load(); }   // Dry/Wet ของช่องนั้น (0–1)

    void clearFilters()          { for (auto& e : engines) e.requestClear(); }
    void clearChannel (int ch)   { engines[(size_t) ch].requestClear(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static BusesProperties makeBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::array<FeedbackEngine, maxChannels> engines;          // 1 engine ต่อช่อง
    std::array<std::atomic<float>, maxChannels> inLevelDb {}, outLevelDb {};

    juce::AudioBuffer<float> dryBuffer;
    std::array<juce::SmoothedValue<float>, maxChannels> mixSmooth;

    std::array<std::atomic<float>*, maxChannels> mixP {};     // mix1..mix8
    std::atomic<float> *bypassP, *sensP, *depthP, *widthP, *releaseP, *lockP;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HowlShieldAudioProcessor)
};
