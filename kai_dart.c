/*
    Direct Audio Interface for K Audio Interface
    Copyright (C) 2007-2015 by KO Myung-Hun <komh@chollian.net>
    Copyright (C) by Alex Strelnikov
    Copyright (C) 1998 by Andrew Zabolotny <bit@eltech.ru>

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

#define INCL_OS2MM
#include <os2me.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __WATCOMC__
#include <process.h>
#endif

#include "kai.h"
#include "kai_internal.h"
#include "kai_dart.h"

// Currently(Warp4 FixPak #15), MDM.DLL allows only one load per process.
// Otherwise, DosLoadModule() return ERROR_INIT_ROUTINE_FAILED.
// So we should load it only once, and let system free it automatically
// at finish.
#define LET_SYSTEM_FREE_MODULE

// MCI_RELEASEDEVICE is used to release a device instance being used
// exclusively. However, MCI_CLOSE also do it. So, MCI_RELEASEDEVICE is
// redundant if MCI_CLOSE follows.
// In addition, when libkai is used in multimedia sub-system, MCI_RELEASEDEVICE
// in dartClose() is blocked.
#define USE_RELEASEDEVICE   0

// DART need 2 buffers at least to play.
#define DART_MIN_BUFFERS     2

#pragma pack( 1 )
typedef struct DARTINFO
{
    USHORT              usDeviceID;
    BYTE                bSilence;
    BOOL                fShareable;
    USHORT              usLeftVol;
    USHORT              usRightVol;
    PMCI_MIX_BUFFER     pMixBuffers;
    MCI_MIXSETUP_PARMS  MixSetupParms;
    MCI_BUFFER_PARMS    BufferParms;
    ULONG               ulBufferSize;
    ULONG               ulNumBuffers;
    BOOL                fWaitStreamEnd;
    PFNKAICB            pfndicb;
    PVOID               pCBData;
    MCI_MIX_BUFFER      mixBuffer;
    HEV                 hevFill;
    HEV                 hevFillDone;
    TID                 tidFillThread;
    BOOL volatile       fPlaying;
    BOOL volatile       fPaused;
    BOOL volatile       fCompleted;
} DARTINFO, *PDARTINFO;
#pragma pack()

static HMODULE m_hmodMDM = NULLHANDLE;

#define mciSendCommand      m_pfnmciSendCommand
#define mciGetErrorString   m_pfnmciGetErrorString

static DECLARE_PFN( ULONG, APIENTRY, mciSendCommand, ( USHORT, USHORT, ULONG, PVOID, USHORT ));
static DECLARE_PFN( ULONG, APIENTRY, mciGetErrorString, ( ULONG, PSZ, USHORT ));

static BOOL m_fDebugMode = FALSE;

static APIRET APIENTRY dartDone( VOID );
static APIRET APIENTRY dartOpen( PKAISPEC pks, PHKAI phkai );
static APIRET APIENTRY dartClose( HKAI hkai );
static APIRET APIENTRY dartPlay( HKAI hkai );
static APIRET APIENTRY dartStop( HKAI hkai );
static APIRET APIENTRY dartPause( HKAI hkai );
static APIRET APIENTRY dartResume( HKAI hkai );
#if 0
static APIRET APIENTRY dartGetPos( HKAI hkai );
static APIRET APIENTRY dartSetPos( HKAI hkai, ULONG ulNewPos );
#endif
static APIRET APIENTRY dartError( APIRET rc );
static APIRET APIENTRY dartSetSoundState( HKAI hkai, ULONG ulCh, BOOL fState );
static APIRET APIENTRY dartSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol );
static APIRET APIENTRY dartGetVolume( HKAI hkai, ULONG ulCh );
static APIRET APIENTRY dartChNum( VOID );
static APIRET APIENTRY dartClearBuffer( HKAI hkai );
static APIRET APIENTRY dartStatus( HKAI hkai );
static APIRET APIENTRY dartOSLibGetAudioPDDName( PSZ pszPDDName );

static VOID freeMDM( VOID )
{
    DosFreeModule( m_hmodMDM );
    m_hmodMDM = NULLHANDLE;
}

static BOOL loadMDM( VOID )
{
    CHAR szFailedName[ 256 ];

    if( m_hmodMDM )
        return TRUE;

    if( DosLoadModule( szFailedName, sizeof( szFailedName ), "MDM", &m_hmodMDM ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodMDM, 1, NULL, ( PFN * )&mciSendCommand ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodMDM, 3, NULL, ( PFN * )&mciGetErrorString ))
        goto exit_error;

    return TRUE;

exit_error:
    // In case of this, MDM.DLL is not a MMOS2 DLL.
    freeMDM();

    return FALSE;
}

APIRET APIENTRY kaiDartInit( PKAIAPIS pkai, PKAICAPS pkc )
{
    if( !loadMDM())
        return KAIE_CANNOT_LOAD_SUB_MODULE;

    pkai->pfnDone          = dartDone;
    pkai->pfnOpen          = dartOpen;
    pkai->pfnClose         = dartClose;
    pkai->pfnPlay          = dartPlay;
    pkai->pfnStop          = dartStop;
    pkai->pfnPause         = dartPause;
    pkai->pfnResume        = dartResume;
    pkai->pfnSetSoundState = dartSetSoundState;
    pkai->pfnSetVolume     = dartSetVolume;
    pkai->pfnGetVolume     = dartGetVolume;
    pkai->pfnClearBuffer   = dartClearBuffer;
    pkai->pfnStatus        = dartStatus;

    pkc->ulMode         = KAIM_DART;
    pkc->ulMaxChannels  = dartChNum();
    dartOSLibGetAudioPDDName( &pkc->szPDDName[ 0 ]);

    m_fDebugMode = getenv("KAIDEBUG") != NULL;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartDone( VOID )
{
#ifndef LET_SYSTEM_FREE_MODULE
    freeMDM();
#endif

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartError( APIRET rc )
{
    if( LOUSHORT( rc ))
    {
        if( m_fDebugMode)
        {
            CHAR szErrorCode[ 256 ];

            mciGetErrorString( rc,
                               ( PSZ )szErrorCode,
                               sizeof( szErrorCode ));

            fprintf( stderr, "\nDART error(%lx):%s\n", rc, szErrorCode );
        }

        return LOUSHORT( rc );
    }

    return MCIERR_SUCCESS;
}

#define USE_UNIAUD_WORKAROUND

static APIRET APIENTRY dartStop( HKAI hkai )
{
    PDARTINFO         pdi = ( PDARTINFO )hkai;
    MCI_GENERIC_PARMS GenericParms;
    ULONG             rc;

    if( !pdi->fPlaying )
        return KAIE_NO_ERROR;

#ifdef USE_UNIAUD_WORKAROUND
    // workaround for uniaud
    // clean up thread before MCI_STOP
    // otherwise looping sound or dead lock can occur when trying to stop
    // but assume that MCI_STOP always succeeds
    pdi->fPlaying = FALSE;
    pdi->fPaused  = FALSE;

    DosPostEventSem( pdi->hevFill );
    while( DosWaitThread( &pdi->tidFillThread, DCWW_WAIT ) == ERROR_INTERRUPT );
    DosCloseEventSem( pdi->hevFill );

    DosCloseEventSem( pdi->hevFillDone );
#endif

    memset( &GenericParms, 0, sizeof( GenericParms ));

    GenericParms.hwndCallback = 0;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_STOP,
                         MCI_WAIT,
                         ( PVOID )&GenericParms,
                         0 );
    if( dartError( rc ))
        return LOUSHORT( rc );

#ifndef USE_UNIAUD_WORKAROUND
    pdi->fPlaying = FALSE;
    pdi->fPaused  = FALSE;

    DosPostEventSem( pdi->hevFill );
    while( DosWaitThread( &pdi->tidFillThread, DCWW_WAIT ) == ERROR_INTERRUPT );
    DosCloseEventSem( pdi->hevFill );

    DosCloseEventSem( pdi->hevFillDone );
#endif

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartClearBuffer( HKAI hkai )
{
    PDARTINFO pdi = ( PDARTINFO )hkai;
    int       i;

    for( i = 0; i < pdi->ulNumBuffers; i++)
       memset( pdi->pMixBuffers[ i ].pBuffer, pdi->bSilence, pdi->ulBufferSize );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartFreeBuffers( HKAI hkai )
{
    PDARTINFO pdi = ( PDARTINFO )hkai;
    APIRET    rc;

    if( pdi->pMixBuffers == NULL )
        return KAIE_NO_ERROR;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_BUFFER,
                         MCI_WAIT | MCI_DEALLOCATE_MEMORY,
                         ( PVOID )&pdi->BufferParms, 0 );
    if( dartError( rc ))
        return LOUSHORT( rc );

    free( pdi->pMixBuffers );
    pdi->pMixBuffers = NULL;

    return KAIE_NO_ERROR;
}

static ULONG dartFillBuffer( PDARTINFO pdi )
{
    ULONG ulWritten;

    ulWritten = pdi->pfndicb( pdi->pCBData, pdi->mixBuffer.pBuffer, pdi->ulBufferSize );
    if( ulWritten < pdi->ulBufferSize )
    {
        memset(( PCH )pdi->mixBuffer.pBuffer + ulWritten, pdi->bSilence, pdi->ulBufferSize - ulWritten );
        pdi->mixBuffer.ulFlags = MIX_BUFFER_EOS;
    }
    else
        pdi->mixBuffer.ulFlags = 0;

    return ulWritten;
}

static void dartFillThread( void *arg )
{
    PDARTINFO pdi = arg;
    ULONG ulPost;

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );

    for(;;)
    {
        while( DosWaitEventSem( pdi->hevFill, SEM_INDEFINITE_WAIT ) == ERROR_INTERRUPT );
        DosResetEventSem( pdi->hevFill, &ulPost );

        if( !pdi->fPlaying )
            break;

        // Transfer buffer to DART
        dartFillBuffer( pdi );

        DosPostEventSem( pdi->hevFillDone );
    }
}

void DART_FILL_BUFFERS( PDARTINFO pdi, PMCI_MIX_BUFFER pBuffer )
{
    ULONG ulPost;

    if( DosWaitEventSem( pdi->hevFillDone, SEM_IMMEDIATE_RETURN ) == NO_ERROR )
    {
        DosResetEventSem( pdi->hevFillDone, &ulPost );

        memcpy( pBuffer->pBuffer, pdi->mixBuffer.pBuffer, pdi->ulBufferSize );
        pBuffer->ulFlags = pdi->mixBuffer.ulFlags;
        if( pBuffer->ulFlags == MIX_BUFFER_EOS )
            pdi->fWaitStreamEnd = TRUE;
        else
            DosPostEventSem( pdi->hevFill );

        return;
    }

    memset( pBuffer->pBuffer, pdi->bSilence, pdi->ulBufferSize );
    pBuffer->ulFlags = 0;
}

static LONG APIENTRY MixHandler( ULONG ulStatus, PMCI_MIX_BUFFER pBuffer, ULONG ulFlags )
{
    switch( ulFlags )
    {
        case MIX_STREAM_ERROR | MIX_WRITE_COMPLETE:
            // on error, fill next buffer and continue
        case MIX_WRITE_COMPLETE:
        {
            PDARTINFO pdi = ( PDARTINFO )pBuffer->ulUserParm;

            // If this is the last buffer, stop
            if( pBuffer->ulFlags & MIX_BUFFER_EOS)
            {
                dartStop(( HKAI )pdi );

                pdi->fCompleted = TRUE;
            }
            else if( pdi->fPlaying && !pdi->fWaitStreamEnd )
            {
                DART_FILL_BUFFERS( pdi, pBuffer );

                pdi->MixSetupParms.pmixWrite( pdi->MixSetupParms.ulMixHandle, pBuffer, 1 );
            }
            break;
        }
    }

    return TRUE;
}

static APIRET APIENTRY dartChNum( VOID )
{
    ULONG               ulDartFlags;
    ULONG               ulChannels;
    MCI_AMP_OPEN_PARMS  AmpOpenParms;
    MCI_GENERIC_PARMS   GenericParms;
    MCI_MIXSETUP_PARMS  MixSetupParms;

    memset( &AmpOpenParms, 0, sizeof( MCI_AMP_OPEN_PARMS ));
    ulDartFlags = MCI_WAIT | MCI_OPEN_TYPE_ID | MCI_OPEN_SHAREABLE;
    AmpOpenParms.usDeviceID = 0;
    AmpOpenParms.pszDeviceType = (PSZ)( MCI_DEVTYPE_AUDIO_AMPMIX |
                                        ( 0 << 16 ));
    if( mciSendCommand( 0, MCI_OPEN, ulDartFlags, ( PVOID )&AmpOpenParms, 0 ))
        return 0;

    ulChannels = 6; // first, try 6 channels
    memset( &MixSetupParms, 0, sizeof( MCI_MIXSETUP_PARMS ));
    MixSetupParms.ulBitsPerSample = 16;
    MixSetupParms.ulSamplesPerSec = 48000;
    MixSetupParms.ulFormatTag = MCI_WAVE_FORMAT_PCM;
    MixSetupParms.ulChannels = ulChannels;
    MixSetupParms.ulFormatMode = MCI_PLAY;
    MixSetupParms.ulDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
    MixSetupParms.pmixEvent = NULL;
    if( mciSendCommand( AmpOpenParms.usDeviceID,
                        MCI_MIXSETUP, MCI_WAIT | MCI_MIXSETUP_INIT,
                        ( PVOID )&MixSetupParms, 0 ))
    {
        ulChannels = 4; // failed. try 4 channels
        MixSetupParms.ulChannels = ulChannels;
        if( mciSendCommand( AmpOpenParms.usDeviceID,
                            MCI_MIXSETUP, MCI_WAIT | MCI_MIXSETUP_INIT,
                            ( PVOID )&MixSetupParms, 0 ))
            ulChannels = 2; // failed again...so, drivers support only 2 channels
    }

    mciSendCommand( AmpOpenParms.usDeviceID,
                    MCI_CLOSE, MCI_WAIT,
                    ( PVOID )&GenericParms, 0 );

    return ulChannels;
}

static APIRET APIENTRY dartOpen( PKAISPEC pks, PHKAI phkai )
{
    PDARTINFO           pdi;
    ULONG               ulDartFlags;
    MCI_AMP_OPEN_PARMS  AmpOpenParms;
    MCI_GENERIC_PARMS   GenericParms;
    APIRET              rc;

    pdi = calloc( 1, sizeof( DARTINFO ));
    if( !pdi )
        return KAIE_NOT_ENOUGH_MEMORY;

    pdi->bSilence = ( pks->ulBitsPerSample == BPS_8 ) ? 0x80 : 0x00;

    pdi->fShareable = pks->fShareable;

    ulDartFlags = MCI_WAIT | MCI_OPEN_TYPE_ID;
    if( pdi->fShareable )
        ulDartFlags |= MCI_OPEN_SHAREABLE;

    memset( &AmpOpenParms, 0, sizeof( MCI_AMP_OPEN_PARMS ));

    AmpOpenParms.usDeviceID = 0;
    AmpOpenParms.pszDeviceType = (PSZ)( MCI_DEVTYPE_AUDIO_AMPMIX |
                                        (( ULONG )pks->usDeviceIndex << 16 ));
    rc = mciSendCommand( 0,
                         MCI_OPEN,
                         ulDartFlags,
                         ( PVOID )&AmpOpenParms,
                         0 );
    if( dartError( rc ))
        goto exit_free;

    pdi->usDeviceID = AmpOpenParms.usDeviceID;

    if( !pdi->fShareable )
    {
        // Grab exclusive rights to device instance (NOT entire device)
        GenericParms.hwndCallback = 0;
        rc = mciSendCommand( pdi->usDeviceID,
                             MCI_ACQUIREDEVICE,
                             MCI_WAIT | MCI_EXCLUSIVE_INSTANCE,
                             ( PVOID )&GenericParms,
                             0 );
        if( dartError( rc ))
            goto exit_close;
    }

    // Setup the mixer for playback of wave data
    memset( &pdi->MixSetupParms, 0, sizeof( MCI_MIXSETUP_PARMS ));

    pdi->MixSetupParms.ulBitsPerSample = pks->ulBitsPerSample;
    pdi->MixSetupParms.ulSamplesPerSec = pks->ulSamplingRate;
    pdi->MixSetupParms.ulFormatTag     = MCI_WAVE_FORMAT_PCM;
    pdi->MixSetupParms.ulChannels      = pks->ulChannels;
    pdi->MixSetupParms.ulFormatMode    = MCI_PLAY;
    pdi->MixSetupParms.ulDeviceType    = MCI_DEVTYPE_WAVEFORM_AUDIO;
    pdi->MixSetupParms.pmixEvent       = ( MIXEREVENT * )MixHandler;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_MIXSETUP,
                         MCI_WAIT | MCI_MIXSETUP_INIT,
                         ( PVOID )&pdi->MixSetupParms,
                         0 );

    if( dartError( rc ))
        goto exit_release;

    // Use the suggested buffer number and size provide by the mixer device if 0
    if( pks->ulNumBuffers == 0 )
        pks->ulNumBuffers = pdi->MixSetupParms.ulNumBuffers;

    if( pks->ulNumBuffers < DART_MIN_BUFFERS )
        pks->ulNumBuffers = DART_MIN_BUFFERS;

    if( pks->ulBufferSize == 0 )
        pks->ulBufferSize = pdi->MixSetupParms.ulBufferSize;

    // Allocate mixer buffers
    pdi->pMixBuffers = ( MCI_MIX_BUFFER * )malloc( sizeof( MCI_MIX_BUFFER ) * pks->ulNumBuffers );

    // Set up the BufferParms data structure and allocate device buffers
    // from the Amp-Mixer
    pdi->BufferParms.ulStructLength = sizeof( MCI_BUFFER_PARMS );
    pdi->BufferParms.ulNumBuffers   = pks->ulNumBuffers;
    pdi->BufferParms.ulBufferSize   = pks->ulBufferSize;
    pdi->BufferParms.pBufList       = pdi->pMixBuffers;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_BUFFER,
                         MCI_WAIT | MCI_ALLOCATE_MEMORY,
                         ( PVOID )&pdi->BufferParms,
                         0 );
    if( dartError( rc ))
        goto exit_free_mix_buffers;

    // The mixer possibly changed these values
    pdi->ulNumBuffers = pdi->BufferParms.ulNumBuffers;
    pdi->ulBufferSize = pdi->BufferParms.ulBufferSize;

    pdi->mixBuffer.pBuffer = malloc( pdi->ulBufferSize );
    if( !pdi->mixBuffer.pBuffer )
        goto exit_deallocate;

    pdi->pfndicb = pks->pfnCallBack;
    pdi->pCBData = pks->pCallBackData;

    pks->ulNumBuffers = pdi->ulNumBuffers;
    pks->ulBufferSize = pdi->ulBufferSize;
    pks->bSilence     = pdi->bSilence;

    *phkai = ( HKAI )pdi;

    return KAIE_NO_ERROR;

exit_deallocate :
    mciSendCommand( pdi->usDeviceID,
                    MCI_BUFFER,
                    MCI_WAIT | MCI_DEALLOCATE_MEMORY,
                    ( PVOID )&pdi->BufferParms, 0 );

exit_free_mix_buffers :
    free( pdi->pMixBuffers );

exit_release :
    GenericParms.hwndCallback = 0;

#if USE_RELEASEDEVICE
    if( !pdi->fShareable )
    {
        // Release exclusive rights to device instance (NOT entire device)
        mciSendCommand( pdi->usDeviceID,
                        MCI_RELEASEDEVICE,
                        MCI_WAIT | MCI_RETURN_RESOURCE,
                        ( PVOID )&GenericParms,
                        0 );
    }
#endif

exit_close :
    mciSendCommand( pdi->usDeviceID,
                    MCI_CLOSE,
                    MCI_WAIT,
                    ( PVOID )&GenericParms,
                    0 );

exit_free :
    free( pdi );

    return LOUSHORT( rc );
}

static APIRET APIENTRY dartClose( HKAI hkai )
{
    PDARTINFO           pdi = ( PDARTINFO )hkai;
    MCI_GENERIC_PARMS   GenericParms;
    APIRET              rc;

    dartStop( hkai );
    dartFreeBuffers( hkai );

    GenericParms.hwndCallback = 0;

#if USE_RELEASEDEVICE
    if( !pdi->fShareable )
    {
        // Release exclusive rights to device instance (NOT entire device)
        rc = mciSendCommand( pdi->usDeviceID,
                             MCI_RELEASEDEVICE,
                             MCI_WAIT | MCI_RETURN_RESOURCE,
                             ( PVOID )&GenericParms,
                             0 );
        if( dartError( rc ))
            return LOUSHORT( rc );
    }
#endif

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_CLOSE,
                         MCI_WAIT,
                         ( PVOID )&GenericParms,
                         0 );
    if( dartError( rc ))
        return LOUSHORT( rc );

    free( pdi->mixBuffer.pBuffer );
    free( pdi );
    pdi = NULL;

    return KAIE_NO_ERROR;
}

// check hkai is inactive
static BOOL isReady( HKAI hkai )
{
    PDARTINFO        pdi = ( PDARTINFO )hkai;
    MCI_STATUS_PARMS StatusParms;
    ULONG            rc;

    memset( &StatusParms, 0, sizeof( MCI_STATUS_PARMS ));
    StatusParms.ulItem = MCI_STATUS_READY;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_STATUS,
                         MCI_WAIT | MCI_STATUS_ITEM,
                         ( PVOID )&StatusParms,
                         0 );
    if( dartError( rc ))
        return MCI_FALSE;

    return StatusParms.ulReturn;
}

static APIRET APIENTRY dartPlay( HKAI hkai )
{
    PDARTINFO pdi = ( PDARTINFO )hkai;
    int       i;
    ULONG     rc;

    if( pdi->fPlaying )
        return KAIE_NO_ERROR;

    if( !isReady( hkai ))
        return KAIE_NOT_READY;

    pdi->fWaitStreamEnd  = FALSE;
    pdi->fPlaying        = TRUE;
    pdi->fPaused         = FALSE;
    pdi->fCompleted      = FALSE;

    DosCreateEventSem( NULL, &pdi->hevFill, 0, TRUE );
    DosCreateEventSem( NULL, &pdi->hevFillDone, 0, FALSE );
    pdi->tidFillThread = _beginthread( dartFillThread, NULL, 256 * 1024, pdi );

    for( i = 0; i < pdi->ulNumBuffers; i++ )
    {
        pdi->pMixBuffers[ i ].ulBufferLength = pdi->ulBufferSize;
        pdi->pMixBuffers[ i ].ulFlags = 0;
        pdi->pMixBuffers[ i ].ulUserParm = ( ULONG )pdi;
    }

    for( i = 0; i < DART_MIN_BUFFERS; i++ )
    {
        DART_FILL_BUFFERS( pdi, &pdi->pMixBuffers[ i ]);

        if( pdi->fWaitStreamEnd )
            break;
    }

    if( i < DART_MIN_BUFFERS )
        i++;

    rc = pdi->MixSetupParms.pmixWrite( pdi->MixSetupParms.ulMixHandle,
                                       pdi->pMixBuffers, i );
    if( dartError( rc ))
    {
        dartStop( hkai );

        return LOUSHORT( rc );
    }

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartPause( HKAI hkai )
{
    PDARTINFO         pdi = ( PDARTINFO )hkai;
    MCI_GENERIC_PARMS GenericParms;
    ULONG             rc;

    if( pdi->fPaused )
        return KAIE_NO_ERROR;

    pdi->usLeftVol  = dartGetVolume( hkai, MCI_STATUS_AUDIO_LEFT );
    pdi->usRightVol = dartGetVolume( hkai, MCI_STATUS_AUDIO_RIGHT );

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_PAUSE,
                         MCI_WAIT,
                         ( PVOID )&GenericParms,
                         0 );
    if( dartError( rc ))
        return LOUSHORT( rc );

    pdi->fPaused = TRUE;

    return KAIE_NO_ERROR;
}


static APIRET APIENTRY dartResume( HKAI hkai )
{
    PDARTINFO         pdi = ( PDARTINFO )hkai;
    MCI_GENERIC_PARMS GenericParms;
    ULONG             rc;

    if( !pdi->fPaused )
        return KAIE_NO_ERROR;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_RESUME,
                         MCI_WAIT,
                         ( PVOID )&GenericParms,
                         0 );
    if( dartError( rc ))
        return LOUSHORT( rc );

    pdi->fPaused = FALSE;

    // setting volume of channels separately can be failed.
    if( pdi->usLeftVol == pdi->usRightVol )
        dartSetVolume( hkai, MCI_SET_AUDIO_ALL, pdi->usLeftVol );
    else
    {
        dartSetVolume( hkai, MCI_SET_AUDIO_LEFT, pdi->usLeftVol );
        dartSetVolume( hkai, MCI_SET_AUDIO_RIGHT, pdi->usRightVol );
    }

    return KAIE_NO_ERROR;
}

#if 0
static APIRET APIENTRY dartGetPos( HKAI hkai )
{
    PDARTINFO        pdi = ( PDARTINFO )hkai;
    MCI_STATUS_PARMS StatusParms;
    ULONG            rc;

    memset( &StatusParms, 0, sizeof( MCI_STATUS_PARMS ));
    StatusParms.ulItem = MCI_STATUS_POSITION;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_STATUS,
                         MCI_WAIT | MCI_STATUS_ITEM,
                         ( PVOID )&StatusParms,
                         0 );
    if( dartError( rc ))
        return 0;

    return StatusParms.ulReturn;
}

static APIRET APIENTRY dartSetPos( HKAI hkai, ULONG ulNewPos )
{
    PDARTINFO       pdi = ( PDARTINFO )hkai;
    MCI_SEEK_PARMS  SeekParms;
    APIRET          rc;

    SeekParms.hwndCallback = 0;
    SeekParms.ulTo = ulNewPos;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_SEEK,
                         MCI_WAIT | MCI_TO,
                         ( PVOID )&SeekParms,
                         0 );
    if( dartError( rc ))
        return LOUSHORT( rc );

    return KAIE_NO_ERROR;
}
#endif

static APIRET APIENTRY dartSetSoundState( HKAI hkai, ULONG ulCh, BOOL fState )
{
    PDARTINFO     pdi = ( PDARTINFO )hkai;
    MCI_SET_PARMS SetParms;
    USHORT        usSt;
    ULONG         rc;

    if( fState)
        usSt = MCI_SET_ON;
    else
        usSt = MCI_SET_OFF;

    SetParms.ulAudio = ulCh;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_SET,
                         MCI_WAIT | MCI_SET_AUDIO | usSt,
                         ( PVOID)&SetParms, 0 );
    if( dartError( rc ))
        return LOUSHORT( rc );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol )
{
    PDARTINFO     pdi = ( PDARTINFO )hkai;
    MCI_SET_PARMS SetParms;
    ULONG         rc;

    SetParms.ulLevel = usVol;
    SetParms.ulAudio = ulCh;
    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_SET,
                         MCI_WAIT | MCI_SET_AUDIO |
                         MCI_SET_VOLUME,
                         ( PVOID )&SetParms, 0);
    if( dartError( rc ))
        return LOUSHORT( rc );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartGetVolume( HKAI hkai, ULONG ulCh )
{
    PDARTINFO        pdi = ( PDARTINFO )hkai;
    MCI_STATUS_PARMS StatusParms;
    USHORT           usLeft, usRight;
    ULONG            rc;

    memset( &StatusParms, 0, sizeof( MCI_STATUS_PARMS ));
    StatusParms.ulItem = MCI_STATUS_VOLUME;

    rc = mciSendCommand( pdi->usDeviceID,
                         MCI_STATUS,
                         MCI_WAIT | MCI_STATUS_ITEM,
                         ( PVOID )&StatusParms,
                         0 );
    if( dartError( rc ))
        return 0;

    usLeft  = LOUSHORT( StatusParms.ulReturn );
    usRight = HIUSHORT( StatusParms.ulReturn );

    if( ulCh == MCI_STATUS_AUDIO_LEFT )
        return usLeft;
    else if( ulCh == MCI_STATUS_AUDIO_RIGHT )
        return usRight;

    return ( usLeft + usRight ) / 2;
}

static APIRET APIENTRY dartStatus( HKAI hkai )
{
    PDARTINFO pdi = ( PDARTINFO )hkai;
    ULONG     ulStatus = 0;

    if( pdi->fPlaying )
        ulStatus |= KAIS_PLAYING;

    if( pdi->fPaused )
        ulStatus |= KAIS_PAUSED;

    if( pdi->fCompleted )
        ulStatus |= KAIS_COMPLETED;

    return ulStatus;
}

/******************************************************************************/
// OS/2 32-bit program to query the Physical Device Driver name
// for the default MMPM/2 WaveAudio device.  Joe Nord 10-Mar-1999
/******************************************************************************/
static APIRET APIENTRY dartOSLibGetAudioPDDName( PSZ pszPDDName )
{
    ULONG                   ulRC;
    CHAR                    szAmpMix[9] = "AMPMIX01";

    MCI_SYSINFO_PARMS       SysInfo;
    MCI_SYSINFO_LOGDEVICE   SysInfoParm;
    MCI_SYSINFO_QUERY_NAME  QueryNameParm;

    if( !loadMDM())
        return KAIE_CANNOT_LOAD_SUB_MODULE;

    memset( &SysInfo, '\0', sizeof( SysInfo ));
    memset( &SysInfoParm, '\0', sizeof( SysInfoParm ));
    memset( &QueryNameParm, '\0', sizeof( QueryNameParm ));

    SysInfo.ulItem       = MCI_SYSINFO_QUERY_NAMES;
    SysInfo.usDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
    SysInfo.pSysInfoParm = &QueryNameParm;

    strcpy( QueryNameParm.szLogicalName, szAmpMix );

    ulRC = mciSendCommand( 0,
                           MCI_SYSINFO,
                           MCI_SYSINFO_ITEM | MCI_WAIT,
                           ( PVOID )&SysInfo,
                           0 );
    if( dartError( ulRC ))
        goto exit;

//   printf("Audio:\n install name [%s]\n logical name [%s]\n alias [%s]\n type num: %i\n ord num: %i\n",
//          QueryNameParm.szInstallName,  /*  Device install name. */
//          QueryNameParm.szLogicalName,  /*  Logical device name. */
//          QueryNameParm.szAliasName,    /*  Alias name. */
//          QueryNameParm.usDeviceType,   /*  Device type number. */
//          QueryNameParm.usDeviceOrd);   /*  Device type o */

    // Get PDD associated with our AmpMixer
    // Device name is in pSysInfoParm->szPDDName
    SysInfo.ulItem       = MCI_SYSINFO_QUERY_DRIVER;
    SysInfo.usDeviceType = MCI_DEVTYPE_WAVEFORM_AUDIO;
    SysInfo.pSysInfoParm = &SysInfoParm;

    strcpy( SysInfoParm.szInstallName, QueryNameParm.szInstallName );

    ulRC = mciSendCommand( 0,
                           MCI_SYSINFO,
                           MCI_SYSINFO_ITEM | MCI_WAIT,
                           ( PVOID )&SysInfo,
                           0 );
    if( dartError( ulRC ))
        goto exit;

//    strcpy( pszPDDName, SysInfoParm.szPDDName );
    strcpy( pszPDDName, SysInfoParm.szProductInfo );
//    printf("Audio:\n product info [%s]\n\n",SysInfoParm.szProductInfo);
//    printf("Audio:\n inst name [%s]\n version [%s]\n MCD drv [%s]\n VSD drv [%s]\n res name: [%s]\n",
//           SysInfoParm.szInstallName,
//           SysInfoParm.szVersionNumber,
//           SysInfoParm.szMCDDriver,
//           SysInfoParm.szVSDDriver,
//           SysInfoParm.szResourceName);

exit:
#ifndef LET_SYSTEM_FREE_MODULE
    freeMDM();
#endif

    return LOUSHORT( ulRC );
}

