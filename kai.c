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

#include <string.h>
#include <stdlib.h>

#include "kai.h"
#include "kai_internal.h"
#include "kai_instance.h"
#include "kai_mixer.h"
#include "kai_dart.h"
#include "kai_uniaud.h"
#include "kai_spinlock.h"
#include "kai_debug.h"

#define DEFAULT_MIN_SAMPLES 2048

static ULONG    m_ulInitCount = 0;
static SPINLOCK m_lockInitCount = SPINLOCK_INIT;

static KAIAPIS  m_kai = { NULL, };
static KAICAPS  m_kaic = { 0, };

static BOOL     m_fDebugMode = FALSE;
static BOOL     m_fSoftVol = FALSE;
static BOOL     m_fSoftMixer = FALSE;
static ULONG    m_ulMinSamples = DEFAULT_MIN_SAMPLES;
static int      m_iResamplerQ = 0;
static ULONG    m_ulPlayLatency = 100;

static SPINLOCK  m_lockMixer = SPINLOCK_INIT;
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

APIRET DLLEXPORT APIENTRY kaiInit( ULONG ulMode )
{
    const char *pszEnv;
    APIRET rc = KAIE_INVALID_PARAMETER;

    spinLock( &m_lockInitCount );

    if( m_ulInitCount )
    {
        m_ulInitCount++;

        spinUnlock( &m_lockInitCount );

        return KAIE_NO_ERROR;
    }

    // Enable debug mode if KAI_DEBUG is set
    m_fDebugMode = getenv("KAI_DEBUG") != NULL;

    // Use the soft volume mode unless KAI_NOSOFTVOLUME is specified
    m_fSoftVol = getenv("KAI_NOSOFTVOLUME") ? FALSE : TRUE;

    // Use the soft mixer mode unless KAI_NOSOFTMIXER is specified
    m_fSoftMixer = getenv("KAI_NOSOFTMIXER") ? FALSE : TRUE;

    // User the sampling rate of KAI_MIXERRATE if specified
    pszEnv = getenv("KAI_MIXERRATE");
    if( pszEnv )
    {
        ULONG ulRate = atoi( pszEnv );

        if( ulRate != 0 )
            m_ks.ulSamplingRate = ulRate;
    }

    // Use the minimum samples of KAI_MINSAMPLES if specified
    pszEnv = getenv("KAI_MINSAMPLES");
    if( pszEnv )
    {
        ULONG ulMinSamples = atoi( pszEnv );

        if( ulMinSamples != 0 )
        {
            m_ulMinSamples = ulMinSamples;
            m_ks.ulBufferSize = SAMPLESTOBYTES( m_ulMinSamples, m_ks );
        }
    }

    // User the resampler quality of KAI_RESAMPLERQ if specified
    pszEnv = getenv("KAI_RESAMPLERQ");
    if( pszEnv )
    {
        int q = atoi( pszEnv );

        if( q < 0 )
            q = 0;
        else if( q > 10 )
            q = 10;

        m_iResamplerQ = q;
    }

    // Use the latency of KAI_PLAYLATENCY if specified
    pszEnv = getenv("KAI_PLAYLATENCY");
    if( pszEnv )
    {
        int latency = atoi( pszEnv );
        if( latency < 0 )
            latency = 0;

        m_ulPlayLatency = latency;
    }

    // Use the specified mode by KAI_AUTOMODE if auto mode
    pszEnv = getenv("KAI_AUTOMODE");
    if( ulMode == KAIM_AUTO && pszEnv )
    {
        if( !stricmp(pszEnv, "UNIAUD"))
            ulMode = KAIM_UNIAUD;
        else if( !stricmp(pszEnv, "DART"))
            ulMode = KAIM_DART;
    }

    if( ulMode == KAIM_UNIAUD || ulMode == KAIM_AUTO )
    {
        rc = _kaiUniaudInit( &m_kai, &m_kaic );
        if( !rc )
            ulMode = KAIM_UNIAUD;
    }

    if( ulMode == KAIM_DART || ulMode == KAIM_AUTO )
    {
        rc = _kaiDartInit( &m_kai, &m_kaic );
        if( !rc )
            ulMode = KAIM_DART;
    }

    if( !rc )
        m_ulInitCount++;

    spinUnlock( &m_lockInitCount );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiDone( VOID )
{
    APIRET rc;

    spinLock( &m_lockInitCount );

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

    spinUnlock( &m_lockInitCount );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiGetInitCount( VOID )
{
    return m_ulInitCount;
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
        spinLock( &m_lockMixer );

        rc = streamOpen( &m_ks, &m_hkm, pksWanted, pksObtained, phkai );

        spinUnlock( &m_lockMixer );

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
        spinLock( &m_lockMixer );

        rc = streamClose( &m_hkm, hkai );

        spinUnlock( &m_lockMixer );

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
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        rc = streamPlay( pil );
    }
    else
    {
        /* Normal instance */
        rc = m_kai.pfnPlay( hkai );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiStop( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        rc = streamStop( pil );
    }
    else
    {
        /* Normal instance */
        rc = m_kai.pfnStop( hkai );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiPause( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        rc = streamPause( pil );
    }
    else
    {
        /* Normal instance */
        rc = m_kai.pfnPause( hkai );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiResume( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        rc = streamResume( pil );
    }
    else
    {
        /* Normal instance */
        rc = m_kai.pfnResume( hkai );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiSetSoundState( HKAI hkai, ULONG ulCh,
                                            BOOL fState )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( pil->fSoftVol )
    {
        if( pil->ks.ulChannels == 1 ||
            ulCh == MCI_SET_AUDIO_LEFT || ulCh == MCI_SET_AUDIO_ALL )
            pil->fLeftState = fState;

        if( pil->ks.ulChannels > 1 &&
            ( ulCh == MCI_SET_AUDIO_RIGHT || ulCh == MCI_SET_AUDIO_ALL ))
            pil->fRightState = fState;

        rc = KAIE_NO_ERROR;
    }
    else
        rc = m_kai.pfnSetSoundState( hkai, ulCh, fState );

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

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

        rc = KAIE_NO_ERROR;
    }
    else
        rc = m_kai.pfnSetVolume( hkai, ulCh, usVol );

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiGetVolume( HKAI hkai, ULONG ulCh )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

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
        rc = ( lLeftVol + lRightVol ) / 2;
    }
    else
        rc = m_kai.pfnGetVolume( hkai, ulCh );

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiClearBuffer( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_STREAM )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        rc = streamClearBuffer( pil );
    }
    else
    {
        /* Normal instance */
        rc = m_kai.pfnClearBuffer( hkai );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiStatus( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_ANY )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        rc = streamStatus( pil );
    }
    else
    {
        /* Normal instance or mixer instance */
        rc = m_kai.pfnStatus( hkai );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiEnableSoftVolume( HKAI hkai, BOOL fEnable )
{
    PINSTANCELIST pil;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    pil->fSoftVol = fEnable;

    instanceUnlock( pil );

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

APIRET DLLEXPORT APIENTRY kaiEnableSoftMixer( BOOL fEnable,
                                              const PKAISPEC pks )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    spinLock( &m_lockMixer );

    m_fSoftMixer = fEnable;
    if( fEnable && pks )
        memcpy( &m_ks, pks, sizeof( KAISPEC ));

    spinUnlock( &m_lockMixer );

    return KAIE_NO_ERROR;
}

PKAIAPIS _kaiGetApi( VOID )
{
    return &m_kai;
}

BOOL _kaiIsDebugMode( VOID )
{
    return m_fDebugMode;
}

BOOL _kaiIsSoftVolume( VOID )
{
    return m_fSoftVol;
}

ULONG _kaiGetMinSamples( VOID )
{
    return m_ulMinSamples;
}

int _kaiGetResamplerQ( VOID )
{
    return m_iResamplerQ;
}

ULONG _kaiGetPlayLatency( VOID )
{
    return m_ulPlayLatency;
}
