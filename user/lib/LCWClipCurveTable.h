/*
Copyright 2020 Tomoaki Itoh
This software is released under the MIT License, see LICENSE.txt.
//*/

#include <stdint.h>

#define LCW_CLIP_CURVE_FRAC_BITS (6)
#define LCW_CLIP_CURVE_TABLE_SIZE (257)

#define LCW_CLIP_CURVE_VALUE_BITS (12)
#define LCW_CLIP_CURVE_VALUE_MAX (1 << (LCW_CLIP_CURVE_VALUE_BITS))

extern const uint16_t gLcwClipCurveTable[LCW_CLIP_CURVE_TABLE_SIZE];

