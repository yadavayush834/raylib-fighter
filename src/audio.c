// Every sound in the game is synthesised at start-up: layered impact effects and two original
// music loops (a battle theme and a pentatonic menu theme). Nothing is loaded from disk.
#include "audio.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define RATE 44100

// ---------------------------------------------------------------------------------------------
// DSP toolkit
// ---------------------------------------------------------------------------------------------
static uint32_t noiseState = 22222;
static float Noise(void) {
    noiseState = noiseState * 1664525u + 1013904223u;
    return (float)((noiseState >> 9) & 0x7fffff) / 4194303.5f - 1;
}
typedef struct {
    float y;
} OnePole;
static float Lowpass(OnePole *f, float x, float cutoff) {
    float a = 1 - expf(-2 * PI * fminf(cutoff, RATE * 0.45f) / RATE);
    f->y += (x - f->y) * a;
    return f->y;
}
static float Highpass(OnePole *f, float x, float cutoff) {
    return x - Lowpass(f, x, cutoff);
}
static float Saturate(float x) {
    return tanhf(x);
}
static float *Buffer(float seconds, unsigned *frames) {
    *frames = (unsigned)(seconds * RATE);
    return calloc(*frames, sizeof(float));
}
// A few feedback combs make a cheap, dense tail.
static void Reverb(float *buf, unsigned frames, float mix, float feedback) {
    const int delays[4] = {1557, 1617, 1491, 1422};
    float *out = calloc(frames, sizeof(float));
    if (!out)
        return;
    for (int d = 0; d < 4; ++d) {
        float *line = calloc((size_t)delays[d], sizeof(float));
        if (!line)
            continue;
        int idx = 0;
        OnePole damp = {0};
        for (unsigned i = 0; i < frames; ++i) {
            float y = line[idx];
            line[idx] = buf[i] + Lowpass(&damp, y, 3500) * feedback;
            idx = (idx + 1) % delays[d];
            out[i] += y * 0.25f;
        }
        free(line);
    }
    for (unsigned i = 0; i < frames; ++i)
        buf[i] = buf[i] * (1 - mix * 0.5f) + out[i] * mix;
    free(out);
}
static Sound ToSound(float *buf, unsigned frames, float gain) {
    short *samples = malloc(frames * sizeof(short));
    Sound s = {0};
    if (samples) {
        for (unsigned i = 0; i < frames; ++i) {
            float fade = i + 64 > frames ? (frames - i) / 64.0f : 1;
            samples[i] = (short)(Saturate(buf[i] * gain) * fade * 30000);
        }
        Wave wave = {.frameCount = frames, .sampleRate = RATE, .sampleSize = 16, .channels = 1, .data = samples};
        s = LoadSoundFromWave(wave);
        free(samples);
    }
    free(buf);
    return s;
}

// Building blocks.
static void Thump(float *b, unsigned n, float start, float f0, float f1, float decay, float amp) {
    float phase = 0;
    for (unsigned i = (unsigned)(start * RATE); i < n; ++i) {
        float t = (float)i / RATE - start;
        float f = f1 + (f0 - f1) * expf(-t * 25);
        phase += 2 * PI * f / RATE;
        b[i] += sinf(phase) * expf(-t * decay) * amp;
    }
}
static void NoiseBurst(float *b, unsigned n, float start, float cutoff, float decay, float amp, bool high) {
    OnePole f = {0};
    for (unsigned i = (unsigned)(start * RATE); i < n; ++i) {
        float t = (float)i / RATE - start;
        float x = Noise();
        float y = high ? Highpass(&f, x, cutoff) : Lowpass(&f, x, cutoff);
        b[i] += y * expf(-t * decay) * amp;
    }
}
static void Ring(float *b, unsigned n, float start, float freq, float decay, float amp) {
    for (unsigned i = (unsigned)(start * RATE); i < n; ++i) {
        float t = (float)i / RATE - start;
        b[i] += sinf(2 * PI * freq * t) * expf(-t * decay) * amp * fminf(1, t * 2000);
    }
}
static void Sweep(float *b, unsigned n, float c0, float c1, float amp, float q) {
    OnePole lo = {0}, hi = {0};
    for (unsigned i = 0; i < n; ++i) {
        float p = (float)i / n;
        float cutoff = c0 * powf(c1 / c0, p);
        float x = Highpass(&hi, Lowpass(&lo, Noise(), cutoff), cutoff * q);
        b[i] += x * sinf(PI * p) * amp;
    }
}

