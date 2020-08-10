/*
Copyright 2020 Tomoaki Itoh
This software is released under the MIT License, see LICENSE.txt.
//*/

#include "userosc.h"
#include "LCWCommon.h"
#include "LCWPitchTable.h"
#include "LCWClipCurveTable.h"
#include "LCWEgSourceTable.h"
#include "LCWEgReleaseTable.h"

typedef struct {
  int32_t noteNo;
  uint32_t t;
  uint32_t dt;
  uint32_t gateTime;
  int32_t trigger;
  int32_t isAlive;
  int32_t egMode;
  UQ8_24 egTimer;
  SQ3_28 egOut;
} VoiceBlock;

#define NUMBER_OF_VOICE (8)
static VoiceBlock voiceBlocks[NUMBER_OF_VOICE];
static VoiceBlock *activeBlocks[NUMBER_OF_VOICE];

#define DEFAULT_GATE_TIME ((uint32_t)(48000.f * 0.0001f))

static struct {
  int16_t shape = 0;
  int16_t shiftshape = 0; // = Release Time
  int16_t chord = 0;
  int16_t mode = 0;
} s_param;

static struct {
  int32_t shape_lfo = 0; // = OSC_CYCLEで更新したshape_lfo
} s_modulation;

#define LCW_SOFT_CLIP_FRAC_BITS (16 - LCW_CLIP_CURVE_FRAC_BITS)
#define LCW_SOFT_CLIP_FRAC_MASK ((1 << LCW_SOFT_CLIP_FRAC_BITS) - 1)
__fast_inline int32_t softClip(SQ15_16 src)
{
  int32_t ret = (int32_t)( gLcwClipCurveTable[LCW_CLIP_CURVE_TABLE_SIZE - 1] );

  const int32_t x = LCW_ABS( src );
  const int32_t i = x >> LCW_SOFT_CLIP_FRAC_BITS;
  if ( i < (LCW_CLIP_CURVE_TABLE_SIZE - 1) ) {
    const int32_t y0 = (int32_t)gLcwClipCurveTable[i];
    const int32_t y1 = (int32_t)gLcwClipCurveTable[i + 1];
    const int32_t dy = y1 - y0;
    const int32_t frac = x & LCW_SOFT_CLIP_FRAC_MASK;
    ret = y0 + ((dy * frac) >> LCW_SOFT_CLIP_FRAC_BITS);
  }

  return ( src < 0 ) ? -ret : ret;
}

static int32_t noteOff(int32_t noteNo)
{
  for (int32_t i=0; i<NUMBER_OF_VOICE; i++) {
    // memo:
    // 本来の処理は、
    // 見つかったblockをgateTimeを過ぎているblockの手前に移動して、Releaseに移行
    if ( activeBlocks[i]->noteNo == noteNo ) {
        VoiceBlock *block = activeBlocks[i];
        if ( block->isAlive ) {
          block->trigger = 0;
          return i;
        }
    }
  }

  return -1;
}

static void noteOffAll(void)
{
  for (int32_t i=0; i<NUMBER_OF_VOICE; i++) {
    if ( activeBlocks[i]->trigger ) {
        VoiceBlock *block = activeBlocks[i];
        block->trigger = 0;
    }
  }
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
  for (int32_t i=0; i<NUMBER_OF_VOICE; i++) {
    VoiceBlock *p = &(voiceBlocks[0]) + i;
    p->noteNo = 0;
    p->t = 0;
    p->dt = 0;
    p->trigger = 0;
    p->isAlive = 0;
    activeBlocks[i] = p;
  }
}

#define LCW_OSC_TIMER_MAX ((1 << LCW_PITCH_DELTA_VALUE_BITS) - 1)
#define LCW_OSC_TIMER_MASK (LCW_OSC_TIMER_MAX - 1)
#define LCW_EG_TIMER_FRAC_BITS (28 - LCW_EG_SOURCE_TABLE_BITS)
#define LCW_EG_TIMER_FRAC_MASK ((1 << LCW_EG_TIMER_FRAC_BITS) - 1)
void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{
  // Shapeに対するLFOの値をサンプリング（ノートオン時に参照する）
  s_modulation.shape_lfo = params->shape_lfo;

  const uint32_t egReleaseParam =
    (clipmaxu32(s_param.shiftshape, 0x3FF) * LCW_EG_RELEASE_PARAM_TABLE_SIZE) >> 10;
  const UQ4_28 egDelta = gLcwEgReleaseParamTable[egReleaseParam];

  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;

  for (; y != y_e; ) {
    int32_t out = 0;
    for (int32_t i=0; i<NUMBER_OF_VOICE; i++) {
      VoiceBlock *block = activeBlocks[i];
      block->t = (block->t + block->dt) & LCW_OSC_TIMER_MASK;

      // egMode=0 : Attack
      // egMode=1 : Release
      if ( block->egMode == 0 ) {
        if ( 0 < block->gateTime ) { block->gateTime--; }
        if ( block->gateTime == 0 ) {
          block->egMode = 1;
          block->egTimer = 0;
        }
      }
      else {
        block->egTimer += egDelta;
        int64_t gain =
          gLcwEgSourceTable[block->egTimer >> LCW_EG_TIMER_FRAC_BITS];
        block->egTimer &= LCW_EG_TIMER_FRAC_MASK;

        int64_t tmp = (int64_t)block->egOut * gain;
        block->egOut = (int32_t)( tmp >> 28 );
        //if ( block->egOut < 536870/*LCW_SQ3_28(0.002)*/ ) {
        if ( block->egOut < 5368700/*LCW_SQ3_28(0.02)*/ ) {
          block->isAlive = 0;
        }
      }

      if ( block->isAlive ) {
        out += (LCW_OSC_TIMER_MAX >> 1) < block->t
          ? block->egOut : -block->egOut;
      }
    }

    out = clipminmaxi32( -4095, softClip(out >> (28-16)), 4095 );
    *(y++) = ( out << (31 - 12) );
  }
}

