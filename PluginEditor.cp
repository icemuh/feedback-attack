#include "PluginEditor.h"

#if HOWL_INAPP_SETTINGS
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

HowlShieldAudioProcessorEditor::HowlShieldAudioProcessorEditor (HowlShieldAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p),
      dashboard (p, [this] (int c) { selectChannel (c); }),
      bypassAttach (p.apvts, "bypass", bypassButton),
      sensAttach   (p.apvts, "sens",   sensKnob)
{
    setSize (780, 430);
    setLookAndFeel (&laf);

    addAndMakeVisible (logo);
    addAndMakeVisible (spectrum);
    addChildComponent (dashboard);               // ซ่อนไว้ก่อน เปิดด้วยปุ่ม DASHBOARD
    addAndMakeVisible (dashButton);
    dashButton.setClickingTogglesState (true);
    dashButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1e2125));
    dashButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff25454c));
    dashButton.setColour (juce::TextButton::textColourOffId,  ui::text);
    dashButton.setColour (juce::TextButton::textColourOnId,   ui::accent);
    dashButton.onClick = [this] { setDashboard (dashButton.getToggleState()); };

    // ปุ่ม SETTINGS + บรรทัดสถานะอุปกรณ์เสียง (เฉพาะ Standalone ที่เปิด HOWL_INAPP_SETTINGS)
    if (HOWL_INAPP_SETTINGS && juce::JUCEApplicationBase::isStandaloneApp())
    {
        addAndMakeVisible (settingsButton);
        settingsButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff1e2125));
        settingsButton.setColour (juce::TextButton::textColourOffId, ui::text);
        settingsButton.onClick = [this] { openAudioSettings(); };

        styleValueLabel (deviceInfo, juce::Justification::centredLeft);
        deviceInfo.setFont (juce::Font (11.f));
    }
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);
    addAndMakeVisible (mixSlider);
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (sensKnob);

    styleValueLabel (inValue,   juce::Justification::centredLeft);
    styleValueLabel (outValue,  juce::Justification::centredLeft);
    styleValueLabel (mixValue,  juce::Justification::centred);
    styleValueLabel (sensValue, juce::Justification::centredLeft);
    styleValueLabel (notchInfo, juce::Justification::centredRight);
    mixValue.setColour  (juce::Label::textColourId, ui::accent);
    sensValue.setColour (juce::Label::textColourId, ui::accent);
    notchInfo.setColour (juce::Label::textColourId, ui::violet);

    // ปุ่มเลือกช่อง 1–8 (radio)
    for (int i = 0; i < HowlShieldAudioProcessor::maxChannels; ++i)
    {
        auto& b = chButtons[(size_t) i];
        b.setButtonText (juce::String (i + 1));
        b.setRadioGroupId (1001);
        b.setClickingTogglesState (true);
        b.onClick = [this, i]
        {
            if (chButtons[(size_t) i].getToggleState()) { selected = i; spectrum.setChannel (i); dashboard.setSelected (i); attachMix(); }
        };
        addAndMakeVisible (b);
    }
    chButtons[0].setToggleState (true, juce::dontSendNotification);

    sensKnob.setRotaryParameters (juce::degreesToRadians (225.f), juce::degreesToRadians (495.f), true);
    sensKnob.setDoubleClickReturnValue (true, 0.5);

    mixSlider.onValueChange = [this]
    {
        mixValue.setText (juce::String (juce::roundToInt (mixSlider.getValue() * 100.0)) + "%",
                          juce::dontSendNotification);
    };
    sensKnob.onValueChange = [this]
    {
        sensValue.setText (juce::String (juce::roundToInt (sensKnob.getValue() * 100.0)) + "%",
                           juce::dontSendNotification);
    };
    attachMix();                       // สไลด์ใหญ่ผูกกับ Dry/Wet ของช่องที่เลือก
    sensKnob.onValueChange();

    refreshChannels();
    startTimerHz (30);
}

HowlShieldAudioProcessorEditor::~HowlShieldAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void HowlShieldAudioProcessorEditor::styleValueLabel (juce::Label& l, juce::Justification j)
{
    l.setJustificationType (j);
    l.setColour (juce::Label::textColourId, ui::dim);
    l.setFont (juce::Font (15.f, juce::Font::bold));
    addAndMakeVisible (l);
}

