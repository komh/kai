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
#include "kai_spinlock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IVF_NORMAL  0x0001
#define IVF_MIXER   0x0002
#define IVF_STREAM  0x0004
#define IVF_ANY     ( IVF_NORMAL | IVF_MIXER | IVF_STREAM )

// VAC++ 3.08 does not like multiple definition of the same type
#ifndef PMIXERSTREAM_DEFINED
#define PMIXERSTREAM_DEFINED
typedef struct tagMIXERSTREAM *PMIXERSTREAM;
#endif

typedef struct tagINSTANCELIST INSTANCELIST;

// VAC++ 3.08 does not like multiple definition of the same type
#ifndef PINSTANCELIST_DEFINED
#define PINSTANCELIST_DEFINED
typedef struct tagINSTANCELIST *PINSTANCELIST;
#endif

struct tagINSTANCELIST
{
    ULONG    id;
    HKAI     hkai;
    LONG     lLeftVol;
    LONG     lRightVol;
    BOOL     fLeftState;
    BOOL     fRightState;
    BOOL     fSoftVol;
    SPINLOCK lock;
    KAISPEC  ks;
    PFNKAICB pfnUserCb;
    PVOID    pUserData;

    PMIXERSTREAM pms;

    PINSTANCELIST    pilNext;
};

#define ISNORMAL( pil ) (( pil )->pfnUserCb && !( pil )->pms )
#define ISMIXER( pil )  (!( pil )->pfnUserCb && !( pil )->pms )
#define ISSTREAM( pil ) (( pil )->pfnUserCb && ( pil )->pms )

PINSTANCELIST _kaiInstanceNew( BOOL fStream, PKAISPEC pksMixer, PKAISPEC pks );
VOID _kaiInstanceFree( PINSTANCELIST pil );
VOID _kaiInstanceAdd( ULONG id, HKAI hkai, PINSTANCELIST pil );
VOID _kaiInstanceDel( ULONG id );
VOID _kaiInstanceDelAll( VOID );
VOID _kaiInstanceLoop( VOID ( *callback )( PINSTANCELIST, VOID * ),
                       VOID *arg );
PINSTANCELIST _kaiInstanceVerify( ULONG id, ULONG ivf );
LONG _kaiInstanceStreamCount( HKAIMIXER hkm );
LONG _kaiInstancePlayingStreamCount( HKAIMIXER hkm );

#define instanceNew                 _kaiInstanceNew
#define instanceFree                _kaiInstanceFree
#define instanceAdd                 _kaiInstanceAdd
#define instanceDel                 _kaiInstanceDel
#define instanceDelAll              _kaiInstanceDelAll
#define instanceLoop                _kaiInstanceLoop
#define instanceVerify              _kaiInstanceVerify
#define instanceStreamCount         _kaiInstanceStreamCount
#define instancePlayingStreamCount  _kaiInstancePlayingStreamCount
#define instanceLock( pil )         spinLock( &( pil )->lock )
#define instanceUnlock( pil )       spinUnlock( &( pil )->lock )

#ifdef __cplusplus
}
#endif

#endif
