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
#define INCL_DOSERRORS
#include <os2.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "kai.h"
#include "kai_internal.h"
#include "kai_dart.h"
#include "kai_uniaud.h"

#include "speex/speex_resampler.h"

#ifdef __KLIBC__
#include <emx/umalloc.h>

#define calloc _lcalloc
#define malloc _lmalloc
#endif

#ifdef __WATCOMC__
#include <alloca.h>
#include <process.h>
#endif

#define SAMPLESTOBYTES( s, ks ) (( s ) * (( ks ).ulBitsPerSample >> 3 ) * \
                                 ( ks ).ulChannels )
#define BYTESTOSAMPLES( b, ks ) (( b ) / (( ks ).ulBitsPerSample >> 3 ) / \
                                 ( ks ).ulChannels )

#define IVF_NORMAL  0x0001
#define IVF_MIXER   0x0002
#define IVF_STREAM  0x0004
#define IVF_ANY     ( IVF_NORMAL | IVF_MIXER | IVF_STREAM )

#define DEFAULT_MIN_SAMPLES 2048

static ULONG    m_ulInitCount = 0;
static KAIAPIS  m_kai = { NULL, };
static KAICAPS  m_kaic = { 0, };
static BOOL     m_fSoftVol = FALSE;
static ULONG    m_ulMinSamples = DEFAULT_MIN_SAMPLES;

static BOOL      m_fSoftMixer = FALSE;
static int       m_iResamplerQ = 0;
static HKAIMIXER m_hkm = NULLHANDLE;
static KAISPEC   m_ks = {
    0,                                      /* usDeviceIndex */
    KAIT_PLAY,                              /* ulType */
    16,                                     /* ulBitsPerSample */
    48000,                                  /* ulSamplingRate */
    0,                                      /* ulDataFormat */
    2,                                      /* ulChannels */
    2,                                      /* ulNumBuffers */
    DEFAULT_MIN_SAMPLES * 2/* 16 bits */ *
    2/* stereo */,                          /* ulBufferSize */
    TRUE,                                   /* fShareable */
    NULL,                                   /* pfnCallBack */
    NULL,                                   /* pCallBackData */
    0,                                      /* bSilence */
};

static HMTX m_hmtx = NULLHANDLE;

static BOOL m_fDebugMode = FALSE;

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

    HEV hevFill;
    HEV hevFillDone;
} MIXERSTREAM, *PMIXERSTREAM;

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

static PINSTANCELIST m_pilStart = NULL;

#define APPLY_SOFT_VOLUME( ptype, buf, size, pi )                       \
do {                                                                    \
    ptype pEnd = ( ptype )( buf ) + ( size ) / sizeof( *pEnd );         \
    ptype p;                                                            \
                                                                        \
    for( p = ( ptype )( buf ); p < pEnd; p++ )                          \
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

static void mixerFillThread( void *arg );

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

static void instanceFree( PINSTANCELIST pil );

static PINSTANCELIST instanceNew( BOOL fStream,
                                  PKAISPEC pksMixer, PKAISPEC pks )
{
    PINSTANCELIST pilNew;

    pilNew = calloc( 1, sizeof( INSTANCELIST ));
    if( !pilNew )
        return NULL;

    if( fStream )
    {
        ULONG ulBufSize = pksMixer->ulBufferSize * pksMixer->ulSamplingRate /
                          pks->ulSamplingRate;
        PMIXERSTREAM pms;

        if( pksMixer->ulSamplingRate % pks->ulSamplingRate )
            ulBufSize += pksMixer->ulBufferSize;

        pms = pilNew->pms = calloc( 1, sizeof( MIXERSTREAM ));
        if( pms )
        {
            pms->buf.pch = malloc( ulBufSize );
            pms->bufFill.pch = malloc( ulBufSize );
        }

        if( !pms || !pms->buf.pch || !pms->bufFill.pch )
        {
            instanceFree( pilNew );

            return NULL;
        }

        pms->pksMixer = pksMixer;
        pms->buf.ulSize = ulBufSize;
        pms->bufFill.ulSize = ulBufSize;
    }

    pilNew->lLeftVol    = 100;
    pilNew->lRightVol   = 100;
    pilNew->fLeftState  = TRUE;
    pilNew->fRightState = TRUE;
    pilNew->fSoftVol    = m_fSoftVol ||
                          // Mixer stream always use the soft volume mode
                          fStream;

    return pilNew;
}

static void instanceFree( PINSTANCELIST pil )
{
    if( pil )
    {
        PMIXERSTREAM pms = pil->pms;

        if( pms )
        {
            free( pms->buf.pch );
            free( pms->bufFill.pch );

            free( pms );
        }

        free( pil );
    }
}

static void instanceAdd( ULONG id, HKAI hkai, PINSTANCELIST pil )
{
    pil->id      = id;
    pil->hkai    = hkai;
    pil->pilNext = m_pilStart;

    m_pilStart = pil;
}

static VOID instanceDel( ULONG id )
{
    PINSTANCELIST pil, pilPrev = NULL;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->id == id )
            break;

        pilPrev = pil;
    }

    if( !pil )
        return;

    if( pilPrev )
        pilPrev->pilNext = pil->pilNext;
    else
        m_pilStart = pil->pilNext;

    instanceFree( pil );
}