static Sound Make(SfxId id) {
    unsigned n;
    float *b;
    switch (id) {
    case SFX_WHOOSH_LIGHT:
        b = Buffer(0.14f, &n);
        Sweep(b, n, 1800, 5200, 0.9f, 0.4f);
        return ToSound(b, n, 0.8f);
    case SFX_WHOOSH_HEAVY:
        b = Buffer(0.24f, &n);
        Sweep(b, n, 700, 2600, 1.4f, 0.35f);
        return ToSound(b, n, 0.9f);
    case SFX_HIT_LIGHT:
        b = Buffer(0.16f, &n);
        Thump(b, n, 0, 260, 70, 28, 0.9f);
        NoiseBurst(b, n, 0, 2500, 70, 0.9f, true);
        NoiseBurst(b, n, 0, 1200, 30, 0.5f, false);
        return ToSound(b, n, 1.5f);
    case SFX_HIT_HEAVY:
        b = Buffer(0.34f, &n);
        Thump(b, n, 0, 190, 45, 13, 1.2f);
        NoiseBurst(b, n, 0, 3800, 90, 1.0f, true);
        NoiseBurst(b, n, 0.004f, 900, 16, 0.8f, false);
        return ToSound(b, n, 2.2f);
    case SFX_HIT_SUPER:
        b = Buffer(0.6f, &n);
        Thump(b, n, 0, 160, 40, 9, 1.2f);
        NoiseBurst(b, n, 0, 3000, 40, 0.8f, true);
        Ring(b, n, 0, 820, 9, 0.25f);
        Ring(b, n, 0, 1237, 11, 0.2f);
        Ring(b, n, 0, 1711, 14, 0.15f);
        Reverb(b, n, 0.35f, 0.7f);
        return ToSound(b, n, 2.0f);
    case SFX_BLOCK:
        b = Buffer(0.2f, &n);
        Ring(b, n, 0, 1420, 30, 0.45f);
        Ring(b, n, 0, 2130, 38, 0.35f);
        Ring(b, n, 0, 3190, 50, 0.25f);
        NoiseBurst(b, n, 0, 4000, 120, 0.6f, true);
        Thump(b, n, 0, 300, 120, 40, 0.4f);
        return ToSound(b, n, 1.2f);
    case SFX_COUNTER:
        b = Buffer(0.5f, &n);
        Ring(b, n, 0, 1318.5f, 7, 0.4f);
        Ring(b, n, 0.06f, 1975.5f, 7, 0.4f);
        Ring(b, n, 0.06f, 3951, 12, 0.12f);
        Reverb(b, n, 0.3f, 0.6f);
        return ToSound(b, n, 1.1f);
    case SFX_GUARD_BREAK:
        b = Buffer(0.7f, &n);
        NoiseBurst(b, n, 0, 5000, 14, 0.8f, true);
        for (int k = 0; k < 14; ++k)
            Ring(b, n, (float)k * 0.012f, 2200 + (float)((k * 7919) % 2600), 9 + k, 0.12f);
        Thump(b, n, 0, 140, 50, 10, 0.8f);
        Reverb(b, n, 0.35f, 0.65f);
        return ToSound(b, n, 1.6f);
    case SFX_LAUNCH:
        b = Buffer(0.32f, &n);
        Sweep(b, n, 600, 6000, 1.0f, 0.3f);
        Thump(b, n, 0, 120, 60, 18, 0.6f);
        return ToSound(b, n, 1.2f);
    case SFX_WALL:
        b = Buffer(0.45f, &n);
        Thump(b, n, 0, 120, 38, 10, 1.3f);
        NoiseBurst(b, n, 0, 1400, 12, 0.9f, false);
        NoiseBurst(b, n, 0, 5000, 60, 0.6f, true);
        return ToSound(b, n, 2.0f);
    case SFX_KNOCKDOWN:
        b = Buffer(0.4f, &n);
        Thump(b, n, 0, 110, 40, 12, 1.2f);
        NoiseBurst(b, n, 0, 600, 12, 1.0f, false);
        return ToSound(b, n, 1.8f);
    case SFX_LAND:
        b = Buffer(0.1f, &n);
        Thump(b, n, 0, 140, 60, 40, 0.5f);
        NoiseBurst(b, n, 0, 800, 50, 0.4f, false);
        return ToSound(b, n, 1.0f);
    case SFX_JUMP:
        b = Buffer(0.12f, &n);
        Sweep(b, n, 900, 2500, 0.5f, 0.5f);
        return ToSound(b, n, 0.8f);
    case SFX_DASH:
        b = Buffer(0.22f, &n);
        Sweep(b, n, 2500, 900, 1.2f, 0.25f);
        return ToSound(b, n, 0.9f);
    case SFX_FIRE: {
        b = Buffer(0.4f, &n);
        OnePole lo = {0}, hi = {0};
        float phase = 0;
        for (unsigned i = 0; i < n; ++i) {
            float t = (float)i / RATE, p = (float)i / n;
            float cutoff = 1500 + sinf(t * 70) * 900;
            b[i] += Highpass(&hi, Lowpass(&lo, Noise(), cutoff), cutoff * 0.5f) * (1 - p) * 1.4f;
            phase += 2 * PI * (520 + 500 * p) / RATE;
            b[i] += sinf(phase) * (1 - p) * expf(-t * 4) * 0.25f;
        }
        Thump(b, n, 0, 200, 90, 30, 0.5f);
        return ToSound(b, n, 1.2f);
    }
    case SFX_CLASH:
        b = Buffer(0.9f, &n);
        Thump(b, n, 0, 140, 36, 6, 1.3f);
        NoiseBurst(b, n, 0, 2000, 6, 1.0f, false);
        Ring(b, n, 0, 990, 6, 0.25f);
        Ring(b, n, 0, 1490, 8, 0.2f);
        Reverb(b, n, 0.45f, 0.75f);
        return ToSound(b, n, 1.6f);
    case SFX_SUPER_FLASH: {
        b = Buffer(1.1f, &n);
        OnePole lo = {0};
        float p1 = 0, p2 = 0, p3 = 0;
        for (unsigned i = 0; i < n; ++i) {
            float t = (float)i / RATE, p = t / 0.75f;
            float env = p < 1 ? p * p : expf(-(t - 0.75f) * 8);
            p1 += 2 * PI * 110 / RATE, p2 += 2 * PI * 164.8f / RATE, p3 += 2 * PI * 220.5f / RATE;
            float saw = (fmodf(p1, 2 * PI) / PI - 1) + (fmodf(p2, 2 * PI) / PI - 1) + (fmodf(p3, 2 * PI) / PI - 1);
            b[i] += Lowpass(&lo, saw * 0.3f + Noise() * 0.2f, 300 + fminf(p, 1) * 6000) * env;
        }
        Ring(b, n, 0.72f, 2637, 5, 0.35f);
        Ring(b, n, 0.72f, 3951, 7, 0.2f);
        Thump(b, n, 0, 90, 45, 3, 0.7f);
        Reverb(b, n, 0.4f, 0.75f);
        return ToSound(b, n, 1.4f);
    }
    case SFX_KO:
        b = Buffer(2.2f, &n);
        Thump(b, n, 0, 140, 38, 2.5f, 1.4f);
        NoiseBurst(b, n, 0, 900, 3, 1.0f, false);
        NoiseBurst(b, n, 0, 6000, 30, 0.6f, true);
        Ring(b, n, 0.02f, 440, 3, 0.18f);
        Ring(b, n, 0.02f, 659, 3.5f, 0.12f);
        Reverb(b, n, 0.6f, 0.82f);
        return ToSound(b, n, 1.8f);
    case SFX_ROUND:
        b = Buffer(1.2f, &n);
        Thump(b, n, 0, 160, 62, 7, 1.3f);
        NoiseBurst(b, n, 0, 700, 20, 0.6f, false);
        Ring(b, n, 0, 660, 4, 0.22f);
        Ring(b, n, 0, 990, 5, 0.12f);
        Reverb(b, n, 0.45f, 0.78f);
        return ToSound(b, n, 1.5f);
    case SFX_FIGHT:
        b = Buffer(1.3f, &n);
        Thump(b, n, 0, 170, 60, 9, 1.2f);
        Thump(b, n, 0.13f, 170, 58, 7, 1.3f);
        NoiseBurst(b, n, 0.13f, 4500, 12, 0.5f, true);
        Ring(b, n, 0.13f, 880, 4, 0.18f);
        Ring(b, n, 0.13f, 1108.7f, 4, 0.15f);
        Ring(b, n, 0.13f, 1318.5f, 4, 0.15f);
        Reverb(b, n, 0.5f, 0.8f);
        return ToSound(b, n, 1.5f);
    case SFX_PERFECT:
        b = Buffer(1.2f, &n);
        for (int k = 0; k < 5; ++k)
            Ring(b, n, k * 0.07f, 880 * powf(2, (float)(int[]){0, 4, 7, 12, 16}[k] / 12), 4, 0.18f);
        Reverb(b, n, 0.5f, 0.8f);
        return ToSound(b, n, 1.2f);
    case SFX_THUNDER: {
        b = Buffer(3.0f, &n);
        OnePole lo = {0}, lo2 = {0};
        for (unsigned i = 0; i < n; ++i) {
            float t = (float)i / RATE;
            float crackle = t < 0.25f ? (Noise() > 0.97f ? Noise() * 2 : 0) * (0.25f - t) * 4 : 0;
            float rumble = Lowpass(&lo2, Lowpass(&lo, Noise(), 180), 120) * 6;
            b[i] += (rumble * (fminf(1, t * 8)) * expf(-t * 1.2f) + crackle) * (1 + 0.3f * sinf(t * 9));
        }
        return ToSound(b, n, 1.1f);
    }
    case SFX_UI_MOVE:
        b = Buffer(0.06f, &n);
        Ring(b, n, 0, 1560, 60, 0.4f);
        return ToSound(b, n, 1.0f);
    case SFX_UI_SELECT:
        b = Buffer(0.3f, &n);
        Ring(b, n, 0, 880, 18, 0.35f);
        Ring(b, n, 0.06f, 1318.5f, 14, 0.35f);
        Thump(b, n, 0, 200, 90, 30, 0.4f);
        return ToSound(b, n, 1.1f);
    case SFX_UI_BACK:
        b = Buffer(0.18f, &n);
        Ring(b, n, 0, 990, 20, 0.35f);
        Ring(b, n, 0.05f, 660, 20, 0.35f);
        return ToSound(b, n, 1.0f);
    default:
        b = Buffer(0.05f, &n);
        return ToSound(b, n, 0);
    }
}

