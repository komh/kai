/*
    Soft Mixer for K Audio Interface
    Copyright (C) 2021 by KO Myung-Hun <komh@chollian.net>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __KAI_MIXER_H_
#define __KAI_MIXER_H_

#include "kai.h"

#include "speex/speex_resampler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagBUFFER
{
    PCHAR pch;
    ULONG ulSize;
    ULONG ulLen;
    ULONG ulPos;
} BUFFER, *PBUFFER;

typedef struct tagMIXERSTREAM
{
    BOOL fPlaying;
    BOOL fPaused;
    BOOL fCompleted;
    BOOL fEOS;
    LONG lCountDown;

    SpeexResamplerState *srs;

    TID tid;
    PKAISPEC pksMixer;
    BUFFER buf;
    BUFFER bufFill;

    BOOL fMoreData;
    BOOL fFilling;

    HEV hevFill;
    HEV hevFillDone;
} MIXERSTREAM;

// VAC++ 3.08 does not like multiple definition of the same type
#ifndef PMIXERSTREAM_DEFINED
#define PMIXERSTREAM_DEFINED
typedef struct tagMIXERSTREAM *PMIXERSTREAM;
#endif

// VAC++ 3.08 does not like multiple definition of the same type
#ifndef PINSTANCELIST_DEFINED
#define PINSTANCELIST_DEFINED
typedef struct tagINSTANCELIST *PINSTANCELIST;
#endif

APIRET _kaiStreamOpen( PKAISPEC pksMixer, PHKAIMIXER phkm,
                       const PKAISPEC pksWanted, PKAISPEC pksObtained,
                       PHKAI phkai  );
APIRET _kaiStreamClose( PHKAIMIXER phkm, HKAIMIXERSTREAM hkms );
APIRET _kaiStreamPlay( PINSTANCELIST pil );
APIRET _kaiStreamStop( PINSTANCELIST pil );
APIRET _kaiStreamPause( PINSTANCELIST pil );
APIRET _kaiStreamResume( PINSTANCELIST pil );
APIRET _kaiStreamClearBuffer( PINSTANCELIST pil );
APIRET _kaiStreamStatus( PINSTANCELIST pil );

#define streamOpen          _kaiStreamOpen
#define streamClose         _kaiStreamClose
#define streamPlay          _kaiStreamPlay
#define streamStop          _kaiStreamStop
#define streamPause         _kaiStreamPause
#define streamResume        _kaiStreamResume
#define streamClearBuffer   _kaiStreamClearBuffer
#define streamStatus        _kaiStreamStatus

#ifdef __cplusplus
}
#endif

#endif