static VOID instanceDelAll( VOID )
{
    PINSTANCELIST pil, pilNext;

    for( pil = m_pilStart; pil; pil = pilNext )
    {
        pilNext = pil->pilNext;

        instanceFree( pil );
    }

    m_pilStart = NULL;
}

static PINSTANCELIST instanceStart( VOID )
{
    return m_pilStart;
}

static PINSTANCELIST instanceVerify( ULONG id, ULONG ivf )
{
    PINSTANCELIST pil;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->id == id)
        {
            if(( ivf & IVF_NORMAL ) && ISNORMAL( pil ))
                break;

            if(( ivf & IVF_MIXER ) && ISMIXER( pil ))
                break;

            if(( ivf & IVF_STREAM ) && ISSTREAM( pil ))
                break;

            /* Oooops... not matched! */
            return NULL;
        }
    }

    return pil;
}

static int instanceStreamCount( HKAIMIXER hkm )
{
    PINSTANCELIST pil;
    int count = 0;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkm && ISSTREAM( pil ))
            count++;
    }

    return count;
}

static int instancePlayingStreamCount( HKAIMIXER hkm )
{
    PINSTANCELIST pil;
    int count = 0;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkm && ISSTREAM( pil ) && pil->pms->fPlaying )
            count++;
    }

    return count;
}

