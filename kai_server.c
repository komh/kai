/*
    Audio Server Interface for K Audio Interface
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

#include <stdio.h>
#include <string.h>
#include <process.h>

// While processing MCI_CLOSE in MCD such as ksoftseq, if the other MCI_CLOSE
// of DART in the other process is called, it will be blocked. As a result,
// trying to read rc from MCI_CLOSE of kaisrv in MCI_CLOSE of ksoftseq leads
// to dead-lock.
#define WAIT_CLOSE_RC   0

typedef struct CBPARM
{
    PINSTANCELIST pil;
    int          *pState;
} CBPARM, *PCBPARM;

static void callbackThread( void *args )
{
    PCBPARM pcbparm = args;
    PINSTANCELIST pil = pcbparm->pil;
    int *pState = pcbparm->pState;

    HPIPE hpipe;
    PFNKAICB pfnCb;
    PVOID pData;

    PVOID pBuffer;
    ULONG ulBufSize;
    ULONG ulActual;
    ULONG rc;

    // Wait for a state to change
    while( LOAD( pState ) == 0 )
        DosSleep( 1 );

    // Error ?
    if( LOAD( pState ) == -1 )
        return;

    // State must be 1, then initialize varaibles
    hpipe = pil->hpipeCb;
    pfnCb = pil->pfnUserCb;
    pData = pil->pUserData;

    // Now we're ready
    STORE( pState, 0 );

    boostThread();

    for(;;)
    {
        REINTR( DosConnectNPipe( hpipe ), rc );

        DosRead( hpipe, &ulBufSize, sizeof( ulBufSize ), &ulActual );
        if( ulBufSize == 0 )
        {
            DosDisConnectNPipe( hpipe );

            break;
        }

        pBuffer = malloc( ulBufSize );

        ulBufSize = pfnCb( pData, pBuffer, ulBufSize );

        DosWrite( hpipe, &ulBufSize, sizeof( ulBufSize ), &ulActual );
        DosWrite( hpipe, pBuffer, ulBufSize, &ulActual );

        free( pBuffer );

        // Wait for ack before disconnecting a pipe
        DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

        DosDisConnectNPipe( hpipe );
    }
}

APIRET _kaiServerCheck( void )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_CHECK;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerCaps( PKAICAPS pkaic )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_CAPS;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );
    DosRead( hpipe, pkaic, sizeof( *pkaic ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerOpen( const PKAISPEC pksWanted, PKAISPEC pksObtained,
                       PHKAI phkai )
{
    return _kaiServerMixerStreamOpen( NULL, pksWanted, pksObtained,
                                      ( PHKAIMIXERSTREAM )phkai );
}

APIRET _kaiServerClose( PINSTANCELIST pil )
{
    return _kaiServerMixerStreamClose( NULL, pil );
}

APIRET _kaiServerPlay( PINSTANCELIST pil )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_PLAY;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerStop( PINSTANCELIST pil )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_STOP;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerPause( PINSTANCELIST pil )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_PAUSE;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerResume( PINSTANCELIST pil )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_RESUME;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerSetSoundState( PINSTANCELIST pil, ULONG ulCh, BOOL fState )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_SETSOUNDSTATE;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );
    DosWrite( hpipe, &ulCh, sizeof( ulCh ), &ulActual );
    DosWrite( hpipe, &fState, sizeof( fState ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerSetVolume( PINSTANCELIST pil, ULONG ulCh, USHORT usVol )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_SETVOLUME;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );
    DosWrite( hpipe, &ulCh, sizeof( ulCh ), &ulActual );
    DosWrite( hpipe, &usVol, sizeof( usVol ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerGetVolume( PINSTANCELIST pil, ULONG ulCh )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_GETVOLUME;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );
    DosWrite( hpipe, &ulCh, sizeof( ulCh ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerClearBuffer( PINSTANCELIST pil )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_CLEARBUFFER;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerStatus( PINSTANCELIST pil )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_STATUS;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerEnableSoftVolume( PINSTANCELIST pil, BOOL fEnable )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_ENABLESOFTVOLUME;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );
    DosWrite( hpipe, &fEnable, sizeof( fEnable ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerMixerOpen( const PKAISPEC pksWanted, PKAISPEC pksObtained,
                            PHKAIMIXER phkm )
{
    PINSTANCELIST pil;
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_MIXEROPEN;
    ULONG ulActual;
    ULONG rc;

    pil = instanceNew( FALSE, NULL, NULL );
    if( !pil )
        return KAIE_NOT_ENOUGH_MEMORY;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
    {
        instanceFree( pil );

        return KAIE_NOT_OPENED;
    }

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, pksWanted, sizeof( *pksWanted ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );
    DosRead( hpipe, &pil->ks, sizeof( pil->ks ), &ulActual );
    DosRead( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );

    pipeClose( hpipe );

    if( rc )
        instanceFree( pil );
    else
    {
        pil->hpipeCb = 0;   // Fake HPIPE for server mode

        memcpy( pksObtained, &pil->ks, sizeof( *pksObtained ));

        *phkm = ( HKAIMIXER )pil;

        instanceAdd( *phkm, pil->hkai, pil );
    }

    return rc;
}

APIRET _kaiServerMixerClose( const PINSTANCELIST pil )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_MIXERCLOSE;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );

#if WAIT_CLOSE_RC
    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );
#else
    // Assuming that rc will be 0 is reasonable. Because if an instance is
    // valid, kaiClose()/kaiMixerClose()/kaiMixerStreamClose() almost succeeds.
    rc = 0;
#endif

    pipeClose( hpipe );

    if( !rc )
        instanceDel( pil->id );

    return rc;
}

APIRET _kaiServerMixerStreamOpen( const PINSTANCELIST pilMixer,
                                  const PKAISPEC pksWanted,
                                  PKAISPEC pksObtained,
                                  PHKAIMIXERSTREAM phkms )
{
    static int count = 0;
    static SPINLOCK lock = SPINLOCK_INIT;

    PINSTANCELIST pil;
    HPIPE hpipeCb = -1;
    HPIPE hpipeCmd;
    ULONG ulCmd = pilMixer ? KAISRV_MIXERSTREAMOPEN : KAISRV_OPEN;
    char szPipeName[ CCHMAXPATH ];
    TID tid;
    int state = 0;
    CBPARM cbparm;
    ULONG ulActual;
    ULONG rc = KAIE_NOT_OPENED;

    pil = instanceNew( FALSE, NULL, NULL );
    if( !pil )
        return KAIE_NOT_ENOUGH_MEMORY;

    spinLock( &lock );

#ifndef __IBMC__
    snprintf( szPipeName, sizeof( szPipeName ), "%s\\%x\\%x",
              KAISRV_PIPE_CB_BASE, getpid(), count++ );
#else
    sprintf( szPipeName, "%s\\%x\\%x",
             KAISRV_PIPE_CB_BASE, getpid(), count++ );
#endif

    spinUnlock( &lock );

    if( DosCreateNPipe( szPipeName, &hpipeCb,
                        NP_ACCESS_DUPLEX | NP_NOINHERIT,
                        NP_WAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 1,
                        32768, 32768, 0 ))
        goto cleanup;

    cbparm.pil    = pil;
    cbparm.pState = &state;

    tid = _beginthread( callbackThread, NULL, 1024 * 1024, &cbparm );
    if( tid == -1 )
        goto cleanup;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipeCmd ))
        goto cleanup;

    DosWrite( hpipeCmd, &ulCmd, sizeof( ulCmd ), &ulActual );
    if( pilMixer )
        DosWrite( hpipeCmd, &pilMixer->hkai, sizeof( pilMixer->hkai ),
                  &ulActual );
    DosWrite( hpipeCmd, pksWanted, sizeof( *pksWanted ), &ulActual );
    DosWrite( hpipeCmd, szPipeName, sizeof( szPipeName ), &ulActual );

    DosRead( hpipeCmd, &rc, sizeof( rc ), &ulActual );
    DosRead( hpipeCmd, &pil->ks, sizeof( pil->ks ), &ulActual );
    DosRead( hpipeCmd, &pil->hkai, sizeof( pil->hkai ), &ulActual );

    pipeClose( hpipeCmd );

cleanup:
    if( rc )
    {
        if( tid != -1 )
        {
            ULONG rc2;

            STORE( &state, -1 );

            REINTR( DosWaitThread( &tid, DCWW_WAIT ), rc2 );
        }

        if( hpipeCb != -1 )
            DosClose( hpipeCb );

        instanceFree( pil );
    }
    else
    {
        pil->pfnUserCb = pksWanted->pfnCallBack;
        pil->pUserData = pksWanted->pCallBackData;

        strcpy( pil->szPipeCbName, szPipeName );
        pil->hpipeCb   = hpipeCb;
        pil->tidCb     = tid;

        STORE( &state, 1 );

        // Wait for a callback thread to be ready
        while( LOAD( &state ) != 0 )
            DosSleep( 1 );

        memcpy( pksObtained, &pil->ks, sizeof( *pksObtained ));
        pksObtained->pfnCallBack   = pksWanted->pfnCallBack;
        pksObtained->pCallBackData = pksWanted->pCallBackData;

        *phkms = ( HKAIMIXERSTREAM )pil;

        instanceAdd( *phkms, pil->hkai, pil );
    }

    return rc;
}

APIRET _kaiServerMixerStreamClose( const PINSTANCELIST pilMixer,
                                   const PINSTANCELIST pil )
{
    HPIPE hpipe;
    ULONG ulCmd = pilMixer ? KAISRV_MIXERSTREAMCLOSE: KAISRV_CLOSE;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    if( pilMixer )
        DosWrite( hpipe, &pilMixer->hkai, sizeof( pilMixer->hkai ),
                  &ulActual );
    DosWrite( hpipe, &pil->hkai, sizeof( pil->hkai ), &ulActual );
    DosWrite( hpipe, &pil->ks.pCallBackData, sizeof( pil->ks.pCallBackData ),
              &ulActual );

#if WAIT_CLOSE_RC
    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );
#else
    // Assuming that rc will be 0 is reasonable. Because if an instance is
    // valid, kaiClose()/kaiMixerClose()/kaiMixerStreamClose() almost succeed.
    rc = 0;
#endif

    pipeClose( hpipe );

    if( !rc )
    {
        ULONG rc2;

        // Make a callback thread quit
        if( pipeOpen( pil->szPipeCbName, &hpipe ) == 0 )
        {
            ulCmd = 0;
            DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );

            pipeClose( hpipe );
        }

        REINTR( DosWaitThread( &pil->tidCb, DCWW_WAIT ), rc2 );
        DosClose( pil->hpipeCb );

        instanceDel( pil->id );
    }

    return rc;
}

APIRET _kaiServerEnableSoftMixer( BOOL fEnable, const PKAISPEC pks )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_MIXERSTREAMCLOSE;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &fEnable, sizeof( fEnable ), &ulActual );
    DosWrite( hpipe, &pks, sizeof( pks ), &ulActual );
    if( pks )
        DosWrite( hpipe, pks, sizeof( *pks ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerGetCardCount( VOID )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_GETCARDCOUNT;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

    pipeClose( hpipe );

    return rc;
}

APIRET _kaiServerCapsEx( ULONG ulDeviceIndex, PKAICAPS pkaic )
{
    HPIPE hpipe;
    ULONG ulCmd = KAISRV_CAPSEX;
    ULONG ulActual;
    ULONG rc;

    if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        return KAIE_NOT_OPENED;

    DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );
    DosWrite( hpipe, &ulDeviceIndex, sizeof( ulDeviceIndex ), &ulActual );

    DosRead( hpipe, &rc, sizeof( rc ), &ulActual );
    DosRead( hpipe, pkaic, sizeof( *pkaic ), &ulActual );

    pipeClose( hpipe );

    return rc;
}
