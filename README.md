# Prototype Series No.01
Chord-like sounds with a ribbon controller.

## Preparation
- OSC
  - LFO depth : S100
- EG
  - Type : Open
- ARP
  - On/Latch
  - Arpegglator setting : Octave (= Default)

## How to play
Change these parameters.
- OSC
  - A knob (Range of arpegglator)
  - B knob (Release time)
  - parameter 1 (Chord Type)
  - LFO rate (Arpegglator pattern)
- ARP
  - ARP pattern length (Arpegglator pattern)

# Parameters
- shape (NTS-1: A, other: Shape)  
Range of arpegglator.
- shiftshape (NTS-1: B, other: Shift+Shape)  
Release time.
- parameter 1  
Chord Type (1 .. 5)
- parameter 2  
Mode(Voice Assign) (1 .. 2)

# How to build
1. Clone (or download) and setup [logue-sdk](https://github.com/korginc/logue-sdk).
1. Clone (or download) this project.
1. Change `PLATFORMDIR` of Makefile according to your environment.

# LICENSE
Copyright 2020 Tomoaki Itoh
This software is released under the MIT License, see LICENSE.txt.

# AUTHOR
Tomoaki Itoh(neko) techno.cat.miau@gmail.com