// ---------------------------------------------------------------------------------------------
// Music
// ---------------------------------------------------------------------------------------------
typedef struct {
    short *data;
    unsigned frames;
} Loop;
static Loop loops[3];
static volatile int currentTrack = MUSIC_NONE, wantedTrack = MUSIC_NONE;
static volatile float musicGain = 0, duckLevel = 1;
static unsigned playhead = 0;

static float NoteHz(int midi) {
    return 440.0f * powf(2, (midi - 69) / 12.0f);
}
static void AddTone(float *b, unsigned n, float start, float dur, float hz, float amp, int shape, float decay) {
    unsigned s = (unsigned)(start * RATE), e = (unsigned)((start + dur) * RATE);
    float phase = 0;
    for (unsigned i = s; i < e && i < n; ++i) {
        float t = (float)(i - s) / RATE;
        phase += hz / RATE;
        phase -= floorf(phase);
        float v = shape == 0 ? sinf(2 * PI * phase) : shape == 1 ? phase * 2 - 1 : (phase < 0.5f ? 1.0f : -1.0f);
        float env = fminf(1, t * 200) * expf(-t * decay) * fminf(1, (dur - t) * 60);
        b[i] += v * env * amp;
    }
}
static void Pluck(float *b, unsigned n, float start, float hz, float amp, float damping) {
    int period = (int)(RATE / hz);
    if (period < 2)
        return;
    float *line = malloc((size_t)period * sizeof(float));
    if (!line)
        return;
    for (int k = 0; k < period; ++k)
        line[k] = Noise();
    unsigned s = (unsigned)(start * RATE);
    int idx = 0;
    float prev = 0;
    for (unsigned i = s; i < n && i < s + RATE * 3; ++i) {
        float v = line[idx];
        float next = (v + prev) * 0.5f * damping;
        prev = v;
        line[idx] = next;
        idx = (idx + 1) % period;
        b[i] += v * amp;
    }
    free(line);
}
static void Kick(float *b, unsigned n, float start, float amp) {
    Thump(b, (unsigned)fminf((float)n, (start + 0.4f) * RATE), start, 150, 45, 9, amp);
}
static void Snare(float *b, unsigned n, float start, float amp) {
    unsigned end = (unsigned)fminf((float)n, (start + 0.25f) * RATE);
    NoiseBurst(b, end, start, 1800, 18, amp, true);
    Thump(b, end, start, 240, 180, 30, amp * 0.5f);
}
static void Hat(float *b, unsigned n, float start, float amp) {
    NoiseBurst(b, (unsigned)fminf((float)n, (start + 0.06f) * RATE), start, 7000, 70, amp, true);
}

