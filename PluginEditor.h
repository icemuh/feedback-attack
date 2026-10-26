#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// 1 = เปิดปุ่ม SETTINGS (เลือกอุปกรณ์เสียง/USB) ในหน้าต่างโปรแกรม — เฉพาะ Standalone และยังเป็นแบบทดลอง
// เปิดด้วย CMake: -DHOWL_INAPP_SETTINGS=ON  (ปิดไว้ก็ใช้ปุ่ม Options ของ JUCE ได้ตามปกติ)
#ifndef HOWL_INAPP_SETTINGS
 #define HOWL_INAPP_SETTINGS 0
#endif

// ---------------------------------------------------------------------------
// สี/ธีม
// ---------------------------------------------------------------------------
namespace ui
{
    const juce::Colour bg     { 0xff17191c };
    const juce::Colour panel  { 0xff0e1012 };
    const juce::Colour edge   { 0xff2a2d31 };
    const juce::Colour text   { 0xffe6e6ea };
    const juce::Colour dim    { 0xff8a8d94 };
    const juce::Colour accent { 0xff5fc7d6 };
    const juce::Colour violet { 0xff9aa0e8 };
}

// ---------------------------------------------------------------------------
// LookAndFeel: สไลเดอร์ Dry/Wet + ปุ่ม Bypass
// ---------------------------------------------------------------------------
struct HowlLookAndFeel : juce::LookAndFeel_V4
{
    int getSliderThumbRadius (juce::Slider&) override { return 10; }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override
    {
        auto b     = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        auto track = b.withSizeKeepingCentre (b.getWidth(), 6.f);

        g.setColour (juce::Colour (0xff0a0c0d));
        g.fillRoundedRectangle (track, 3.f);

        g.setColour (ui::edge.brighter (0.3f));
        for (int i = 0; i <= 4; ++i)
        {
            auto tx = juce::jmap ((float) i, 0.f, 4.f, b.getX() + 10.f, b.getRight() - 10.f);
            g.drawLine (tx, track.getY() - 4.f, tx, track.getBottom() + 4.f, 1.f);
        }

        g.setColour (ui::accent);
        g.fillRoundedRectangle (track.withRight (pos), 3.f);

        auto thumb = juce::Rectangle<float> (20.f, 38.f).withCentre ({ pos, b.getCentreY() });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b6f76), thumb.getX(), thumb.getY(),
                                                 juce::Colour (0xff3a3d42), thumb.getX(), thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 5.f);
        g.setColour (juce::Colour (0xffe8e8ea));
        g.drawLine (thumb.getCentreX(), thumb.getY() + 9.f, thumb.getCentreX(), thumb.getBottom() - 9.f, 2.f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
        const float r   = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto  c   = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const auto  st  = juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track, value;
        track.addCentredArc (c.x, c.y, r, r, 0.f, a0, a1, true);
        value.addCentredArc (c.x, c.y, r, r, 0.f, a0, ang, true);
        g.setColour (juce::Colour (0xff0a0c0d));
        g.strokePath (track, st);
        g.setColour (ui::accent);
        g.strokePath (value, st);

        const float kr = r * 0.72f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3e44), c.x, c.y - kr,
                                                 juce::Colour (0xff1b1d20), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);

        juce::Path ptr;
        ptr.startNewSubPath (0.f, -kr * 0.85f);
        ptr.lineTo (0.f, -kr * 0.35f);
        g.setColour (ui::text);
        g.strokePath (ptr, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r  = b.getLocalBounds().toFloat().reduced (1.f);
        bool on = b.getToggleState();

        if (b.getRadioGroupId() != 0)          // ปุ่มเลือกช่อง 1–8
        {
            const int notches = (int) b.getProperties().getWithDefault ("notches", 0);
            g.setColour (on ? ui::accent.withAlpha (0.16f) : juce::Colour (over ? 0xff23272b : 0xff1e2125));
            g.fillRoundedRectangle (r, 7.f);
            g.setColour (on ? ui::accent.withAlpha (0.7f) : ui::edge);
            g.drawRoundedRectangle (r, 7.f, 1.f);
            g.setColour (on ? ui::accent : ui::text);
            g.setFont (juce::Font (13.f, juce::Font::bold));
            g.drawText (b.getButtonText(), r, juce::Justification::centred);
            if (notches > 0)                   // จุดม่วง = ช่องนี้มี notch ทำงานอยู่
            {
                g.setColour (ui::violet);
                g.fillEllipse (r.getRight() - 9.f, r.getY() + 4.f, 5.f, 5.f);
            }
            return;
        }

        g.setColour (on ? ui::accent.withAlpha (0.14f)
                        : juce::Colour (over ? 0xff23272b : 0xff1e2125));
        g.fillRoundedRectangle (r, 12.f);
        g.setColour (on ? ui::accent.withAlpha (0.7f) : ui::edge);
        g.drawRoundedRectangle (r, 12.f, 1.f);

        auto col = on ? ui::accent : ui::text;
        g.setColour (col);

        // ไอคอนปุ่มเพาเวอร์
        const float cx = r.getCentreX() - 36.f, cy = r.getCentreY();
        juce::Path arc;
        arc.addCentredArc (cx, cy, 11.f, 11.f, 0.f, 0.55f,
                           juce::MathConstants<float>::twoPi - 0.55f, true);
        g.strokePath (arc, juce::PathStrokeType (2.f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        juce::Path stem;
        stem.startNewSubPath (cx, cy - 14.f);
        stem.lineTo (cx, cy - 2.f);
        g.strokePath (stem, juce::PathStrokeType (2.f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        g.setFont (18.f);
        g.drawText ("Bypass", juce::Rectangle<float> (cx + 22.f, r.getY(), 90.f, r.getHeight()),
                    juce::Justification::centredLeft);
    }
};

// ---------------------------------------------------------------------------
// มิเตอร์แบบแบ่งเป็นช่อง (ฟ้า → เขียว → เหลือง → ส้ม)
// ---------------------------------------------------------------------------
class SegmentMeter : public juce::Component
{
public:
    void setLevelDb (float db)
    {
        auto target = juce::jlimit (0.f, 1.f, juce::jmap (db, -60.f, 0.f, 0.f, 1.f));
        level = target > level ? target : level * 0.88f + target * 0.12f;   // ตกช้า
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a0c0d));
        g.fillRoundedRectangle (r, 4.f);

        constexpr int n = 14;
        const float gap  = 2.f;
        const float segW = (r.getWidth() - gap * (n + 1)) / n;
        const int   lit  = (int) std::round (level * n);

        for (int i = 0; i < n; ++i)
        {
            juce::Rectangle<float> s (r.getX() + gap + i * (segW + gap), r.getY() + gap,
                                      segW, r.getHeight() - gap * 2.f);
            g.setColour (i < lit ? colourAt (i / (float) (n - 1)) : juce::Colour (0xff1a1d20));
            g.fillRoundedRectangle (s, 1.5f);
        }
    }

private:
    static juce::Colour colourAt (float t)
    {
        static const juce::Colour stops[] = { juce::Colour (0xff5ec8d8), juce::Colour (0xff8fd47a),
                                              juce::Colour (0xffe2c36a), juce::Colour (0xffe0905a) };
        auto p = juce::jlimit (0.f, 0.999f, t) * 3.f;
        int  i = (int) p;
        return stops[i].interpolatedWith (stops[i + 1], p - (float) i);
    }

    float level = 0.f;
};

// ---------------------------------------------------------------------------
// โลโก้ (ออกแบบเอง): เส้นหลายเส้นเป็นรูประฆังที่มี "รอยบาก" ตรงกลาง = notch filter
// ---------------------------------------------------------------------------
class NotchLogo : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        constexpr int lines = 16;

        for (int k = 0; k < lines; ++k)
        {
            const float t = k / (float) (lines - 1);
            juce::Path p;

            for (int i = 0; i <= 120; ++i)
            {
                const float u    = i / 120.f;
                const float uu   = u - (t - 0.5f) * 0.08f;
                const float bell = std::exp (-std::pow ((uu - 0.5f) / 0.2f, 2.f));
                const float slit = 1.f - 0.7f * std::exp (-std::pow ((uu - 0.5f) / 0.03f, 2.f));
                const float amp  = 0.75f + 0.25f * (1.f - t);

                const float px = r.getX() + u * r.getWidth();
                const float py = r.getBottom() - 8.f - (r.getHeight() - 16.f) * amp * bell * slit + t * 8.f;

                i == 0 ? p.startNewSubPath (px, py) : p.lineTo (px, py);
            }

            g.setColour (ui::violet.interpolatedWith (juce::Colours::white, 1.f - t)
                                   .withAlpha (0.35f + 0.5f * (1.f - t)));
            g.strokePath (p, juce::PathStrokeType (1.f));
        }
    }
};

