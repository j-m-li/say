/* =========================================================================
   SAY - Amiga 500 Text-to-Speech Synthesizer (ANSI C90)
   Dedicated to the Public Domain under CC0 1.0 Universal / Unlicense.
   ========================================================================= */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 8000
#define FRAME_MS 10
#define SAMPLES_PER_FRAME (SAMPLE_RATE * FRAME_MS / 1000) /* 80 samples per frame */

/* =========================================================================
   Phonetic Inventory & Definitions
   ========================================================================= */
enum PhonemeCode {
    PH_NONE = 0,
    /* Vowels */
    PH_IY, PH_IH, PH_EH, PH_AE, PH_AA, PH_AH, PH_AO, PH_UH, PH_UW, PH_ER, PH_AX,
    /* Diphthongs */
    PH_EY, PH_AY, PH_OY, PH_OW, PH_AW, PH_YU,
    /* Liquids & Semivowels */
    PH_W, PH_Y, PH_R, PH_L,
    /* Nasals */
    PH_M, PH_N, PH_NG,
    /* Voiced Fricatives */
    PH_V, PH_DH, PH_Z, PH_ZH,
    /* Voiceless Fricatives */
    PH_F, PH_TH, PH_S, PH_SH, PH_HH,
    /* Voiced Plosives */
    PH_B, PH_D, PH_G,
    /* Voiceless Plosives */
    PH_P, PH_T, PH_K,
    /* Affricates */
    PH_CH, PH_JH,
    /* Pauses */
    PH_PA_WORD, PH_PA_COMMA, PH_PA_PERIOD, PH_PA_QMARK, PH_PA_EXCL,
    PH_COUNT
};

enum NoiseType {
    NOISE_NONE = 0,
    NOISE_RESONANT,
    NOISE_ASPIRATION
};

struct PhonemeDef {
    const char *name;
    int f1, f2, f3;             /* Formant frequencies (Hz) */
    int f1_end, f2_end, f3_end; /* End formant frequencies for diphthongs (Hz) */
    int bw1, bw2, bw3;          /* Formant bandwidths (Hz) */
    double a1, a2, a3;          /* Formant amplitudes (0.0 - 1.0) */
    double voice_amp;           /* Voicing amplitude (0.0 - 1.0) */
    double noise_amp;           /* Noise amplitude (0.0 - 1.0) */
    enum NoiseType noise_type;  /* Noise filtering mode */
    int noise_fc;               /* Noise filter center frequency (Hz) */
    int noise_bw;               /* Noise filter bandwidth (Hz) */
    int base_frames;            /* Base duration in frames (10ms each) */
    int is_vowel;
    int is_plosive;
    int is_voiced_plosive;
    int is_pause;
    int f2_locus;               /* Consonant F2 locus for coarticulation (Hz) */
};