APIRET DLLEXPORT APIENTRY kaiInit( ULONG ulMode )
{
    const char *pszMinSamples;
    const char *pszResamplerQ;
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

    m_fDebugMode = getenv("KAI_DEBUG") != NULL;

    // Use the soft volume mode unless KAI_NOSOFTVOLUME is specified
    m_fSoftVol = getenv("KAI_NOSOFTVOLUME") ? FALSE : TRUE;

    // Use the soft mixer mode unless KAI_NOSOFTMIXER is specified
    m_fSoftMixer = getenv("KAI_NOSOFTMIXER") ? FALSE : TRUE;

    // Use the minimum samples of KAI_MINSAMPLES if specified
    pszMinSamples = getenv("KAI_MINSAMPLES");
    if( pszMinSamples )
    {
        ULONG ulMinSamples = atoi( pszMinSamples );

        if( ulMinSamples != 0 )
        {
            m_ulMinSamples = ulMinSamples;
            m_ks.ulBufferSize = SAMPLESTOBYTES( m_ulMinSamples, m_ks );
        }
    }

    // User the resampler quality of KAI_RESAMPLERQ if specified
    pszResamplerQ = getenv("KAI_RESAMPLERQ");
    if( pszResamplerQ )
    {
        int q = atoi( pszResamplerQ );

        if( q < 0 )
            q = 0;
        else if( q > 10 )
            q = 10;

        m_iResamplerQ = q;
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

    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !pksWanted || !pksObtained || !phkai )
        return KAIE_INVALID_PARAMETER;

    if( !pksWanted->pfnCallBack )
        return KAIE_INVALID_PARAMETER;

    if( m_fSoftMixer )
    {
        /* Soft mixer mode */
        DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

        if( !m_hkm )
        {
            KAISPEC ksObtained;

            rc = kaiMixerOpen( &m_ks, &ksObtained, &m_hkm );
            if( !rc )
                memcpy( &m_ks, &ksObtained, sizeof( KAISPEC ));
        }

        if( !rc && m_hkm )
            rc = kaiMixerStreamOpen( m_hkm, pksWanted, pksObtained, phkai );

        DosReleaseMutexSem( m_hmtx );

        return rc;
    }

    /* Normal mode */
    pil = instanceNew( FALSE, NULL, NULL );
    if( !pil )
        return KAIE_NOT_ENOUGH_MEMORY;

    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    if( pil->ks.ulBufferSize > 0 &&
        pil->ks.ulBufferSize < SAMPLESTOBYTES( m_ulMinSamples, pil->ks ))
        pil->ks.ulBufferSize = SAMPLESTOBYTES( m_ulMinSamples, pil->ks );
    pil->ks.pfnCallBack   = kaiCallBack;
    pil->ks.pCallBackData = pil;
    pil->pfnUserCb        = pksWanted->pfnCallBack;
    pil->pUserData        = pksWanted->pCallBackData;

    rc = m_kai.pfnOpen( &pil->ks, phkai );
    if( rc )
    {
        instanceFree( pil );

        return rc;
    }

    memcpy( pksObtained, &pil->ks, sizeof( KAISPEC ));
    pksObtained->pfnCallBack   = pksWanted->pfnCallBack;
    pksObtained->pCallBackData = pksWanted->pCallBackData;

    instanceAdd( *phkai, *phkai, pil );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiClose( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    if( ISSTREAM( pil ) && pil->hkai != m_hkm )
        return KAIE_INVALID_HANDLE;

    if( m_fSoftMixer )
    {
        /* Soft mixer mode */
        DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

        rc = kaiMixerStreamClose( m_hkm, hkai );
        if( !rc && instanceStreamCount( m_hkm ) == 0 )
        {
            rc = kaiMixerClose( m_hkm );
            if( !rc )
                m_hkm = NULLHANDLE;
        }

        DosReleaseMutexSem( m_hmtx );

        return rc;
    }

    /* Normal mode */
    rc = m_kai.pfnClose( hkai );
    if( rc )
        return rc;

    instanceDel( hkai );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiPlay( HKAI hkai )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;
        ULONG ulCount;
        APIRET rc = KAIE_NO_ERROR;

        if( pms->fPlaying )
            return KAIE_NO_ERROR;

        DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

        /* Set fPlaying to TRUE before calling pfnPlay() to prevent
           calling call back function from being stopped */
        pms->fPlaying = TRUE;
        pms->fPaused = FALSE;
        pms->fCompleted = FALSE;

        pms->buf.ulLen = 0;
        pms->buf.ulPos = 0;

        pms->fMoreData = TRUE;

        DosPostEventSem( pms->hevFill );
        DosResetEventSem( pms->hevFillDone, &ulCount );

        pms->tid = _beginthread( mixerFillThread, NULL, THREAD_STACK_SIZE,
                                 pil );

        if( instancePlayingStreamCount( pil->hkai ) == 1 )
            rc = m_kai.pfnPlay( pil->hkai );

        if( !rc && ( kaiStatus( pil->hkai ) & KAIS_PAUSED ))
            rc = m_kai.pfnResume( pil->hkai );

        if( rc )
        {
            /* If error, clear */
            kaiStop( pil->id );
        }

        DosReleaseMutexSem( m_hmtx );

        return rc;
    }

    /* Normal instance */
    return m_kai.pfnPlay( hkai );
}

