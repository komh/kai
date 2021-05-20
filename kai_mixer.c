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

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include <stdlib.h>
#include <string.h>

#ifdef __WATCOMC__
#include <alloca.h>
#include <process.h>
#endif

#include "kai.h"
#include "kai_internal.h"
#include "kai_mixer.h"
#include "kai_instance.h"
#include "kai_debug.h"
#include "kai_atomic.h"

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

    boostThread();

    while( pms->fMoreData )
    {
        ULONG ulPost;

        while( DosWaitEventSem( pms->hevFill, SEM_INDEFINITE_WAIT ) ==
                    ERROR_INTERRUPT )
            /* nothing */;

        DosResetEventSem( pms->hevFill, &ulPost );

        if( !pms->fFilling )
            break;

        ulLen = pil->pfnUserCb( pil->pUserData, pchBuf, ulSize );
        normalize( pchBuf, ulLen, pil );

        if( ulLen < ulSize )
            STORE( &pms->fMoreData, FALSE );

        DosPostEventSem( pms->hevFillDone );
    }
}

APIRET _kaiStreamOpen( PKAISPEC pksMixer, PHKAIMIXER phkm,
                       const PKAISPEC pksWanted, PKAISPEC pksObtained,
                       PHKAI phkai  )
{
    HKAIMIXER hkm = *phkm;
    KAISPEC ksMixerObtained;
    ULONG rc = KAIE_NO_ERROR;

    if( !*phkm )
        rc = kaiMixerOpen( pksMixer, &ksMixerObtained, &hkm );

    if( !rc )
        rc = kaiMixerStreamOpen( hkm, pksWanted, pksObtained, phkai );

    // tried to open a mixer at this time ?
    if( !*phkm )
    {
        // all succeeded ?
        if( !rc )
        {
            // copy obtained values
            memcpy( pksMixer, &ksMixerObtained, sizeof( KAISPEC ));
            *phkm = hkm;
        }
        else if( hkm )              // error but mixer opened ?
            kaiMixerClose( hkm );   // then close
    }

    return rc;
}

APIRET _kaiStreamClose( PHKAIMIXER phkm, HKAIMIXERSTREAM hkms )
{
    ULONG rc;

    rc = kaiMixerStreamClose( *phkm, hkms );
    if( !rc && instanceStreamCount( *phkm ) == 0 )
    {
        rc = kaiMixerClose( *phkm );
        if( !rc )
            *phkm = NULLHANDLE;
    }

    return rc;
}

APIRET _kaiStreamPlay( PINSTANCELIST pil )
{
    PMIXERSTREAM pms = pil->pms;
    PINSTANCELIST pilMixer;
    ULONG ulCount;
    APIRET rc = KAIE_NO_ERROR;

    if( pms->fPlaying )
        return KAIE_NO_ERROR;

    pms->buf.ulLen = 0;
    pms->buf.ulPos = 0;

    DosPostEventSem( pms->hevFill );
    DosResetEventSem( pms->hevFillDone, &ulCount );

    STORE( &pms->fMoreData, TRUE );
    STORE( &pms->fFilling, TRUE );

    pms->tid = _beginthread( mixerFillThread, NULL, THREAD_STACK_SIZE, pil );

    // prevent initial buffer-underrun and unnecessary latency
    DosWaitEventSem( pms->hevFillDone, INITIAL_TIMEOUT );

    pilMixer = instanceVerify( pil->hkai, IVF_MIXER );

    instanceLock( pilMixer );

    /* Set fPlaying to TRUE before calling pfnPlay() to prevent
       calling call back function from being stopped */
    STORE( &pms->fPlaying, TRUE );
    STORE( &pms->fPaused, FALSE );
    STORE( &pms->fCompleted, FALSE );

    if( instancePlayingStreamCount( pil->hkai ) == 1 )
    {
        /* Ensure to stop playing in sub-system before trying to play in
           sub-system because sub-system plays a little more even if playing
           a mixer stream is completed. Otherwise sub-system does not start
           to play because it thinks it is already playing */
        _kaiGetApi()->pfnStop( pil->hkai );

        rc = _kaiGetApi()->pfnPlay( pil->hkai );
        if( rc )
            streamStop( pil );  // clean up
    }

    instanceUnlock( pilMixer );

    return rc;
}

