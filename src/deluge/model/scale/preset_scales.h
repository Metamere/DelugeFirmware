#pragma once

#include "model/scale/note_set.h"

#include <array>
#include <bitset>
#include <cstdint>

/* This "defines" all preset scales in one place, so the various tables they inhabit
 * automatically stay in sync.
 *
 * NOTE: songs currently store scales as modeNotes, making them reasonably robust to
 * redefinitions, but the default scale is stored in the flash storage as an index.
 * See OFFSET_6|5_NOTE_SCALE for how to deal with that.
 *
 * The actual definitions expand this macro, picking the thing they need:
 * - enum Scale
 * - scalelikeNames
 * - presetScaleNotes
 *
 * Use by defining DEF(id, name, notes) before invoking DEF_SCALES() and undefining it
 * after.
 *
 * DEF_NOTES keeps the notes array intact on the trip through the preprocessor, since
 * cpp doesn't understand arrays, seeing { and } as a normal tokens.
 */
#define DEF_NOTES(...) NoteSet({__VA_ARGS__})

// Note: Value of note intervals taken from here: https://www.apassion4jazz.net/scales2.html
#define DEF_SCALES()                                                                                                   \
	/* ============================== 7-note scales ============================== */                                  \
	/* ------------- ORIGINAL DELUGE SCALES: All diatonic scale modes ------------ */                                  \
	/* MAJO Major (Ionian) */                                                                                          \
	DEF(MAJOR_SCALE, "MAJOR", DEF_NOTES(0, 2, 4, 5, 7, 9, 11))                                                         \
	/* MINO Natural Minor (Aeolian) */                                                                                 \
	DEF(MINOR_SCALE, "MINOR", DEF_NOTES(0, 2, 3, 5, 7, 8, 10))                                                         \
	/* DORI Dorian (minor with raised 6th) */                                                                          \
	DEF(DORIAN_SCALE, "DORIAN", DEF_NOTES(0, 2, 3, 5, 7, 9, 10))                                                       \
	/* PHRY Phrygian (minor with flattened 2nd) */                                                                     \
	DEF(PHRYGIAN_SCALE, "PHRYGIAN", DEF_NOTES(0, 1, 3, 5, 7, 8, 10))                                                   \
	/* LYDI Lydian (major with raised 4th) */                                                                          \
	DEF(LYDIAN_SCALE, "LYDIAN", DEF_NOTES(0, 2, 4, 6, 7, 9, 11))                                                       \
	/* MIXO Mixolydian (major with flattened 7th) */                                                                   \
	DEF(MIXOLYDIAN_SCALE, "MIXOLYDIAN", DEF_NOTES(0, 2, 4, 5, 7, 9, 10))                                               \
	/* LOCR Locrian (minor with flattened 2nd and 5th) */                                                              \
	DEF(LOCRIAN_SCALE, "LOCRIAN", DEF_NOTES(0, 1, 3, 5, 6, 8, 10))                                                     \
	/* ------------- ADDITIONAL 7-NOTE SCALES ------------------------------------ */                                  \
	/* MELO Melodic Minor (Ascending) (matches Launchpad scale) */                                                     \
	DEF(MELODIC_MINOR_SCALE, "MELODIC MINOR", DEF_NOTES(0, 2, 3, 5, 7, 9, 11))                                         \
	/* HARM Harmonic Minor (matches Launchpad and Lumi scale) */                                                       \
	DEF(HARMONIC_MINOR_SCALE, "HARMONIC MINOR", DEF_NOTES(0, 2, 3, 5, 7, 8, 11))                                       \
	/* HUNG Hungarian Minor (a.k.a Flamenco mode, Gypsy minor, matches Launchpad scale) */                             \
	DEF(HUNGARIAN_MINOR_SCALE, "HUNGARIAN MINOR", DEF_NOTES(0, 2, 3, 6, 7, 8, 11))                                     \
	/* MARV Marva (a.k.a Marva That, Mela Gamanasrama. matches Launchpad scale) */                                     \
	DEF(MARVA_SCALE, "MARVA", DEF_NOTES(0, 1, 4, 6, 7, 9, 11))                                                         \
	/* ACOU Acoustic (a.k.a. Overtone, Lydian Dominant, Raga Bhusavati) */                                             \
	DEF(ACOUSTIC_SCALE, "ACOUSTIC", DEF_NOTES(0, 2, 4, 6, 7, 9, 10))                                                   \
	/* ARAB Arabian (a.k.a. Major Locrian. matches Lumi's ARABIC_B scale) */                                           \
	DEF(ARABIAN_SCALE, "ARABIAN", DEF_NOTES(0, 2, 4, 5, 6, 8, 10))                                                     \
	/* ============================== 6-note scales ============================== */                                  \
	/* WHOL Whole Tone (matches Launchpad and Lumi scale) */                                                           \
	DEF(WHOLE_TONE_SCALE, "WHOLE TONE", DEF_NOTES(0, 2, 4, 6, 8, 10))                                                  \
	/* WHOL Whole Tone (matches Launchpad and Lumi scale) */                                                           \
	DEF(PROMETHEUS_SCALE, "PROMETHEUS", DEF_NOTES(0, 2, 4, 6, 9, 10))                                                  \
	/* AUGM Augmented (a.k.a Raga Devamani) */                                                                         \
	DEF(AUGMENTED_SCALE, "AUGMENTED", DEF_NOTES(0, 3, 4, 7, 8, 11))                                                    \
	/* BLUE Blues Minor (matches Launchpad and Lumi BLUES scale) */                                                    \
	DEF(BLUES_SCALE, "BLUES", DEF_NOTES(0, 3, 5, 6, 7, 10))                                                            \
	/* HEXA Hexatonic Minor (a.k.a. Raga Manirangu, Palasi) */                                                         \
	DEF(HEXATONIC_MINOR_SCALE, "HEXATONIC MINOR", DEF_NOTES(0, 2, 3, 5, 7, 10))                                        \
	/* ============================== 5-note scales ============================== */                                  \
	/* PENT Pentatonic Minor (a.k.a. Min'yo, Rāga Dhani. Matches Launchpad and Lumi scale) */                          \
	DEF(PENTATONIC_MINOR_SCALE, "PENTATONIC MINOR", DEF_NOTES(0, 3, 5, 7, 10))                                         \
	/* RAGA Raga Savethri (one of the possible Mixolydian Pentatonic variants) */                                      \
	DEF(RAGA_SAVETHRI_SCALE, "RAGA SAVETHRI", DEF_NOTES(0, 4, 5, 7, 10))                                               \
	/* KUMO Kumoi (a.k.a. Dorian Pentatonic, Raga Shivranjani) */                                                      \
	DEF(KUMOI, "KUMOI", DEF_NOTES(0, 2, 3, 7, 9))                                                                      \
	/* HIRA Hirajoshi (Actually is Aeolian Pentatonic or Yona Nuki Minor, though this matches Launchpad scale?) */     \
	DEF(HIRAJOSHI_SCALE, "HIRAJOSHI", DEF_NOTES(0, 2, 3, 7, 8))                                                        \
	/* BALI Balinese Pelog (a.k.a. Phrygian Pentatonic)*/                                                              \
	DEF(BALINESE_PELOG_SCALE, "BALINESE PELOG", DEF_NOTES(0, 1, 3, 7, 8))                                              \
	/* ============================== 8-note scales ============================== */                                  \
	/* DIMI Diminished Blues (a.k.a. Dominant Diminished. The first six notes approximate the Istrian scale) */        \
	DEF(DIMINISHED_BLUES_SCALE, "DIMINISHED BLUES", DEF_NOTES(0, 1, 3, 4, 6, 7, 9, 10))

