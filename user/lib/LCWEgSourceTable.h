/*
Copyright 2020 Tomoaki Itoh
This software is released under the MIT License, see LICENSE.txt.
//*/

#include <stdint.h>

#define LCW_EG_SOURCE_TABLE_BITS (8)
#define LCW_EG_SOURCE_TABLE_SIZE (256)
#define LCW_EG_SOURCE_TABLE_MASK (0xFF)
#define LCW_EG_SOURCE_VALUE_MAX  (0x10000000)

extern const int32_t gLcwEgSourceTable[LCW_EG_SOURCE_TABLE_SIZE];

