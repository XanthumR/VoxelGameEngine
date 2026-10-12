"""Makes the game's sounds (assets/sounds/*.wav) from recordings in the public domain (CC0) at
BigSoundBank (https://bigsoundbank.com): downloads each MP3, takes the part used, and writes it as a
22 kHz mono WAV. Loops are cross-faded end into start so they repeat without a seam; one-shots are
trimmed and faded out. Also writes assets/sounds/CREDITS.md. Needs numpy and the miniaudio Python
package (pip install miniaudio). Run from the project root:
    python tools/sounds/fetch.py
"""

import os
import tempfile
import urllib.request
import wave

import miniaudio
import numpy as np

RATE = 22050
OUT = os.path.join("assets", "sounds")
CACHE = os.path.join(tempfile.gettempdir(), "voxel_anno_sounds")
SOURCE = "https://bigsoundbank.com/UPLOAD/mp3/{:04d}.mp3"
PAGE = "https://bigsoundbank.com/s{:04d}.html"
CROSSFADE = 1.5  # Seconds of a loop's end faded into its start

# name: (BigSoundBank number, title, start, end) in seconds of the recording
LOOPS = {
    "sea": (698, "Sea waves", 30.0, 60.0),
    "wind": (595, "Wind", 10.0, 40.0),
    "saw": (559, "Saw wood", 1.0, 11.0),
    "smithy": (3589, "Anvil #1", 13.0, 27.0),
    "mill": (2768, "Watermill", 20.0, 50.0),
    "animals": (3220, "Flock of sheep and cows", 15.0, 55.0),
    "town": (2728, "Outdoor market #1", 30.0, 60.0),
    "harbor": (2573, "Gulls on the harbor", 14.0, 44.0),
}
ONE_SHOTS = {
    "click": (1742, "Apple Magic Mouse, single click", 0.05, 0.35),
    "place": (6, "Nail and hammer #2", 0.25, 1.65),
    "demolish": (1634, "Wheelbarrow of limestone, reversed", 5.6, 7.6),
    "road": (85, "Step on gravel", 0.05, 0.5),
    "notify": (292, "Small bell #1", 0.0, 1.6),
    "coins": (339, "Coin in cup #1", 0.0, 0.9),
    "upgrade": (2079, "Chimes \"Dream\" #1", 0.0, 2.3),
    "error": (1588, "Gavel, 1 blow", 0.0, 0.3),
}


def recording(number):
    """The recording as mono floats at RATE, downloaded once into CACHE."""
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, "{:04d}.mp3".format(number))
    if not os.path.exists(path):
        urllib.request.urlretrieve(SOURCE.format(number), path)
    decoded = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.FLOAT32, nchannels=1, sample_rate=RATE)
    return np.asarray(decoded.samples, dtype=np.float32)


def loop(samples, start, end):
    """start..end, with the CROSSFADE after end blended into its start (equal power)."""
    a, b, fade = int(start * RATE), int(end * RATE), int(CROSSFADE * RATE)
    body = samples[a:b].copy()
    tail = samples[b:b + fade]
    ramp = np.linspace(0.0, 1.0, len(tail))
    body[:len(tail)] = body[:len(tail)] * np.sqrt(ramp) + tail * np.sqrt(1.0 - ramp)
    # The same loudness for every loop, unless that would clip its loudest moment
    return body * min(0.12 / (np.sqrt(np.mean(body ** 2)) + 1e-9), 0.95 / (np.abs(body).max() + 1e-9))


def one_shot(samples, start, end):
    part = samples[int(start * RATE):int(end * RATE)].copy()
    fade = min(len(part) // 4, int(0.08 * RATE))
    part[:int(0.003 * RATE)] *= np.linspace(0.0, 1.0, int(0.003 * RATE))  # No click at the cut
    part[-fade:] *= np.linspace(1.0, 0.0, fade)
    return part / (np.abs(part).max() + 1e-9) * 0.8


def save(name, samples):
    data = (np.clip(samples, -1.0, 1.0) * 32767).astype("<i2").tobytes()
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    credits = ["# Sound credits", "",
        "Every sound here is cut from a recording in the public domain (CC0) at BigSoundBank, made into game sounds by "
        "`tools/sounds/fetch.py` (looped or trimmed, mono, 22 kHz). No attribution is required; listed with thanks.", "",
        "| File | Recording | Part used |", "|---|---|---|"]
    for table, make in ((LOOPS, loop), (ONE_SHOTS, one_shot)):
        for name, (number, title, start, end) in table.items():
            save(name, make(recording(number), start, end))
            credits.append("| `{}.wav` | [{} (#{:04d})]({}) | {:.2f}-{:.2f} s |".format(name, title, number, PAGE.format(number), start, end))
    with open(os.path.join(OUT, "CREDITS.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(credits) + "\n")
    print("Wrote {} loops and {} sounds to {}".format(len(LOOPS), len(ONE_SHOTS), OUT))