// แสดงเฉพาะปุ่มช่องที่มีอยู่จริง + จุดม่วงบอกว่าช่องไหนมี notch ทำงาน
void HowlShieldAudioProcessorEditor::refreshChannels()
{
    const int n = proc.getNumActiveChannels();

    if (n != numCh)
    {
        numCh = n;
        for (int i = 0; i < HowlShieldAudioProcessor::maxChannels; ++i)
            chButtons[(size_t) i].setVisible (i < n);

        if (selected >= n)
        {
            selected = 0;
            chButtons[0].setToggleState (true, juce::dontSendNotification);
            spectrum.setChannel (0);
            dashboard.setSelected (0);
            attachMix();
        }
    }

    for (int i = 0; i < n; ++i)
    {
        chButtons[(size_t) i].getProperties().set ("notches", proc.getActiveFilterCount (i));
        chButtons[(size_t) i].repaint();
    }
}

// เปิดหน้าต่างตั้งค่าเสียงของ JUCE Standalone: เลือกอุปกรณ์ USB, ช่องอินพุต/เอาต์พุต, sample rate, buffer
void HowlShieldAudioProcessorEditor::openAudioSettings()
{
#if HOWL_INAPP_SETTINGS
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->showAudioSettingsDialog();
#endif
}

// ผูกสไลด์ Dry/Wet ใหญ่กับพารามิเตอร์ mix ของช่องที่เลือกอยู่ (mix1 … mix8)
void HowlShieldAudioProcessorEditor::attachMix()
{
    mixAttach.reset();                 // ปลดตัวเก่าก่อนผูกตัวใหม่
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    proc.apvts, "mix" + juce::String (selected + 1), mixSlider);
    mixSlider.onValueChange();         // อัปเดตตัวเลข %
}

// สลับระหว่างหน้าปกติ (สเปกตรัมใหญ่ + ปุ่มปรับ) กับ Dashboard 8 ช่อง
void HowlShieldAudioProcessorEditor::setDashboard (bool on)
{
    dashOn = on;
    dashButton.setToggleState (on, juce::dontSendNotification);
    dashboard.setVisible (on);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &spectrum, &inMeter, &outMeter, &inValue, &outValue,
             &mixSlider, &mixValue, &sensKnob, &sensValue, &bypassButton })
        c->setVisible (! on);

    repaint();
}

void HowlShieldAudioProcessorEditor::selectChannel (int c)
{
    selected = c;
    chButtons[(size_t) c].setToggleState (true, juce::dontSendNotification);
    spectrum.setChannel (c);
    dashboard.setSelected (c);
    attachMix();
    setDashboard (false);          // คลิกการ์ดช่องไหน → กลับไปหน้าปกติของช่องนั้น
}

void HowlShieldAudioProcessorEditor::timerCallback()
{
    refreshChannels();

    const float i = proc.getInLevelDb (selected), o = proc.getOutLevelDb (selected);
    inMeter.setLevelDb (i);
    outMeter.setLevelDb (o);
    inValue.setText  (juce::String (i, 1), juce::dontSendNotification);
    outValue.setText (juce::String (o, 1), juce::dontSendNotification);
    notchInfo.setText (juce::String (proc.getActiveFilterCount (selected)) + " / "
                           + juce::String (FeedbackEngine::maxSlots) + " notches",
                       juce::dontSendNotification);
    spectrum.update();
    if (dashOn) dashboard.update();

#if HOWL_INAPP_SETTINGS
    if (deviceInfo.getParentComponent() != nullptr && (infoTick++ % 15) == 0)      // อัปเดตสถานะอุปกรณ์ ~2 ครั้ง/วินาที
    {
        juce::String t = "No audio device - press SETTINGS";
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
            if (auto* dev = holder->deviceManager.getCurrentAudioDevice())
                t = dev->getName() + "  |  " + juce::String (dev->getCurrentSampleRate() / 1000.0, 1) + " kHz  |  "
                    + juce::String (dev->getCurrentBufferSizeSamples()) + " smp  |  "
                    + juce::String (dev->getActiveInputChannels().countNumberOfSetBits()) + " in / "
                    + juce::String (dev->getActiveOutputChannels().countNumberOfSetBits()) + " out";
        deviceInfo.setText (t, juce::dontSendNotification);
    }
#endif
}

void HowlShieldAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::bg);

    auto panel = getLocalBounds().toFloat().reduced (12.f);
    g.setColour (ui::panel);
    g.fillRoundedRectangle (panel, 16.f);
    g.setColour (ui::edge);
    g.drawRoundedRectangle (panel, 16.f, 1.f);

    // ---- ชื่อปลั๊กอิน ----
    auto spaced = [] (float size, juce::Font::FontStyleFlags style)
    {
        juce::Font f (juce::Font::getDefaultSansSerifFontName(), size, style);
        f.setExtraKerningFactor (0.45f);
        return f;
    };

    const juce::Rectangle<int> titleArea (290, 24, 426, 104);

    g.setColour (ui::text);
    g.setFont (spaced (38.f, juce::Font::plain));
    g.drawText ("HOWL", titleArea.withHeight (46), juce::Justification::centred);

    g.setColour (ui::violet);
    g.drawText ("SHIELD", titleArea.withTrimmedTop (42).withHeight (46), juce::Justification::centred);

    g.setColour (ui::dim);
    g.setFont (spaced (11.f, juce::Font::plain));
    g.drawText ("ADAPTIVE NOTCH ENGINE", titleArea.withTrimmedTop (88), juce::Justification::centred);
    g.setColour (ui::edge.brighter (0.4f));
    g.drawLine (340.f, 119.f, 395.f, 119.f, 1.f);
    g.drawLine (611.f, 119.f, 666.f, 119.f, 1.f);

    // ---- เส้นคั่น ----
    g.setColour (ui::edge);
    g.drawLine (36.f, 140.f, (float) getWidth() - 36.f, 140.f, 1.f);
    if (! dashOn) g.drawLine (36.f, 316.f, (float) getWidth() - 36.f, 316.f, 1.f);

    // ---- ป้ายกำกับ ----
    g.setColour (ui::dim);
    g.setFont (spaced (11.f, juce::Font::bold));
    g.drawText ("CHANNEL",     36,  152, 70,  16, juce::Justification::left);
    if (dashOn) return;   // โหมด Dashboard: ซ่อนป้ายของส่วนล่าง
    g.drawText ("METERS  CH " + juce::String (selected + 1), 36, 324, 140, 16, juce::Justification::left);
    g.drawText ("SENSITIVITY", 296, 324, 110, 16, juce::Justification::left);
    g.drawText ("DRY",         410, 324, 40,  16, juce::Justification::left);
    g.drawText ("WET",         560, 324, 40,  16, juce::Justification::right);

    g.setColour (ui::text.withAlpha (0.85f));
    g.setFont (juce::Font (15.f, juce::Font::bold));
    g.drawText ("IN",  36, 352, 28, 22, juce::Justification::left);
    g.drawText ("OUT", 36, 382, 32, 22, juce::Justification::left);
}

void HowlShieldAudioProcessorEditor::resized()
{
    logo.setBounds (24, 28, 240, 96);

    for (int i = 0; i < HowlShieldAudioProcessor::maxChannels; ++i)
        chButtons[(size_t) i].setBounds (110 + i * 40, 148, 34, 24);
    notchInfo.setBounds (getWidth() - 36 - 160, 148, 160, 24);

    spectrum.setBounds (36, 180, getWidth() - 72, 128);
    dashboard.setBounds (36, 180, getWidth() - 72, 228);
    dashButton.setBounds (440, 148, 110, 24);
    settingsButton.setBounds (656, 24, 88, 24);
    deviceInfo.setBounds (36, 124, 260, 14);

    inMeter.setBounds  (70, 352, 166, 22);
    outMeter.setBounds (70, 382, 166, 22);
    inValue.setBounds  (242, 352, 46, 22);
    outValue.setBounds (242, 382, 46, 22);

    sensKnob.setBounds  (296, 346, 60, 60);
    sensValue.setBounds (360, 366, 44, 22);

    mixValue.setBounds  (470, 320, 70, 22);
    mixSlider.setBounds (410, 348, 190, 50);

    bypassButton.setBounds (616, 344, 128, 58);
}
