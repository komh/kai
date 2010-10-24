/*
    Direct Audio Interface for K Audio Interface
    Copyright (C) 2007-2010 by KO Myung-Hun <komh@chollian.net>
    Copyright (C) by Alex Strelnikov
    Copyright (C) 1998 by Andrew Zabolotny <bit@eltech.ru>

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

    Changes :
        KO Myung-Hun <komh@chollian.net> 2007/02/11
            - Removed the following variables in DARTSTRUCT
                BOOL  fResampleFlg;
                BOOL  fNormalizeFlg;
                ULONG ulLastTimeStamp;
                ULONG ulCurrTimeStamp;

            - Removed the following macros
                DART_CH_ALL      0
                DART_CH_LEFT     1
                DART_CH_RIGHT    2

            - Instead, use MCI_SET_AUDIO_* macros.

        KO Myung-Hun <komh@chollian.net> 2007/02/16
            - Changed fStopped to fPlaying in DARTSTRUCT

        KO Myung-Hun <komh@chollian.net> 2007/02/25
            - Removed the following variables in DARTSTRUCT
                BOOL               fPaused
                BOOL               fWaitStreamEnd
                ULONG              ulBytesPlayed
                ULONG              ulSeekPosition
                BOOL               fShareable
                ULONG              ulCurrVolume
                BOOL               fSamplesPlayed
                USHORT             usDeviceID
                PMCI_MIX_BUFFER    pMixBuffers
                MCI_MIXSETUP_PARMS MixSetupParms
                MCI_BUFFER_PARMS   BufferParms
                PFNDICB            pfndicb

        KO Myung-Hun <komh@chollian.net> 2008/03/30
            - Added callback data as a parameter to dartInit()

        KO Myung-Hun <komh@chollian.net> 2010/01/09
            - Revised for KAI
*/

#ifndef __KAI_DART_H__
#define __KAI_DART_H__

#include <os2.h>

#ifdef __cplusplus
extern "C" {
#endif

APIRET APIENTRY kaiDartInit( PKAIAPIS pkai, PULONG pulMaxChannels );
APIRET APIENTRY kaiOSLibGetAudioPDDName( PSZ pszPDDName );

#ifdef __cpluslus
}
#endif

#endif