/* Resonant acoustic table: Consonants use vocal tract shaping without raw noise */
static const struct PhonemeDef PHONEME_TABLE[PH_COUNT] = {
    /* PH_NONE */
    {"NONE", 0, 0, 0,  0, 0, 0,  0, 0, 0,  0.0, 0.0, 0.0,  0.0, 0.0, NOISE_NONE, 0, 0, 1, 0, 0, 0, 1, 0},

    /* VOWELS */
    /* PH_IY ("beet", "see") */
    {"IY",  280, 2250, 2900,  280, 2250, 2900,  60,  90, 150,  1.0, 0.70, 0.40,  1.0, 0.0, NOISE_NONE, 0, 0, 10, 1, 0, 0, 0, 2250},
    /* PH_IH ("bit", "sit") */
    {"IH",  400, 1950, 2600,  400, 1950, 2600,  70, 100, 160,  1.0, 0.80, 0.40,  1.0, 0.0, NOISE_NONE, 0, 0,  8, 1, 0, 0, 0, 1950},
    /* PH_EH ("bet", "red") */
    {"EH",  550, 1800, 2550,  550, 1800, 2550,  70, 100, 160,  1.0, 0.80, 0.40,  1.0, 0.0, NOISE_NONE, 0, 0,  9, 1, 0, 0, 0, 1800},
    /* PH_AE ("bat", "cat") */
    {"AE",  690, 1650, 2450,  690, 1650, 2450,  70, 100, 160,  1.0, 0.80, 0.35,  1.0, 0.0, NOISE_NONE, 0, 0, 10, 1, 0, 0, 0, 1650},
    /* PH_AA ("father", "hot") */
    {"AA",  750, 1100, 2450,  750, 1100, 2450,  80, 100, 160,  1.0, 0.70, 0.30,  1.0, 0.0, NOISE_NONE, 0, 0, 10, 1, 0, 0, 0, 1100},
    /* PH_AH ("but", "cut") */
    {"AH",  640, 1250, 2400,  640, 1250, 2400,  80, 100, 160,  1.0, 0.70, 0.30,  1.0, 0.0, NOISE_NONE, 0, 0,  8, 1, 0, 0, 0, 1250},
    /* PH_AO ("bought", "all") */
    {"AO",  580,  880, 2400,  580,  880, 2400,  80, 100, 160,  1.0, 0.60, 0.25,  1.0, 0.0, NOISE_NONE, 0, 0, 10, 1, 0, 0, 0,  880},
    /* PH_UH ("book", "put") */
    {"UH",  450, 1050, 2250,  450, 1050, 2250,  80, 100, 160,  1.0, 0.60, 0.25,  1.0, 0.0, NOISE_NONE, 0, 0,  8, 1, 0, 0, 0, 1050},
    /* PH_UW ("boot", "two") */
    {"UW",  310,  870, 2250,  310,  870, 2250,  70,  90, 150,  1.0, 0.50, 0.20,  1.0, 0.0, NOISE_NONE, 0, 0,  9, 1, 0, 0, 0,  870},
    /* PH_ER ("bird", "her", "computer") */
    {"ER",  490, 1350, 1690,  490, 1350, 1690,  80, 100, 120,  1.0, 0.80, 0.60,  1.0, 0.0, NOISE_NONE, 0, 0, 11, 1, 0, 0, 0, 1350},
    /* PH_AX ("about", "the") */
    {"AX",  500, 1450, 2450,  500, 1450, 2450,  80, 100, 160,  0.85, 0.60, 0.30, 0.9, 0.0, NOISE_NONE, 0, 0,  7, 1, 0, 0, 0, 1450},

    /* DIPHTHONGS */
    /* PH_EY ("say", "make", "day") */
    {"EY",  550, 1800, 2550,  360, 2150, 2700,  70, 100, 160,  1.0, 0.80, 0.40,  1.0, 0.0, NOISE_NONE, 0, 0, 12, 1, 0, 0, 0, 1800},
    /* PH_AY ("high", "five", "my") */
    {"AY",  750, 1100, 2450,  360, 2100, 2700,  80, 100, 160,  1.0, 0.75, 0.35,  1.0, 0.0, NOISE_NONE, 0, 0, 13, 1, 0, 0, 0, 1100},
    /* PH_OY ("boy", "voice", "coin") */
    {"OY",  580,  880, 2400,  360, 2050, 2600,  80, 100, 160,  1.0, 0.70, 0.30,  1.0, 0.0, NOISE_NONE, 0, 0, 13, 1, 0, 0, 0,  880},
    /* PH_OW ("go", "boat", "home") */
    {"OW",  580,  900, 2400,  380,  800, 2200,  80, 100, 160,  1.0, 0.65, 0.25,  1.0, 0.0, NOISE_NONE, 0, 0, 12, 1, 0, 0, 0,  900},
    /* PH_AW ("now", "out", "sound") */
    {"AW",  750, 1150, 2450,  420,  850, 2250,  80, 100, 160,  1.0, 0.70, 0.30,  1.0, 0.0, NOISE_NONE, 0, 0, 13, 1, 0, 0, 0, 1150},
    /* PH_YU ("you", "cube", "music") */
    {"YU",  280, 2250, 2900,  310,  870, 2250,  60,  90, 150,  1.0, 0.70, 0.35,  1.0, 0.0, NOISE_NONE, 0, 0, 12, 1, 0, 0, 0, 2250},

    /* LIQUIDS & SEMIVOWELS */
    /* PH_W ("we", "one") */
    {"W",   300,  650, 2200,  300,  650, 2200,  70,  90, 160,  0.85, 0.40, 0.20, 0.9, 0.0, NOISE_NONE, 0, 0,  7, 0, 0, 0, 0,  650},
    /* PH_Y ("yes", "yet") */
    {"Y",   280, 2250, 2850,  280, 2250, 2850,  60,  90, 150,  0.90, 0.70, 0.40, 0.9, 0.0, NOISE_NONE, 0, 0,  7, 0, 0, 0, 0, 2250},
    /* PH_R ("red", "run") */
    {"R",   350, 1150, 1650,  350, 1150, 1650,  70,  90, 120,  0.90, 0.70, 0.50, 0.9, 0.0, NOISE_NONE, 0, 0,  7, 0, 0, 0, 0, 1150},
    /* PH_L ("let", "amiga") */
    {"L",   380, 1100, 2700,  380, 1100, 2700,  70,  90, 160,  0.85, 0.60, 0.30, 0.9, 0.0, NOISE_NONE, 0, 0,  7, 0, 0, 0, 0, 1100},

    /* NASALS */
    /* PH_M ("me", "amiga") */
    {"M",   280,  900, 2200,  280,  900, 2200,  60, 120, 180,  0.70, 0.25, 0.15, 0.75, 0.0, NOISE_NONE, 0, 0,  7, 0, 0, 0, 0,  900},
    /* PH_N ("no", "nine") */
    {"N",   280, 1500, 2500,  280, 1500, 2500,  60, 120, 180,  0.70, 0.30, 0.15, 0.75, 0.0, NOISE_NONE, 0, 0,  7, 0, 0, 0, 0, 1500},
    /* PH_NG ("sing", "thing") */
    {"NG",  280, 2000, 2600,  280, 2000, 2600,  60, 120, 180,  0.70, 0.35, 0.15, 0.75, 0.0, NOISE_NONE, 0, 0,  7, 0, 0, 0, 0, 2000},

    /* VOICED FRICATIVES (Pure formant resonance, zero white noise) */
    /* PH_V ("voice", "five") */
    {"V",   280, 1100, 2400,  280, 1100, 2400,  70, 100, 160,  0.85, 0.35, 0.20, 0.90, 0.0,  NOISE_NONE, 0, 0,  7, 0, 0, 0, 0, 1100},
    /* PH_DH ("the", "that", "this", "there", "with") */
    {"DH",  300, 1400, 2400,  300, 1400, 2400,  70, 100, 160,  0.85, 0.40, 0.20, 0.90, 0.0,  NOISE_NONE, 0, 0,  7, 0, 0, 0, 0, 1450},
    /* PH_Z ("zero", "is", "please") */
    {"Z",   280, 1500, 2500,  280, 1500, 2500,  70,  90, 150,  0.70, 0.30, 0.20, 0.75, 0.06, NOISE_RESONANT, 3300, 300,  6, 0, 0, 0, 0, 1500},
    /* PH_ZH ("pleasure", "vision") */
    {"ZH",  300, 1800, 2600,  300, 1800, 2600,  70,  90, 150,  0.70, 0.35, 0.20, 0.75, 0.05, NOISE_RESONANT, 2400, 400,  6, 0, 0, 0, 0, 1800},

    /* VOICELESS FRICATIVES (Anchored with soft formant voicing + subtle oral friction) */
    /* PH_F ("four", "first", "five") */
    {"F",   280, 1100, 2200,  280, 1100, 2200,  80, 100, 160,  0.50, 0.30, 0.20, 0.50,  0.06, NOISE_ASPIRATION, 1100, 1500,  5, 0, 0, 0, 0, 1100},
    /* PH_TH ("think", "three", "math", "thing") */
    {"TH",  280, 1400, 2400,  280, 1400, 2400,  80, 100, 160,  0.50, 0.30, 0.20, 0.50,  0.05, NOISE_ASPIRATION, 1400, 1500,  5, 0, 0, 0, 0, 1450},
    /* PH_S ("say", "sound", "six", "speak") */
    {"S",   280, 1600, 2600,  280, 1600, 2600,  70, 100, 150,  0.20, 0.30, 0.20, 0.0,  0.10, NOISE_RESONANT, 3300, 300,  6, 0, 0, 0, 0, 1500},
    /* PH_SH ("she", "speech") */
    {"SH",  300, 1800, 2600,  300, 1800, 2600,  70,  90, 150,  0.20, 0.30, 0.20, 0.0,  0.10, NOISE_RESONANT, 2400, 450,  6, 0, 0, 0, 0, 1800},
    /* PH_HH ("hello", "hundred", "he") */
    {"HH",  500, 1500, 2500,  500, 1500, 2500,  80, 100, 160,  0.40, 0.30, 0.20, 0.0,  0.08, NOISE_ASPIRATION, 1500, 2000,  6, 0, 0, 0, 0, 1500},

    /* VOICED PLOSIVES (Pure voiced bar + smooth formant locus sweep) */
    /* PH_B ("byte", "bit") */
    {"B",   180,  900, 2200,  180,  900, 2200,  60, 100, 160,  0.50, 0.20, 0.10, 0.60, 0.0,   NOISE_NONE, 0, 0,  4, 0, 1, 1, 0,  900},
    /* PH_D ("dos", "day", "world") */
    {"D",   180, 1700, 2500,  180, 1700, 2500,  60, 100, 160,  0.50, 0.20, 0.10, 0.60, 0.0,   NOISE_NONE, 0, 0,  4, 0, 1, 1, 0, 1700},
    /* PH_G ("good", "game") */
    {"G",   180, 2100, 2600,  180, 2100, 2600,  60, 100, 160,  0.50, 0.20, 0.10, 0.60, 0.0,   NOISE_NONE, 0, 0,  4, 0, 1, 1, 0, 2100},

    /* VOICELESS PLOSIVES (Short 20ms closure + gentle 10ms burst) */
    /* PH_P ("paul", "computer") */
    {"P",   300,  900, 2200,  300,  900, 2200,  70, 100, 160,  0.0,  0.0,  0.0,  0.0,  0.10, NOISE_RESONANT, 1000,  600,  4, 0, 1, 0, 0,  900},
    /* PH_T ("two", "text") */
    {"T",   300, 1700, 2500,  300, 1700, 2500,  70, 100, 160,  0.0,  0.0,  0.0,  0.0,  0.12, NOISE_RESONANT, 3300,  400,  4, 0, 1, 0, 0, 1700},
    /* PH_K ("commodore", "talk") */
    {"K",   300, 2100, 2600,  300, 2100, 2600,  70, 100, 160,  0.0,  0.0,  0.0,  0.0,  0.10, NOISE_RESONANT, 1900,  600,  4, 0, 1, 0, 0, 2100},

    /* AFFRICATES */
    /* PH_CH ("church", "speech") */
    {"CH",  300, 1800, 2600,  300, 1800, 2600,  70,  90, 150,  0.0,  0.0,  0.0,  0.0,  0.12, NOISE_RESONANT, 2600,  500,  7, 0, 0, 0, 0, 1800},
    /* PH_JH ("jump", "amiga") */
    {"JH",  300, 1800, 2600,  300, 1800, 2600,  70,  90, 150,  0.60, 0.30, 0.20, 0.70, 0.06, NOISE_RESONANT, 2400,  500,  7, 0, 0, 0, 0, 1800},

    /* PAUSES */
    /* PH_PA_WORD (pause between words) */
    {"PA_W", 0, 0, 0,  0, 0, 0,  0, 0, 0,  0.0, 0.0, 0.0,  0.0, 0.0, NOISE_NONE, 0, 0,  4, 0, 0, 0, 1, 0},
    /* PH_PA_COMMA (comma pause) */
    {"PA_C", 0, 0, 0,  0, 0, 0,  0, 0, 0,  0.0, 0.0, 0.0,  0.0, 0.0, NOISE_NONE, 0, 0, 10, 0, 0, 0, 1, 0},
    /* PH_PA_PERIOD (period pause) */
    {"PA_P", 0, 0, 0,  0, 0, 0,  0, 0, 0,  0.0, 0.0, 0.0,  0.0, 0.0, NOISE_NONE, 0, 0, 14, 0, 0, 0, 1, 0},
    /* PH_PA_QMARK (question mark pause) */
    {"PA_Q", 0, 0, 0,  0, 0, 0,  0, 0, 0,  0.0, 0.0, 0.0,  0.0, 0.0, NOISE_NONE, 0, 0, 14, 0, 0, 0, 1, 0},
    /* PH_PA_EXCL (exclamation pause) */
    {"PA_E", 0, 0, 0,  0, 0, 0,  0, 0, 0,  0.0, 0.0, 0.0,  0.0, 0.0, NOISE_NONE, 0, 0, 14, 0, 0, 0, 1, 0}
};

static enum PhonemeCode phoneme_from_name(const char *name) {
    int i;
    for (i = 1; i < PH_COUNT; ++i) {
        if (strcmp(PHONEME_TABLE[i].name, name) == 0) {
            return (enum PhonemeCode)i;
        }
    }
    return PH_NONE;
}

/* =========================================================================
   Dictionary of Common & Irregular English Words
   ========================================================================= */
struct DictEntry {
    const char *word;
    const char *phonemes;
};