APIRET DLLEXPORT APIENTRY kaiStop( HKAI hkai )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;
        APIRET rc = KAIE_NO_ERROR;

        if( !pms->fPlaying )
            return KAIE_NO_ERROR;

        DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

        if( instancePlayingStreamCount( pil->hkai ) == 1 )
            rc = m_kai.pfnStop( pil->hkai );

        if( !rc )
        {
            pms->fPlaying = FALSE;
            pms->fPaused = FALSE;

            DosPostEventSem( pms->hevFill );
            while( DosWaitThread( &pms->tid, DCWW_WAIT ) == ERROR_INTERRUPT );
        }

        DosReleaseMutexSem( m_hmtx );

        return rc;
    }

    /* Normal instance */
    return m_kai.pfnStop( hkai );
}

APIRET DLLEXPORT APIENTRY kaiPause( HKAI hkai )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;
        APIRET rc = KAIE_NO_ERROR;

        if( pms->fPaused )
            return KAIE_NO_ERROR;

        DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

        if( instancePlayingStreamCount( pil->hkai ) == 1 )
            rc = m_kai.pfnPause( pil->hkai );

        if( !rc )
            pms->fPaused = TRUE;

        DosReleaseMutexSem( m_hmtx );

        return rc;
    }

    /* Normal instance */
    return m_kai.pfnPause( hkai );
}

APIRET DLLEXPORT APIENTRY kaiResume( HKAI hkai )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;
        APIRET rc = KAIE_NO_ERROR;

        if( !pms->fPaused )
            return KAIE_NO_ERROR;

        DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

        if( instancePlayingStreamCount( pil->hkai ) == 1 )
            rc = m_kai.pfnResume( pil->hkai );

        if( !rc )
            pms->fPaused = FALSE;

        return rc;
    }

    /* Normal instance */
    return m_kai.pfnResume( hkai );
}

APIRET DLLEXPORT APIENTRY kaiSetSoundState( HKAI hkai, ULONG ulCh,
                                            BOOL fState )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
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

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
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

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
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
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;

        memset( pms->buf.pch, pil->ks.bSilence, pms->buf.ulSize );

        return KAIE_NO_ERROR;
    }

    /* Normal instance */
    return m_kai.pfnClearBuffer( hkai );
}

APIRET DLLEXPORT APIENTRY kaiStatus( HKAI hkai )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_ANY )))
        return KAIE_INVALID_HANDLE;

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;
        ULONG ulStatus = 0;

        if( pms->fPlaying )
            ulStatus |= KAIS_PLAYING;

        if( pms->fPaused )
            ulStatus |= KAIS_PAUSED;

        if( pms->fCompleted )
            ulStatus |= KAIS_COMPLETED;

        return ulStatus;
    }

    /* Normal instance or mixer instance */
    return m_kai.pfnStatus( hkai );
}

APIRET DLLEXPORT APIENTRY kaiEnableSoftVolume( HKAI hkai, BOOL fEnable )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL )))
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