static void RenderBattle(Loop *loop) {
    const float bpm = 138, beat = 60 / bpm, bar = beat * 4;
    unsigned n;
    float *b = Buffer(bar * 8, &n);
    // A minor: Am F C G | Am F G E  (roots as MIDI notes)
    const int roots[8] = {57, 53, 48, 55, 57, 53, 55, 52};
    const int thirds[8] = {3, 4, 4, 4, 3, 4, 4, 4};
    const int lead[32] = {69, 72, 76, 74, 72, 69, 67, 69, 65, 69, 72, 71, 69, 65, 64, 65,
                          67, 71, 74, 72, 71, 67, 69, 71, 72, 71, 69, 68, 64, 68, 71, 76};
    for (int barIndex = 0; barIndex < 8; ++barIndex) {
        float t0 = barIndex * bar;
        int root = roots[barIndex];
        for (int k = 0; k < 8; ++k) {
            float t = t0 + k * beat / 2;
            int note = root - 12 + ((k % 2) ? 12 : 0);
            AddTone(b, n, t, beat / 2 * 0.9f, NoteHz(note), 0.16f, 1, 6);
            AddTone(b, n, t, beat / 2 * 0.9f, NoteHz(note) * 1.005f, 0.1f, 2, 8);
        }
        for (int k = 0; k < 16; ++k) {
            float t = t0 + k * beat / 4;
            int chord[3] = {root + 12, root + 12 + thirds[barIndex], root + 19};
            AddTone(b, n, t, beat / 4, NoteHz(chord[k % 3] + (k % 6 >= 3 ? 12 : 0)), 0.045f, 2, 22);
        }
        if (barIndex >= 4 || barIndex % 2 == 1)
            for (int k = 0; k < 4; ++k) {
                int note = lead[(barIndex % 4) * 8 + k * 2];
                AddTone(b, n, t0 + k * beat, beat * 0.95f, NoteHz(note), 0.07f, 0, 3);
                AddTone(b, n, t0 + k * beat, beat * 0.95f, NoteHz(note + 12), 0.025f, 1, 5);
            }
        Kick(b, n, t0, 0.9f);
        Kick(b, n, t0 + beat * 1.5f, 0.6f);
        Kick(b, n, t0 + beat * 2, 0.9f);
        if (barIndex % 2 == 1)
            Kick(b, n, t0 + beat * 3.5f, 0.6f);
        Snare(b, n, t0 + beat, 0.55f);
        Snare(b, n, t0 + beat * 3, 0.55f);
        if (barIndex == 7)
            for (int k = 0; k < 4; ++k)
                Snare(b, n, t0 + beat * 3 + k * beat / 4, 0.3f + k * 0.08f);
        for (int k = 0; k < 8; ++k)
            Hat(b, n, t0 + k * beat / 2 + beat / 4, k % 2 ? 0.18f : 0.12f);
    }
    loop->frames = n;
    loop->data = malloc(n * sizeof(short));
    if (loop->data)
        for (unsigned i = 0; i < n; ++i)
            loop->data[i] = (short)(Saturate(b[i] * 0.9f) * 22000);
    free(b);
}
static void RenderMenu(Loop *loop) {
    const float bpm = 84, beat = 60 / bpm, bar = beat * 4;
    unsigned n;
    float *b = Buffer(bar * 8, &n);
    // A Japanese "in" flavoured pentatonic: A Bb D E F.
    const int scale[5] = {57, 58, 62, 64, 65};
    const int melody[32] = {4, 3, 2, 0, -1, 2, 3, -1, 4, 6, 5, 3, -1, 2, 0, -1,
                            2, 3, 4, 6, -1, 7, 6, 4, 3, -1, 2, 3, 0, -1, -1, -1};
    const int drones[8] = {45, 45, 46, 43, 45, 45, 41, 40};
    for (int barIndex = 0; barIndex < 8; ++barIndex) {
        float t0 = barIndex * bar;
        AddTone(b, n, t0, bar, NoteHz(drones[barIndex]), 0.08f, 0, 0.4f);
        AddTone(b, n, t0, bar, NoteHz(drones[barIndex] + 7), 0.04f, 0, 0.4f);
        for (int k = 0; k < 4; ++k) {
            int m = melody[(barIndex % 4) * 8 + k * 2 + (barIndex >= 4 ? 1 : 0)];
            if (m < 0)
                continue;
            int note = scale[m % 5] + 12 * (m / 5) + 12;
            Pluck(b, n, t0 + k * beat, NoteHz(note), 0.32f, 0.996f);
        }
        Pluck(b, n, t0 + beat * 2.5f, NoteHz(scale[(barIndex * 2) % 5]), 0.18f, 0.994f);
        Kick(b, n, t0, 0.45f);
        if (barIndex % 2)
            Kick(b, n, t0 + beat * 2.75f, 0.3f);
    }
    Reverb(b, n, 0.45f, 0.8f);
    loop->frames = n;
    loop->data = malloc(n * sizeof(short));
    if (loop->data)
        for (unsigned i = 0; i < n; ++i)
            loop->data[i] = (short)(Saturate(b[i]) * 22000);
    free(b);
}

