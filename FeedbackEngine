#pragma once
#include <JuceHeader.h>   // ต้องเปิดโมดูล juce_dsp (ใช้ FFT + window)

// ---------------------------------------------------------------------------
// Biquad (RBJ peaking EQ ใช้เป็น notch โดยใส่ gain ติดลบ) — ใช้ double กันเพี้ยนที่ความถี่ต่ำ/Q สูง
// ---------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    void setPeak (double fs, double f0, double Q, double gainDb)
    {
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = juce::MathConstants<double>::twoPi * f0 / fs;
        const double alpha = std::sin (w0) / (2.0 * Q);
        const double c     = std::cos (w0);
        const double a0    = 1.0 + alpha / A;

        b0 = (1.0 + alpha * A) / a0;
        b1 = -2.0 * c / a0;
        b2 = (1.0 - alpha * A) / a0;
        a1 = -2.0 * c / a0;
        a2 = (1.0 - alpha / A) / a0;
    }
};

struct BiquadState
{
    double z1 = 0, z2 = 0;

    float process (const Biquad& c, float xf) noexcept   // transposed direct form II
    {
        const double x = xf;
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return (float) y;
    }
};

// ---------------------------------------------------------------------------
// Engine: วิเคราะห์สเปกตรัม → หาพีคแคบที่ค้างอยู่ → วาง notch อัตโนมัติ
// ไม่มีการ allocate ใน process() (เรียกจาก audio thread ได้)
// ---------------------------------------------------------------------------
class FeedbackEngine
{
public:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize  = 1 << fftOrder;   // 4096
    static constexpr int hopSize  = 1024;
    static constexpr int maxSlots = 10;
    static constexpr int maxCh    = 1;   // 1 engine ต่อ 1 ช่องเสียง (ตรวจจับ/วาง notch แยกอิสระ)
    static constexpr int displayPoints = 192;   // จำนวนจุดสเปกตรัมที่ส่งให้ UI (20 Hz–20 kHz แบบ log)

    struct Params
    {
        float sensitivity = 0.5f;   // 0..1  (สูง = จับไวขึ้น)
        float maxDepthDb  = 12.f;   // ตัดลึกสุดต่อ 1 ฟิลเตอร์
        float q           = 30.f;   // ความแคบของ notch
        float releaseSec  = 6.f;    // เวลาก่อนปล่อยฟิลเตอร์ที่ไม่ได้ใช้
        bool  lock        = false;  // true = ไม่เพิ่ม/ไม่ปล่อยฟิลเตอร์
    };

    FeedbackEngine()
        : fft (fftOrder),
          window (fftSize, juce::dsp::WindowingFunction<float>::hann) {}

    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void requestClear() noexcept     { clearFlag = true; }

    int getActiveCount() const noexcept
    {
        int n = 0;
        for (auto& s : slots) n += s.active ? 1 : 0;
        return n;
    }

    // --- ข้อมูลสำหรับ UI (อ่านได้จากทุก thread) ---
    float getSpectrumDb (int i) const noexcept  { return display[(size_t) i].load(); }
    float getSlotFreq   (int i) const noexcept  { return slotFreq[(size_t) i].load(); }   // 0 = ไม่ได้ใช้
    float getSlotGainDb (int i) const noexcept  { return slotGain[(size_t) i].load(); }

    void process (float* const* ch, int numCh, int n, const Params& p)
    {
        if (clearFlag.exchange (false)) reset();
        numCh = juce::jmin (numCh, maxCh);
        if (numCh <= 0) return;

        // 1) ป้อนสัญญาณ (mono sum) เข้าตัวตรวจจับ
        for (int i = 0; i < n; ++i)
        {
            float m = 0.f;
            for (int c = 0; c < numCh; ++c) m += ch[c][i];
            ring[(size_t) writePos] = m / (float) numCh;
            writePos = (writePos + 1) & (fftSize - 1);

            if (++sinceHop >= hopSize) { sinceHop = 0; analyse (p); }
        }

        publishSlots();

        // 2) ผ่าน notch ที่ active อยู่
        int act[maxSlots], na = 0;
        for (int s = 0; s < maxSlots; ++s)
            if (slots[(size_t) s].active) act[na++] = s;

        if (na == 0) return;

        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                for (int j = 0; j < na; ++j)
                    x = state[c][act[j]].process (slots[(size_t) act[j]].bq, x);
                ch[c][i] = x;
            }
    }

