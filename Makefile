# =============================================================================
# Makefile for Amiga 500 Text-to-Speech Synthesizer (say / tts)
# Standard: ANSI C90 (C89)
# License:  Public Domain (CC0 1.0 Universal / Unlicense)
# =============================================================================

CC      ?= gcc
CFLAGS  ?= -std=c90 -pedantic -Wall -Wextra -Werror -O2
LDFLAGS ?= -lm
TARGET   = say
SRC      = say.c

# Audio player detection (aplay, paplay, pw-play, ffplay)
PLAYER ?= $(shell which aplay 2>/dev/null || which paplay 2>/dev/null || which pw-play 2>/dev/null || which ffplay 2>/dev/null || echo "true")
PLAYER_OPTS ?= $(if $(findstring ffplay,$(PLAYER)),-nodisp -autoexit -loglevel quiet,)

.PHONY: all clean test play play-amiga play-question play-fast play-retro help

# Default target
all: $(TARGET)

# Compile binary
$(TARGET): $(SRC)
	@echo "==> Compiling $(TARGET) with ANSI C90 strict flags..."
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(TARGET)
	@echo "==> Built $(TARGET) successfully."

# Alias for tts
tts: $(TARGET)

# Run automated test suite
test: $(TARGET)
	@echo "================================================================="
	@echo "               Running Amiga TTS Test Suite                     "
	@echo "================================================================="
	@echo "[Test 1/6] Synthesizing standard introduction..."
	./$(TARGET) -v -o test_intro.wav "Hello world! I am the Amiga 500 computer."
	@echo ""
	@echo "[Test 2/6] Testing number expansion (500, 1985, 42)..."
	./$(TARGET) -o test_numbers.wav "Commodore Amiga 500 was released in 1985 with 42 special features."
	@echo ""
	@echo "[Test 3/6] Testing question intonation (rising pitch)..."
	./$(TARGET) -p 130 -o test_question.wav "Can the computer really speak?"
	@echo ""
	@echo "[Test 4/6] Testing direct ARPAbet phonetic mode..."
	./$(TARGET) -ph -o test_phonemes.wav "HH EH L OW PA_W W ER L D PA_P"
	@echo ""
	@echo "[Test 5/6] Testing standard input stream (pipe)..."
	echo "Speech synthesis from piped standard input." | ./$(TARGET) -o test_pipe.wav
	@echo ""
	@echo "[Test 6/6] Verifying WAV format specs (8000 Hz, 8-bit, Mono)..."
	@python3 -c "import wave; [exec('with wave.open(f, \"rb\") as w:\n assert w.getframerate()==8000 and w.getsampwidth()==1 and w.getnchannels()==1\n print(f\"  [PASS] {f:<18} : 8kHz, 8-bit, Mono, {w.getnframes()/8000:.2f}s\")') for f in ['test_intro.wav', 'test_numbers.wav', 'test_question.wav', 'test_phonemes.wav', 'test_pipe.wav']]"
	@echo "================================================================="
	@echo "               All tests passed successfully!                   "
	@echo "================================================================="

# Generate and play iconic Amiga phrase
play: $(TARGET)
	@echo "==> Generating demo speech..."
	./$(TARGET) -o demo.wav "Hello world! I am the Commodore Amiga 500 computer."
	@echo "==> Playing demo.wav with $(PLAYER)..."
	@$(PLAYER) $(PLAYER_OPTS) demo.wav || echo "Audio player exited or sound device unavailable."

# Play Amiga introduction
play-amiga: $(TARGET)
	./$(TARGET) -o amiga_demo.wav "Welcome to the Amiga 500. Guru meditation error resolved."
	@$(PLAYER) $(PLAYER_OPTS) amiga_demo.wav || true

# Play question with rising inflection
play-question: $(TARGET)
	./$(TARGET) -p 140 -o question_demo.wav "Are you ready to create retro software?"
	@$(PLAYER) $(PLAYER_OPTS) question_demo.wav || true

# Play fast speech
play-fast: $(TARGET)
	./$(TARGET) -s 1.6 -p 150 -o fast_demo.wav "Fast speech synthesis on the Motorola sixty eight thousand processor."
	@$(PLAYER) $(PLAYER_OPTS) fast_demo.wav || true

# Play deep robotic voice
play-deep: $(TARGET)
	./$(TARGET) -p 80 -t 0.9 -o deep_demo.wav "I am a deep robotic voice from the past."
	@$(PLAYER) $(PLAYER_OPTS) deep_demo.wav || true

# Clean build artifacts and test wav files
clean:
	@echo "==> Cleaning build artifacts and test files..."
	rm -f $(TARGET) tts *.o *.wav
	@echo "==> Done."

# Display help
help:
	@echo "Available make targets:"
	@echo "  make               - Build the 'say' executable (C90, strict flags)"
	@echo "  make test          - Run full test suite and verify 8kHz 8-bit WAV headers"
	@echo "  make play          - Generate and play the classic Amiga demo speech"
	@echo "  make play-amiga    - Generate and play Amiga welcome phrase"
	@echo "  make play-question - Demonstrate question intonation pitch rise"
	@echo "  make play-fast     - Demonstrate high-speed speech"
	@echo "  make play-deep     - Demonstrate deep robotic pitch"
	@echo "  make clean         - Remove binary and generated .wav files"
	@echo "  make help          - Display this help message"