static void normalize( PVOID pBuffer, ULONG ulLen, PINSTANCELIST pil )
{
    PKAISPEC pks = &pil->ks;
    PMIXERSTREAM pms = pil->pms;
    PKAISPEC pksMixer = pms->pksMixer;

    short *ps16Buf = alloca( pksMixer->ulBufferSize );

    int samples = BYTESTOSAMPLES( ulLen, *pks );

    if( pks->ulBitsPerSample == 8 )
    {
        /* 8 bits mono/stereo to 16 bits stereo */
        unsigned char *pu8 = pBuffer;
        short *ps16 = ps16Buf;

        int i;

        for( i = 0; i < samples; i++ )
        {
            int sample = *pu8++ * 65535 / 255 - 32768;

            *ps16++ = sample;

            if( pks->ulChannels == 1 )
                *ps16++ = sample;
        }

        ulLen = ( ps16 - ps16Buf ) * sizeof( *ps16 );
        pBuffer = ps16Buf;
    }
    else if( pks->ulBitsPerSample == 16 && pks->ulChannels == 1 )
    {
        /* 16 bits mono to 16 bits stereo */
        short *ps16m = pBuffer;
        short *ps16s = ps16Buf;

        int i;

        for( i = 0; i < samples; i++ )
        {
            *ps16s++ = *ps16m;
            *ps16s++ = *ps16m++;
        }

        ulLen = ( ps16s - ps16Buf ) * sizeof( *ps16s );
        pBuffer = ps16Buf;
    }

    if( pks->ulSamplingRate != pksMixer->ulSamplingRate )
    {
        /* resampling */
        spx_uint32_t inSamples = samples;
        spx_uint32_t outSamples =
            BYTESTOSAMPLES( pms->bufFill.ulSize, *pksMixer );

        speex_resampler_set_rate( pms->srs,
            pks->ulSamplingRate, pksMixer->ulSamplingRate );

        speex_resampler_process_interleaved_int( pms->srs,
            ( spx_int16_t * )pBuffer, &inSamples,
            ( spx_int16_t * )pms->bufFill.pch, &outSamples );

        ulLen = SAMPLESTOBYTES( outSamples, *pksMixer );
    }
    else
    {
        /* straight copy */
        memcpy( pms->bufFill.pch, pBuffer, ulLen );
    }

    pms->bufFill.ulLen = ulLen;
}

static void mixerFillThread( void *arg )
{
    PINSTANCELIST pil = arg;
    PMIXERSTREAM pms = pil->pms;
    PKAISPEC pks = &pil->ks;
    ULONG ulSize = pks->ulBufferSize;
    ULONG ulLen = ulSize;
    PCHAR pchBuf = alloca( ulSize );

    while( pms->fMoreData )
    {
        ULONG ulPost;

        while( DosWaitEventSem( pms->hevFill, SEM_INDEFINITE_WAIT ) ==
                    ERROR_INTERRUPT );

        DosResetEventSem( pms->hevFill, &ulPost );

        if( !pms->fPlaying )
            break;

        ulLen = pil->pfnUserCb( pil->pUserData, pchBuf, ulSize );
        normalize( pchBuf, ulLen, pil );

        if( ulLen < ulSize )
            pms->fMoreData = FALSE;

        DosPostEventSem( pms->hevFillDone );
    }
}

