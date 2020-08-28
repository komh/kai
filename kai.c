/*
    K Audio Interface library for OS/2
    Copyright (C) 2010-2015 by KO Myung-Hun <komh@chollian.net>

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

#define INCL_DOS
#include <os2.h>

#include <string.h>
#include <stdlib.h>

#include "kai.h"
#include "kai_internal.h"
#include "kai_dart.h"
#include "kai_uniaud.h"

#ifdef __KLIBC__
#include <emx/umalloc.h>

#define calloc _lcalloc
#define malloc _lmalloc
#endif

static ULONG    m_ulInitCount = 0;
static KAIAPIS  m_kai = { NULL, };
static KAICAPS  m_kaic = { 0, };

typedef struct tagINSTANCELIST INSTANCELIST, *PINSTANCELIST;
struct tagINSTANCELIST
{
    HKAI     hkai;
    LONG     lLeftVol;
    LONG     lRightVol;
    BOOL     fLeftState;
    BOOL     fRightState;
    BOOL     fSoftVol;
    KAISPEC  ks;
    PFNKAICB pfnUserCb;
    PVOID    pUserData;

    PINSTANCELIST    pilNext;
};

PINSTANCELIST m_pilStart = NULL;

#define APPLY_SOFT_VOLUME( ptype, buf, size, pi )                       \
do {                                                                    \
    ptype pEnd = ( ptype )( buf ) + ( size ) / sizeof( *pEnd );         \
    ptype p;                                                            \
                                                                        \
    for( p = ( buf ); p < pEnd; p++ )                                   \
    {                                                                   \
        *p = ( *p - ( pi )->ks.bSilence ) *                             \
             ( pi )->lLeftVol / 100 * !!( pi )->fLeftState +            \
             ( pi )->ks.bSilence;                                       \
                                                                        \
        if(( pi )->ks.ulChannels > 1 && ( p + 1 ) < pEnd )              \
        {                                                               \
            p++;                                                        \
            *p = ( *p - ( pil )->ks.bSilence ) *                        \
                 ( pi )->lRightVol / 100 * !!( pi )->fRightState +      \
                 ( pi )->ks.bSilence;                                   \
        }                                                               \
    }                                                                   \
} while( 0 )

static ULONG APIENTRY kaiCallBack( PVOID pCBData, PVOID pBuffer,
                                   ULONG ulBufSize )
{
    PINSTANCELIST pil = pCBData;

    ULONG ulLen = pil->pfnUserCb( pil->pUserData, pBuffer, ulBufSize );

    if( pil->fSoftVol &&
        ( pil->lLeftVol  != 100 || !pil->fLeftState ||
          pil->lRightVol != 100 || !pil->fRightState ))
    {
        switch( pil->ks.ulBitsPerSample )
        {
            case 8 :
                APPLY_SOFT_VOLUME( PBYTE, pBuffer, ulBufSize, pil );
                break;

            case 16 :
                APPLY_SOFT_VOLUME( PSHORT, pBuffer, ulBufSize, pil );
                break;

            case 32 :
            default :
                /* Oooops... Possible ? */
                break;
        }

    }

    return ulLen;
}

static PINSTANCELIST instanceNew( VOID )
{
    PINSTANCELIST pilNew;

    pilNew = calloc( 1, sizeof( INSTANCELIST ));
    if( !pilNew )
        return NULL;

    pilNew->lLeftVol    = 100;
    pilNew->lRightVol   = 100;
    pilNew->fLeftState  = TRUE;
    pilNew->fRightState = TRUE;

    // Use the soft volume mode unless KAI_NOSOFTVOLUME is specified
    pilNew->fSoftVol = getenv("KAI_NOSOFTVOLUME") ? FALSE : TRUE;

    return pilNew;
}

static void instanceAdd( HKAI hkai, PINSTANCELIST pil )
{
    pil->hkai    = hkai;
    pil->pilNext = m_pilStart;

    m_pilStart = pil;
}

