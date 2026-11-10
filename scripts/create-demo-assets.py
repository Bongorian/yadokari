#!/usr/bin/env python3
"""Generate original PCM16 sample material; no downloaded audio or presets."""
from pathlib import Path
import math
import struct
import wave
path = Path('assets/samples/demo-slices.wav')
path.parent.mkdir(parents=True, exist_ok=True)
frames = bytearray()
for i in range(48000):
    section, offset = divmod(i, 12000)
    frequency = (220, 330, 440, 660)[section]
    envelope = math.exp(-offset / 3000) * min(offset / 100, 1)
    value = 0.65 * envelope * math.sin(2 * math.pi * frequency * offset / 48000)
    frames += struct.pack('<h', round(value * 32767))
with wave.open(str(path), 'wb') as out:
    out.setnchannels(1)
    out.setsampwidth(2)
    out.setframerate(48000)
    out.writeframes(frames)