/** Indexes into scalelikeNames and presetScaleNotes -arrays, and total
 *  number of preset scales.
 */
enum Scale : uint8_t {
#define DEF(id, name, notes) id,
	DEF_SCALES()
#undef DEF
	// clang-format off
	LAST_PRESET_SCALE = DIMINISHED_BLUES_SCALE,
	NUM_PRESET_SCALES,
	USER_SCALE = NUM_PRESET_SCALES,
	NUM_ALL_SCALES,
	RANDOM_SCALE = NUM_ALL_SCALES,
	NO_SCALE,
	NUM_SCALELIKE
	// clang-format on
};

#define FIRST_5_NOTE_SCALE_INDEX PENTATONIC_MINOR_SCALE
#define FIRST_6_NOTE_SCALE_INDEX WHOLE_TONE_SCALE

// These are scale ids / indexes as stored in flash memory, by the official
// firmware.
#define OFFICIAL_FIRMWARE_RANDOM_SCALE_INDEX 7
#define OFFICIAL_FIRMWARE_NONE_SCALE_INDEX 8

extern const NoteSet presetScaleNotes[NUM_PRESET_SCALES];
extern std::array<char const*, NUM_SCALELIKE> scalelikeNames;

const char* getScaleName(Scale scale);

Scale getScale(NoteSet notes);

bool isUserScale(NoteSet notes);

void ensureNotAllPresetScalesDisabled(std::bitset<NUM_PRESET_SCALES>& disabled);

// When storing scale ids in the flash storage we have eg. RANDOM_SCALE and
// NO_SCALE at 254 and 255 respectively, to leave rest of the range for future
// scales, but at runtime it is nicer to have them contiguous.
//
// Similarly, there are gaps in the range for future 6-note and 5-note scales.
//
// So these two functions handle conversion from flash storage domain to
// runtime domain.
Scale flashStorageCodeToScale(uint8_t code);
uint8_t scaleToFlashStorageCode(Scale scale);