static void MusicCallback(void *buffer, unsigned int frames) {
    short *out = buffer;
    for (unsigned i = 0; i < frames; ++i) {
        float target = wantedTrack == currentTrack ? 0.55f * duckLevel : 0;
        musicGain += (target - musicGain) * 0.0004f;
        if (musicGain < 0.002f && wantedTrack != currentTrack) {
            currentTrack = wantedTrack;
            playhead = 0;
        }
        const Loop *loop = &loops[currentTrack];
        if (currentTrack == MUSIC_NONE || !loop->data || !loop->frames) {
            out[i] = 0;
            continue;
        }
        out[i] = (short)(loop->data[playhead] * musicGain);
        playhead = (playhead + 1) % loop->frames;
    }
}

// ---------------------------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------------------------
void GameAudioInit(GameAudio *a) {
    memset(a, 0, sizeof(*a));
    InitAudioDevice();
    a->ready = IsAudioDeviceReady();
    if (!a->ready)
        return;
    for (int i = 0; i < SFX_COUNT; ++i) {
        a->effects[i][0] = Make((SfxId)i);
        for (int v = 1; v < SFX_VOICES; ++v)
            a->effects[i][v] = LoadSoundAlias(a->effects[i][0]);
    }
    RenderBattle(&loops[MUSIC_BATTLE]);
    RenderMenu(&loops[MUSIC_MENU]);
    SetAudioStreamBufferSizeDefault(2048);
    a->stream = LoadAudioStream(RATE, 16, 1);
    if (IsAudioStreamValid(a->stream)) {
        SetAudioStreamCallback(a->stream, MusicCallback);
        PlayAudioStream(a->stream);
        a->streaming = true;
    }
    SetMasterVolume(0.55f);
}
void GameAudioSfx(GameAudio *a, SfxId id) {
    if (!a->ready || a->muted || id >= SFX_COUNT)
        return;
    Sound *s = &a->effects[id][a->next[id]];
    a->next[id] = (a->next[id] + 1) % SFX_VOICES;
    if (!s->frameCount)
        return;
    float variance = id <= SFX_HIT_SUPER || id == SFX_BLOCK || id == SFX_LAND ? 0.08f : 0.0f;
    SetSoundPitch(*s, 1 + ((float)GetRandomValue(-100, 100) / 100) * variance);
    PlaySound(*s);
}
void GameAudioPlay(GameAudio *a, const GameEvent *events, int count) {
    for (int i = 0; i < count; ++i) {
        const GameEvent *e = &events[i];
        float s = e->strength;
        switch (e->type) {
        case EV_HIT:
            GameAudioSfx(a, s >= 2 ? SFX_HIT_HEAVY : SFX_HIT_LIGHT);
            break;
        case EV_SUPER_HIT:
            GameAudioSfx(a, s >= 4 ? SFX_HIT_SUPER : SFX_HIT_HEAVY);
            break;
        case EV_BLOCK:
            GameAudioSfx(a, SFX_BLOCK);
            break;
        case EV_COUNTER:
            GameAudioSfx(a, SFX_COUNTER);
            break;
        case EV_GUARD_BREAK:
            GameAudioSfx(a, SFX_GUARD_BREAK);
            break;
        case EV_LAUNCH:
            GameAudioSfx(a, SFX_LAUNCH);
            break;
        case EV_WALL_BOUNCE:
            GameAudioSfx(a, SFX_WALL);
            break;
        case EV_KNOCKDOWN:
            GameAudioSfx(a, SFX_KNOCKDOWN);
            break;
        case EV_LAND:
            GameAudioSfx(a, SFX_LAND);
            break;
        case EV_JUMP:
            GameAudioSfx(a, s >= 3 ? SFX_LAUNCH : SFX_JUMP);
            break;
        case EV_DASH:
            GameAudioSfx(a, SFX_DASH);
            break;
        case EV_WHIFF:
            if (s < 3)
                GameAudioSfx(a, s >= 2 ? SFX_WHOOSH_HEAVY : SFX_WHOOSH_LIGHT);
            break;
        case EV_FIRE:
            GameAudioSfx(a, SFX_FIRE);
            break;
        case EV_CLASH:
            GameAudioSfx(a, SFX_CLASH);
            break;
        case EV_SUPER_FLASH:
            GameAudioSfx(a, SFX_SUPER_FLASH);
            break;
        case EV_KO:
            GameAudioSfx(a, SFX_KO);
            break;
        case EV_TIME_UP:
        case EV_ROUND_START:
            GameAudioSfx(a, SFX_ROUND);
            break;
        case EV_FIGHT:
            GameAudioSfx(a, SFX_FIGHT);
            break;
        case EV_PERFECT:
            GameAudioSfx(a, SFX_PERFECT);
            break;
        default:
            break;
        }
    }
}
void GameAudioMusic(GameAudio *a, MusicTrack track) {
    (void)a;
    wantedTrack = track;
}
void GameAudioDuck(GameAudio *a, float level) {
    (void)a;
    duckLevel = fmaxf(0, fminf(1, level));
}
void GameAudioMute(GameAudio *a) {
    a->muted = !a->muted;
    if (a->ready)
        SetMasterVolume(a->muted ? 0 : 0.55f);
}
void GameAudioClose(GameAudio *a) {
    if (!a->ready)
        return;
    if (a->streaming)
        UnloadAudioStream(a->stream);
    for (int i = 0; i < SFX_COUNT; ++i) {
        for (int v = 1; v < SFX_VOICES; ++v)
            if (a->effects[i][v].frameCount)
                UnloadSoundAlias(a->effects[i][v]);
        if (a->effects[i][0].frameCount)
            UnloadSound(a->effects[i][0]);
    }
    for (int i = 0; i < 3; ++i) {
        free(loops[i].data);
        loops[i].data = NULL;
    }
    CloseAudioDevice();
}