// ポルタメントやピッチベンドが加味されると正しく動作しない
void OSC_NOTEON(const user_osc_param_t * const params)
{
  // If changed array length of chordTable, update "manifest.json".
  // ["Chrd",   0,  4,  ""]
  //                ^ = Array length - 1
  static const struct {
    int16_t table[8];
    int16_t n;
  } chordTable[] = {
    { { 0, 2, 4, 5, 7, 9,11, 0 }, 7 }, // C,D,E,F,G,A,B from C
    { { 0, 3, 7, 0, 0, 0, 0, 0 }, 3 }, // C
    { { 0, 4, 7,11, 0, 0, 0, 0 }, 4 }, // CM7
    { { 0, 3, 7, 0, 0, 0, 0, 0 }, 3 }, // Cm
    { { 0, 3, 7,10, 0, 0, 0, 0 }, 4 }  // Cm7
  };

  // 空きがなければ鳴らさない
  if (activeBlocks[NUMBER_OF_VOICE - 1]->isAlive) {
    return;
  }

  const int16_t *noteDeltaTable = &(chordTable[s_param.chord].table[0]);
  const int32_t n = chordTable[s_param.chord].n;

  // 四捨五入でノートNo.を確定
  const int32_t noteNo0 = ((int32_t)params->pitch + 0x80) >> 8;

  // テーブルにマッピング
  const SQ15_16 tmp = (s_modulation.shape_lfo >> 20) * (s_param.shape >> 3);
  const SQ15_16 frac = (SQ15_16)((uint32_t)tmp & 0xFFFF);
  const int32_t delta = noteDeltaTable[(frac * n) >> 16] + ((tmp >> 16) * 12);
  const int32_t noteNo = noteNo0 + delta;

  // s11.20に拡張してから、整数部がoctaveになるように加工
  int32_t pitch = ((noteNo - LCW_NOTE_NO_A4) << 20) / 12;

  // memo:
  // 最後に使用したblockが先頭にくるように並び替え
  int32_t pos = noteOff( noteNo );
  VoiceBlock *block = activeBlocks[0];
  if ( pos < 0 ) {
    for (int32_t i=1; i<NUMBER_OF_VOICE; i++) {
      if ( !block->isAlive ) {
        break;
      }

      // 鳴り終わってないblockを1つずつ下にずらしていく
      VoiceBlock *tmp = activeBlocks[i];
      activeBlocks[i] = block;
      block = tmp;
    }
  }
  else {
    if ( s_param.mode ) {
      return;
    }

    for (int32_t i=1; i<=pos; i++) {
      // 同じノートNo.のblockを確保できるまで、1つずつ下にずらしていく
      VoiceBlock *tmp = activeBlocks[i];
      activeBlocks[i] = block;
      block = tmp;
    }
  }

  block->noteNo = noteNo;
  block->dt = pitch_to_timer_delta(pitch >> (20 - 16));
  block->trigger = 1;
  block->isAlive = 1;
  block->gateTime = DEFAULT_GATE_TIME;
  block->egMode = 0;
  block->egTimer = 0;
  block->egOut = LCW_SQ3_28(1.0);

  // 割り当てblockが確定
  activeBlocks[0] = block;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
}

void OSC_PARAM(uint16_t index, uint16_t value)
{
  switch (index) {
  case k_user_osc_param_shape:
    s_param.shape = (int16_t)value;
    break;
  case k_user_osc_param_shiftshape:
    s_param.shiftshape = (int16_t)value;
    break;
  case k_user_osc_param_id1:
    s_param.chord = (int16_t)value;
    break;
  case k_user_osc_param_id2:
    s_param.mode = (int16_t)value;
    break;
  default:
    break;
  }
}
