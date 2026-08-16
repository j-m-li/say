# Amiga 500 Text-to-Speech Synthesizer (`say`)


agy --conversation=59664997-caf0-4f8a-aacf-744e75641e9b

A lightweight, standalone Text-to-Speech (TTS) synthesizer written in strict **ANSI C (C90 / ISO C89)**, inspired by the classic Commodore Amiga 500 `Say` command and formant speech synthesis (SAM, narrator.device).

Dedicated to the **Public Domain** under **Creative Commons CC0 1.0 Universal / Unlicense**.

---

## Features

- **Strict ANSI C90 Standard**: Written in pure ANSI C without non-standard extensions. Compiles cleanly with `-std=c90 -pedantic -Wall -Wextra -Werror`.
- **Pure Standard Library**: Zero external library dependencies (only standard `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<ctype.h>`, `<math.h>`).
- **Standard Audio Output**: Directly writes RIFF WAVE files at **8000 Hz sample rate**, **8-bit unsigned PCM**, **Mono** (1 channel).
- **Formant Resonator Acoustic Pipeline**:
  - 3 parallel 2nd-order digital IIR biquad resonators ($F_1, F_2, F_3$) with variable bandwidths.
  - Smooth raised-cosine glottal flow oscillator with continuous derivative (zero DC clicks or derivative spikes).
  - Consonant locus coarticulation glides.
  - 16-bit retro Galois LFSR noise generator for authentic retro console / Amiga Paula chip timbre.
- **English Grapheme-to-Phoneme (G2P) Engine**:
  - Comprehensive English letter-to-sound rule engine (magic-E, vowel digraphs, consonant blends, suffixes).
  - Built-in irregular dictionary for common vocabulary.
  - Automatic number expansion for integers from `0` up to `999,999` (e.g. `500` $\to$ *five hundred*, `1985` $\to$ *nineteen eighty five*).
- **Natural Prosody & Intonation**:
  - Sentence pitch declination and vowel stress.
  - Rising question intonation on sentences ending with `?`.
- **Flexible CLI**: Supports direct text arguments, standard input stream piping (`stdin`), ARPAbet phonetic mode (`-ph`), and pitch/speed/tone parameters.

---

## Build Instructions

To build with `gcc` (or `clang` / any standard C90 compiler):

```bash
make
```

Or manually:

```bash
gcc -std=c90 -pedantic -Wall -Wextra -Werror -O2 say.c -lm -o say
```

---

## Usage

```text
Usage: ./say [options] ["text to speak"]

Options:
  -o <file>      Output WAV file (default: output.wav)
  -p <pitch>     Base pitch F0 in Hz (default: 120, range: 40-400)
  -s <speed>     Speech speed multiplier (default: 0.80, range: 0.3-3.0)
  -t <tone>      Formant frequency scale / tone (default: 1.0, range: 0.5-2.0)
  -ph            Direct phonetic mode (ARPAbet phoneme sequence)
  -v             Verbose output (show phonetic breakdown)
  -h, --help     Show help message
  --license      Show Public Domain dedication notice
```

### Examples

#### 1. Basic Text to Speech
```bash
./say "Hello world! I am the Commodore Amiga 500 computer."
```

#### 2. Specifying Output WAV File
```bash
./say -o welcome.wav "Welcome to the Amiga Workbench."
```

#### 3. Pitch, Speed, and Tone Controls
```bash
# Higher pitch (female / child voice)
./say -p 175 -s 1.1 -t 1.15 -o robotic.wav "Voice pitch and tone are adjustable."

# Deep pitch (male / bass voice)
./say -p 85 -s 0.9 -t 0.9 -o deep.wav "Greetings, human."
```

#### 4. Automatic Number Expansion
```bash
./say "Commodore Amiga 500 was released in 1985 with 512 kilobytes of RAM."
```

#### 5. Question Intonation
```bash
./say "Can the computer really speak and understand speech?"
```

#### 6. Piped Input (Standard In)
```bash
echo "Text received from standard input stream." | ./say -o pipe.wav
```

#### 7. Direct ARPAbet Phonetic Mode
```bash
./say -ph -o phonemes.wav "HH EH L OW . W ER L D ."
```

---

## Makefile Targets

| Target | Description |
| :--- | :--- |
| `make` | Builds the `say` executable with strict ANSI C90 flags. |
| `make test` | Runs the full automated verification test suite (format, headers, numbers, phonemes, pipes). |
| `make play` | Synthesizes and plays the default introduction demo using system audio (`aplay`, `paplay`, `ffplay`). |
| `make play-amiga` | Plays a demo showcasing Amiga hardware terminology. |
| `make play-question` | Plays a demo demonstrating rising question intonation. |
| `make play-fast` | Plays high-speed speech synthesis. |
| `make play-deep` | Plays deep robotic voice synthesis. |
| `make clean` | Removes compiled binaries, object files, and generated WAV files. |

---

## Supported Phonetic Inventory (ARPAbet)

| Type | Phonemes |
| :--- | :--- |
| **Vowels** | `IY` (*beet*), `IH` (*bit*), `EH` (*bet*), `AE` (*bat*), `AA` (*hot*), `AH` (*cut*), `AO` (*all*), `UH` (*put*), `UW` (*boot*), `ER` (*bird*), `AX` (*about*) |
| **Diphthongs** | `EY` (*say*), `AY` (*five*), `OY` (*boy*), `OW` (*go*), `AW` (*now*), `YU` (*you*) |
| **Liquids / Glides** | `W` (*we*), `Y` (*yes*), `R` (*red*), `L` (*let*) |
| **Nasals** | `M` (*me*), `N` (*no*), `NG` (*sing*) |
| **Voiced Fricatives** | `V` (*voice*), `DH` (*this*), `Z` (*zero*), `ZH` (*vision*) |
| **Voiceless Fricatives** | `F` (*four*), `TH` (*think*), `S` (*say*), `SH` (*she*), `HH` (*hello*) |
| **Voiced Plosives** | `B` (*byte*), `D` (*day*), `G` (*good*) |
| **Voiceless Plosives** | `P` (*paul*), `T` (*two*), `K` (*computer*) |
| **Affricates** | `CH` (*church*), `JH` (*jump*) |
| **Pauses** | `PA_C` (*comma*), `PA_P` (*period*), `PA_Q` (*question*), `PA_E` (*exclamation*) |

---

## License

This software and source code is dedicated to the **Public Domain** under **Creative Commons CC0 1.0 Universal** / **The Unlicense**.

You may copy, modify, distribute, perform, and use the work for any purpose, including commercial applications, without asking permission.
