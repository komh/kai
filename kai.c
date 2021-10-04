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
#include "kai_instance.h"
#include "kai_mixer.h"
#include "kai_server.h"
#include "kai_dart.h"
#include "kai_uniaud.h"
#include "kai_spinlock.h"
#include "kai_debug.h"
#include "kai_atomic.h"

#define MAX_AUDIO_CARDS 16  /* Up to 16 audio cards */

#define DEFAULT_MIN_SAMPLES 2048

static ULONG    m_ulInitCount = 0;
static SPINLOCK m_lockInitCount = SPINLOCK_INIT;

static KAIAPIS  m_kai = { NULL, };
static LONG     m_lDefaultIndex = 0;
static LONG     m_lCardCount = 0;
static KAICAPS  m_aCaps[ MAX_AUDIO_CARDS + 1 /* for default device */];

static BOOL     m_fDebugMode = FALSE;
static BOOL     m_fSoftVol = FALSE;
static BOOL     m_fSoftMixer = FALSE;
static BOOL     m_fServer = FALSE;
static ULONG    m_ulMinSamples = DEFAULT_MIN_SAMPLES;
static int      m_iResamplerQ = 0;
static ULONG    m_ulPlayLatency = 100;

typedef struct MIXERDEVICE
{
    SPINLOCK  lock;
    HKAIMIXER hkm;
    KAISPEC   spec;
} MIXERDEVICE, *PMIXERDEVICE;

MIXERDEVICE m_aDevices[ MAX_AUDIO_CARDS + 1 /* for default device */ ];

