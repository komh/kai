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

#include "speex/speex_resampler.h"

#ifdef __KLIBC__
#include <emx/umalloc.h>

#define calloc _lcalloc
#define malloc _lmalloc
#endif

#define MIXER_SAMPLES           512
#define MIXER_MAX_SAMPLES       ( MIXER_SAMPLES * 8 )
#define MIXER_MAX_CHANNELS      2
#define MIXER_MAX_SAMPLEBITS    16  /* bits per sample */
#define MIXER_MAX_SAMPLEBYTES   2   /* bytes per sample */

#define SAMPLESTOBYTES( s, ks ) (( s ) * (( ks ).ulBitsPerSample >> 3 ) * \
                                 ( ks ).ulChannels )
#define BYTESTOSAMPLES( b, ks ) (( b ) / (( ks ).ulBitsPerSample >> 3 ) / \
                                 ( ks ).ulChannels )

static ULONG    m_ulInitCount = 0;
static KAIAPIS  m_kai = { NULL, };
static KAICAPS  m_kaic = { 0, };

static HMTX m_hmtx = NULLHANDLE;

typedef struct tagMIXERSTREAM
{
    BOOL fPlaying;
    BOOL fPaused;
    BOOL fCompleted;
    BOOL fEOS;

    CHAR pBuffer[ MIXER_MAX_SAMPLES * MIXER_MAX_SAMPLEBYTES *
                  MIXER_MAX_CHANNELS ];

    ULONG ulBufLen;
    ULONG ulBufPos;

    SpeexResamplerState *srs;
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

static PINSTANCELIST instanceNew( BOOL fStream )
{
    PINSTANCELIST pilNew;

    pilNew = calloc( 1, sizeof( INSTANCELIST ));
    if( !pilNew )
        return NULL;

    if( fStream )
    {
        pilNew->pms = calloc( 1, sizeof( MIXERSTREAM ));
        if( !pilNew->pms )
        {
            free( pilNew );

            return NULL;
        }
    }

    pilNew->lLeftVol    = 100;
    pilNew->lRightVol   = 100;
    pilNew->fLeftState  = TRUE;
    pilNew->fRightState = TRUE;

    // Use the soft volume mode unless KAI_NOSOFTVOLUME is specified
    pilNew->fSoftVol = getenv("KAI_NOSOFTVOLUME") ? FALSE : TRUE;

    // Mixer stream always use the soft volume mode
    pilNew->fSoftVol = pilNew->fSoftVol || fStream;

    return pilNew;
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

    free( pil->pms );
    free( pil );
}

static VOID instanceDelAll( VOID )
{
    PINSTANCELIST pil, pilNext;

    for( pil = m_pilStart; pil; pil = pilNext )
    {
        pilNext = pil->pilNext;
        free( pil->pms );
        free( pil );
    }

    m_pilStart = NULL;
}

static PINSTANCELIST instanceVerify( ULONG id )
{
    PINSTANCELIST pil;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->id == id )
            break;
    }

    return pil;
}

