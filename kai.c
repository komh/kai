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
static HKAI     m_hkai = NULLHANDLE;
static KAISPEC  m_ks = { 0, };

static HMTX m_hmtx = NULLHANDLE;

typedef struct tagINSTANCELIST INSTANCELIST, *PINSTANCELIST;
struct tagINSTANCELIST
{
    HKAI     hkai;
    LONG     lLeftVol;
    LONG     lRightVol;
    BOOL     fLeftState;
    BOOL     fRightState;
    BOOL     fSoftVol;
    BOOL     fPlaying;
    BOOL     fPaused;
    BOOL     fCompleted;
    BOOL     fEOS;
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

static ULONG normalize( PVOID pBuffer, ULONG ulLen, ULONG ulSize,
                       PINSTANCELIST pil )
{
    short s16[ ulSize / sizeof( short )];

    int samples = ulLen / pil->ks.ulChannels /
                  ( pil->ks.ulBitsPerSample >> 3 );

    if( pil->ks.ulBitsPerSample == 8 )
    {
        unsigned char *pu8 = pBuffer;
        short *ps16 = s16;

        int i;

        for( i = 0; i < samples; i++ )
        {
            int sample = *pu8++ * 65535 / 255 - 32768;

            *ps16++ = sample;

            if( pil->ks.ulChannels == 1 )
                *ps16++ = sample;
        }

        ulLen = ( ps16 - s16 ) * sizeof( *ps16 );
        memcpy( pBuffer, s16, ulLen );
    }
    else if( pil->ks.ulBitsPerSample == 16 && pil->ks.ulChannels == 1 )
    {
        short *ps16m = pBuffer;
        short *ps16s = s16;

        int i;

        for( i = 0; i < samples; i++ )
        {
            *ps16s++ = *ps16m;
            *ps16s++ = *ps16m++;
        }

        ulLen = ( ps16s - s16 ) * sizeof( *ps16s );
        pBuffer = s16;
    }

    if( pil->fSoftVol &&
        ( pil->lLeftVol  != 100 || !pil->fLeftState ||
          pil->lRightVol != 100 || !pil->fRightState ))
    {
#if 0
        switch( pil->ks.ulBitsPerSample )
        {
            case 8 :
                APPLY_SOFT_VOLUME( PBYTE, pBuffer, ulLen, pil );
                break;

            case 16 :
#endif
                APPLY_SOFT_VOLUME( PSHORT, pBuffer, ulLen, pil );
#if 0
                break;

            case 32 :
            default :
                /* Oooops... Possible ? */
                break;
        }
#endif
    }

    return ulLen;
}

static ULONG APIENTRY kaiCallBack( PVOID pCBData, PVOID pBuffer,
                                   ULONG ulBufSize )
{
    PINSTANCELIST pilStart = *( PINSTANCELIST * )pCBData;
    short pBuf[ ulBufSize ];

    PINSTANCELIST pil;
    ULONG ulMaxLen = 0;

    int samples = ulBufSize / m_ks.ulChannels / ( m_ks.ulBitsPerSample >> 3 );

    memset( pBuffer, 0, ulBufSize );

    for( pil = pilStart; pil; pil = pil->pilNext )
    {
        if( !pil->fPlaying || pil->fPaused )
            continue;

        if( pil->fEOS )
        {
            pil->fPlaying = FALSE;
            pil->fPaused = FALSE;
            pil->fCompleted = TRUE;
            pil->fEOS = FALSE;

            continue;
        }

        ULONG ulLen = pil->pfnUserCb( pil->pUserData, pBuf,
                                      samples * pil->ks.ulChannels *
                                      ( pil->ks.ulBitsPerSample >> 3 ));

        ulLen = normalize( pBuf, ulLen, ulBufSize, pil );

        if( ulLen < ulBufSize )
        {
            pil->fEOS = TRUE;

            memset((( CHAR * )pBuf) + ulLen, m_ks.bSilence,
                   ulBufSize - ulLen );
            ulLen = ulBufSize;
        }

        short *pDst = pBuffer;
        short *pSrc = pBuf;
        short *pEnd = pSrc + ulLen / sizeof( *pSrc );

        while( pSrc < pEnd )
        {
            int sample = *pDst;

            sample += *pSrc++;

            if( sample > 32767 )
                sample = 32767;
            else if( sample < -32768 )
                sample = -32768;

            *pDst++ = sample;
        }

        if( ulMaxLen < ulLen )
            ulMaxLen = ulLen;
    }

    return ulMaxLen;
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

static int instanceCount( VOID )
{
    PINSTANCELIST pil;
    int count = 0;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
        count++;

    return count;
}

static int instancePlayingCount( VOID )
{
    PINSTANCELIST pil;
    int count = 0;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->fPlaying )
            count++;
    }

    return count;
}

