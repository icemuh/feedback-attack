#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using NR  = juce::NormalisableRange<float>;
using PID = juce::ParameterID;

juce::AudioProcessorValueTreeState::ParameterLayout HowlShieldAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    for (int c = 1; c <= maxChannels; ++c)       // Dry/Wet แยกอิสระช่องละค่า: mix1 … mix8
        p.push_back (std::make_unique<APF> (PID { "mix" + juce::String (c), 1 }, "Dry/Wet CH " + juce::String (c),
                                            NR (0.f, 1.f, 0.001f), 1.f));
    p.push_back (std::make_unique<APB> (PID { "bypass",  1 }, "Bypass",      false));
    p.push_back (std::make_unique<APF> (PID { "sens",    1 }, "Sensitivity", NR (0.f, 1.f, 0.01f),  0.5f));
    p.push_back (std::make_unique<APF> (PID { "depth",   1 }, "Depth",       NR (3.f, 24.f, 0.5f),  12.f));
    p.push_back (std::make_unique<APF> (PID { "width",   1 }, "Q",           NR (5.f, 60.f, 0.1f, 0.6f), 30.f));
    p.push_back (std::make_unique<APF> (PID { "release", 1 }, "Release",     NR (0.5f, 30.f, 0.1f, 0.5f), 6.f));
    p.push_back (std::make_unique<APB> (PID { "lock",    1 }, "Lock",        false));

    return { p.begin(), p.end() };
}

// Standalone เริ่มต้นที่ 8 ช่อง, ปลั๊กอิน (VST3/AU) เริ่มต้นที่สเตอริโอ แต่รองรับ 1–8 ช่อง
juce::AudioProcessor::BusesProperties HowlShieldAudioProcessor::makeBuses()
{
    const auto set = juce::JUCEApplicationBase::isStandaloneApp() ? juce::AudioChannelSet::discreteChannels (maxChannels)
                                                                  : juce::AudioChannelSet::stereo();
    return BusesProperties().withInput ("Input", set, true).withOutput ("Output", set, true);
}

HowlShieldAudioProcessor::HowlShieldAudioProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    for (int c = 0; c < maxChannels; ++c)
        mixP[(size_t) c] = apvts.getRawParameterValue ("mix" + juce::String (c + 1));
    bypassP  = apvts.getRawParameterValue ("bypass");
    sensP    = apvts.getRawParameterValue ("sens");
    depthP   = apvts.getRawParameterValue ("depth");
    widthP   = apvts.getRawParameterValue ("width");
    releaseP = apvts.getRawParameterValue ("release");
    lockP    = apvts.getRawParameterValue ("lock");

    for (auto& l : inLevelDb)  l = -100.f;
    for (auto& l : outLevelDb) l = -100.f;
}

bool HowlShieldAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& in  = l.getMainInputChannelSet();
    const auto& out = l.getMainOutputChannelSet();

    const bool sizesOk = ! in.isDisabled() && ! out.isDisabled()
                      && in.size()  >= 1 && in.size()  <= maxChannels
                      && out.size() >= 1 && out.size() <= maxChannels;

    // Standalone: อินเทอร์เฟซ USB บางรุ่นมี in/out ไม่เท่ากัน (เช่น 8 in / 2 out) ให้ใช้ได้
    // ปลั๊กอิน (VST3/AU): in/out ต้องเท่ากัน
    return sizesOk && (juce::JUCEApplicationBase::isStandaloneApp() || in == out);
}

void HowlShieldAudioProcessor::prepareToPlay (double sr, int blockSize)
{
    for (auto& e : engines) e.prepare (sr);
    dryBuffer.setSize (maxChannels, juce::jmax (1, blockSize));
    for (int c = 0; c < maxChannels; ++c)
    {
        mixSmooth[(size_t) c].reset (sr, 0.02);
        mixSmooth[(size_t) c].setCurrentAndTargetValue (mixP[(size_t) c]->load());
    }
}

void HowlShieldAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n  = buffer.getNumSamples();
    const int nc = juce::jmin (buffer.getNumChannels(), maxChannels);

    for (int c = 0; c < maxChannels; ++c)
        inLevelDb[(size_t) c] = c < nc ? juce::Decibels::gainToDecibels (buffer.getMagnitude (c, 0, n), -100.f) : -100.f;

    if (bypassP->load() < 0.5f && nc > 0)
    {
        // เก็บสัญญาณดิบไว้ทำ dry/wet
        dryBuffer.setSize (nc, n, false, false, true);
        for (int c = 0; c < nc; ++c) dryBuffer.copyFrom (c, 0, buffer, c, 0, n);

        FeedbackEngine::Params ep;
        ep.sensitivity = sensP->load();
        ep.maxDepthDb  = depthP->load();
        ep.q           = widthP->load();
        ep.releaseSec  = releaseP->load();
        ep.lock        = lockP->load() > 0.5f;

        // แต่ละช่องมี engine ของตัวเอง: ตรวจจับ + notch แยกกัน (ไมค์คนละตัวหอนคนละความถี่)
        for (int c = 0; c < nc; ++c)
        {
            float* data = buffer.getWritePointer (c);
            engines[(size_t) c].process (&data, 1, n, ep);
        }

        for (int c = 0; c < nc; ++c)             // ผสม dry/wet แยกตามช่อง
        {
            auto& sm = mixSmooth[(size_t) c];
            sm.setTargetValue (mixP[(size_t) c]->load());
            auto* wet = buffer.getWritePointer (c);
            const auto* dry = dryBuffer.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float m = sm.getNextValue();
                wet[i] = wet[i] * m + dry[i] * (1.f - m);
            }
        }
    }

    for (int c = 0; c < maxChannels; ++c)
        outLevelDb[(size_t) c] = c < nc ? juce::Decibels::gainToDecibels (buffer.getMagnitude (c, 0, n), -100.f) : -100.f;
}

juce::AudioProcessorEditor* HowlShieldAudioProcessor::createEditor()
{
    return new HowlShieldAudioProcessorEditor (*this);
}

void HowlShieldAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void HowlShieldAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HowlShieldAudioProcessor();
}