static ULONG APIENTRY kaiMixerCallBack( PVOID pCBData, PVOID pBuffer,
                                        ULONG ulBufSize )
{
    PINSTANCELIST pilMixer = pCBData;
    PCHAR pchBuf = alloca( ulBufSize );

    PINSTANCELIST pil;
    ULONG ulMaxLen = 0;

    memset( pBuffer, 0, ulBufSize );

    for( pil = instanceStart(); pil; pil = pil->pilNext )
    {
        PMIXERSTREAM pms = pil->pms;
        ULONG ulLen = 0;
        ULONG ulRem = ulBufSize;

        short *pDst;
        short *pSrc;
        short *pEnd;

        if( !ISSTREAM( pil ) || pil->hkai != pilMixer->hkai || !pms->fPlaying )
            continue;

        if( pms->fEOS && --pms->lCountDown == 0 )
        {
            pms->fPlaying = FALSE;
            pms->fPaused = FALSE;
            pms->fCompleted = TRUE;
            pms->fEOS = FALSE;

            /* Terminate fill thread */
            DosPostEventSem( pms->hevFill );

            continue;
        }

        if( pms->fEOS || pms->fPaused ||
            ( pms->fMoreData && pms->buf.ulLen < ulBufSize &&
              DosWaitEventSem( pms->hevFillDone, SEM_IMMEDIATE_RETURN )))
        {
            memset( pchBuf, 0, ulBufSize );
            ulLen = ulBufSize;
        }
        else
        {
            ULONG ulCopyLen;

            if( pms->buf.ulLen < ulBufSize )
            {
                /* hevFillDone posted */

                ULONG ulCount;

                DosResetEventSem( pms->hevFillDone, &ulCount );

                /* Copy remained buffer */
                memcpy( pchBuf, pms->buf.pch + pms->buf.ulPos,
                        pms->buf.ulLen );

                ulLen += pms->buf.ulLen;
                ulRem -= pms->buf.ulLen;

                memcpy( pms->buf.pch, pms->bufFill.pch, pms->bufFill.ulLen );
                pms->buf.ulLen = pms->bufFill.ulLen;
                pms->buf.ulPos = 0;

                if( pms->fMoreData )
                    DosPostEventSem( pms->hevFill );
                else
                    pms->bufFill.ulLen = 0;
            }

            ulCopyLen = ulRem < pms->buf.ulLen ? ulRem : pms->buf.ulLen;
            memcpy( pchBuf + ulLen, pms->buf.pch + pms->buf.ulPos, ulCopyLen );

            pms->buf.ulLen -= ulCopyLen;
            pms->buf.ulPos += ulCopyLen;

            ulLen += ulCopyLen;
            ulRem -= ulCopyLen;
        }

        if( pil->fSoftVol &&
            ( pil->lLeftVol  != 100 || !pil->fLeftState ||
              pil->lRightVol != 100 || !pil->fRightState ))
            APPLY_SOFT_VOLUME( PSHORT, pchBuf, ulLen, pil );

        if( ulLen < ulBufSize )
        {
            pms->fEOS = TRUE;
            pms->lCountDown = pil->ks.ulNumBuffers;

            memset( pchBuf + ulLen, 0, ulRem );

            ulLen = ulBufSize;
        }

        pDst = pBuffer;
        pSrc = ( short * )pchBuf;
        pEnd = pSrc + ulLen / sizeof( *pSrc );

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

APIRET DLLEXPORT APIENTRY kaiMixerOpen( const PKAISPEC pksWanted,
                                        PKAISPEC pksObtained,
                                        PHKAIMIXER phkm )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !pksWanted || !pksObtained || !phkm )
        return KAIE_INVALID_PARAMETER;

    /* Support 16 bits stereo audio only */
    if( pksWanted->ulBitsPerSample != 16 || pksWanted->ulChannels != 2 )
        return KAIE_INVALID_PARAMETER;

    pil = instanceNew( FALSE, NULL, NULL );
    if( !pil )
        return KAIE_NOT_ENOUGH_MEMORY;

    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    if( pil->ks.ulBufferSize > 0 &&
        pil->ks.ulBufferSize < SAMPLESTOBYTES( m_ulMinSamples, pil->ks ))
        pil->ks.ulBufferSize = SAMPLESTOBYTES( m_ulMinSamples, pil->ks );
    pil->ks.pfnCallBack   = kaiMixerCallBack;
    pil->ks.pCallBackData = pil;
    pil->pfnUserCb        = NULL;
    pil->pUserData        = NULL;

    rc = m_kai.pfnOpen( &pil->ks, phkm );
    if( rc )
    {
        instanceFree( pil );

        return rc;
    }

    memcpy( pksObtained, &pil->ks, sizeof( KAISPEC ));
    pksObtained->pfnCallBack   = NULL;
    pksObtained->pCallBackData = NULL;

    instanceAdd( *phkm, *phkm, pil );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiMixerClose( HKAIMIXER hkm )
{
    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkm, IVF_MIXER ))
        return KAIE_INVALID_HANDLE;

    if( instanceStreamCount( hkm ) != 0 )
        return KAIE_STREAMS_NOT_CLOSED;

    rc = m_kai.pfnClose( hkm );
    if( rc )
        return rc;

    instanceDel( hkm );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiMixerStreamOpen( HKAIMIXER hkm,
                                              const PKAISPEC pksWanted,
                                              PKAISPEC pksObtained,
                                              PHKAIMIXERSTREAM phkms )
{
    PINSTANCELIST pilMixer;
    PINSTANCELIST pil;
    ULONG ulBufSize;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pilMixer = instanceVerify( hkm, IVF_MIXER )))
        return KAIE_INVALID_HANDLE;

    if( !pksWanted || !pksObtained || !phkms )
        return KAIE_INVALID_PARAMETER;

    if( !pksWanted->pfnCallBack )
        return KAIE_INVALID_PARAMETER;

    if( pksWanted->ulType != pilMixer->ks.ulType )
        return KAIE_INVALID_PARAMETER;

    if( pksWanted->ulBitsPerSample > pilMixer->ks.ulBitsPerSample )
        return KAIE_INVALID_PARAMETER;

    if( pksWanted->ulChannels > pilMixer->ks.ulChannels )
        return KAIE_INVALID_PARAMETER;

    ulBufSize = pilMixer->ks.ulBufferSize *
                pilMixer->ks.ulSamplingRate / pksWanted->ulSamplingRate;
    if( pilMixer->ks.ulSamplingRate % pksWanted->ulSamplingRate )
        ulBufSize += pilMixer->ks.ulBufferSize;

    pil = instanceNew( TRUE, &pilMixer->ks, pksWanted );
    if( pil )
    {
        int err;
        pil->pms->srs = speex_resampler_init( pilMixer->ks.ulChannels,
                                              pksWanted->ulSamplingRate,
                                              pilMixer->ks.ulSamplingRate,
                                              m_iResamplerQ, &err );

        DosCreateEventSem( NULL, &pil->pms->hevFill, 0, FALSE );
        DosCreateEventSem( NULL, &pil->pms->hevFillDone, 0, FALSE );
    }

    if( !pil || !pil->pms->srs )
    {
        instanceFree( pil );

        return KAIE_NOT_ENOUGH_MEMORY;
    }

    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    pil->ks.usDeviceIndex = pilMixer->ks.usDeviceIndex;
    pil->ks.ulNumBuffers  = pilMixer->ks.ulNumBuffers;
    pil->ks.ulBufferSize  =
        SAMPLESTOBYTES( BYTESTOSAMPLES(pilMixer->ks.ulBufferSize,
                                       pilMixer->ks), pil->ks);
    pil->ks.fShareable    = TRUE;
    pil->ks.pfnCallBack   = NULL;
    pil->ks.pCallBackData = NULL;
    pil->ks.bSilence      = pksWanted->ulBitsPerSample == 8 ? 0x80 : 0;
    pil->pfnUserCb        = pksWanted->pfnCallBack;
    pil->pUserData        = pksWanted->pCallBackData;

    memcpy( pksObtained, &pil->ks, sizeof( KAISPEC ));
    pksObtained->pfnCallBack   = pksWanted->pfnCallBack;
    pksObtained->pCallBackData = pksWanted->pCallBackData;

    *phkms = ( HKAIMIXERSTREAM )pil;

    instanceAdd( *phkms, pilMixer->hkai, pil );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiMixerStreamClose( HKAIMIXER hkm,
                                               HKAIMIXERSTREAM hkms )
{
    PINSTANCELIST pilMixer;
    PINSTANCELIST pilStream;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pilMixer = instanceVerify( hkm, IVF_MIXER )))
        return KAIE_INVALID_HANDLE;

    if( !( pilStream = instanceVerify( hkms, IVF_STREAM )) ||
        pilStream->hkai != pilMixer->hkai )
        return KAIE_INVALID_HANDLE;

    kaiStop( hkms );

    DosCloseEventSem( pilStream->pms->hevFill );
    DosCloseEventSem( pilStream->pms->hevFillDone );

    speex_resampler_destroy( pilStream->pms->srs );

    instanceDel( hkms );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiEnableSoftMixer( BOOL fEnable,
                                              const PKAISPEC pks )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    m_fSoftMixer = fEnable;
    if( fEnable && pks )
        memcpy( &m_ks, pks, sizeof( KAISPEC ));

    return KAIE_NO_ERROR;
}
