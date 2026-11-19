"""Synthesizes MaximumTrainer's workout sounds (original, no samples).

Usage (from the repo root): python3 scripts/generate_sounds.py [output_dir]
Default output: resources/sound (the files MyResources.qrc bundles).

Each sound is built from struck-bar / bell partials with exponential decay,
a soft attack, a short room reverb, and is normalised to the same loudness so
the set feels consistent. Output: 48 kHz, 16-bit, stereo WAV.
"""
import sys, wave
import numpy as np

SR = 48000
rng = np.random.default_rng(7)


def note(freq, dur, kind="bell", amp=1.0):
    t = np.arange(int(SR * dur)) / SR
    if kind == "bell":        # glassy, long ring
        partials = [(1.0, 1.0, 1.6), (2.0, 0.45, 2.4), (3.01, 0.22, 3.5), (4.2, 0.12, 5.0), (5.43, 0.06, 7.0)]
    elif kind == "marimba":   # warm, woody, short
        partials = [(1.0, 1.0, 5.0), (3.93, 0.32, 14.0), (9.2, 0.08, 30.0)]
    elif kind == "tick":      # very short, rounded
        partials = [(1.0, 1.0, 26.0), (2.76, 0.25, 45.0)]
    else:                     # "chime": bright, medium
        partials = [(1.0, 1.0, 3.2), (2.0, 0.5, 4.5), (3.0, 0.25, 6.0), (4.0, 0.12, 8.0)]
    y = np.zeros_like(t)
    for ratio, gain, decay in partials:
        y += gain * np.sin(2 * np.pi * freq * ratio * t) * np.exp(-decay * t)
    attack = np.minimum(1.0, t / 0.004)           # 4 ms soft attack, no click
    return amp * y * attack


def place(tracks, length):
    out = np.zeros(int(SR * length))
    for start, sig in tracks:
        i = int(SR * start)
        out[i:i + len(sig)] += sig[: max(0, len(out) - i)]
    return out


def reverb(x, mix=0.18, size=0.35):
    n = int(SR * size)
    ir = rng.standard_normal(n) * np.exp(-np.arange(n) / (SR * size / 5))
    wet = np.convolve(x, ir)[: len(x)]
    wet /= (np.max(np.abs(wet)) + 1e-9)
    dry = x / (np.max(np.abs(x)) + 1e-9)
    return (1 - mix) * dry + mix * wet


def finish(x, name, target_rms=0.12, width=0.004):
    x = reverb(x)
    fade = int(SR * 0.03)
    x[-fade:] *= np.linspace(1, 0, fade)
    x *= target_rms / (np.sqrt(np.mean(x ** 2)) + 1e-9)
    peak = np.max(np.abs(x))
    if peak > 0.89:                                # keep headroom (about -1 dBFS)
        x *= 0.89 / peak
    d = int(SR * width)                            # tiny Haas delay for width
    left = x
    right = np.concatenate([np.zeros(d), x[:-d]])
    stereo = np.stack([left, right], axis=1)
    pcm = (stereo * 32767).astype(np.int16)
    with wave.open(f"{out_dir}/{name}.wav", "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(pcm.tobytes())


C4, E4, G4, A4 = 261.63, 329.63, 392.00, 440.00
C5, D5, E5, G5, A5 = 523.25, 587.33, 659.25, 783.99, 880.00
C6, E6, G6, C7 = 1046.50, 1318.51, 1567.98, 2093.00

out_dir = sys.argv[1] if len(sys.argv) > 1 else "resources/sound"

# Countdown: clean "race start" beeps at 3, 2 and 1 s before an interval
# change, then a longer "go" an octave higher as it starts.
def tone(freq, dur, harmonics=((1, 1.0),), attack=0.003, release=0.04, sustain_decay=0.0):
    t = np.arange(int(SR * dur)) / SR
    y = sum(g * np.sin(2 * np.pi * freq * r * t) for r, g in harmonics)
    env = np.minimum(1.0, t / attack) * np.minimum(1.0, (dur - t) / release) * np.exp(-sustain_decay * t)
    return y * np.clip(env, 0, 1)


finish(place([(0, tone(880, 0.12, ((1, 1.0), (2, 0.15))))], 0.2), "first", target_rms=0.09)
finish(place([(0, tone(1760, 0.55, ((1, 1.0), (2, 0.12)), sustain_decay=2.0))], 0.65), "second", target_rms=0.11)

# Start / resume: short rising two-note chime.
finish(place([(0, note(G5, 0.8, "chime")), (0.11, note(C6, 0.9, "chime"))], 1.0), "resume")

# Workout complete: warm rising arpeggio landing on a ringing chord.
end = [(0.00, note(C5, 2.2, "bell")), (0.16, note(E5, 2.0, "bell")), (0.32, note(G5, 1.9, "bell")),
       (0.48, note(C6, 1.9, "bell")), (0.48, note(E6, 1.9, "bell", 0.35))]
finish(place(end, 2.6), "end_workout", target_rms=0.11)

# Achievement: quick sparkling run up with a shimmer on top.
run = [(i * 0.055, note(f, 1.0, "bell", 0.8)) for i, f in enumerate([C6, E6, G6, C7])]
shimmer = rng.standard_normal(int(SR * 0.6)) * np.exp(-np.arange(int(SR * 0.6)) / (SR * 0.12)) * 0.05
hp = np.diff(shimmer, prepend=0)                   # crude high-pass for an airy "sparkle"
finish(place(run + [(0.18, hp)], 1.3), "fire_torch")

# Alerts: rising = push harder (too low), falling = ease off (too high).
# Power uses bright bells, cadence a woody marimba, so they are told apart.
finish(place([(0, note(E5, 0.6, "bell")), (0.16, note(A5, 0.8, "bell"))], 1.0), "PowerTooLow")
finish(place([(0, note(A5, 0.6, "bell")), (0.16, note(E5, 0.8, "bell"))], 1.0), "PowerTooHigh")
finish(place([(0, note(C4 * 2, 0.4, "marimba")), (0.13, note(G4 * 2, 0.5, "marimba"))], 0.75), "CadenceTooLow")
finish(place([(0, note(G4 * 2, 0.4, "marimba")), (0.13, note(C4 * 2, 0.5, "marimba"))], 0.75), "CadenceTooHigh")
print("ok")