// ---------------------------------------------------------------------------
// กราฟสเปกตรัมแบบเรียลไทม์ (20 Hz–20 kHz, log) + เครื่องหมายตำแหน่ง notch
// ---------------------------------------------------------------------------
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (HowlShieldAudioProcessor& p) : proc (p) { smooth.fill (-100.f); }
    void setChannel (int c) { channel = c; smooth.fill (-100.f); }

    void update()
    {
        for (int i = 0; i < N; ++i)
        {
            const float v = proc.getSpectrumDb (channel, i);
            smooth[(size_t) i] += (v - smooth[(size_t) i]) * (v > smooth[(size_t) i] ? 0.6f : 0.15f);  // ขึ้นเร็ว ลงช้า
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff0a0c0d));
        g.fillRoundedRectangle (r, 8.f);

        const auto plot = r.reduced (8.f, 16.f);
        auto xOf = [&] (float f)  { return plot.getX() + plot.getWidth() * std::log (f / 20.f) / std::log (1000.f); };
        auto yOf = [&] (float db) { return juce::jmap (juce::jlimit (-90.f, 0.f, db), -90.f, 0.f, plot.getBottom(), plot.getY()); };
        auto fmt = [] (float f)   { return f >= 1000.f ? juce::String (f / 1000.f, f >= 10000.f ? 0 : 1) + "k"
                                                       : juce::String ((int) f); };

        // เส้นกริด
        g.setFont (10.f);
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
        {
            g.setColour (ui::edge.withAlpha (0.7f));
            g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());
            g.setColour (ui::dim);
            g.drawText (fmt (f), (int) xOf (f) - 16, (int) plot.getBottom() + 2, 32, 12, juce::Justification::centred);
        }
        for (float d : { -20.f, -40.f, -60.f, -80.f })
        {
            g.setColour (ui::edge.withAlpha (0.7f));
            g.drawHorizontalLine ((int) yOf (d), plot.getX(), plot.getRight());
            g.setColour (ui::dim);
            g.drawText (juce::String ((int) d), (int) plot.getX() + 2, (int) yOf (d) - 11, 26, 10, juce::Justification::left);
        }

        // สเปกตรัม
        juce::Path line;
        for (int i = 0; i < N; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * ((float) i + 0.5f) / (float) N;
            const float y = yOf (smooth[(size_t) i]);
            i == 0 ? line.startNewSubPath (x, y) : line.lineTo (x, y);
        }
        juce::Path area (line);
        area.lineTo (plot.getRight(), plot.getBottom());
        area.lineTo (plot.getX(), plot.getBottom());
        area.closeSubPath();

        g.setGradientFill (juce::ColourGradient (ui::accent.withAlpha (0.35f), 0.f, plot.getY(),
                                                 ui::accent.withAlpha (0.02f), 0.f, plot.getBottom(), false));
        g.fillPath (area);
        g.setColour (ui::accent);
        g.strokePath (line, juce::PathStrokeType (1.5f));

        // ตำแหน่ง notch
        g.setFont (juce::Font (10.f, juce::Font::bold));
        for (int s = 0; s < FeedbackEngine::maxSlots; ++s)
        {
            const float f = proc.getSlotFreq (channel, s);
            if (f <= 0.f) continue;

            const float gdb = proc.getSlotGainDb (channel, s);
            const float x   = xOf (f);
            const float t   = juce::jlimit (0.f, 1.f, -gdb / 24.f);

            g.setColour (ui::violet.withAlpha (0.12f + 0.22f * t));
            g.fillRect (juce::Rectangle<float> (x - 5.f, plot.getY(), 10.f, plot.getHeight()));
            g.setColour (ui::violet);
            g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.f);

            juce::Path tri;
            tri.addTriangle (x - 4.f, plot.getY() - 10.f, x + 4.f, plot.getY() - 10.f, x, plot.getY() - 2.f);
            g.fillPath (tri);

            const bool flip = x > plot.getRight() - 80.f;
            g.drawText (fmt (f) + "  " + juce::String ((int) std::round (gdb)) + " dB",
                        juce::Rectangle<int> ((int) (flip ? x - 78.f : x + 4.f), (int) plot.getY() + 1, 74, 12),
                        flip ? juce::Justification::right : juce::Justification::left);
        }
    }