static const struct DictEntry DICTIONARY[] = {
    {"A", "EY"},
    {"ABOUT", "AH B AW T"},
    {"AFTER", "AE F T ER"},
    {"AGAIN", "AH G EH N"},
    {"AGNUS", "AE G N AH S"},
    {"ALL", "AO L"},
    {"ALSO", "AO L S OW"},
    {"ALWAYS", "AO L W EY Z"},
    {"AM", "AE M"},
    {"AMIGA", "AH M IY G AX"},
    {"AMIGADOS", "AH M IY G AX D AA S"},
    {"AN", "AE N"},
    {"AND", "AE N D"},
    {"ANY", "EH N IY"},
    {"ARE", "AA R"},
    {"AS", "AE Z"},
    {"AT", "AE T"},
    {"BE", "B IY"},
    {"BECAUSE", "B IH K AO Z"},
    {"BEEN", "B IH N"},
    {"BEFORE", "B IH F AO R"},
    {"BEST", "B EH S T"},
    {"BETTER", "B EH T ER"},
    {"BIG", "B IH G"},
    {"BIT", "B IH T"},
    {"BITS", "B IH T S"},
    {"BYTE", "B AY T"},
    {"BYTES", "B AY T S"},
    {"BUT", "B AH T"},
    {"BY", "B AY"},
    {"CALL", "K AO L"},
    {"CALLED", "K AO L D"},
    {"CAN", "K AE N"},
    {"CANT", "K AE N T"},
    {"CAN'T", "K AE N T"},
    {"CHIP", "CH IH P"},
    {"CHIPS", "CH IH P S"},
    {"COME", "K AH M"},
    {"COMMODORE", "K AA M AH D AO R"},
    {"COMPUTER", "K AH M P Y UW T ER"},
    {"COMPUTERS", "K AH M P Y UW T ER Z"},
    {"COULD", "K UH D"},
    {"DAY", "D EY"},
    {"DENISE", "D EH N IY S"},
    {"DID", "D IH D"},
    {"DISC", "D IH S K"},
    {"DISK", "D IH S K"},
    {"DO", "D UW"},
    {"DOES", "D AH Z"},
    {"DONE", "D AH N"},
    {"DONT", "D OW N T"},
    {"DON'T", "D OW N T"},
    {"DOS", "D AA S"},
    {"DOWN", "D AW N"},
    {"EACH", "IY CH"},
    {"EIGHT", "EY T"},
    {"EIGHTEEN", "EY T IY N"},
    {"EIGHTY", "EY T IY"},
    {"ELEVEN", "IH L EH V AH N"},
    {"EVEN", "IY V AH N"},
    {"EVERY", "EH V R IY"},
    {"FAST", "F AE S T"},
    {"FIFTEEN", "F IH F T IY N"},
    {"FIFTY", "F IH F T IY"},
    {"FIND", "F AY N D"},
    {"FIRST", "F ER S T"},
    {"FIVE", "F AY V"},
    {"FOR", "F AO R"},
    {"FOUR", "F AO R"},
    {"FOURTEEN", "F AO R T IY N"},
    {"FORTY", "F AO R T IY"},
    {"FROM", "F R AH M"},
    {"GAME", "G EY M"},
    {"GAMES", "G EY M Z"},
    {"GET", "G EH T"},
    {"GIVE", "G IH V"},
    {"GO", "G OW"},
    {"GOOD", "G UH D"},
    {"GRAPHICS", "G R AE F IH K S"},
    {"GREAT", "G R EY T"},
    {"GURU", "G UH R UW"},
    {"HAD", "HH AE D"},
    {"HARDWARE", "HH AA R D W EH R"},
    {"HAS", "HH AE Z"},
    {"HAVE", "HH AE V"},
    {"HE", "HH IY"},
    {"HELLO", "HH EH L OW"},
    {"HER", "HH ER"},
    {"HERE", "HH IH R"},
    {"HIGH", "HH AY"},
    {"HIM", "HH IH M"},
    {"HIS", "HH IH Z"},
    {"HOME", "HH OW M"},
    {"HOW", "HH AW"},
    {"HUNDRED", "HH AH N D R AH D"},
    {"I", "AY"},
    {"IF", "IH F"},
    {"IM", "AY M"},
    {"I'M", "AY M"},
    {"IN", "IH N"},
    {"INTO", "IH N T UW"},
    {"IS", "IH Z"},
    {"IT", "IH T"},
    {"ITS", "IH T S"},
    {"IT'S", "IH T S"},
    {"JUST", "JH AH S T"},
    {"KICKSTART", "K IH K S T AA R T"},
    {"KNOW", "N OW"},
    {"KNOWN", "N OW N"},
    {"LANGUAGE", "L AE NG G W IH JH"},
    {"LIKE", "L AY K"},
    {"LITTLE", "L IH T AH L"},
    {"LIVE", "L IH V"},
    {"LONG", "L AO NG"},
    {"LOOK", "L UH K"},
    {"MADE", "M EY D"},
    {"MAKE", "M EY K"},
    {"MANY", "M EH N IY"},
    {"MAY", "M EY"},
    {"ME", "M IY"},
    {"MEDITATION", "M EH D IH T EY SH AH N"},
    {"MEMORY", "M EH M ER IY"},
    {"MIGHT", "M AY T"},
    {"MILLION", "M IH L Y AH N"},
    {"MORE", "M AO R"},
    {"MOST", "M OW S T"},
    {"MOTOROLA", "M OW T ER OW L AX"},
    {"MUCH", "M AH CH"},
    {"MUSIC", "M Y UW Z IH K"},
    {"MY", "M AY"},
    {"NINE", "N AY N"},
    {"NINETEEN", "N AY N T IY N"},
    {"NINETY", "N AY N T IY"},
    {"NO", "N OW"},
    {"NOT", "N AA T"},
    {"NOW", "N AW"},
    {"NUMBER", "N AH M B ER"},
    {"OF", "AH V"},
    {"OFF", "AO F"},
    {"OLD", "OW L D"},
    {"ON", "AA N"},
    {"ONE", "W AH N"},
    {"ONLY", "OW N L IY"},
    {"OR", "AO R"},
    {"OTHER", "AH DH ER"},
    {"OUR", "AW ER"},
    {"OUT", "AW T"},
    {"OVER", "OW V ER"},
    {"PAULA", "P AO L AX"},
    {"PEOPLE", "P IY P AH L"},
    {"PROGRAM", "P R OW G R AE M"},
    {"PROGRAMS", "P R OW G R AE M Z"},
    {"PUT", "P UH T"},
    {"RAM", "R AE M"},
    {"RETRO", "R EH T R OW"},
    {"RIGHT", "R AY T"},
    {"ROBOT", "R OW B AA T"},
    {"ROM", "R AA M"},
    {"SAID", "S EH D"},
    {"SAME", "S EY M"},
    {"SAY", "S EY"},
    {"SAYS", "S EH Z"},
    {"SEE", "S IY"},
    {"SEVEN", "S EH V AH N"},
    {"SEVENTEEN", "S EH V AH N T IY N"},
    {"SEVENTY", "S EH V AH N T IY"},
    {"SHE", "SH IY"},
    {"SHOULD", "SH UH D"},
    {"SIX", "S IH K S"},
    {"SIXTEEN", "S IH K S T IY N"},
    {"SIXTY", "S IH K S T IY"},
    {"SMALL", "S M AO L"},
    {"SO", "S OW"},
    {"SOFTWARE", "S AO F T W EH R"},
    {"SOME", "S AH M"},
    {"SOUND", "S AW N D"},
    {"SOUNDS", "S AW N D Z"},
    {"SPEAK", "S P IY K"},
    {"SPEECH", "S P IY CH"},
    {"STILL", "S T IH L"},
    {"SUCH", "S AH CH"},
    {"SYSTEM", "S IH S T AH M"},
    {"TALK", "T AO K"},
    {"TELL", "T EH L"},
    {"TEN", "T EH N"},
    {"TEXT", "T EH K S T"},
    {"THAN", "DH AE N"},
    {"THAT", "DH AE T"},
    {"THE", "DH AX"},
    {"THEIR", "DH EH R"},
    {"THEM", "DH EH M"},
    {"THEN", "DH EH N"},
    {"THERE", "DH EH R"},
    {"THESE", "DH IY Z"},
    {"THEY", "DH EY"},
    {"THING", "TH IH NG"},
    {"THINGS", "TH IH NG Z"},
    {"THINK", "TH IH NG K"},
    {"THIRD", "TH ER D"},
    {"THIRTEEN", "TH ER T IY N"},
    {"THIRTY", "TH ER T IY"},
    {"THIS", "DH IH S"},
    {"THOSE", "DH OW Z"},
    {"THOUSAND", "TH AW Z AH N D"},
    {"THREE", "TH R IY"},
    {"THROUGH", "TH R UW"},
    {"TIME", "T AY M"},
    {"TO", "T UW"},
    {"TODAY", "T AH D EY"},
    {"TOO", "T UW"},
    {"TTS", "T IY T IY EH S"},
    {"TWELVE", "T W EH L V"},
    {"TWENTY", "T W EH N T IY"},
    {"TWO", "T UW"},
    {"UNDER", "AH N D ER"},
    {"UP", "AH P"},
    {"US", "AH S"},
    {"USE", "Y UW Z"},
    {"VERY", "V EH R IY"},
    {"VOICE", "V OY S"},
    {"WANT", "W AA N T"},
    {"WAS", "W AA Z"},
    {"WATER", "W AA T ER"},
    {"WAY", "W EY"},
    {"WE", "W IY"},
    {"WELL", "W EH L"},
    {"WENT", "W EH N T"},
    {"WERE", "W ER"},
    {"WHAT", "W AA T"},
    {"WHEN", "W EH N"},
    {"WHERE", "W EH R"},
    {"WHICH", "W IH CH"},
    {"WHILE", "W AY L"},
    {"WHO", "HH UW"},
    {"WHY", "W AY"},
    {"WILL", "W IH L"},
    {"WITH", "W IH DH"},
    {"WORD", "W ER D"},
    {"WORDS", "W ER D Z"},
    {"WORK", "W ER K"},
    {"WORKBENCH", "W ER K B EH N CH"},
    {"WORLD", "W ER L D"},
    {"WOULD", "W UH D"},
    {"WRITE", "R AY T"},
    {"YEAR", "Y IH R"},
    {"YEARS", "Y IH R Z"},
    {"YES", "Y EH S"},
    {"YOU", "Y UW"},
    {"YOUR", "Y AO R"},
    {"ZERO", "Z IH R OW"}
};