static KAISPEC m_spec0 = {
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

static PMIXERDEVICE getMixerDevice( ULONG ulDeviceIndex )
{
    if( ulDeviceIndex > kaiGetCardCount())
        return NULL;

    /* Default index ? */
    if( ulDeviceIndex == 0 )
        ulDeviceIndex = _kaiGetDefaultIndex(); /* Use real index */

    return &m_aDevices[ ulDeviceIndex ];
}

APIRET DLLEXPORT APIENTRY kaiInit( ULONG ulMode )
{
    const char *pszEnv;
    int i;
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

    // Use the server mode if KAI_NOSERVER is unset and a server is running
    m_fServer = ( getenv("KAI_NOSERVER") ? FALSE : TRUE ) &&
                serverCheck() == 0;

    // User the sampling rate of KAI_MIXERRATE if specified
    pszEnv = getenv("KAI_MIXERRATE");
    if( pszEnv )
    {
        ULONG ulRate = atoi( pszEnv );

        if( ulRate != 0 )
            m_spec0.ulSamplingRate = ulRate;
    }

    // Use the minimum samples of KAI_MINSAMPLES if specified
    pszEnv = getenv("KAI_MINSAMPLES");
    if( pszEnv )
    {
        ULONG ulMinSamples = atoi( pszEnv );

        if( ulMinSamples != 0 )
        {
            m_ulMinSamples = ulMinSamples;
            m_spec0.ulBufferSize = SAMPLESTOBYTES( m_ulMinSamples, m_spec0 );
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

    for( i = 0; i <= MAX_AUDIO_CARDS; i++ )
    {
        PMIXERDEVICE pDevice = &m_aDevices[ i ];
        char szEnvName[ 30 ];

        spinLockInit( &pDevice->lock );

        pDevice->hkm  = NULLHANDLE;
        pDevice->spec = m_spec0;

        pDevice->spec.usDeviceIndex = i;

        // Override a sampling rate if a device-specific one is given
        sprintf( szEnvName, "KAI_MIXERRATE%d", i );
        pszEnv = getenv( szEnvName );
        if( pszEnv )
        {
            ULONG ulRate = atoi( pszEnv );

            if( ulRate != 0 )
                pDevice->spec.ulSamplingRate = ulRate;
        }
    }

    if( m_fServer )
    {
        // If server mode, no need to initialize sub-system.
        // And this fixes hiccup when kaiInit() is called in another process.
        // Especially, dartChNum() being called in _kaiDartInit() causes
        // hiccup.
        rc = KAIE_NO_ERROR;
    }
    else
    {
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
            rc = _kaiUniaudInit( &m_kai, &m_aCaps[ 0 ]);
            if( !rc )
                ulMode = KAIM_UNIAUD;
        }

        if( ulMode == KAIM_DART || ulMode == KAIM_AUTO )
        {
            rc = _kaiDartInit( &m_kai, &m_aCaps[ 0 ]);
            if( !rc )
                ulMode = KAIM_DART;
        }

        // Initialize system-wide information
        if( !rc )
        {
            int i;

            m_lDefaultIndex = m_kai.pfnGetDefaultIndex();

            m_lCardCount = m_kai.pfnGetCardCount();
            for( i = 1; i <= m_lCardCount; i++ )
                m_kai.pfnCapsEx( i, &m_aCaps[ i ]);
        }
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
    else if( !m_fServer && m_ulInitCount == 1)
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

    if( m_fServer )
        return serverCaps( pkc );

    memcpy( pkc, &m_aCaps[ 0 ], sizeof( KAICAPS ));

    return KAIE_NO_ERROR;
}

APIRET DLLEXPORT APIENTRY kaiCapsEx( ULONG ulDeviceIndex, PKAICAPS pkc )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !pkc )
        return KAIE_INVALID_PARAMETER;

    if( m_fServer )
        return serverCapsEx( ulDeviceIndex, pkc );

    memcpy( pkc, &m_aCaps[ ulDeviceIndex ], sizeof( *pkc ));

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

    if( m_fServer )
    {
        /* Server mode */
        return serverOpen( pksWanted, pksObtained, phkai );
    }

    if( m_fSoftMixer )
    {
        /* Soft mixer mode */
        PMIXERDEVICE pDevice = getMixerDevice( pksWanted->usDeviceIndex );

        if( !pDevice )
            return KAIE_INVALID_PARAMETER;

        spinLock( &pDevice->lock );

        rc = streamOpen( &pDevice->spec, &pDevice->hkm,
                         pksWanted, pksObtained, phkai );

        spinUnlock( &pDevice->lock);

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

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    if( ISSERVER( pil ))
    {
        /* Server instance */
        return serverClose( pil );
    }

    if( ISSTREAM( pil ))
    {
        /* Mixer stream */
        PMIXERDEVICE pDevice = getMixerDevice( pil->ks.usDeviceIndex );

        if( !pDevice )
            return KAIE_INVALID_HANDLE;

        if( pil->hkai != pDevice->hkm )
            return KAIE_INVALID_HANDLE;

        spinLock( &pDevice->lock );

        rc = streamClose( &pDevice->hkm, hkai );

        spinUnlock( &pDevice->lock );

        return rc;
    }

    /* Normal instance */
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

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverPlay( pil );
    }
    else if( ISSTREAM( pil ))
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

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverStop( pil );
    }
    else if( ISSTREAM( pil ))
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

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverPause( pil );
    }
    else if( ISSTREAM( pil ))
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

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverResume( pil );
    }
    else if( ISSTREAM( pil ))
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

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverSetSoundState( pil, ulCh, fState );
    }
    else if( pil->fSoftVol )
    {
        /* Normal instance with soft volume control or mixer stream */
        if( pil->ks.ulChannels == 1 ||
            ulCh == MCI_SET_AUDIO_LEFT || ulCh == MCI_SET_AUDIO_ALL )
            pil->fLeftState = fState;

        if( pil->ks.ulChannels > 1 &&
            ( ulCh == MCI_SET_AUDIO_RIGHT || ulCh == MCI_SET_AUDIO_ALL ))
            pil->fRightState = fState;

        rc = KAIE_NO_ERROR;
    }
    else
    {
        /* Normal instance without soft volume mode */
        rc = m_kai.pfnSetSoundState( hkai, ulCh, fState );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( usVol > 100 )
        usVol = 100;

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverSetVolume( pil, ulCh, usVol );
    }
    else if( pil->fSoftVol )
    {
        /* Normal instance with soft volume control or mixer stream */
        if( pil->ks.ulChannels == 1 ||
            ulCh == MCI_SET_AUDIO_LEFT || ulCh == MCI_SET_AUDIO_ALL )
            pil->lLeftVol = usVol;

        if( pil->ks.ulChannels > 1 &&
            ( ulCh == MCI_SET_AUDIO_RIGHT || ulCh == MCI_SET_AUDIO_ALL ))
            pil->lRightVol = usVol;

        rc = KAIE_NO_ERROR;
    }
    else
    {
        /* Normal instance without soft volume control */
        rc = m_kai.pfnSetVolume( hkai, ulCh, usVol );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiGetVolume( HKAI hkai, ULONG ulCh )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverGetVolume( pil, ulCh );
    }
    else if( pil->fSoftVol )
    {
        /* Normal instance with soft volume control or mixer stream */
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
    {
        /* Normal instance without soft volume control */
        rc = m_kai.pfnGetVolume( hkai, ulCh );
    }

    instanceUnlock( pil );

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiClearBuffer( HKAI hkai )
{
    PINSTANCELIST pil;
    APIRET rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_PLAYABLE )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverClearBuffer( pil );
    }
    else if( ISSTREAM( pil ))
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

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverStatus( pil );
    }
    else if( ISSTREAM( pil ))
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
    ULONG rc;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( !( pil = instanceVerify( hkai, IVF_NORMAL | IVF_SERVER )))
        return KAIE_INVALID_HANDLE;

    instanceLock( pil );

    if( ISSERVER( pil ))
    {
        /* Server instance */
        rc = serverEnableSoftVolume( pil, fEnable );
    }
    else
    {
        /* Normal instance */
        pil->fSoftVol = fEnable;

        rc = KAIE_NO_ERROR;
    }

    instanceUnlock( pil );

    return rc;
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
    ULONG rc = KAIE_NO_ERROR;

    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( m_fServer )
        rc = serverEnableSoftMixer( fEnable, pks );
    else
    {
        if( fEnable && pks )
        {
            PMIXERDEVICE pDevice = getMixerDevice( pks->usDeviceIndex );

            if( pDevice )
            {
                spinLock( &pDevice->lock );
                memcpy( &pDevice->spec, pks, sizeof( KAISPEC ));
                spinUnlock( &pDevice->lock );
            }
            else
                rc = KAIE_INVALID_PARAMETER;
        }

        if( !rc )
            STORE( &m_fSoftMixer, fEnable );
    }

    return rc;
}

APIRET DLLEXPORT APIENTRY kaiGetCardCount( VOID )
{
    if( !m_ulInitCount )
        return KAIE_NOT_INITIALIZED;

    if( m_fServer )
        return serverGetCardCount();

    return m_lCardCount;
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

BOOL _kaiIsServer( VOID )
{
    return m_fServer;
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

APIRET _kaiGetDefaultIndex( VOID )
{
    return m_lDefaultIndex;
}