private:
    struct Slot
    {
        bool   active = false;
        float  freq = 0.f, gainDb = 0.f, quiet = 0.f;
        Biquad bq;
    };

    void reset()
    {
        slots.fill (Slot{});
        for (auto& ch : state) for (auto& s : ch) s = BiquadState{};
        ring.fill (0.f);
        persist.fill (0);
        prevDb.fill (-120.f);
        writePos = sinceHop = 0;
    }

    void recompute (Slot& s, const Params& p) { s.bq.setPeak (fs, s.freq, p.q, s.gainDb); }

    // ย่อสเปกตรัมเป็นจุดแบบ log เพื่อส่งให้ UI วาด
    void publishSpectrum()
    {
        const float binHz = (float) fs / (float) fftSize;
        int prevBin = 1;
        for (int j = 0; j < displayPoints; ++j)
        {
            const float fHi = 20.f * std::pow (1000.f, (float) (j + 1) / (float) displayPoints);
            const int   hi  = juce::jlimit (prevBin, fftSize / 2, (int) std::ceil (fHi / binHz));
            float m = -120.f;
            for (int k = prevBin; k <= hi; ++k) m = juce::jmax (m, db[(size_t) k]);
            display[(size_t) j] = m;
            prevBin = hi;
        }
    }

    void publishSlots()
    {
        for (size_t i = 0; i < (size_t) maxSlots; ++i)
        {
            slotFreq[i] = slots[i].active ? slots[i].freq   : 0.f;
            slotGain[i] = slots[i].active ? slots[i].gainDb : 0.f;
        }
    }

    void updateSlots (const Params& p, float dt)
    {
        for (int i = 0; i < maxSlots; ++i)
        {
            auto& s = slots[(size_t) i];
            if (! s.active) continue;

            s.gainDb = juce::jmax (s.gainDb, -p.maxDepthDb);

            if (! p.lock)
            {
                s.quiet += dt;
                if (s.quiet > p.releaseSec)                       // ค่อยๆ คลายออก
                {
                    s.gainDb = juce::jmin (0.f, s.gainDb + (12.f / juce::jmax (0.2f, p.releaseSec)) * dt);
                    if (s.gainDb > -0.5f) { s.active = false; continue; }
                }
            }
            recompute (s, p);
        }
    }

    void analyse (const Params& p)
    {
        updateSlots (p, (float) hopSize / (float) fs);
        // (โหมด Lock ยังคำนวณสเปกตรัมไว้โชว์ แต่จะไม่ตรวจจับ/วางฟิลเตอร์ใหม่)

        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = ring[(size_t) ((writePos + i) & (fftSize - 1))];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.f);

        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        constexpr int half = fftSize / 2;
        for (int k = 0; k <= half; ++k)
            db[(size_t) k] = 20.f * std::log10 (fftData[(size_t) k] / (fftSize * 0.25f) + 1.0e-9f);

        publishSpectrum();
        if (p.lock) { persist.fill (0); prevDb = db; return; }

        const float binHz = (float) (fs / fftSize);
        const int kMin = juce::jmax (8, (int) (80.f / binHz));
        const int kMax = juce::jmin (half - 8, (int) ((float) juce::jmin (12000.0, 0.45 * fs) / binHz));

        const float thr   = juce::jmap (p.sensitivity, 24.f, 10.f);     // พีคต้องสูงกว่ารอบข้างกี่ dB
        const float floor = juce::jmap (p.sensitivity, -45.f, -65.f);   // ระดับต่ำสุดที่สนใจ (dBFS)
        const int   need  = juce::roundToInt (juce::jmap (p.sensitivity, 8.f, 3.f));   // ต้องค้างกี่เฟรม

        std::array<int, half + 1> np {};
        int best = -1; float bestProm = 0.f;

        for (int k = kMin; k <= kMax; ++k)
        {
            const float v = db[(size_t) k];
            if (! (v > db[(size_t) k - 1] && v >= db[(size_t) k + 1] && v > floor)) continue;

            const float avg = (db[(size_t) k - 6] + db[(size_t) k - 5] + db[(size_t) k - 4] + db[(size_t) k - 3]
                             + db[(size_t) k + 3] + db[(size_t) k + 4] + db[(size_t) k + 5] + db[(size_t) k + 6]) / 8.f;
            const float prom = v - avg;

            if (prom < thr) continue;                         // ไม่โดดพอ
            if (v < prevDb[(size_t) k] - 0.5f) continue;      // กำลังลดลง → ไม่ใช่เสียงหอน

            np[(size_t) k] = 1 + juce::jmax (persist[(size_t) k - 1], persist[(size_t) k], persist[(size_t) k + 1]);

            if (np[(size_t) k] >= need && prom > bestProm) { best = k; bestProm = prom; }
        }

        persist = np;
        prevDb  = db;

        if (best >= 0)
        {
            placeNotch (best, binHz, p);
            for (int k = best - 1; k <= best + 1; ++k) persist[(size_t) k] = 0;   // รอเก็บหลักฐานใหม่ก่อนลึกขึ้นอีก
        }
    }

    void placeNotch (int k, float binHz, const Params& p)
    {
        // parabolic interpolation หาความถี่ละเอียดกว่า bin
        const float a = db[(size_t) k - 1], b = db[(size_t) k], c = db[(size_t) k + 1];
        const float den = a - 2.f * b + c;
        const float delta = std::abs (den) > 1.0e-6f ? juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / den) : 0.f;
        const float f = ((float) k + delta) * binHz;

        for (auto& s : slots)                                // มีฟิลเตอร์แถวนี้อยู่แล้ว (±1/6 octave) → ตัดลึกขึ้น
            if (s.active && std::abs (std::log2 (f / s.freq)) < (1.f / 6.f))
            {
                s.quiet  = 0.f;
                s.gainDb = juce::jmax (-p.maxDepthDb, s.gainDb - 3.f);
                s.freq   = 0.7f * s.freq + 0.3f * f;
                recompute (s, p);
                return;
            }

        int idx = -1;
        for (int i = 0; i < maxSlots; ++i)
            if (! slots[(size_t) i].active) { idx = i; break; }

        if (idx < 0)                                         // เต็ม → แย่งช่องที่ตัดตื้นสุด
        {
            float shallow = -1000.f;
            for (int i = 0; i < maxSlots; ++i)
                if (slots[(size_t) i].gainDb > shallow) { shallow = slots[(size_t) i].gainDb; idx = i; }
        }

        auto& s  = slots[(size_t) idx];
        s.active = true;
        s.freq   = f;
        s.quiet  = 0.f;
        s.gainDb = -juce::jmin (6.f, p.maxDepthDb);
        for (int c = 0; c < maxCh; ++c) state[c][idx] = BiquadState{};
        recompute (s, p);
    }

    double fs = 48000.0;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize>          ring {};
    std::array<float, fftSize * 2>      fftData {};
    std::array<float, fftSize / 2 + 1>  db {}, prevDb {};
    std::array<int,   fftSize / 2 + 1>  persist {};

    std::array<Slot, maxSlots> slots {};

    std::array<std::atomic<float>, displayPoints> display {};
    std::array<std::atomic<float>, maxSlots> slotFreq {}, slotGain {};
    BiquadState state[maxCh][maxSlots];

    int writePos = 0, sinceHop = 0;
    std::atomic<bool> clearFlag { false };
};