private:
    static constexpr int N = FeedbackEngine::displayPoints;
    HowlShieldAudioProcessor& proc;
    int channel = 0;
    std::array<float, (size_t) N> smooth {};
};

// ---------------------------------------------------------------------------
// Dashboard: ทุกช่องพร้อมกัน (สเปกตรัมย่อ + IN/OUT + mix)
// ---------------------------------------------------------------------------
class DashboardView : public juce::Component
{
public:
    DashboardView (HowlShieldAudioProcessor& p, std::function<void (int)> onPick)
        : proc (p), pick (std::move (onPick))
    {
        for (auto& s : spec) s.fill (-100.f);
        lvIn.fill (-100.f);
        lvOut.fill (-100.f);
    }

    void setSelected (int c) { selected = c; repaint(); }

    void update()
    {
        const int n = proc.getNumActiveChannels();
        auto follow = [] (float& lv, float t) { lv += (t - lv) * (t > lv ? 1.f : 0.12f); };   // ขึ้นทันที ลงช้า

        for (int c = 0; c < n; ++c)
        {
            for (int i = 0; i < N; ++i)
            {
                const float v = proc.getSpectrumDb (c, i);
                auto& s = spec[(size_t) c][(size_t) i];
                s += (v - s) * (v > s ? 0.6f : 0.15f);
            }
            follow (lvIn[(size_t) c],  proc.getInLevelDb (c));
            follow (lvOut[(size_t) c], proc.getOutLevelDb (c));
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const int   n   = proc.getNumActiveChannels();

        for (int c = 0; c < n; ++c)
        {
            const auto r = cell (c);
            g.setColour (juce::Colour (0xff0a0c0d));
            g.fillRoundedRectangle (r, 8.f);
            g.setColour (c == selected ? ui::accent.withAlpha (0.7f) : ui::edge);
            g.drawRoundedRectangle (r, 8.f, 1.f);

            // หัวการ์ด
            const int notches = proc.getActiveFilterCount (c);
            g.setFont (juce::Font (12.f, juce::Font::bold));
            g.setColour (ui::text);
            g.drawText ("CH " + juce::String (c + 1), r.withTrimmedLeft (8.f).withHeight (20.f),
                        juce::Justification::centredLeft);
            g.setColour (notches > 0 ? ui::violet : ui::dim);
            g.drawText (juce::String (notches) + " notch", r.withTrimmedRight (8.f).withHeight (20.f),
                        juce::Justification::centredRight);

            // สเปกตรัมย่อ
            const juce::Rectangle<float> sp (r.getX() + 6.f, r.getY() + 22.f, r.getWidth() - 12.f, 36.f);
            juce::Path line;
            for (int i = 0; i < N; ++i)
            {
                const float x = sp.getX() + sp.getWidth() * ((float) i + 0.5f) / (float) N;
                const float y = juce::jmap (juce::jlimit (-90.f, 0.f, spec[(size_t) c][(size_t) i]),
                                            -90.f, 0.f, sp.getBottom(), sp.getY());
                i == 0 ? line.startNewSubPath (x, y) : line.lineTo (x, y);
            }
            juce::Path area (line);
            area.lineTo (sp.getRight(), sp.getBottom());
            area.lineTo (sp.getX(), sp.getBottom());
            area.closeSubPath();
            g.setGradientFill (juce::ColourGradient (ui::accent.withAlpha (0.35f), 0.f, sp.getY(),
                                                     ui::accent.withAlpha (0.02f), 0.f, sp.getBottom(), false));
            g.fillPath (area);
            g.setColour (ui::accent);
            g.strokePath (line, juce::PathStrokeType (1.f));

            for (int s = 0; s < FeedbackEngine::maxSlots; ++s)     // เส้นม่วง = ตำแหน่ง notch
            {
                const float f = proc.getSlotFreq (c, s);
                if (f <= 0.f) continue;
                g.setColour (ui::violet.withAlpha (0.85f));
                g.drawVerticalLine ((int) (sp.getX() + sp.getWidth() * std::log (f / 20.f) / std::log (1000.f)),
                                    sp.getY(), sp.getBottom());
            }

            // แถว IN / OUT / MIX
            auto row = [&] (int k, const char* name, float t, const juce::String& val, bool isMix)
            {
                const float y = r.getY() + 62.f + (float) k * 15.f;
                g.setFont (juce::Font (10.f, juce::Font::bold));
                g.setColour (ui::dim);
                g.drawText (name, juce::Rectangle<float> (r.getX() + 8.f, y, 26.f, 12.f), juce::Justification::centredLeft);

                const juce::Rectangle<float> bar (r.getX() + 36.f, y + 3.f, r.getWidth() - 36.f - 48.f, 6.f);
                g.setColour (juce::Colour (0xff1a1d20));
                g.fillRoundedRectangle (bar, 3.f);

                if (isMix) g.setColour (ui::accent);
                else       g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5ec8d8), bar.getX(), 0.f,
                                                                    juce::Colour (0xffe0905a), bar.getRight(), 0.f, false));
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.f, 1.f, t)), 3.f);

                g.setColour (ui::text.withAlpha (0.85f));
                g.drawText (val, juce::Rectangle<float> (r.getRight() - 46.f, y, 40.f, 12.f), juce::Justification::centredRight);
            };
            auto toT = [] (float db) { return (db + 60.f) / 60.f; };

            row (0, "IN",  toT (lvIn[(size_t) c]),  juce::String (lvIn[(size_t) c], 1),  false);
            row (1, "OUT", toT (lvOut[(size_t) c]), juce::String (lvOut[(size_t) c], 1), false);
            const float mix = proc.getMix (c);
            row (2, "MIX", mix, juce::String (juce::roundToInt (mix * 100.f)) + "%", true);

            const auto mb = mixBar (c);                    // จุดจับ = ลากได้
            g.setColour (ui::text);
            g.fillEllipse (mb.getX() + mb.getWidth() * mix - 5.f, mb.getCentreY() - 5.f, 10.f, 10.f);
        }
    }

    // ลากแถบ MIX ของการ์ดไหน = ปรับ Dry/Wet ของช่องนั้นช่องเดียว / คลิกที่อื่นในการ์ด = เลือกช่อง
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = proc.getNumActiveChannels();
        for (int c = 0; c < n; ++c)
            if (mixHit (c).contains (e.position))
            {
                dragging = c;
                if (auto* prm = mixParam (c)) prm->beginChangeGesture();
                setMixFromX (c, e.position.x);
                return;
            }
        for (int c = 0; c < n; ++c)
            if (cell (c).contains (e.position)) { pick (c); return; }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging >= 0) setMixFromX (dragging, e.position.x);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging < 0) return;
        if (auto* prm = mixParam (dragging)) prm->endChangeGesture();
        dragging = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override     // ดับเบิลคลิกที่แถบ = กลับเป็น 100% (Wet เต็ม)
    {
        for (int c = 0; c < proc.getNumActiveChannels(); ++c)
            if (mixHit (c).contains (e.position))
                if (auto* prm = mixParam (c)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (1.f); prm->endChangeGesture(); }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        bool over = false;
        for (int c = 0; c < proc.getNumActiveChannels(); ++c) over = over || mixHit (c).contains (e.position);
        setMouseCursor (over ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

private:
    juce::RangedAudioParameter* mixParam (int c) const { return proc.apvts.getParameter ("mix" + juce::String (c + 1)); }

    juce::Rectangle<float> mixBar (int c) const        // ตำแหน่งเดียวกับแถว MIX ที่วาดใน paint()
    {
        const auto r = cell (c);
        return { r.getX() + 36.f, r.getY() + 62.f + 2.f * 15.f + 3.f, r.getWidth() - 36.f - 48.f, 6.f };
    }
    juce::Rectangle<float> mixHit (int c) const { return mixBar (c).expanded (6.f, 8.f); }

    void setMixFromX (int c, float x)
    {
        const auto b = mixBar (c);
        if (auto* prm = mixParam (c))
            prm->setValueNotifyingHost (juce::jlimit (0.f, 1.f, (x - b.getX()) / b.getWidth()));
    }

    juce::Rectangle<float> cell (int c) const
    {
        const float gap = 10.f;
        const float w = ((float) getWidth() - 3.f * gap) / 4.f, h = ((float) getHeight() - gap) / 2.f;
        return { (float) (c % 4) * (w + gap), (float) (c / 4) * (h + gap), w, h };
    }

    static constexpr int N = FeedbackEngine::displayPoints;
    HowlShieldAudioProcessor& proc;
    std::function<void (int)> pick;
    int dragging = -1;
    int selected = 0;
    std::array<std::array<float, (size_t) N>, (size_t) HowlShieldAudioProcessor::maxChannels> spec;
    std::array<float, (size_t) HowlShieldAudioProcessor::maxChannels> lvIn, lvOut;
};

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
class HowlShieldAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit HowlShieldAudioProcessorEditor (HowlShieldAudioProcessor&);
    ~HowlShieldAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void styleValueLabel (juce::Label&, juce::Justification);
    void refreshChannels();
    void setDashboard (bool on);
    void selectChannel (int c);
    void attachMix();
    void updateDeviceInfo();
    void openAudioSettings();

    HowlShieldAudioProcessor& proc;
    HowlLookAndFeel laf;

    NotchLogo    logo;
    SpectrumView spectrum;
    DashboardView dashboard;
    SegmentMeter inMeter, outMeter;
    juce::Label  inValue, outValue, mixValue;
    juce::Slider mixSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ToggleButton bypassButton;
    juce::Slider sensKnob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label  sensValue, notchInfo;
    std::array<juce::ToggleButton, 8> chButtons;
    int selected = 0, numCh = -1;
    juce::TextButton dashButton { "DASHBOARD" };
    juce::TextButton settingsButton { "AUDIO SETTINGS" };   // เฉพาะ Standalone
    juce::Label deviceInfo;
    int infoTick = 0;
    juce::TextButton settingsButton { "SETTINGS" };
    juce::Label deviceInfo;
    int infoTick = 0;
    bool dashOn = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;   // ผูกกับ mix ของช่องที่เลือก
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttach;
    juce::AudioProcessorValueTreeState::SliderAttachment sensAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HowlShieldAudioProcessorEditor)
};