APIRET _kaiStreamStop( PINSTANCELIST pil )
{
    PMIXERSTREAM pms = pil->pms;
    PINSTANCELIST pilMixer;
    APIRET rc = KAIE_NO_ERROR;

    if( !pms->fPlaying )
        return KAIE_NO_ERROR;

    pilMixer = instanceVerify( pil->hkai, IVF_MIXER );

    instanceLock( pilMixer );

    if( instancePlayingStreamCount( pil->hkai ) == 1 )
        rc = _kaiGetApi()->pfnStop( pil->hkai );

    if( !rc )
    {
        STORE( &pms->fFilling, FALSE );

        DosPostEventSem( pms->hevFill );
        while( DosWaitThread( &pms->tid, DCWW_WAIT ) == ERROR_INTERRUPT )
            /* nothing */;

        STORE( &pms->fPlaying, FALSE );
        STORE( &pms->fPaused, FALSE );
    }

    instanceUnlock( pilMixer );

    return rc;
}

APIRET _kaiStreamPause( PINSTANCELIST pil )
{
    PMIXERSTREAM pms = pil->pms;

    if( !pms->fPlaying )
        return KAIE_NO_ERROR;

    if( pms->fPaused )
        return KAIE_NO_ERROR;

    STORE( &pms->fPaused, TRUE );

    return KAIE_NO_ERROR;
}

APIRET _kaiStreamResume( PINSTANCELIST pil )
{
    PMIXERSTREAM pms = pil->pms;

    if( !pms->fPlaying )
        return KAIE_NO_ERROR;

    if( !pms->fPaused )
        return KAIE_NO_ERROR;

    STORE( &pms->fPaused, FALSE );

    return KAIE_NO_ERROR;
}

APIRET _kaiStreamClearBuffer( PINSTANCELIST pil )
{
    PMIXERSTREAM pms = pil->pms;

    memset( pms->buf.pch, pil->ks.bSilence, pms->buf.ulSize );

    return KAIE_NO_ERROR;
}