static const int NUM_DICT_ENTRIES = sizeof(DICTIONARY) / sizeof(DICTIONARY[0]);

static const char* lookup_dictionary(const char *word) {
    int left = 0, right = NUM_DICT_ENTRIES - 1;
    while (left <= right) {
        int mid = (left + right) / 2;
        int cmp = strcmp(word, DICTIONARY[mid].word);
        if (cmp == 0) return DICTIONARY[mid].phonemes;
        if (cmp < 0) right = mid - 1;
        else left = mid + 1;
    }
    return NULL;
}

/* =========================================================================
   Letter-to-Sound Rule Engine (English G2P)
   ========================================================================= */
static int is_vowel_c(char c) {
    return (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y');
}

static int is_front_vowel_c(char c) {
    return (c == 'E' || c == 'I' || c == 'Y');
}

static int is_voiced_cons_c(char c) {
    return (c == 'B' || c == 'D' || c == 'G' || c == 'J' || c == 'L' ||
            c == 'M' || c == 'N' || c == 'R' || c == 'V' || c == 'W' || c == 'Z');
}

static void append_ph(char *out, size_t out_max, const char *ph) {
    size_t len = strlen(out);
    if (len > 0 && out[len - 1] != ' ') {
        if (len + 1 < out_max) {
            out[len] = ' ';
            out[len + 1] = '\0';
            len++;
        }
    }
    if (len + strlen(ph) < out_max) {
        strcat(out, ph);
    }
}

static void translate_word_rules(const char *word, char *out, size_t out_max) {
    int len = (int)strlen(word);
    int i = 0;
    out[0] = '\0';

    if (len == 0) return;

    if (len == 1) {
        if (word[0] == 'A') { append_ph(out, out_max, "EY"); return; }
        if (word[0] == 'I') { append_ph(out, out_max, "AY"); return; }
        if (word[0] == 'O') { append_ph(out, out_max, "OW"); return; }
    }

    while (i < len) {
        int rem = len - i;

        /* Suffixes at word end */
        if (rem == 4 && strncmp(&word[i], "TION", 4) == 0) {
            append_ph(out, out_max, "SH AH N"); i += 4; continue;
        }
        if (rem == 4 && strncmp(&word[i], "SION", 4) == 0) {
            append_ph(out, out_max, "ZH AH N"); i += 4; continue;
        }
        if (rem == 4 && strncmp(&word[i], "TURE", 4) == 0) {
            append_ph(out, out_max, "CH ER"); i += 4; continue;
        }
        if (rem == 4 && strncmp(&word[i], "ABLE", 4) == 0) {
            append_ph(out, out_max, "AH B AH L"); i += 4; continue;
        }
        if (rem == 4 && strncmp(&word[i], "SIBLE", 4) == 0) {
            append_ph(out, out_max, "IH B AH L"); i += 4; continue;
        }
        if (rem == 4 && strncmp(&word[i], "LESS", 4) == 0) {
            append_ph(out, out_max, "L AH S"); i += 4; continue;
        }
        if (rem == 4 && strncmp(&word[i], "NESS", 4) == 0) {
            append_ph(out, out_max, "N AH S"); i += 4; continue;
        }
        if (rem == 4 && strncmp(&word[i], "MENT", 4) == 0) {
            append_ph(out, out_max, "M AH N T"); i += 4; continue;
        }
        if (rem == 3 && strncmp(&word[i], "ING", 3) == 0) {
            append_ph(out, out_max, "IH NG"); i += 3; continue;
        }
        if (rem == 3 && strncmp(&word[i], "FUL", 3) == 0) {
            append_ph(out, out_max, "F UH L"); i += 3; continue;
        }
        if (rem == 3 && strncmp(&word[i], "OUS", 3) == 0) {
            append_ph(out, out_max, "AH S"); i += 3; continue;
        }
        if (rem == 2 && strncmp(&word[i], "LY", 2) == 0) {
            append_ph(out, out_max, "L IY"); i += 2; continue;
        }
        if (rem == 2 && strncmp(&word[i], "ED", 2) == 0) {
            if (i > 0 && (word[i - 1] == 'T' || word[i - 1] == 'D')) {
                append_ph(out, out_max, "IH D");
            } else if (i > 0 && (word[i - 1] == 'P' || word[i - 1] == 'K' || word[i - 1] == 'S' || word[i - 1] == 'F' || word[i - 1] == 'C')) {
                append_ph(out, out_max, "T");
            } else {
                append_ph(out, out_max, "D");
            }
            i += 2; continue;
        }
        if (rem == 2 && strncmp(&word[i], "ES", 2) == 0) {
            if (i > 0 && (word[i - 1] == 'S' || word[i - 1] == 'Z' || word[i - 1] == 'X' || word[i - 1] == 'C')) {
                append_ph(out, out_max, "IH Z");
            } else {
                append_ph(out, out_max, "Z");
            }
            i += 2; continue;
        }

        /* Magic 'E' Rule: Vowel + Consonant + E */
        if (is_vowel_c(word[i]) && rem >= 3 && !is_vowel_c(word[i + 1]) && word[i + 2] == 'E' && (rem == 3 || (rem == 4 && word[i + 3] == 'S'))) {
            char v = word[i];
            if (v == 'A') append_ph(out, out_max, "EY");
            else if (v == 'E') append_ph(out, out_max, "IY");
            else if (v == 'I') append_ph(out, out_max, "AY");
            else if (v == 'O') append_ph(out, out_max, "OW");
            else if (v == 'U') append_ph(out, out_max, "YU");
            else append_ph(out, out_max, "AY");

            if (word[i + 1] == 'C') append_ph(out, out_max, "S");
            else if (word[i + 1] == 'G') append_ph(out, out_max, "JH");
            else if (word[i + 1] == 'S') append_ph(out, out_max, "Z");
            else {
                char tmp[2]; tmp[0] = word[i + 1]; tmp[1] = '\0';
                append_ph(out, out_max, tmp);
            }

            if (rem == 4 && word[i + 3] == 'S') {
                append_ph(out, out_max, "Z");
                i += 4;
            } else {
                i += 3;
            }
            continue;
        }

        /* Digraphs */
        if (rem >= 2 && strncmp(&word[i], "CH", 2) == 0) { append_ph(out, out_max, "CH"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "SH", 2) == 0) { append_ph(out, out_max, "SH"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "TH", 2) == 0) {
            if (i == 0 && rem > 2 && is_vowel_c(word[i + 2])) {
                append_ph(out, out_max, "DH");
            } else {
                append_ph(out, out_max, "TH");
            }
            i += 2; continue;
        }
        if (rem >= 2 && strncmp(&word[i], "PH", 2) == 0) { append_ph(out, out_max, "F"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "WH", 2) == 0) { append_ph(out, out_max, "W"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "QU", 2) == 0) { append_ph(out, out_max, "K W"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "CK", 2) == 0) { append_ph(out, out_max, "K"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "NG", 2) == 0) { append_ph(out, out_max, "NG"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "NK", 2) == 0) { append_ph(out, out_max, "NG K"); i += 2; continue; }

        if (i == 0 && rem >= 2 && strncmp(&word[i], "KN", 2) == 0) { append_ph(out, out_max, "N"); i += 2; continue; }
        if (i == 0 && rem >= 2 && strncmp(&word[i], "WR", 2) == 0) { append_ph(out, out_max, "R"); i += 2; continue; }
        if (i == 0 && rem >= 2 && strncmp(&word[i], "PS", 2) == 0) { append_ph(out, out_max, "S"); i += 2; continue; }
        if (i == 0 && rem >= 2 && strncmp(&word[i], "GN", 2) == 0) { append_ph(out, out_max, "N"); i += 2; continue; }

        /* Vowel digraphs */
        if (rem >= 2 && (strncmp(&word[i], "AI", 2) == 0 || strncmp(&word[i], "AY", 2) == 0)) { append_ph(out, out_max, "EY"); i += 2; continue; }
        if (rem >= 2 && (strncmp(&word[i], "EE", 2) == 0 || strncmp(&word[i], "EA", 2) == 0)) { append_ph(out, out_max, "IY"); i += 2; continue; }
        if (rem >= 2 && (strncmp(&word[i], "EI", 2) == 0 || strncmp(&word[i], "EY", 2) == 0)) { append_ph(out, out_max, "EY"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "IE", 2) == 0) {
            if (rem == 2) append_ph(out, out_max, "AY");
            else append_ph(out, out_max, "IY");
            i += 2; continue;
        }
        if (rem >= 2 && strncmp(&word[i], "OA", 2) == 0) { append_ph(out, out_max, "OW"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "OO", 2) == 0) {
            if (rem > 2 && (word[i + 2] == 'K' || word[i + 2] == 'D')) append_ph(out, out_max, "UH");
            else append_ph(out, out_max, "UW");
            i += 2; continue;
        }
        if (rem >= 2 && strncmp(&word[i], "OU", 2) == 0) { append_ph(out, out_max, "AW"); i += 2; continue; }
        if (rem >= 2 && strncmp(&word[i], "OW", 2) == 0) {
            if (rem == 2 || (rem > 2 && word[i + 2] == 'N')) append_ph(out, out_max, "OW");
            else append_ph(out, out_max, "AW");
            i += 2; continue;
        }
        if (rem >= 2 && (strncmp(&word[i], "OI", 2) == 0 || strncmp(&word[i], "OY", 2) == 0)) { append_ph(out, out_max, "OY"); i += 2; continue; }
        if (rem >= 2 && (strncmp(&word[i], "AU", 2) == 0 || strncmp(&word[i], "AW", 2) == 0)) { append_ph(out, out_max, "AO"); i += 2; continue; }

        /* R-controlled vowels */
        if (rem >= 2 && strncmp(&word[i], "AR", 2) == 0) { append_ph(out, out_max, "AA R"); i += 2; continue; }
        if (rem >= 2 && (strncmp(&word[i], "ER", 2) == 0 || strncmp(&word[i], "IR", 2) == 0 || strncmp(&word[i], "UR", 2) == 0)) {
            append_ph(out, out_max, "ER"); i += 2; continue;
        }
        if (rem >= 2 && strncmp(&word[i], "OR", 2) == 0) { append_ph(out, out_max, "AO R"); i += 2; continue; }

        /* Single character rules */
        {
            char c = word[i];
            if (c == 'A') {
                if (rem >= 2 && word[i + 1] == 'L') append_ph(out, out_max, "AO");
                else append_ph(out, out_max, "AE");
            } else if (c == 'B') {
                if (rem == 1 && i > 0 && word[i - 1] == 'M') { /* silent */ }
                else append_ph(out, out_max, "B");
            } else if (c == 'C') {
                if (rem > 1 && is_front_vowel_c(word[i + 1])) append_ph(out, out_max, "S");
                else append_ph(out, out_max, "K");
            } else if (c == 'D') {
                append_ph(out, out_max, "D");
            } else if (c == 'E') {
                if (rem == 1 && len > 2) { /* silent trailing E */ }
                else append_ph(out, out_max, "EH");
            } else if (c == 'F') {
                append_ph(out, out_max, "F");
            } else if (c == 'G') {
                if (rem > 1 && is_front_vowel_c(word[i + 1])) append_ph(out, out_max, "JH");
                else append_ph(out, out_max, "G");
            } else if (c == 'H') {
                append_ph(out, out_max, "HH");
            } else if (c == 'I') {
                if (rem >= 3 && word[i + 1] == 'N' && word[i + 2] == 'D') append_ph(out, out_max, "AY");
                else if (rem >= 3 && word[i + 1] == 'L' && word[i + 2] == 'D') append_ph(out, out_max, "AY");
                else append_ph(out, out_max, "IH");
            } else if (c == 'J') {
                append_ph(out, out_max, "JH");
            } else if (c == 'K') {
                append_ph(out, out_max, "K");
            } else if (c == 'L') {
                append_ph(out, out_max, "L");
            } else if (c == 'M') {
                append_ph(out, out_max, "M");
            } else if (c == 'N') {
                append_ph(out, out_max, "N");
            } else if (c == 'O') {
                if (rem >= 3 && word[i + 1] == 'L' && word[i + 2] == 'D') append_ph(out, out_max, "OW");
                else if (rem >= 3 && word[i + 1] == 'S' && word[i + 2] == 'T') append_ph(out, out_max, "OW");
                else append_ph(out, out_max, "AA");
            } else if (c == 'P') {
                append_ph(out, out_max, "P");
            } else if (c == 'Q') {
                append_ph(out, out_max, "K");
            } else if (c == 'R') {
                append_ph(out, out_max, "R");
            } else if (c == 'S') {
                if (i > 0 && rem > 1 && is_vowel_c(word[i - 1]) && is_vowel_c(word[i + 1])) {
                    append_ph(out, out_max, "Z");
                } else if (rem == 1 && i > 0 && is_voiced_cons_c(word[i - 1])) {
                    append_ph(out, out_max, "Z");
                } else {
                    append_ph(out, out_max, "S");
                }
            } else if (c == 'T') {
                append_ph(out, out_max, "T");
            } else if (c == 'U') {
                append_ph(out, out_max, "AH");
            } else if (c == 'V') {
                append_ph(out, out_max, "V");
            } else if (c == 'W') {
                append_ph(out, out_max, "W");
            } else if (c == 'X') {
                if (i == 0) append_ph(out, out_max, "Z");
                else append_ph(out, out_max, "K S");
            } else if (c == 'Y') {
                if (i == 0) append_ph(out, out_max, "Y");
                else if (rem == 1 && len <= 3) append_ph(out, out_max, "AY");
                else if (rem == 1) append_ph(out, out_max, "IY");
                else append_ph(out, out_max, "IH");
            } else if (c == 'Z') {
                append_ph(out, out_max, "Z");
            }
            i++;
        }
    }
}

static void translate_word(const char *word, char *out, size_t out_max) {
    const char *dict_match = lookup_dictionary(word);
    if (dict_match) {
        strncpy(out, dict_match, out_max - 1);
        out[out_max - 1] = '\0';
    } else {
        translate_word_rules(word, out, out_max);
    }
}

/* =========================================================================
   Number Expander (0 to 999,999)
   ========================================================================= */
static void append_text_word(char *out, size_t out_max, const char *w) {
    size_t len = strlen(out);
    if (len > 0 && out[len - 1] != ' ') {
        if (len + 1 < out_max) {
            out[len] = ' ';
            out[len + 1] = '\0';
            len++;
        }
    }
    if (len + strlen(w) < out_max) {
        strcat(out, w);
    }
}

static void expand_number_under_1000(int n, char *out, size_t out_max) {
    const char *ones[] = {"", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE",
                          "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN", "SIXTEEN",
                          "SEVENTEEN", "EIGHTEEN", "NINETEEN"};
    const char *tens[] = {"", "", "TWENTY", "THIRTY", "FORTY", "FIFTY", "SIXTY", "SEVENTY", "EIGHTY", "NINETY"};
    
    if (n >= 100) {
        append_text_word(out, out_max, ones[n / 100]);
        append_text_word(out, out_max, "HUNDRED");
        n %= 100;
    }
    if (n >= 20) {
        append_text_word(out, out_max, tens[n / 10]);
        if (n % 10 > 0) append_text_word(out, out_max, ones[n % 10]);
    } else if (n > 0) {
        append_text_word(out, out_max, ones[n]);
    }
}

static void expand_number(int n, char *out, size_t out_max) {
    out[0] = '\0';
    if (n == 0) {
        append_text_word(out, out_max, "ZERO");
        return;
    }
    if (n >= 1000000) {
        int mill = n / 1000000;
        expand_number_under_1000(mill, out, out_max);
        append_text_word(out, out_max, "MILLION");
        n %= 1000000;
    }
    if (n >= 1000) {
        int thou = n / 1000;
        if (n >= 1100 && n <= 1999 && (n % 100 != 0)) {
            int century = n / 100;
            int rest = n % 100;
            expand_number_under_1000(century, out, out_max);
            expand_number_under_1000(rest, out, out_max);
            return;
        }
        expand_number_under_1000(thou, out, out_max);
        append_text_word(out, out_max, "THOUSAND");
        n %= 1000;
    }
    if (n > 0) {
        expand_number_under_1000(n, out, out_max);
    }
}

/* =========================================================================
   Sentence Preprocessor & Phoneme Tokenizer (Connected Speech Flow)
   ========================================================================= */
static void text_to_phonemes(const char *text, char *out_ph, size_t out_max) {
    size_t i = 0, len = strlen(text);
    char token[128];
    size_t tok_len = 0;
    char word_ph[256];

    out_ph[0] = '\0';

    while (i <= len) {
        char c = (i < len) ? text[i] : '\0';

        if (isalnum((unsigned char)c) || c == '\'') {
            if (tok_len + 1 < sizeof(token)) {
                token[tok_len++] = (char)toupper((unsigned char)c);
            }
        } else {
            if (tok_len > 0) {
                token[tok_len] = '\0';
                tok_len = 0;

                if (isdigit((unsigned char)token[0])) {
                    int num = atoi(token);
                    char num_words[256];
                    char *w;
                    expand_number(num, num_words, sizeof(num_words));
                    w = strtok(num_words, " ");
                    while (w) {
                        translate_word(w, word_ph, sizeof(word_ph));
                        append_ph(out_ph, out_max, word_ph);
                        append_ph(out_ph, out_max, "PA_W");
                        w = strtok(NULL, " ");
                    }
                } else {
                    translate_word(token, word_ph, sizeof(word_ph));
                    append_ph(out_ph, out_max, word_ph);
                    append_ph(out_ph, out_max, "PA_W");
                }
            }

            if (c == '.' || c == '?' || (unsigned char)c == 0xBF || c == '!' || (unsigned char)c == 0xA1 || c == ',' || c == ';' || c == ':') {
                /* Replace preceding PA_W with punctuation pause */
                size_t plen = strlen(out_ph);
                if (plen >= 4 && strcmp(&out_ph[plen - 4], "PA_W") == 0) {
                    out_ph[plen - 4] = '\0';
                    if (plen > 4 && out_ph[plen - 5] == ' ') {
                        out_ph[plen - 5] = '\0';
                    }
                }
                if (c == '.') {
                    append_ph(out_ph, out_max, "PA_P");
                } else if (c == '?' || (unsigned char)c == 0xBF) {
                    append_ph(out_ph, out_max, "PA_Q");
                } else if (c == '!' || (unsigned char)c == 0xA1) {
                    append_ph(out_ph, out_max, "PA_E");
                } else if (c == ',' || c == ';' || c == ':') {
                    append_ph(out_ph, out_max, "PA_C");
                }
            }
        }
        i++;
    }
}

static int parse_phoneme_string(const char *ph_str, enum PhonemeCode *out_array, int max_phonemes) {
    char buf[4096];
    char *token;
    int count = 0;

    strncpy(buf, ph_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    token = strtok(buf, " \t\r\n");
    while (token && count < max_phonemes) {
        enum PhonemeCode code = phoneme_from_name(token);
        if (code != PH_NONE) {
            out_array[count++] = code;
        }
        token = strtok(NULL, " \t\r\n");
    }
    return count;
}

/* =========================================================================
   Acoustic Formant Resonator & LFSR Noise Generator
   ========================================================================= */
struct BiquadResonator {
    double a1, a2, b0;
    double y1, y2;
};

static void init_resonator(struct BiquadResonator *r) {
    r->a1 = 0.0;
    r->a2 = 0.0;
    r->b0 = 0.0;
    r->y1 = 0.0;
    r->y2 = 0.0;
}

static void update_resonator(struct BiquadResonator *r, double freq, double bw) {
    double radius, theta;
    if (freq <= 20.0 || bw <= 10.0) {
        r->a1 = 0.0;
        r->a2 = 0.0;
        r->b0 = 0.0;
        return;
    }
    if (freq > (SAMPLE_RATE / 2.0 - 50.0)) {
        freq = SAMPLE_RATE / 2.0 - 50.0;
    }
    radius = exp(-M_PI * bw / (double)SAMPLE_RATE);
    theta = 2.0 * M_PI * freq / (double)SAMPLE_RATE;
    r->a1 = -2.0 * radius * cos(theta);
    r->a2 = radius * radius;
    r->b0 = (1.0 - radius) * sin(theta);
}

static double process_resonator(struct BiquadResonator *r, double in) {
    double out = r->b0 * in - r->a1 * r->y1 - r->a2 * r->y2;
    r->y2 = r->y1;
    r->y1 = out;
    return out;
}

struct FrameParams {
    double f1, f2, f3;
    double bw1, bw2, bw3;
    double a1, a2, a3;
    double voice_amp;
    double noise_amp;
    enum NoiseType noise_type;
    double noise_fc, noise_bw;
    double f0;
    int is_pause;
};

struct SynthConfig {
    double base_pitch;    /* Base pitch in Hz (default: 120.0) */
    double speed;         /* Speed rate multiplier (default: 1.0) */
    double formant_shift; /* Formant multiplier / vocal tract scale (default: 1.0) */
    int verbose;          /* Verbose log flag */
};

static unsigned char* synthesize_phonemes(const enum PhonemeCode *phonemes, int num_phonemes, const struct SynthConfig *cfg, unsigned long *out_num_samples) {
    int p, f, s;
    int total_frames = 0;
    int *ph_start_frame;
    int *ph_num_frames;
    struct FrameParams *frames;
    double *raw_audio;
    unsigned char *audio_buf;
    unsigned long total_samples;
    struct BiquadResonator r1, r2, r3, r_nasal, r_noise;
    unsigned short lfsr = 0xACE1; /* 16-bit retro noise shift register */
    double glottal_phase = 0.0;
    double cur_f1, cur_f2, cur_f3, cur_a1, cur_a2, cur_a3;
    double cur_vamp, cur_namp, cur_nfc, cur_nbw, cur_f0;
    double cur_bw1, cur_bw2, cur_bw3;
    int sentence_end_frame = 0;
    int is_question = 0;

    if (num_phonemes <= 0) {
        *out_num_samples = 0;
        return NULL;
    }

    ph_start_frame = (int*)malloc((size_t)num_phonemes * sizeof(int));
    ph_num_frames = (int*)malloc((size_t)num_phonemes * sizeof(int));
    if (!ph_start_frame || !ph_num_frames) {
        if (ph_start_frame) free(ph_start_frame);
        if (ph_num_frames) free(ph_num_frames);
        return NULL;
    }

    /* Calculate frame durations */
    for (p = 0; p < num_phonemes; ++p) {
        enum PhonemeCode code = phonemes[p];
        const struct PhonemeDef *def = &PHONEME_TABLE[code];
        int num_f = (int)floor((double)def->base_frames / cfg->speed + 0.5);
        if (num_f < 2) num_f = 2;
        ph_start_frame[p] = total_frames;
        ph_num_frames[p] = num_f;
        total_frames += num_f;
    }

    frames = (struct FrameParams*)malloc((size_t)total_frames * sizeof(struct FrameParams));
    if (!frames) {
        free(ph_start_frame);
        free(ph_num_frames);
        return NULL;
    }

    /* Check for question ending */
    for (p = num_phonemes - 1; p >= 0; --p) {
        if (phonemes[p] == PH_PA_QMARK) {
            is_question = 1;
            break;
        } else if (phonemes[p] == PH_PA_PERIOD || phonemes[p] == PH_PA_EXCL) {
            break;
        }
    }
    sentence_end_frame = total_frames;

    /* Build frame parameters */
    for (p = 0; p < num_phonemes; ++p) {
        enum PhonemeCode code = phonemes[p];
        const struct PhonemeDef *def = &PHONEME_TABLE[code];
        int start_f = ph_start_frame[p];
        int count_f = ph_num_frames[p];
        const struct PhonemeDef *prev_def = (p > 0) ? &PHONEME_TABLE[phonemes[p - 1]] : &PHONEME_TABLE[PH_NONE];
        const struct PhonemeDef *next_def = (p + 1 < num_phonemes) ? &PHONEME_TABLE[phonemes[p + 1]] : &PHONEME_TABLE[PH_NONE];

        for (f = 0; f < count_f; ++f) {
            int global_f = start_f + f;
            struct FrameParams *fp = &frames[global_f];
            double u = (count_f > 1) ? ((double)f / (double)(count_f - 1)) : 0.0;
            double declination, stress, intonation;

            fp->f1 = ((double)def->f1 + u * (double)(def->f1_end - def->f1)) * cfg->formant_shift;
            fp->f2 = ((double)def->f2 + u * (double)(def->f2_end - def->f2)) * cfg->formant_shift;
            fp->f3 = ((double)def->f3 + u * (double)(def->f3_end - def->f3)) * cfg->formant_shift;
            fp->bw1 = (double)def->bw1;
            fp->bw2 = (double)def->bw2;
            fp->bw3 = (double)def->bw3;
            fp->a1 = def->a1;
            fp->a2 = def->a2;
            fp->a3 = def->a3;
            fp->voice_amp = def->voice_amp;
            fp->noise_amp = def->noise_amp;
            fp->noise_type = def->noise_type;
            fp->noise_fc = (double)def->noise_fc;
            fp->noise_bw = (double)def->noise_bw;
            fp->is_pause = def->is_pause;

            /* Consonant locus transition */
            if (def->is_vowel) {
                if (prev_def->f2_locus > 0 && f < 3) {
                    double trans = (3.0 - (double)f) / 3.0 * 0.4;
                    fp->f2 = fp->f2 * (1.0 - trans) + (double)prev_def->f2_locus * trans * cfg->formant_shift;
                }
                if (next_def->f2_locus > 0 && f >= count_f - 3) {
                    double trans = ((double)f - (double)(count_f - 3)) / 3.0 * 0.4;
                    fp->f2 = fp->f2 * (1.0 - trans) + (double)next_def->f2_locus * trans * cfg->formant_shift;
                }
            }

            /* Plosive phases */
            if (def->is_plosive) {
                int burst_f = count_f - 1;
                if (f < burst_f) {
                    if (def->is_voiced_plosive) {
                        /* Voiced bar */
                        fp->voice_amp = 0.50;
                        fp->f1 = 180.0;
                        fp->a1 = 0.6; fp->a2 = 0.0; fp->a3 = 0.0;
                        fp->noise_amp = 0.0;
                    } else {
                        /* Soft closure */
                        fp->voice_amp = 0.0;
                        fp->noise_amp = 0.0;
                    }
                } else {
                    /* Release burst */
                    fp->voice_amp = def->is_voiced_plosive ? 0.30 : 0.0;
                    fp->noise_amp = def->noise_amp;
                }
            }

            /* Intonation & Prosody */
            declination = 1.0 - 0.08 * ((double)global_f / (double)sentence_end_frame);
            stress = def->is_vowel ? 1.05 : 1.0;
            intonation = 1.0;

            if (is_question) {
                if (global_f > sentence_end_frame - 25) {
                    double q_ratio = (double)(global_f - (sentence_end_frame - 25)) / 25.0;
                    intonation = 1.0 + 0.35 * q_ratio * q_ratio;
                }
            } else {
                if (global_f > sentence_end_frame - 20) {
                    double end_ratio = (double)(global_f - (sentence_end_frame - 20)) / 20.0;
                    intonation = 1.0 - 0.15 * end_ratio;
                }
            }

            fp->f0 = cfg->base_pitch * declination * stress * intonation;
        }
    }

    total_samples = (unsigned long)total_frames * (unsigned long)SAMPLES_PER_FRAME;
    raw_audio = (double*)malloc((size_t)total_samples * sizeof(double));
    audio_buf = (unsigned char*)malloc((size_t)total_samples);
    if (!raw_audio || !audio_buf) {
        if (raw_audio) free(raw_audio);
        if (audio_buf) free(audio_buf);
        free(frames);
        free(ph_start_frame);
        free(ph_num_frames);
        return NULL;
    }

    init_resonator(&r1);
    init_resonator(&r2);
    init_resonator(&r3);
    init_resonator(&r_nasal);
    init_resonator(&r_noise);

    cur_f1 = frames[0].f1; cur_f2 = frames[0].f2; cur_f3 = frames[0].f3;
    cur_bw1 = frames[0].bw1; cur_bw2 = frames[0].bw2; cur_bw3 = frames[0].bw3;
    cur_a1 = frames[0].a1; cur_a2 = frames[0].a2; cur_a3 = frames[0].a3;
    cur_vamp = frames[0].voice_amp; cur_namp = frames[0].noise_amp;
    cur_nfc = (frames[0].noise_fc > 0.0) ? frames[0].noise_fc : 2500.0;
    cur_nbw = (frames[0].noise_bw > 0.0) ? frames[0].noise_bw : 1500.0;
    cur_f0 = frames[0].f0;

    for (f = 0; f < total_frames; ++f) {
        const struct FrameParams *tgt = &frames[f];

        /* Pause frames write absolute silence */
        if (tgt->is_pause) {
            cur_vamp = 0.0;
            cur_namp = 0.0;
            r1.y1 = r1.y2 = 0.0;
            r2.y1 = r2.y2 = 0.0;
            r3.y1 = r3.y2 = 0.0;
            r_noise.y1 = r_noise.y2 = 0.0;

            for (s = 0; s < SAMPLES_PER_FRAME; ++s) {
                unsigned long sample_idx = (unsigned long)f * SAMPLES_PER_FRAME + (unsigned long)s;
                raw_audio[sample_idx] = 0.0;
            }
            continue;
        }

        if (tgt->noise_fc > 0.0 && cur_namp < 0.02) {
            cur_nfc = tgt->noise_fc;
            cur_nbw = tgt->noise_bw;
        }

        for (s = 0; s < SAMPLES_PER_FRAME; ++s) {
            unsigned long sample_idx = (unsigned long)f * SAMPLES_PER_FRAME + (unsigned long)s;
            double alpha_formant = 0.05;
            double alpha_noise = (tgt->noise_amp < cur_namp) ? 0.20 : 0.08;
            double glottal_deriv, noise_raw, noise_flt;
            double v1, v2, v3, voiced_sig, total_sig;
            unsigned short bit;

            cur_f1 += alpha_formant * (tgt->f1 - cur_f1);
            cur_f2 += alpha_formant * (tgt->f2 - cur_f2);
            cur_f3 += alpha_formant * (tgt->f3 - cur_f3);
            cur_bw1 += alpha_formant * (tgt->bw1 - cur_bw1);
            cur_bw2 += alpha_formant * (tgt->bw2 - cur_bw2);
            cur_bw3 += alpha_formant * (tgt->bw3 - cur_bw3);
            cur_a1 += alpha_formant * (tgt->a1 - cur_a1);
            cur_a2 += alpha_formant * (tgt->a2 - cur_a2);
            cur_a3 += alpha_formant * (tgt->a3 - cur_a3);
            cur_vamp += alpha_formant * (tgt->voice_amp - cur_vamp);
            cur_namp += alpha_noise * (tgt->noise_amp - cur_namp);
            if (tgt->noise_fc > 0.0) {
                cur_nfc += alpha_formant * (tgt->noise_fc - cur_nfc);
                cur_nbw += alpha_formant * (tgt->noise_bw - cur_nbw);
            }
            cur_f0 += alpha_formant * (tgt->f0 - cur_f0);

            update_resonator(&r1, cur_f1, cur_bw1);
            update_resonator(&r2, cur_f2, cur_bw2);
            update_resonator(&r3, cur_f3, cur_bw3);
            update_resonator(&r_noise, cur_nfc, cur_nbw);

            /* Smooth raised-cosine glottal excitation */
            glottal_phase += cur_f0 / (double)SAMPLE_RATE;
            if (glottal_phase >= 1.0) {
                glottal_phase -= 1.0;
            }

            if (glottal_phase < 0.40) {
                double u = glottal_phase / 0.40;
                glottal_deriv = sin(M_PI * u);
            } else {
                glottal_deriv = 0.0;
            }

            v1 = process_resonator(&r1, glottal_deriv);
            v2 = process_resonator(&r2, glottal_deriv);
            v3 = process_resonator(&r3, glottal_deriv);
            voiced_sig = cur_a1 * v1 + cur_a2 * v2 + cur_a3 * v3;

            /* Retro 16-bit Galois LFSR noise source */
            bit = lfsr & 1;
            lfsr >>= 1;
            if (bit) {
                lfsr ^= 0xB400u;
            }
            noise_raw = bit ? 0.8 : -0.8;

            /* Noise path generation */
            if (cur_namp > 0.001) {
                if (tgt->noise_type == NOISE_ASPIRATION) {
                    /* Formant-shaped aspiration (throat resonance) */
                    noise_flt = (cur_a1 * process_resonator(&r1, noise_raw) +
                                 cur_a2 * process_resonator(&r2, noise_raw) +
                                 cur_a3 * process_resonator(&r3, noise_raw)) * 0.35;
                } else {
                    /* Resonant-filtered sibilance / burst */
                    noise_flt = process_resonator(&r_noise, noise_raw);
                }
            } else {
                noise_flt = 0.0;
                cur_namp = 0.0;
            }

            /* Combined signal */
            total_sig = cur_vamp * voiced_sig * 1.6 + cur_namp * noise_flt * 0.8;
            raw_audio[sample_idx] = total_sig;
        }
    }

    /* Anti-click smooth crossfading into and out of pauses */
    for (p = 0; p < num_phonemes; ++p) {
        enum PhonemeCode code = phonemes[p];
        const struct PhonemeDef *def = &PHONEME_TABLE[code];
        int start_f = ph_start_frame[p];
        int count_f = ph_num_frames[p];
        unsigned long start_samp = (unsigned long)start_f * SAMPLES_PER_FRAME;
        unsigned long end_samp = (unsigned long)(start_f + count_f) * SAMPLES_PER_FRAME;
        int ramp_len = 48; /* 6ms smooth fade in/out */
        int k;

        if (!def->is_pause) {
            /* Fade in if beginning of speech or after a pause */
            if (p == 0 || PHONEME_TABLE[phonemes[p - 1]].is_pause) {
                for (k = 0; k < ramp_len && (start_samp + (unsigned long)k) < total_samples; ++k) {
                    double w = 0.5 * (1.0 - cos(M_PI * (double)k / (double)ramp_len));
                    raw_audio[start_samp + (unsigned long)k] *= w;
                }
            }
            /* Fade out if end of speech or before a pause */
            if (p + 1 == num_phonemes || PHONEME_TABLE[phonemes[p + 1]].is_pause) {
                for (k = 0; k < ramp_len && (end_samp > (unsigned long)k); ++k) {
                    unsigned long idx = end_samp - 1 - (unsigned long)k;
                    double w = 0.5 * (1.0 - cos(M_PI * (double)k / (double)ramp_len));
                    if (idx < total_samples) raw_audio[idx] *= w;
                }
            }
        }
    }

    /* Convert to 8-bit unsigned PCM with soft limiting */
    for (total_samples = (unsigned long)total_frames * SAMPLES_PER_FRAME, s = 0; (unsigned long)s < total_samples; ++s) {
        double val = raw_audio[s];
        int pcm_byte;

        if (fabs(val) < 0.001) {
            pcm_byte = 128;
        } else {
            double limited = tanh(val * 1.4);
            pcm_byte = (int)floor((limited + 1.0) * 127.5 + 0.5);
            if (pcm_byte < 0) pcm_byte = 0;
            if (pcm_byte > 255) pcm_byte = 255;
        }
        audio_buf[s] = (unsigned char)pcm_byte;
    }

    free(raw_audio);
    free(frames);
    free(ph_start_frame);
    free(ph_num_frames);

    *out_num_samples = total_samples;
    return audio_buf;
}

/* =========================================================================
   WAV RIFF File Writer (8kHz 8-bit Mono)
   ========================================================================= */
static void write_wav_header(FILE *fp, unsigned long data_size) {
    unsigned long file_size = 36 + data_size;
    unsigned long sample_rate = 8000;
    unsigned long byte_rate = 8000;
    unsigned short format = 1;
    unsigned short channels = 1;
    unsigned short block_align = 1;
    unsigned short bits_sample = 8;
    unsigned long fmt_size = 16;
    unsigned char h[44];

    h[0] = 'R'; h[1] = 'I'; h[2] = 'F'; h[3] = 'F';
    h[4] = (unsigned char)(file_size & 0xFF);
    h[5] = (unsigned char)((file_size >> 8) & 0xFF);
    h[6] = (unsigned char)((file_size >> 16) & 0xFF);
    h[7] = (unsigned char)((file_size >> 24) & 0xFF);
    h[8] = 'W'; h[9] = 'A'; h[10] = 'V'; h[11] = 'E';

    h[12] = 'f'; h[13] = 'm'; h[14] = 't'; h[15] = ' ';
    h[16] = (unsigned char)(fmt_size & 0xFF);
    h[17] = (unsigned char)((fmt_size >> 8) & 0xFF);
    h[18] = (unsigned char)((fmt_size >> 16) & 0xFF);
    h[19] = (unsigned char)((fmt_size >> 24) & 0xFF);
    h[20] = (unsigned char)(format & 0xFF);
    h[21] = (unsigned char)((format >> 8) & 0xFF);
    h[22] = (unsigned char)(channels & 0xFF);
    h[23] = (unsigned char)((channels >> 8) & 0xFF);
    h[24] = (unsigned char)(sample_rate & 0xFF);
    h[25] = (unsigned char)((sample_rate >> 8) & 0xFF);
    h[26] = (unsigned char)((sample_rate >> 16) & 0xFF);
    h[27] = (unsigned char)((sample_rate >> 24) & 0xFF);
    h[28] = (unsigned char)(byte_rate & 0xFF);
    h[29] = (unsigned char)((byte_rate >> 8) & 0xFF);
    h[30] = (unsigned char)((byte_rate >> 16) & 0xFF);
    h[31] = (unsigned char)((byte_rate >> 24) & 0xFF);
    h[32] = (unsigned char)(block_align & 0xFF);
    h[33] = (unsigned char)((block_align >> 8) & 0xFF);
    h[34] = (unsigned char)(bits_sample & 0xFF);
    h[35] = (unsigned char)((bits_sample >> 8) & 0xFF);

    h[36] = 'd'; h[37] = 'a'; h[38] = 't'; h[39] = 'a';
    h[40] = (unsigned char)(data_size & 0xFF);
    h[41] = (unsigned char)((data_size >> 8) & 0xFF);
    h[42] = (unsigned char)((data_size >> 16) & 0xFF);
    h[43] = (unsigned char)((data_size >> 24) & 0xFF);

    fwrite(h, 1, 44, fp);
}

/* =========================================================================
   Command Line Interface (Amiga 500 "say" style)
   ========================================================================= */
static void print_usage(const char *progname) {
    printf("Amiga 500 Text-to-Speech Synthesizer (ANSI C90, Public Domain)\n");
    printf("Usage: %s [options] [\"text to speak\"]\n\n", progname);
    printf("Options:\n");
    printf("  -o <file>      Output WAV file (default: output.wav)\n");
    printf("  -p <pitch>     Base pitch F0 in Hz (default: 120, range: 50-300)\n");
    printf("  -s <speed>     Speech speed multiplier (default: 0.80, range: 0.3-3.0)\n");
    printf("  -t <tone>      Formant frequency scale / tone (default: 1.0, range: 0.5-2.0)\n");
    printf("  -ph            Direct phonetic mode (ARPAbet phoneme sequence)\n");
    printf("  -v             Verbose output (show phonetic breakdown)\n");
    printf("  -h, --help     Show this help message\n");
    printf("  --license      Show Public Domain dedication notice\n\n");
    printf("Examples:\n");
    printf("  %s \"Hello world, I am the Amiga five hundred computer.\"\n", progname);
    printf("  %s -p 150 -s 1.2 -o amiga.wav \"Text to speech is ready.\"\n", progname);
    printf("  %s -ph \"HH EH L OW W ER L D .\"\n", progname);
    printf("  echo \"Say command on Amiga\" | %s -o test.wav\n", progname);
}

static void print_license(void) {
    printf("Creative Commons CC0 1.0 Universal / Public Domain Dedication\n\n");
    printf("The person who associated a work with this deed has dedicated the work to\n");
    printf("the public domain by waiving all of his or her rights to the work worldwide\n");
    printf("under copyright law, including all related and neighboring rights, to the\n");
    printf("extent allowed by law.\n\n");
    printf("You can copy, modify, distribute and perform the work, even for commercial\n");
    printf("purposes, all without asking permission.\n");
}

int main(int argc, char *argv[]) {
    struct SynthConfig cfg;
    const char *out_filename = "output.wav";
    int phonetic_mode = 0;
    char text_buf[4096];
    char ph_buf[8192];
    enum PhonemeCode phonemes[1024];
    int num_phonemes = 0;
    unsigned long num_samples = 0;
    unsigned char *audio;
    FILE *fp;
    int i;
    int text_arg_idx = -1;

    cfg.base_pitch = 120.0;
    cfg.speed = 0.80;
    cfg.formant_shift = 1.0;
    cfg.verbose = 0;
    text_buf[0] = '\0';
    ph_buf[0] = '\0';

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--license") == 0) {
            print_license();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            cfg.verbose = 1;
        } else if (strcmp(argv[i], "-ph") == 0 || strcmp(argv[i], "--phonetic") == 0) {
            phonetic_mode = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_filename = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            cfg.base_pitch = atof(argv[++i]);
            if (cfg.base_pitch < 40.0) cfg.base_pitch = 40.0;
            if (cfg.base_pitch > 400.0) cfg.base_pitch = 400.0;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            cfg.speed = atof(argv[++i]);
            if (cfg.speed < 0.3) cfg.speed = 0.3;
            if (cfg.speed > 3.0) cfg.speed = 3.0;
        } else if ((strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "-f") == 0) && i + 1 < argc) {
            cfg.formant_shift = atof(argv[++i]);
            if (cfg.formant_shift < 0.5) cfg.formant_shift = 0.5;
            if (cfg.formant_shift > 2.0) cfg.formant_shift = 2.0;
        } else if (argv[i][0] != '-') {
            text_arg_idx = i;
            break;
        }
    }

    if (text_arg_idx >= 0) {
        for (i = text_arg_idx; i < argc; ++i) {
            if (strlen(text_buf) > 0) {
                strncat(text_buf, " ", sizeof(text_buf) - strlen(text_buf) - 1);
            }
            strncat(text_buf, argv[i], sizeof(text_buf) - strlen(text_buf) - 1);
        }
    } else {
        if (!fgets(text_buf, sizeof(text_buf), stdin)) {
            print_usage(argv[0]);
            return 1;
        }
        i = (int)strlen(text_buf);
        while (i > 0 && (text_buf[i - 1] == '\r' || text_buf[i - 1] == '\n')) {
            text_buf[--i] = '\0';
        }
    }

    if (strlen(text_buf) == 0) {
        fprintf(stderr, "Error: No text provided.\n");
        return 1;
    }

    if (phonetic_mode) {
        strncpy(ph_buf, text_buf, sizeof(ph_buf) - 1);
        ph_buf[sizeof(ph_buf) - 1] = '\0';
    } else {
        text_to_phonemes(text_buf, ph_buf, sizeof(ph_buf));
    }

    if (cfg.verbose) {
        printf("Input Text : %s\n", text_buf);
        printf("Phonemes   : %s\n", ph_buf);
        printf("Pitch (F0) : %.1f Hz\n", cfg.base_pitch);
        printf("Speed      : %.2fx\n", cfg.speed);
        printf("Tone/Shift : %.2fx\n", cfg.formant_shift);
    }

    num_phonemes = parse_phoneme_string(ph_buf, phonemes, sizeof(phonemes) / sizeof(phonemes[0]));
    if (num_phonemes <= 0) {
        fprintf(stderr, "Error: No valid phonemes generated from input.\n");
        return 1;
    }

    audio = synthesize_phonemes(phonemes, num_phonemes, &cfg, &num_samples);
    if (!audio) {
        fprintf(stderr, "Error: Synthesis failed.\n");
        return 1;
    }

    fp = fopen(out_filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Could not open output file '%s' for writing.\n", out_filename);
        free(audio);
        return 1;
    }

    write_wav_header(fp, num_samples);
    fwrite(audio, 1, (size_t)num_samples, fp);
    fclose(fp);
    free(audio);

    if (cfg.verbose) {
        printf("Saved '%s' (8kHz, 8-bit Mono PCM, %.2f seconds, %lu samples)\n",
               out_filename, (double)num_samples / 8000.0, num_samples);
    } else {
        printf("WAV file '%s' generated successfully (8kHz 8-bit PCM, %.2fs).\n",
               out_filename, (double)num_samples / 8000.0);
    }

    return 0;
}