APIRET DLLEXPORT APIENTRY kaiInit( ULONG ulMode )
{
    const char *pszAutoMode;
    APIRET rc = KAIE_INVALID_PARAMETER;

    DosEnterCritSec();

    if( !m_hmtx && DosCreateMutexSem( NULL, &m_hmtx, 0, FALSE ))
    {
        m_hmtx = NULLHANDLE;

        DosExitCritSec();

        return KAIE_NOT_ENOUGH_MEMORY;
    }

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
        {
            instanceDelAll();

            DosCloseMutexSem( m_hmtx );
        }
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

    DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

    if( instanceCount() == 0 )
    {
        m_ks.usDeviceIndex    = 0;
        m_ks.ulType           = KAIT_PLAY;
        m_ks.ulBitsPerSample  = 16;     /* 16 bits/sample only */
        m_ks.ulSamplingRate   = pksWanted->ulSamplingRate;
        m_ks.ulDataFormat     = 0;
        m_ks.ulChannels       = 2;      /* stereo channels only */
        m_ks.ulNumBuffers     = 2;
        m_ks.ulBufferSize     = 512 * ( m_ks.ulBitsPerSample >> 3 ) *
                                m_ks.ulChannels; /* 512 samples */
        m_ks.fShareable       = TRUE;
        m_ks.pfnCallBack      = kaiCallBack;
        m_ks.pCallBackData    = &m_pilStart;

        rc = m_kai.pfnOpen( &m_ks, &m_hkai );
        if( rc )
        {
            DosReleaseMutexSem( m_hmtx );

            return rc;
        }
    }

    pil = instanceNew();
    if( !pil )
    {
        DosReleaseMutexSem( m_hmtx );

        return KAIE_NOT_ENOUGH_MEMORY;
    }

    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    pil->ks.pfnCallBack   = kaiCallBack;
    pil->ks.pCallBackData = pil;
    pil->pfnUserCb        = pksWanted->pfnCallBack;
    pil->pUserData        = pksWanted->pCallBackData;

    memcpy( pksObtained, &pil->ks, sizeof( KAISPEC ));
    pksObtained->pfnCallBack   = pksWanted->pfnCallBack;
    pksObtained->pCallBackData = pksWanted->pCallBackData;
    pksObtained->bSilence = pksObtained->ulBitsPerSample == 8 ? 0x80 : 0x00;

    *phkai = ( HKAI )pil;

    instanceAdd( *phkai, pil );

    DosReleaseMutexSem( m_hmtx );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiClose( HKAI hkai )
{
    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

    instanceDel( hkai );

    if( instanceCount() == 0 )
        rc = m_kai.pfnClose( m_hkai );

    DosReleaseMutexSem( m_hmtx );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiPlay( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    if( pil->fPlaying )
        return KAIE_NO_ERROR;

    DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

    if( instancePlayingCount() == 0 )
        rc = m_kai.pfnPlay( m_hkai );

    if( !rc )
    {
        pil->fPlaying = TRUE;
        pil->fPaused = FALSE;
        pil->fCompleted = FALSE;
    }

    DosReleaseMutexSem( m_hmtx );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiStop( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

    if( instancePlayingCount() == 1 )
        rc = m_kai.pfnStop( m_hkai );

    if( !rc )
    {
        pil->fPlaying = FALSE;
        pil->fPaused = FALSE;
        pil->fCompleted = FALSE;
    }

    DosReleaseMutexSem( m_hmtx );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiPause( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    if( pil->fPaused )
        return KAIE_NO_ERROR;

    DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

    if( instancePlayingCount() == 1 )
        rc = m_kai.pfnPause( m_hkai );

    if( !rc )
        pil->fPaused = TRUE;

    DosReleaseMutexSem( m_hmtx );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiResume( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

    if( instancePlayingCount() == 1 )
        rc = m_kai.pfnResume( m_hkai );

    if( !rc )
        pil->fPaused = FALSE;

    return rc;
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

    return m_kai.pfnSetSoundState( m_hkai, ulCh, fState );
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

    return m_kai.pfnSetVolume( m_hkai, ulCh, usVol );
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

    return m_kai.pfnGetVolume( m_hkai, ulCh );
}

APIRET DLLEXPORT APIENTRY kaiClearBuffer( HKAI hkai )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnClearBuffer( m_hkai );
}

APIRET DLLEXPORT APIENTRY kaiStatus( HKAI hkai )
{
    PINSTANCELIST pil;
    ULONG ulStatus;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )))
        return KAIE_INVALID_HANDLE;

    ulStatus = 0;

    if( pil->fPlaying )
        ulStatus |= KAIS_PLAYING;

    if( pil->fPaused )
        ulStatus |= KAIS_PAUSED;

    if( pil->fCompleted )
        ulStatus |= KAIS_COMPLETED;

    return ulStatus;
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
