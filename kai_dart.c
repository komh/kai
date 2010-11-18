/*
    Direct Audio Interface for K Audio Interface
    Copyright (C) 2007-2010 by KO Myung-Hun <komh@chollian.net>
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

    Changes :
        KO Myung-Hun <komh@chollian.net> 2007/02/03
            - if ulNumBuffer in dartInit() is 0, it is set to DART_MIN_BUFFERS
              not the suggested thing by the mixer device.

        KO Myung-Hun <komh@chollian.net> 2007/02/11
            - Use MCI_SET_AUDIO_* macros instead of DART_CH_* macros.

        KO Myung-Hun <komh@chollian.net> 2007/02/16
            - Prevent dartPlay() and dartStop() from executing many times.
            - Added buffer filling thread to reserve enough stack because
              DART callback stack is too small. (by Dmitry Froloff)
            - Use fPlaying instead of fStopped.

        KO Myung-Hun <komh@chollian.net> 2007/02/25
            - Added the following variables as static storage instead of
              as global storage in DARTSTRUCT.

                BOOL               m_fWaitStreamEnd
                BOOL               m_fShareable
                ULONG              m_ulCurrVolume
                USHORT             m_usDeviceID
                PMCI_MIX_BUFFER    m_pMixBuffers
                MCI_MIXSETUP_PARMS m_MixSetupParms
                MCI_BUFFER_PARMS   m_BufferParms
                PFNDICB            m_pfndicb

        KO Myung-Hun <komh@chollian.net> 2007/04/09
            - Changed output stream of dartError() from stdout to stderr.
              ( by Dmitry Froloff )

        KO Myung-Hun <komh@chollian.net> 2007/04/24
            - Use PRTYD_MAXIMUM instead of +31 for DosSetPriority().

        KO Myung-Hun <komh@chollian.net> 2007/06/12
            - Added MCI_WAIT flag to some mciSendCommand() calls.
            - Fixed a invalid command to release the exclusive use of device
              instance.

        KO Myung-Hun <komh@chollian.net> 2007/12/25
            - Do not change the priority of dartFillThread() to TIMECRITICAL.
              SNAP seems not to like this.

        KO Myung-Hun <komh@chollian.net> 2008/03/30
            - Added callback data as a parameter to dartInit().

        KO Myung-Hun <komh@chollian.net> 2008/08/11
            - Load MDM.DLL dynamically, so no need to link with mmpm2.

        KO Myung-Hun <komh@chollian.net> 2008/09/27
            - Include process.h for _beginthread() declaration in case of
              Open Watcom.

        KO Myung-Hun <komh@chollian.net> 2010/01/09
            - Revised for KAI
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

// DART need 2 buffers at least to play.
#define DART_MIN_BUFFERS     2

#pragma pack( 1 )
typedef struct DARTSTRUCT
{
    volatile BOOL    fPlaying;
    volatile BOOL    fPaused;
             BOOL    fCompleted;
             ULONG   ulBufferSize;
             ULONG   ulNumBuffers;
             BYTE    bSilence;
             CHAR    szErrorCode[ CCHMAXPATH ];
} DARTSTRUCT, *PDARTSTRUCT;
#pragma pack()

static DARTSTRUCT m_DART = { 0 };

static HEV   m_hevFill = NULLHANDLE;
static HEV   m_hevFillDone = NULLHANDLE;
static TID   m_tidFillThread = 0;
static PVOID m_pFillArg = NULL;

static BOOL               m_fWaitStreamEnd = FALSE;
static BOOL               m_fShareable = FALSE;
static ULONG              m_ulCurrVolume = 0;
static USHORT             m_usDeviceID = 0;
static PMCI_MIX_BUFFER    m_pMixBuffers = NULL;
static MCI_MIXSETUP_PARMS m_MixSetupParms = { 0 , };
static MCI_BUFFER_PARMS   m_BufferParms = { 0, };
static PFNKAICB           m_pfndicb = NULL;
static PVOID              m_pCBData = NULL;

static HMODULE m_hmodMDM = NULLHANDLE;

static DECLARE_PFN( ULONG, APIENTRY, m_pfnmciSendCommand, ( USHORT, USHORT, ULONG, PVOID, USHORT ));
static DECLARE_PFN( ULONG, APIENTRY, m_pfnmciGetErrorString, ( ULONG, PSZ, USHORT ));

static APIRET APIENTRY dartDone( VOID );
static APIRET APIENTRY dartOpen( PKAISPEC pks );
static APIRET APIENTRY dartClose( VOID );
static APIRET APIENTRY dartPlay( VOID );
static APIRET APIENTRY dartStop( VOID );
static APIRET APIENTRY dartPause( VOID );
static APIRET APIENTRY dartResume( VOID );
static APIRET APIENTRY dartGetPos( VOID );
static APIRET APIENTRY dartSetPos( ULONG ulNewPos );
static APIRET APIENTRY dartError( APIRET rc );
static APIRET APIENTRY dartSetSoundState( ULONG ulCh, BOOL fState );
static APIRET APIENTRY dartSetVolume( ULONG ulCh, USHORT usVol );
static APIRET APIENTRY dartGetVolume( ULONG );
static APIRET APIENTRY dartChNum( VOID );
static APIRET APIENTRY dartClearBuffer( VOID );
static APIRET APIENTRY dartStatus( VOID );

static VOID freeMDM( VOID )
{
    DosFreeModule( m_hmodMDM );
    m_hmodMDM = NULLHANDLE;
}

static BOOL loadMDM( VOID )
{
    UCHAR szFailedName[ 256 ];

    if( m_hmodMDM )
        return TRUE;

    if( DosLoadModule( szFailedName, sizeof( szFailedName ), "MDM.DLL", &m_hmodMDM ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodMDM, 1, NULL, ( PFN * )&m_pfnmciSendCommand ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodMDM, 3, NULL, ( PFN * )&m_pfnmciGetErrorString ))
        goto exit_error;

    return TRUE;

exit_error:
    // In case of this, MDM.DLL is not a MMOS2 DLL.
    freeMDM();

    return FALSE;
}

APIRET APIENTRY kaiDartInit( PKAIAPIS pkai, PULONG pulMaxChannels )
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

    *pulMaxChannels  = dartChNum();

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
    if(( USHORT )rc )
    {
        m_pfnmciGetErrorString( rc,
                                ( PSZ )m_DART.szErrorCode,
                                sizeof( m_DART.szErrorCode ));

        fprintf( stderr, "\nDART error(%lx):%s\n", rc, m_DART.szErrorCode );

        return rc;
    }

    m_DART.szErrorCode[ 0 ] = 0;

    return 0;
}

#define USE_UNIAUD_WORKAROUND

static APIRET APIENTRY dartStop(void)
{
    MCI_GENERIC_PARMS GenericParms;
    ULONG             rc;

    if( !m_DART.fPlaying )
        return KAIE_NO_ERROR;

#ifdef USE_UNIAUD_WORKAROUND
    // workaround for uniaud
    // clean up thread before MCI_STOP
    // otherwise looping sound or dead lock can occur when trying to stop
    // but assume that MCI_STOP always succeeds
    m_DART.fPlaying = FALSE;
    m_DART.fPaused  = FALSE;

    DosPostEventSem( m_hevFill );
    while( DosWaitThread( &m_tidFillThread, DCWW_WAIT ) == ERROR_INTERRUPT );
    DosCloseEventSem( m_hevFill );

    DosPostEventSem( m_hevFillDone);
    DosCloseEventSem( m_hevFillDone );
#endif

    memset( &GenericParms, 0, sizeof( GenericParms ));

    GenericParms.hwndCallback = 0;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_STOP,
                              MCI_WAIT,
                              ( PVOID )&GenericParms,
                              0 );
    if( dartError( rc ))
        return rc;

#ifndef USE_UNIAUD_WORKAROUND
    m_DART.fPlaying = FALSE;
    m_DART.fPaused  = FALSE;

    DosPostEventSem( m_hevFill );
    while( DosWaitThread( &m_tidFillThread, DCWW_WAIT ) == ERROR_INTERRUPT );
    DosCloseEventSem( m_hevFill );

    DosPostEventSem( m_hevFillDone);
    DosCloseEventSem( m_hevFillDone );
#endif

    m_pFillArg = NULL;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartClearBuffer( VOID )
{
    int i;

    for( i = 0; i < m_DART.ulNumBuffers; i++)
       memset( m_pMixBuffers[ i ].pBuffer, m_DART.bSilence, m_DART.ulBufferSize );

    return KAIE_NO_ERROR;
}

static ULONG dartFillBuffer( PMCI_MIX_BUFFER pBuffer )
{
    ULONG ulWritten;

    memset( pBuffer->pBuffer, m_DART.bSilence, m_DART.ulBufferSize );

    ulWritten = m_pfndicb( m_pCBData, pBuffer->pBuffer, m_DART.ulBufferSize );
    if( ulWritten < m_DART.ulBufferSize )
    {
        pBuffer->ulFlags = MIX_BUFFER_EOS;
        m_fWaitStreamEnd = TRUE;
    }
    else
        pBuffer->ulFlags = 0;

    return ulWritten;
}

static APIRET APIENTRY dartFreeBuffers( VOID )
{
    APIRET  rc;

    if( m_pMixBuffers == NULL )
        return KAIE_NO_ERROR;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_BUFFER,
                              MCI_WAIT | MCI_DEALLOCATE_MEMORY,
                              ( PVOID )&m_BufferParms, 0 );
    if( dartError( rc ))
        return rc;

    if( m_pMixBuffers )
        free( m_pMixBuffers );

    m_pMixBuffers = NULL;

    return KAIE_NO_ERROR;
}

static void dartFillThread( void *arg )
{
    ULONG ulPost;

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );

    for(;;)
    {
        while( DosWaitEventSem( m_hevFill, SEM_INDEFINITE_WAIT ) == ERROR_INTERRUPT );
        DosResetEventSem( m_hevFill, &ulPost );

        if( !m_DART.fPlaying )
            break;

        // Transfer buffer to DART
        dartFillBuffer( m_pFillArg );

        DosPostEventSem( m_hevFillDone );
    }
}

#define DART_FILL_BUFFERS( pBuffer ) \
{\
    static ULONG ulPost = 0;\
\
    m_pFillArg = pBuffer;\
    DosPostEventSem( m_hevFill );\
\
    while( DosWaitEventSem( m_hevFillDone, SEM_INDEFINITE_WAIT ) == ERROR_INTERRUPT );\
    DosResetEventSem( m_hevFillDone, &ulPost );\
}

static LONG APIENTRY MixHandler( ULONG ulStatus, PMCI_MIX_BUFFER pBuffer, ULONG ulFlags )
{
    switch( ulFlags )
    {
        case MIX_STREAM_ERROR | MIX_WRITE_COMPLETE:
            // on error, fill next buffer and continue
        case MIX_WRITE_COMPLETE:
        {
            // If this is the last buffer, stop
            if( pBuffer->ulFlags & MIX_BUFFER_EOS)
            {
                dartStop();

                m_DART.fCompleted = TRUE;
            }
            else if( m_DART.fPlaying && !m_fWaitStreamEnd )
            {
                DART_FILL_BUFFERS( pBuffer );

                m_MixSetupParms.pmixWrite( m_MixSetupParms.ulMixHandle, m_pFillArg, 1 );
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
    if( m_pfnmciSendCommand( 0, MCI_OPEN, ulDartFlags, ( PVOID )&AmpOpenParms, 0 ))
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
    if( m_pfnmciSendCommand( AmpOpenParms.usDeviceID,
                             MCI_MIXSETUP, MCI_WAIT | MCI_MIXSETUP_INIT,
                             ( PVOID )&MixSetupParms, 0 ))
    {
        ulChannels = 4; // failed. try 4 channels
        MixSetupParms.ulChannels = ulChannels;
        if( m_pfnmciSendCommand( AmpOpenParms.usDeviceID,
                                 MCI_MIXSETUP, MCI_WAIT | MCI_MIXSETUP_INIT,
                                 ( PVOID )&MixSetupParms, 0 ))
            ulChannels = 2; // failed again...so, drivers support only 2 channels
    }

    m_pfnmciSendCommand( AmpOpenParms.usDeviceID,
                         MCI_CLOSE, MCI_WAIT,
                         ( PVOID )&GenericParms, 0 );

    return ulChannels;
}

static APIRET APIENTRY dartOpen( PKAISPEC pks )
{
    APIRET              rc;
    ULONG               ulDartFlags;
    MCI_AMP_OPEN_PARMS  AmpOpenParms;
    MCI_GENERIC_PARMS   GenericParms;

    memset( &m_DART, 0, sizeof( DARTSTRUCT ));

    m_DART.bSilence = ( pks->ulBitsPerSample == BPS_8 ) ? 0x80 : 0x00;

    m_fShareable = pks->fShareable;
    if( m_fShareable )
        ulDartFlags = MCI_WAIT | MCI_OPEN_TYPE_ID | MCI_OPEN_SHAREABLE;
    else
        ulDartFlags = MCI_WAIT | MCI_OPEN_TYPE_ID;

    memset( &AmpOpenParms, 0, sizeof( MCI_AMP_OPEN_PARMS ));

    AmpOpenParms.usDeviceID = 0;
    AmpOpenParms.pszDeviceType = (PSZ)( MCI_DEVTYPE_AUDIO_AMPMIX |
                                        (( ULONG )pks->usDeviceIndex << 16 ));
    rc = m_pfnmciSendCommand( 0,
                              MCI_OPEN,
                              ulDartFlags,
                              ( PVOID )&AmpOpenParms,
                              0 );
    if( dartError( rc ))
        return rc;

    m_usDeviceID = AmpOpenParms.usDeviceID;

    if( !m_fShareable )
    {
        // Grab exclusive rights to device instance (NOT entire device)
        GenericParms.hwndCallback = 0;
        rc = m_pfnmciSendCommand( m_usDeviceID,
                                  MCI_ACQUIREDEVICE,
                                  MCI_WAIT | MCI_EXCLUSIVE_INSTANCE,
                                  ( PVOID )&GenericParms,
                                  0 );
        if( dartError( rc ))
            goto exit_release;
    }

    // Setup the mixer for playback of wave data
    memset( &m_MixSetupParms, 0, sizeof( MCI_MIXSETUP_PARMS ));

    m_MixSetupParms.ulBitsPerSample = pks->ulBitsPerSample;
    m_MixSetupParms.ulSamplesPerSec = pks->ulSamplingRate;
    m_MixSetupParms.ulFormatTag     = pks->ulDataFormat;
    m_MixSetupParms.ulChannels      = pks->ulChannels;
    m_MixSetupParms.ulFormatMode    = MCI_PLAY;
    m_MixSetupParms.ulDeviceType    = MCI_DEVTYPE_WAVEFORM_AUDIO;
    m_MixSetupParms.pmixEvent       = ( MIXEREVENT * )MixHandler;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_MIXSETUP,
                              MCI_WAIT | MCI_MIXSETUP_INIT,
                              ( PVOID )&m_MixSetupParms,
                              0 );

    if( dartError( rc ))
        goto exit_close;

    // Use the suggested buffer number and size provide by the mixer device if 0
    if( pks->ulNumBuffers == 0 )
        pks->ulNumBuffers = m_MixSetupParms.ulNumBuffers;

    if( pks->ulNumBuffers < DART_MIN_BUFFERS )
        pks->ulNumBuffers = DART_MIN_BUFFERS;

    if( pks->ulBufferSize == 0 )
        pks->ulBufferSize = m_MixSetupParms.ulBufferSize;

    // Allocate mixer buffers
    m_pMixBuffers = ( MCI_MIX_BUFFER * )malloc( sizeof( MCI_MIX_BUFFER ) * pks->ulNumBuffers );

    // Set up the BufferParms data structure and allocate device buffers
    // from the Amp-Mixer
    m_BufferParms.ulStructLength = sizeof( MCI_BUFFER_PARMS );
    m_BufferParms.ulNumBuffers   = pks->ulNumBuffers;
    m_BufferParms.ulBufferSize   = pks->ulBufferSize;
    m_BufferParms.pBufList       = m_pMixBuffers;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_BUFFER,
                              MCI_WAIT | MCI_ALLOCATE_MEMORY,
                              ( PVOID )&m_BufferParms,
                              0 );
    if( dartError( rc ))
        goto exit_deallocate;

    // The mixer possibly changed these values
    m_DART.ulNumBuffers = m_BufferParms.ulNumBuffers;
    m_DART.ulBufferSize = m_BufferParms.ulBufferSize;

    m_pfndicb = pks->pfnCallBack;
    m_pCBData = pks->pCallBackData;

    pks->ulNumBuffers = m_DART.ulNumBuffers;
    pks->ulBufferSize = m_DART.ulBufferSize;
    pks->bSilence     = m_DART.bSilence;

    return KAIE_NO_ERROR;

exit_deallocate :
    free( m_pMixBuffers );
    m_pMixBuffers = NULL;

exit_release :
    if( !m_fShareable )
    {
        // Release exclusive rights to device instance (NOT entire device)
        m_pfnmciSendCommand( m_usDeviceID,
                             MCI_RELEASEDEVICE,
                             MCI_WAIT | MCI_RETURN_RESOURCE,
                             ( PVOID )&GenericParms,
                             0 );
    }

exit_close :
    m_pfnmciSendCommand( m_usDeviceID,
                         MCI_CLOSE,
                         MCI_WAIT,
                         ( PVOID )&GenericParms,
                         0 );

    return rc;
}


static APIRET APIENTRY dartClose( VOID )
{
    MCI_GENERIC_PARMS   GenericParms;
    APIRET              rc;

    dartStop();
    dartFreeBuffers();

    GenericParms.hwndCallback = 0;

    if( !m_fShareable )
    {
        // Release exclusive rights to device instance (NOT entire device)
        rc = m_pfnmciSendCommand( m_usDeviceID,
                                  MCI_RELEASEDEVICE,
                                  MCI_WAIT | MCI_RETURN_RESOURCE,
                                  ( PVOID )&GenericParms,
                                  0 );
        if( dartError( rc ))
            return rc;
    }

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_CLOSE,
                              MCI_WAIT,
                              ( PVOID )&GenericParms,
                              0 );
    if( dartError( rc ))
        return rc;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartPlay( VOID )
{
    int   i;
    ULONG rc;

    if( m_DART.fPlaying )
        return KAIE_NO_ERROR;

    m_fWaitStreamEnd  = FALSE;
    m_DART.fPlaying   = TRUE;
    m_DART.fPaused    = FALSE;
    m_DART.fCompleted = FALSE;

    DosCreateEventSem( NULL, &m_hevFill, 0, FALSE );
    DosCreateEventSem( NULL, &m_hevFillDone, 0, FALSE );
    m_tidFillThread = _beginthread( dartFillThread, NULL, 256 * 1024, NULL );

    for( i = 0; i < m_DART.ulNumBuffers; i++ )
    {
        m_pMixBuffers[ i ].ulBufferLength = m_DART.ulBufferSize;
        m_pMixBuffers[ i ].ulFlags = 0;
    }

    for( i = 0; i < DART_MIN_BUFFERS; i++ )
    {
        DART_FILL_BUFFERS( &m_pMixBuffers[ i ]);

        if( m_fWaitStreamEnd )
            break;
    }

    if( i < DART_MIN_BUFFERS )
        i++;

    rc = m_MixSetupParms.pmixWrite( m_MixSetupParms.ulMixHandle,
                                    m_pMixBuffers, i );
    if( dartError( rc ))
    {
        dartStop();

        return rc;
    }

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartPause( VOID )
{
    MCI_GENERIC_PARMS GenericParms;
    ULONG             rc;

    if( m_DART.fPaused )
        return KAIE_NO_ERROR;

    m_ulCurrVolume = dartGetVolume( MCI_STATUS_AUDIO_ALL );
    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_PAUSE,
                              MCI_WAIT,
                              ( PVOID )&GenericParms,
                              0 );
    if( dartError( rc ))
        return rc;

    m_DART.fPaused = TRUE;

    return KAIE_NO_ERROR;
}


static APIRET APIENTRY dartResume( VOID )
{
    MCI_GENERIC_PARMS GenericParms;
    ULONG             rc;

    if( !m_DART.fPaused )
        return KAIE_NO_ERROR;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_RESUME,
                              MCI_WAIT,
                              ( PVOID )&GenericParms,
                              0 );
    if( dartError( rc ))
        return rc;

    // setting volume of channels separately can be failed.
    if( LOUSHORT( m_ulCurrVolume ) == HIUSHORT( m_ulCurrVolume ))
        dartSetVolume( MCI_SET_AUDIO_ALL, LOUSHORT( m_ulCurrVolume ));
    else
    {
        dartSetVolume( MCI_SET_AUDIO_LEFT, LOUSHORT( m_ulCurrVolume ));
        dartSetVolume( MCI_SET_AUDIO_RIGHT, HIUSHORT( m_ulCurrVolume ));
    }

    m_DART.fPaused = FALSE;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartGetPos( VOID )
{
    MCI_STATUS_PARMS StatusParms;
    ULONG            rc;

    memset( &StatusParms, 0, sizeof( MCI_STATUS_PARMS ));
    StatusParms.ulItem = MCI_STATUS_POSITION;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_STATUS,
                              MCI_WAIT | MCI_STATUS_ITEM,
                              ( PVOID )&StatusParms,
                              0 );
    if( dartError( rc ))
        return 0;

    return StatusParms.ulReturn;
}


static APIRET APIENTRY dartSetPos( ULONG ulNewPos )
{
    APIRET          rc;
    MCI_SEEK_PARMS  SeekParms;

    SeekParms.hwndCallback = 0;
    SeekParms.ulTo = ulNewPos;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_SEEK,
                              MCI_WAIT | MCI_TO,
                              ( PVOID )&SeekParms,
                              0 );
    if( dartError( rc ))
        return rc;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartSetSoundState( ULONG ulCh, BOOL fState)
{
    MCI_SET_PARMS SetParms;
    USHORT        usSt;
    ULONG         rc;

    if( fState)
        usSt = MCI_SET_ON;
    else
        usSt = MCI_SET_OFF;

    SetParms.ulAudio = ulCh;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_SET,
                              MCI_WAIT | MCI_SET_AUDIO | usSt,
                              ( PVOID)&SetParms, 0 );
    if( dartError( rc ))
        return rc;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartSetVolume( ULONG ulCh, USHORT usVol)
{
    MCI_SET_PARMS SetParms;
    ULONG         rc;

    SetParms.ulLevel = usVol;
    SetParms.ulAudio = ulCh;
    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_SET,
                              MCI_WAIT | MCI_SET_AUDIO |
                              MCI_SET_VOLUME,
                              ( PVOID )&SetParms, 0);
    if( dartError( rc ))
        return rc;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY dartGetVolume( ULONG ulCh )
{
    MCI_STATUS_PARMS StatusParms;
    ULONG            rc;

    memset(&StatusParms, 0, sizeof( MCI_STATUS_PARMS ));
    StatusParms.ulItem = MCI_STATUS_VOLUME;

    rc = m_pfnmciSendCommand( m_usDeviceID,
                              MCI_STATUS,
                              MCI_WAIT | MCI_STATUS_ITEM,
                              ( PVOID )&StatusParms,
                              0 );
    if( dartError( rc ))
        return 0;

    return StatusParms.ulReturn;
}

static APIRET APIENTRY dartStatus( VOID )
{
    ULONG ulStatus = 0;

    if( m_DART.fPlaying )
        ulStatus |= KAIS_PLAYING;

    if( m_DART.fPaused )
        ulStatus |= KAIS_PAUSED;

    if( m_DART.fCompleted )
        ulStatus |= KAIS_COMPLETED;

    return ulStatus;
}

/******************************************************************************/
// OS/2 32-bit program to query the Physical Device Driver name
// for the default MMPM/2 WaveAudio device.  Joe Nord 10-Mar-1999
/******************************************************************************/
APIRET APIENTRY kaiOSLibGetAudioPDDName( PSZ pszPDDName )
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

    ulRC = m_pfnmciSendCommand( 0,
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

    ulRC = m_pfnmciSendCommand( 0,
                                MCI_SYSINFO,
                                MCI_SYSINFO_ITEM | MCI_WAIT,
                                ( PVOID )&SysInfo,
                                0 );
    if( dartError( ulRC ))
        goto exit;

//    strcpy( pszPDDName, SysInfoParm.szPDDName );
    strcpy ( pszPDDName, SysInfoParm.szProductInfo );
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

    return ulRC;
}