static int instanceStreamCount( HKAIMIXER hkm )
{
    PINSTANCELIST pil;
    int count = 0;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkm && pil->pms )
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
        if( pil->hkai == hkm && pil->pms && pil->pms->fPlaying )
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

    pil = instanceNew( FALSE );
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

    instanceAdd( *phkai, *phkai, pil );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiClose( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb || pil->pms )
        return KAIE_INVALID_HANDLE;

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

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
        return KAIE_INVALID_HANDLE;

    if( pil->pms )
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;
        APIRET rc = KAIE_NO_ERROR;

        pms = pil->pms;

        if( pms->fPlaying )
            return KAIE_NO_ERROR;

        DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

        if( instancePlayingStreamCount( pil->hkai ) == 0 )
            rc = m_kai.pfnPlay( pil->hkai );

        if( !rc )
        {
            pms->fPlaying = TRUE;
            pms->fPaused = FALSE;
            pms->fCompleted = FALSE;
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

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
        return KAIE_INVALID_HANDLE;

    if( pil->pms )
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;
        APIRET rc = KAIE_NO_ERROR;

        DosRequestMutexSem( m_hmtx, SEM_INDEFINITE_WAIT );

        if( instancePlayingStreamCount( pil->hkai ) == 1 )
            rc = m_kai.pfnStop( pil->hkai );

        if( !rc )
        {
            pms->fPlaying = FALSE;
            pms->fPaused = FALSE;
            pms->fCompleted = FALSE;
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

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
        return KAIE_INVALID_HANDLE;

    if( pil->pms )
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

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
        return KAIE_INVALID_HANDLE;

    if( pil->pms )
    {
        /* Mixer stream */
        PMIXERSTREAM pms = pil->pms;
        APIRET rc = KAIE_NO_ERROR;

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

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
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

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
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

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
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

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
        return KAIE_INVALID_HANDLE;

    /* TODO: for mixer-stream */

    return m_kai.pfnClearBuffer( hkai );
}

APIRET DLLEXPORT APIENTRY kaiStatus( HKAI hkai )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb )
        return KAIE_INVALID_HANDLE;

    if( pil->pms )
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

    /* Normal instance */
    return m_kai.pfnStatus( hkai );
}

APIRET DLLEXPORT APIENTRY kaiEnableSoftVolume( HKAI hkai, BOOL fEnable )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai )) || !pil->pfnUserCb || pil->pms )
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

