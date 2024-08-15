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

#include "kai_internal.h"
#include "kai_server.h"

#include "kai_mixer.h"

#include <string.h>

#ifdef __WATCOMC__
#include <alloca.h>
#include <process.h>
#endif

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
            BYTESTOSAMPLES( pms->bufRes.ulSize, *pksMixer );

        speex_resampler_set_rate( pms->srs,
            pks->ulSamplingRate, pksMixer->ulSamplingRate );

        speex_resampler_process_interleaved_int( pms->srs,
            ( spx_int16_t * )pBuffer, &inSamples,
            ( spx_int16_t * )pms->bufRes.pch, &outSamples );

        ulLen = SAMPLESTOBYTES( outSamples, *pksMixer );
    }
    else
    {
        /* straight copy */
        memcpy( pms->bufRes.pch, pBuffer, ulLen );
    }

    pms->bufRes.ulLen = ulLen;
    pms->bufRes.ulPos = 0;
}

static void mixerFillThread( void *arg )
{
    PINSTANCELIST pil = arg;
    PMIXERSTREAM pms = pil->pms;
    PKAISPEC pks = &pil->ks;
    ULONG ulBufSize;
    ULONG ulBufLen;
    PCH pchBuf;
    ULONG ulCbBufSize = pks->ulBufferSize;
    ULONG ulCbBufLen;
    PCH pchCbBuf = alloca( ulCbBufSize );
    BOOL fMoreData = TRUE;

    boostThread();

    while( fMoreData || pms->bufRes.ulLen > 0 )
    {
        bufWriteLock( pms->pbuf, ( PVOID * )&pchBuf, &ulBufSize );

        if( !pms->fFilling )
            break;

        ulBufLen = 0;

        do
        {
            if( pms->bufRes.ulLen == 0 )
            {
                if( !fMoreData )
                    break;

                ulCbBufLen = pil->pfnUserCb( pil->pUserData,
                                             pchCbBuf, ulCbBufSize );

                if( ulCbBufLen < ulCbBufSize )
                    fMoreData = FALSE;

                normalize( pchCbBuf, ulCbBufLen, pil );
            }

            ulCbBufLen = pms->bufRes.ulLen < ulBufSize ?
                         pms->bufRes.ulLen : ulBufSize;

            memcpy( pchBuf + ulBufLen, pms->bufRes.pch + pms->bufRes.ulPos,
                    ulCbBufLen );

            ulBufLen += ulCbBufLen;
            ulBufSize -= ulCbBufLen;

            pms->bufRes.ulLen -= ulCbBufLen;
            pms->bufRes.ulPos += ulCbBufLen;
        } while( ulBufSize > 0 );

        bufWriteUnlock( pms->pbuf, ulBufLen );
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
    APIRET rc = KAIE_NO_ERROR;

    if( pms->fPlaying )
        return KAIE_NO_ERROR;

    pilMixer = instanceVerify( pil->hkai, IVF_MIXER );

    pms->pbuf = bufCreate( pil->ks.ulNumBuffers,
                           pilMixer->ks.ulBufferSize );
    if( !pms->pbuf )
        return KAIE_NOT_ENOUGH_MEMORY;

    STORE( &pms->fFilling, TRUE );

    pms->tid = _beginthread( mixerFillThread, NULL, THREAD_STACK_SIZE, pil );

    // prevent initial buffer-underrun and unnecessary latency
    bufReadWaitFull( pms->pbuf );

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

        bufWriteCancel( pms->pbuf );
        while( DosWaitThread( &pms->tid, DCWW_WAIT ) == ERROR_INTERRUPT )
            /* nothing */;

        bufDestroy( pms->pbuf );

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

    bufClear( pms->pbuf, pil->ks.bSilence );

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
} FILLBUFFERARGS, *PFILLBUFFERARGS;

static VOID fillBuffer( PINSTANCELIST pil, void *arg )
{
    PFILLBUFFERARGS pargs = arg;
    PINSTANCELIST pilMixer = pargs->pilMixer;
    PVOID pMixerBuffer = pargs->pBuffer;
    ULONG ulMixerBufSize = pargs->ulBufSize;
    PCHAR pchBuf = pargs->pchBuf;

    PMIXERSTREAM pms = pil->pms;
    PVOID pBuffer;
    ULONG ulLen;

    short *pDst;
    short *pSrc;
    short *pEnd;

    if( !ISSTREAM( pil ) || pil->hkai != pilMixer->hkai || !pms->fPlaying )
        return;

    /* Waiting to play remainig buffers passed to audio driver such as DART */
    if( pms->fEOS && --pms->lCountDown == 0 )
    {
        STORE( &pms->fFilling, FALSE );

        /* Terminate fill thread */
        bufWriteCancel( pms->pbuf );

        STORE( &pms->fPlaying, FALSE );
        STORE( &pms->fPaused, FALSE );
        STORE( &pms->fCompleted, TRUE );
        pms->fEOS = FALSE;

        return;
    }

    /* Read from normalized buffer */
    if( pms->fEOS || pms->fPaused ||
        bufReadLock( pms->pbuf, &pBuffer, &ulLen ) == -1 )
    {
        if( !pms->fEOS && !pms->fPaused )
            dprintf("MIXER: buffer underrun!");

        pargs->ulMaxLen = ulMixerBufSize;

        return;
    }
    else
    {
        memcpy( pchBuf, pBuffer, ulLen );

        bufReadUnlock( pms->pbuf );
    }

    /* Process soft volume */
    if( pil->fSoftVol &&
        ( pil->lLeftVol  != 100 || !pil->fLeftState ||
          pil->lRightVol != 100 || !pil->fRightState ))
        APPLY_SOFT_VOLUME( PSHORT, pchBuf, ulLen, pil );

    /* End of stream ? */
    if( ulLen < ulMixerBufSize )
    {
        pms->fEOS = TRUE;
        pms->lCountDown = pilMixer->ks.ulNumBuffers;

        memset( pchBuf + ulLen, 0, ulMixerBufSize - ulLen );

        ulLen = ulMixerBufSize;
    }

    /* Synthesize audio streams */
    pDst = pMixerBuffer;
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

    instanceLoop( fillBuffer, &args );

    return args.ulMaxLen;
}

APIRET DLLEXPORT APIENTRY kaiMixerOpen( const PKAISPEC pksWanted,
                                        PKAISPEC pksObtained,
                                        PHKAIMIXER phkm )
{
    PINSTANCELIST pil;
    ULONG ulMinBufferSize;
    APIRET rc;

    if( !kaiGetInitCount())
        return KAIE_NOT_INITIALIZED;

    if( !pksWanted || !pksObtained || !phkm )
        return KAIE_INVALID_PARAMETER;

    /* Support 16 bits stereo audio only */
    if( pksWanted->ulBitsPerSample != 16 || pksWanted->ulChannels != 2 )
        return KAIE_INVALID_PARAMETER;

    if( _kaiIsServer())
        return serverMixerOpen( pksWanted, pksObtained, phkm );

    pil = instanceNew( FALSE, NULL, NULL );
    if( !pil )
        return KAIE_NOT_ENOUGH_MEMORY;

    ulMinBufferSize =
        SAMPLESTOBYTES( _kaiGetMinSamples( pksWanted->usDeviceIndex ),
                        *pksWanted );
    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    pil->ks.ulNumBuffers = 2;   /* Limit buffers of backend to 2 */
    if( pil->ks.ulBufferSize > 0 && pil->ks.ulBufferSize < ulMinBufferSize )
        pil->ks.ulBufferSize = ulMinBufferSize;
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

    /* Record an user supplied a number of buffers to use later, and it should
       be 2 at least */
    if( pil->ks.ulNumBuffers < pksWanted->ulNumBuffers )
        pil->ks.ulNumBuffers = pksWanted->ulNumBuffers;

    memcpy( pksObtained, &pil->ks, sizeof( KAISPEC ));
    pksObtained->pfnCallBack   = NULL;
    pksObtained->pCallBackData = NULL;

    instanceAdd( *phkm, *phkm, pil );

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiMixerClose( HKAIMIXER hkm )
{
    PINSTANCELIST pil;
    APIRET rc = KAIE_NO_ERROR;

    if( !kaiGetInitCount())
        return KAIE_NOT_INITIALIZED;

    if(( pil = instanceVerify( hkm, IVF_SERVER )) != NULL )
        return serverMixerClose( pil );

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

    if( !kaiGetInitCount())
        return KAIE_NOT_INITIALIZED;

    if( !pksWanted || !pksObtained || !phkms )
        return KAIE_INVALID_PARAMETER;

    if( !pksWanted->pfnCallBack )
        return KAIE_INVALID_PARAMETER;

    if(( pilMixer = instanceVerify( hkm, IVF_SERVER )) != NULL )
        return serverMixerStreamOpen( pilMixer, pksWanted, pksObtained, phkms );

    if( !( pilMixer = instanceVerify( hkm, IVF_MIXER )))
        return KAIE_INVALID_HANDLE;

    if( pksWanted->ulType != pilMixer->ks.ulType )
        return KAIE_INVALID_PARAMETER;

    if( pksWanted->ulBitsPerSample > pilMixer->ks.ulBitsPerSample )
        return KAIE_INVALID_PARAMETER;

    if( pksWanted->ulChannels > pilMixer->ks.ulChannels )
        return KAIE_INVALID_PARAMETER;

    pil = instanceNew( TRUE, &pilMixer->ks, pksWanted );
    if( pil )
    {
        int err;
        pil->pms->srs = speex_resampler_init( pilMixer->ks.ulChannels,
                                              pksWanted->ulSamplingRate,
                                              pilMixer->ks.ulSamplingRate,
                                              _kaiGetResamplerQ(), &err );
    }

    if( !pil || !pil->pms->srs )
    {
        instanceFree( pil );

        return KAIE_NOT_ENOUGH_MEMORY;
    }

    memcpy( &pil->ks, pksWanted, sizeof( KAISPEC ));
    pil->ks.usDeviceIndex = pilMixer->ks.usDeviceIndex;
    /* A number of buffers as twice as a mixer at least */
    pil->ks.ulNumBuffers  = pilMixer->ks.ulNumBuffers * 2;
    if( pil->ks.ulNumBuffers < pksWanted->ulNumBuffers )
        pil->ks.ulNumBuffers = pksWanted->ulNumBuffers;
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

    if(( pilMixer = instanceVerify( hkm, IVF_SERVER )) != NULL &&
       ( pilStream = instanceVerify( hkms, IVF_SERVER )) != NULL )
        return serverMixerStreamClose( pilMixer, pilStream );

    if( !( pilMixer = instanceVerify( hkm, IVF_MIXER )))
        return KAIE_INVALID_HANDLE;

    if( !( pilStream = instanceVerify( hkms, IVF_STREAM )) ||
        pilStream->hkai != pilMixer->hkai )
        return KAIE_INVALID_HANDLE;

    _kaiStreamStop( pilStream );

    instanceDel( hkms );

    return KAIE_NO_ERROR;
}