static VOID instanceDel( HKAI hkai )
{
    PINSTANCELIST pil, pilPrev = NULL;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkai )
            break;

        pilPrev = pil;
    }

    if( !pil )
        return;

    if( pilPrev )
        pilPrev->pilNext = pil->pilNext;
    else
        m_pilStart = pil->pilNext;

    free( pil );
}

static VOID instanceDelAll( VOID )
{
    PINSTANCELIST pil, pilNext;

    for( pil = m_pilStart; pil; pil = pilNext )
    {
        pilNext = pil->pilNext;
        free( pil );
    }

    m_pilStart = NULL;
}

static PINSTANCELIST instanceVerify( HKAI hkai )
{
    PINSTANCELIST pil;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkai )
            break;
    }

    return pil;
}

APIRET DLLEXPORT APIENTRY kaiInit( ULONG ulMode )
{
    const char *pszAutoMode;
    APIRET rc = KAIE_INVALID_PARAMETER;

    DosEnterCritSec();

    if( m_ulInitCount )
    {
        m_ulInitCount++;
        DosExitCritSec();

        return KAIE_NO_ERROR;
    }

    // Use the specified mode by KAI_AUTOMODE if auto mode
    pszAutoMode = getenv("KAI_AUTOMODE");
    if( ulMode == KAIM_AUTO && pszAutoMode )
    {
        if( !stricmp(pszAutoMode, "UNIAUD"))
            ulMode = KAIM_UNIAUD;
        else if( !stricmp(pszAutoMode, "DART"))
            ulMode = KAIM_DART;
    }

    if( ulMode == KAIM_UNIAUD || ulMode == KAIM_AUTO )
    {
        rc = kaiUniaudInit( &m_kai, &m_kaic );
        if( !rc )
            ulMode = KAIM_UNIAUD;
    }

    if( ulMode == KAIM_DART || ulMode == KAIM_AUTO )
    {
        rc = kaiDartInit( &m_kai, &m_kaic );
        if( !rc )
            ulMode = KAIM_DART;
    }

    if( !rc )
        m_ulInitCount++;

    DosExitCritSec();

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiDone( VOID )
{
    APIRET rc;

    DosEnterCritSec();

    if( !m_ulInitCount )
        rc = KAIE_NOT_INITIALIZED;
    else if( m_ulInitCount == 1)
        rc = m_kai.pfnDone();
    else
        rc = KAIE_NO_ERROR;

    if( !rc )
    {
        m_ulInitCount--;

        if( m_ulInitCount == 0 )
            instanceDelAll();
    }

    DosExitCritSec();

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiCaps( PKAICAPS pkc )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !pkc )
        return KAIE_INVALID_PARAMETER;

    memcpy( pkc, &m_kaic, sizeof( KAICAPS ));

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiOpen( const PKAISPEC pksWanted,
                                   PKAISPEC pksObtained, PHKAI phkai )
{
    PINSTANCELIST pil;

    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !pksWanted || !pksObtained || !phkai )
        return KAIE_INVALID_PARAMETER;

    if( !pksWanted->pfnCallBack )
        return KAIE_INVALID_PARAMETER;

    pil = instanceNew();
    if( !pil )
        return KAIE_NOT_ENOUGH_MEMORY;

    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    pil->ks.pfnCallBack   = kaiCallBack;
    pil->ks.pCallBackData = pil;
    pil->pfnUserCb        = pksWanted->pfnCallBack;
    pil->pUserData        = pksWanted->pCallBackData;

    rc = m_kai.pfnOpen( &pil->ks, phkai );
    if( rc )
    {
        free( pil );

        return rc;
    }

    memcpy( pksObtained, &pil->ks, sizeof( KAISPEC ));
    pksObtained->pfnCallBack   = pksWanted->pfnCallBack;
    pksObtained->pCallBackData = pksWanted->pCallBackData;

    instanceAdd( *phkai, pil );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiClose( HKAI hkai )
{
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    rc = m_kai.pfnClose( hkai );
    if( rc )
        return rc;

    instanceDel( hkai );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiPlay( HKAI hkai )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnPlay( hkai );
}

APIRET DLLEXPORT APIENTRY kaiStop( HKAI hkai )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnStop( hkai );
}

