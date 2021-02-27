/*
    Instances for K Audio Interface
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

#ifndef __KAI_INSTANCE_H_
#define __KAI_INSTANCE_H_

#include "kai.h"
#include "kai_mixer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IVF_NORMAL  0x0001
#define IVF_MIXER   0x0002
#define IVF_STREAM  0x0004
#define IVF_ANY     ( IVF_NORMAL | IVF_MIXER | IVF_STREAM )

typedef struct tagINSTANCELIST INSTANCELIST, *PINSTANCELIST;
struct tagINSTANCELIST
{
    ULONG    id;
    HKAI     hkai;
    LONG     lLeftVol;
    LONG     lRightVol;
    BOOL     fLeftState;
    BOOL     fRightState;
    BOOL     fSoftVol;
    KAISPEC  ks;
    PFNKAICB pfnUserCb;
    PVOID    pUserData;

    PMIXERSTREAM pms;

    PINSTANCELIST    pilNext;
};

#define ISNORMAL( pil ) (( pil )->pfnUserCb && !( pil )->pms )
#define ISMIXER( pil )  (!( pil )->pfnUserCb && !( pil )->pms )
#define ISSTREAM( pil ) (( pil )->pfnUserCb && ( pil )->pms )

PINSTANCELIST instanceNew( BOOL fStream, PKAISPEC pksMixer, PKAISPEC pks );
VOID instanceFree( PINSTANCELIST pil );
VOID instanceAdd( ULONG id, HKAI hkai, PINSTANCELIST pil );
VOID instanceDel( ULONG id );
VOID instanceDelAll( VOID );
PINSTANCELIST instanceStart( VOID );
PINSTANCELIST instanceVerify( ULONG id, ULONG ivf );
LONG instanceStreamCount( HKAIMIXER hkm );
LONG instancePlayingStreamCount( HKAIMIXER hkm );

#ifdef __cplusplus
}
#endif

#endif