static void normalize( PVOID pBuffer, ULONG ulLen, ULONG ulSize,
                       PINSTANCELIST pil, PINSTANCELIST pilMixer )
{
    short s16[ ulSize / sizeof( short )];

    int samples = BYTESTOSAMPLES( ulLen, pil->ks );

    PMIXERSTREAM pms = pil->pms;

    if( pil->ks.ulBitsPerSample == 8 )
    {
        /* 8 bits mono/stereo to 16 bits stereo */
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
        pBuffer = s16;
    }
    else if( pil->ks.ulBitsPerSample == 16 && pil->ks.ulChannels == 1 )
    {
        /* 16 bits mono to 16 bits stereo */
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

    if( pil->ks.ulSamplingRate != pilMixer->ks.ulSamplingRate )
    {
        /* resampling */
        speex_resampler_set_rate( pms->srs,
            pil->ks.ulSamplingRate, pilMixer->ks.ulSamplingRate );

        spx_uint32_t inSamples = samples;
        spx_uint32_t outSamples = MIXER_MAX_SAMPLES;

        speex_resampler_process_interleaved_int( pms->srs,
            ( spx_int16_t * )pBuffer, &inSamples,
            ( spx_int16_t * )pms->pBuffer, &outSamples );

        ulLen = SAMPLESTOBYTES( outSamples, pilMixer->ks );
    }
    else
    {
        /* straight copy */
        memcpy( pms->pBuffer, pBuffer, ulLen );
    }

    pms->ulBufLen = ulLen;
    pms->ulBufPos = 0;
}

static ULONG APIENTRY kaiMixerCallBack( PVOID pCBData, PVOID pBuffer,
                                        ULONG ulBufSize )
{
    PINSTANCELIST pilMixer = pCBData;
    CHAR pBuf[ ulBufSize ];

    PINSTANCELIST pil;
    ULONG ulMaxLen = 0;

    int samples = BYTESTOSAMPLES( ulBufSize, pilMixer->ks );

    memset( pBuffer, 0, ulBufSize );

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        PMIXERSTREAM pms;
        ULONG ulLen = 0;
        ULONG ulSize = ulBufSize;

        if( !pil->pms || pil->hkai != pilMixer->hkai )
            continue;

        pms = pil->pms;

        if( !pms->fPlaying || pms->fPaused )
            continue;


        if( pms->fEOS )
        {
            pms->fPlaying = FALSE;
            pms->fPaused = FALSE;
            pms->fCompleted = TRUE;
            pms->fEOS = FALSE;

            continue;
        }

        ULONG ulReqSize = SAMPLESTOBYTES( samples, pil->ks );
        ULONG ulRetLen = ulReqSize;

        while( ulSize > 0 && ulRetLen == ulReqSize )
        {
            if( pms->ulBufLen == 0 )
            {
                CHAR pCBBuf[ ulBufSize ];

                ulRetLen = pil->pfnUserCb( pil->pUserData, pCBBuf, ulReqSize );

                normalize( pCBBuf, ulRetLen, ulBufSize, pil, pilMixer );
            }

            ULONG ulCopyLen = ulSize < pms->ulBufLen ? ulSize : pms->ulBufLen;

            memcpy( pBuf + ulLen, pms->pBuffer + pms->ulBufPos,
                    ulCopyLen );

            pms->ulBufPos += ulCopyLen;
            pms->ulBufLen -= ulCopyLen;

            ulLen  += ulCopyLen;
            ulSize -= ulCopyLen;
        }

        if( pil->fSoftVol &&
            ( pil->lLeftVol  != 100 || !pil->fLeftState ||
              pil->lRightVol != 100 || !pil->fRightState ))
            APPLY_SOFT_VOLUME( PSHORT, pBuf, ulLen, pil );

        if( ulLen < ulBufSize )
        {
            pms->fEOS = TRUE;

            memset( pBuf + ulLen, pilMixer->ks.bSilence, ulBufSize - ulLen );
            ulLen = ulBufSize;
        }

        short *pDst = pBuffer;
        short *pSrc = ( short * )pBuf;
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

    pil = instanceNew( FALSE );
    if( !pil )
        return KAIE_NOT_ENOUGH_MEMORY;

    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    pil->ks.pfnCallBack   = kaiMixerCallBack;
    pil->ks.pCallBackData = pil;
    pil->pfnUserCb        = NULL;
    pil->pUserData        = NULL;

    rc = m_kai.pfnOpen( &pil->ks, phkm );
    if( rc )
    {
        free( pil );

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
    PINSTANCELIST pilMixer;
    APIRET rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pilMixer = instanceVerify( hkm )) || pilMixer->pfnUserCb )
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

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pilMixer = instanceVerify( hkm )) || pilMixer->pfnUserCb )
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

    pil = instanceNew( TRUE );
    if( pil )
    {
        int err;
        pil->pms->srs = speex_resampler_init( pilMixer->ks.ulChannels,
                                              pksWanted->ulSamplingRate,
                                              pilMixer->ks.ulSamplingRate,
                                              SPEEX_RESAMPLER_QUALITY_DESKTOP,
                                              &err );
    }

    if( !pil || !pil->pms->srs )
    {
        free( pil );

        return KAIE_NOT_ENOUGH_MEMORY;
    }

    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    pil->ks.pfnCallBack   = NULL;
    pil->ks.pCallBackData = NULL;
    pil->pfnUserCb        = pksWanted->pfnCallBack;
    pil->pUserData        = pksWanted->pCallBackData;

    memcpy( pksObtained, &pil->ks, sizeof( KAISPEC ));
    pksObtained->usDeviceIndex  = pilMixer->ks.usDeviceIndex;
    pksObtained->ulNumBuffers   = pilMixer->ks.ulNumBuffers;
    pksObtained->ulBufferSize   = pilMixer->ks.ulBufferSize;
    pksObtained->fShareable     = TRUE;
    pksObtained->pfnCallBack    = pksWanted->pfnCallBack;
    pksObtained->pCallBackData  = pksWanted->pCallBackData;
    pksObtained->bSilence       = pksWanted->ulBitsPerSample == 8 ? 0x80 : 0;

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

    if( !( pilMixer = instanceVerify( hkm )) || pilMixer->pfnUserCb )
        return KAIE_INVALID_HANDLE;

    if( !( pilStream = instanceVerify( hkms )) ||
        pilStream->hkai != pilMixer->hkai )
        return KAIE_INVALID_HANDLE;

    speex_resampler_destroy( pilStream->pms->srs );

    instanceDel( hkms );

    return KAIE_NO_ERROR;
}