APIRET _kaiStreamStatus( PINSTANCELIST pil )
{
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

typedef struct fillBufferArgs
{
    PINSTANCELIST pilMixer;
    PVOID pBuffer;
    ULONG ulBufSize;
    PCHAR pchBuf;
    ULONG ulMaxLen;
    ULONG ulTimeout;
} FILLBUFFERARGS, *PFILLBUFFERARGS;

static VOID fillBuffer( PINSTANCELIST pil, void *arg )
{
    PFILLBUFFERARGS pargs = arg;
    PINSTANCELIST pilMixer = pargs->pilMixer;
    PVOID pBuffer = pargs->pBuffer;
    ULONG ulBufSize = pargs->ulBufSize;
    PCHAR pchBuf = pargs->pchBuf;
    ULONG ulTimeout = pargs->ulTimeout;

    PMIXERSTREAM pms = pil->pms;
    ULONG ulLen = 0;
    ULONG ulRem = ulBufSize;

    short *pDst;
    short *pSrc;
    short *pEnd;

    if( !ISSTREAM( pil ) || pil->hkai != pilMixer->hkai || !pms->fPlaying )
        return;

    /* wWiting to play remainig buffers passed to audio driver such as DART */
    if( pms->fEOS && --pms->lCountDown == 0 )
    {
        STORE( &pms->fFilling, FALSE );

        /* Terminate fill thread */
        DosPostEventSem( pms->hevFill );

        STORE( &pms->fPlaying, FALSE );
        STORE( &pms->fPaused, FALSE );
        STORE( &pms->fCompleted, TRUE );
        pms->fEOS = FALSE;

        return;
    }

    /* Read from normalized buffer */
    if( pms->fEOS || pms->fPaused ||
        ( pms->fMoreData && pms->buf.ulLen < ulBufSize &&
          DosWaitEventSem( pms->hevFillDone, ulTimeout )))
    {
        if( !pms->fEOS && !pms->fPaused )
            dprintf("MIXER: buffer underrun!");

        pargs->ulMaxLen = ulBufSize;

        return;
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

        /* Transfer from normalized buffer */
        ulCopyLen = ulRem < pms->buf.ulLen ? ulRem : pms->buf.ulLen;
        memcpy( pchBuf + ulLen, pms->buf.pch + pms->buf.ulPos, ulCopyLen );

        pms->buf.ulLen -= ulCopyLen;
        pms->buf.ulPos += ulCopyLen;

        ulLen += ulCopyLen;
        ulRem -= ulCopyLen;
    }

    /* Process soft volume */
    if( pil->fSoftVol &&
        ( pil->lLeftVol  != 100 || !pil->fLeftState ||
          pil->lRightVol != 100 || !pil->fRightState ))
        APPLY_SOFT_VOLUME( PSHORT, pchBuf, ulLen, pil );

    /* End of stream ? */
    if( ulLen < ulBufSize )
    {
        pms->fEOS = TRUE;
        pms->lCountDown = pil->ks.ulNumBuffers;

        memset( pchBuf + ulLen, 0, ulRem );

        ulLen = ulBufSize;
    }

    /* Synthesize audio streams */
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

    /* Update maximum length of a packet if necessary */
    if( pargs->ulMaxLen < ulLen )
        pargs->ulMaxLen = ulLen;
}

static ULONG APIENTRY kaiMixerCallBack( PVOID pCBData, PVOID pBuffer,
                                        ULONG ulBufSize )
{
    FILLBUFFERARGS args;

    memset( pBuffer, 0, ulBufSize );

    args.pilMixer = pCBData;
    args.pBuffer = pBuffer;
    args.ulBufSize = ulBufSize;
    args.pchBuf = alloca( ulBufSize );
    args.ulMaxLen = 0;

    /* On DART mode, callback is called many times without playing at inital
       time. This may lead to buffer-underrun.
       So if only one stream, wait for like initial time. Buffer-underrun will
       be processed in DART or UNIAUD interface. */
    args.ulTimeout = instancePlayingStreamCount( args.pilMixer->hkai ) == 1 ?
                     INITIAL_TIMEOUT : SEM_IMMEDIATE_RETURN;

    instanceLoop( fillBuffer, &args );

    return args.ulMaxLen;
}

APIRET DLLEXPORT APIENTRY kaiMixerOpen( const PKAISPEC pksWanted,
                                        PKAISPEC pksObtained,
                                        PHKAIMIXER phkm )
{
    PINSTANCELIST pil;
    ULONG ulMinSamples;
    APIRET rc;

    if( !kaiGetInitCount())
        return KAIE_NOT_INITIALIZED;

    if( !pksWanted || !pksObtained || !phkm )
        return KAIE_INVALID_PARAMETER;

    /* Support 16 bits stereo audio only */
    if( pksWanted->ulBitsPerSample != 16 || pksWanted->ulChannels != 2 )
        return KAIE_INVALID_PARAMETER;

    pil = instanceNew( FALSE, NULL, NULL );
    if( !pil )
        return KAIE_NOT_ENOUGH_MEMORY;

    ulMinSamples = _kaiGetMinSamples();
    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    if( pil->ks.ulBufferSize > 0 &&
        pil->ks.ulBufferSize < SAMPLESTOBYTES( ulMinSamples, pil->ks ))
        pil->ks.ulBufferSize = SAMPLESTOBYTES( ulMinSamples, pil->ks );
    pil->ks.pfnCallBack   = kaiMixerCallBack;
    pil->ks.pCallBackData = pil;
    pil->pfnUserCb        = NULL;
    pil->pUserData        = NULL;

    rc = _kaiGetApi()->pfnOpen( &pil->ks, phkm );
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

    if( !kaiGetInitCount())
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkm, IVF_MIXER ))
        return KAIE_INVALID_HANDLE;

    if( instanceStreamCount( hkm ) != 0 )
        return KAIE_STREAMS_NOT_CLOSED;

    rc = _kaiGetApi()->pfnClose( hkm );
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

    if( !kaiGetInitCount())
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
                                              _kaiGetResamplerQ(), &err );

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

    if( !kaiGetInitCount())
        return KAIE_NOT_INITIALIZED;

    if( !( pilMixer = instanceVerify( hkm, IVF_MIXER )))
        return KAIE_INVALID_HANDLE;

    if( !( pilStream = instanceVerify( hkms, IVF_STREAM )) ||
        pilStream->hkai != pilMixer->hkai )
        return KAIE_INVALID_HANDLE;

    _kaiStreamStop( pilStream );

    instanceDel( hkms );

    return KAIE_NO_ERROR;
}