APIRET DLLEXPORT APIENTRY kaiPause( HKAI hkai )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnPause( hkai );
}

APIRET DLLEXPORT APIENTRY kaiResume( HKAI hkai )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnResume( hkai );
}

APIRET DLLEXPORT APIENTRY kaiSetSoundState( HKAI hkai, ULONG ulCh,
                                            BOOL fState )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    if( pil->fSoftVol )
    {
        if( pil->ks.ulChannels == 1 ||
            ulCh == MCI_SET_AUDIO_LEFT || ulCh == MCI_SET_AUDIO_ALL )
            pil->fLeftState = fState;

        if( pil->ks.ulChannels > 1 &&
            ( ulCh == MCI_SET_AUDIO_RIGHT || ulCh == MCI_SET_AUDIO_ALL ))
            pil->fRightState = fState;

        return KAIE_NO_ERROR;
    }

    return m_kai.pfnSetSoundState( hkai, ulCh, fState );
}

APIRET DLLEXPORT APIENTRY kaiSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    if( usVol > 100 )
        usVol = 100;

    if( pil->fSoftVol )
    {
        if( pil->ks.ulChannels == 1 ||
            ulCh == MCI_SET_AUDIO_LEFT || ulCh == MCI_SET_AUDIO_ALL )
            pil->lLeftVol = usVol;

        if( pil->ks.ulChannels > 1 &&
            ( ulCh == MCI_SET_AUDIO_RIGHT || ulCh == MCI_SET_AUDIO_ALL ))
            pil->lRightVol = usVol;

        return KAIE_NO_ERROR;
    }

    return m_kai.pfnSetVolume( hkai, ulCh, usVol );
}

APIRET DLLEXPORT APIENTRY kaiGetVolume( HKAI hkai, ULONG ulCh )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !(pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    if( pil->fSoftVol )
    {
        LONG lLeftVol, lRightVol;

        lLeftVol = pil->lLeftVol;
        lRightVol = pil->ks.ulChannels > 1 ? pil->lRightVol : lLeftVol;

        if( ulCh == MCI_STATUS_AUDIO_LEFT )
            return lLeftVol;

        if( ulCh == MCI_STATUS_AUDIO_RIGHT )
            return lRightVol;

        /* ulCh == MCI_STATUS_AUDIO_ALL */
        return ( lLeftVol + lRightVol ) / 2;
    }

    return m_kai.pfnGetVolume( hkai, ulCh );
}

APIRET DLLEXPORT APIENTRY kaiClearBuffer( HKAI hkai )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnClearBuffer( hkai );
}

APIRET DLLEXPORT APIENTRY kaiStatus( HKAI hkai )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnStatus( hkai );
}

APIRET DLLEXPORT APIENTRY kaiEnableSoftVolume( HKAI hkai, BOOL fEnable )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    pil->fSoftVol = fEnable;

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiFloatToS16( short *dst, int dstLen,
                                         float *src, int srcLen )
{
    short *dstStart = dst;
    short *dstEnd = dst + dstLen / sizeof( *dst );
    float *srcEnd = src + srcLen / sizeof( *src );
    long l;

    while( dst < dstEnd && src < srcEnd )
    {
        float f = *src++;

        /* Convert float samples to s16 samples with rounding */
        if( f > 0 )
            f = f * 32767 + 0.5;
        else
            f = f * 32768 - 0.5;
        l = f;

        /* Bounds check */
        if( l > 32767 )
            l = 32767;
        else if( l < -32768 )
            l = -32768;

        *dst++ = ( short )l;
    }

    return (dst - dstStart) * sizeof( *dst );
}
