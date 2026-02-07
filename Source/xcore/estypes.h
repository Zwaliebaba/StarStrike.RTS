//============================================================================
//
// File: estypes.h
//  
// Copyright (c) 1999-2007, Ensemble Studios
//
//============================================================================
#pragma once

// Standard types
typedef unsigned char     uchar;
typedef unsigned short    ushort;
typedef unsigned int      uint;

typedef char              int8;
typedef unsigned char     uint8;

typedef short             int16;
typedef unsigned short    uint16;

typedef int               int32;
typedef unsigned int      uint32;

typedef __int64           int64;
typedef unsigned __int64  uint64;

// min/max values of our standard types
const uint8 UINT8_MIN   = 0x00;

const uint16 UINT16_MIN = 0x0000;

const uint32 UINT32_MIN = 0x00000000;

const uint64 UINT64_MIN = 0x0000000000000000;
