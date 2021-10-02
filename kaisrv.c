/*
    Audio Server for K Audio Interface
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <process.h>

#include "kai.h"
#include "kai_internal.h"
#include "kai_server.h"

static ULONG APIENTRY kaisrvCallBack( PVOID pCBData, PVOID pBuffer,
                                      ULONG ulBufSize )
{
    const char *pipeName = pCBData;
    HPIPE hpipe;
    ULONG ulBufLen;
    ULONG ulActual;

    if( pipeOpen( pipeName, &hpipe ))
        return 0;

    DosWrite( hpipe, &ulBufSize, sizeof( ulBufSize ), &ulActual );

    DosRead( hpipe, &ulBufLen, sizeof( ulBufLen ), &ulActual );
    DosRead( hpipe, pBuffer, ulBufLen, &ulActual );

    pipeClose( hpipe );

    return ulBufLen;
}

static void kaisrvCheck( HPIPE hpipe )
{
    ULONG ulActual;
    ULONG rc = 0;

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvCaps( HPIPE hpipe )
{
    KAICAPS caps;
    ULONG ulActual;
    ULONG rc;

    rc = kaiCaps( &caps );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
    DosWrite( hpipe, &caps, sizeof( caps ), &ulActual );
}

static void kaisrvOpen( HPIPE hpipe )
{
    KAISPEC specWanted, specObtained;
    char szPipeName[ CCHMAXPATH ];
    HKAI hkai;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &specWanted, sizeof( specWanted ), &ulActual );
    DosRead( hpipe, szPipeName, sizeof( szPipeName ), &ulActual );

    specWanted.pfnCallBack = kaisrvCallBack;
    specWanted.pCallBackData = strdup( szPipeName );
    if( !specWanted.pCallBackData )
        rc = KAIE_NOT_ENOUGH_MEMORY;
    else
        rc = kaiOpen( &specWanted, &specObtained, &hkai );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
    DosWrite( hpipe, &specObtained, sizeof( specObtained ), &ulActual );
    DosWrite( hpipe, &hkai, sizeof( hkai ), &ulActual );
}

static void kaisrvClose( HPIPE hpipe )
{
    HKAI hkai;
    PVOID pCallBackData;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );
    DosRead( hpipe, &pCallBackData, sizeof( pCallBackData ), &ulActual );

    rc = kaiClose( hkai );
    if( !rc )
        free( pCallBackData );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvPlay( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );

    rc = kaiPlay( hkai );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvStop( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );

    rc = kaiStop( hkai );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvPause( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );

    rc = kaiPause( hkai );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvResume( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );

    rc = kaiResume( hkai );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvSetSoundState( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulCh;
    BOOL fState;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );
    DosRead( hpipe, &ulCh, sizeof( ulCh ), &ulActual );
    DosRead( hpipe, &fState, sizeof( fState ), &ulActual );

    rc = kaiSetSoundState( hkai, ulCh, fState );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvSetVolume( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulCh;
    USHORT usVol;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );
    DosRead( hpipe, &ulCh, sizeof( ulCh ), &ulActual );
    DosRead( hpipe, &usVol, sizeof( usVol ), &ulActual );

    rc = kaiSetVolume( hkai, ulCh, usVol );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvGetVolume( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulCh;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );
    DosRead( hpipe, &ulCh, sizeof( ulCh ), &ulActual );

    rc = kaiGetVolume( hkai, ulCh );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvClearBuffer( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );

    rc = kaiClearBuffer( hkai );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvStatus( HPIPE hpipe )
{
    HKAI hkai;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );

    rc = kaiStatus( hkai );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvEnableSoftVolume( HPIPE hpipe )
{
    HKAI hkai;
    BOOL fEnable;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkai, sizeof( hkai ), &ulActual );
    DosRead( hpipe, &fEnable, sizeof( fEnable ), &ulActual );

    if( fEnable )
    {
        // Soft volume mode is always enabled in server mode
        rc = KAIE_NO_ERROR;
    }
    else
    {
        // Soft volume mode should be enabled in server mode
        rc = KAIE_INVALID_PARAMETER;
    }

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvMixerOpen( HPIPE hpipe )
{
    KAISPEC specWanted;
    KAISPEC specObtained;
    HKAIMIXER hkm;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &specWanted, sizeof( specWanted ), &ulActual );

    rc = kaiMixerOpen( &specWanted, &specObtained, &hkm );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
    DosWrite( hpipe, &specObtained, sizeof( specObtained ), &ulActual );
    DosWrite( hpipe, &hkm, sizeof( hkm ), &ulActual );
}

static void kaisrvMixerClose( HPIPE hpipe )
{
    HKAIMIXER hkm;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkm, sizeof( hkm ), &ulActual );

    rc = kaiMixerClose( hkm );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvMixerStreamOpen( HPIPE hpipe )
{
    HKAIMIXER hkm;
    KAISPEC specWanted;
    char szPipeName[ CCHMAXPATH ];

    KAISPEC specObtained;
    HKAIMIXERSTREAM hkms;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkm, sizeof( hkm ), &ulActual );
    DosRead( hpipe, &specWanted, sizeof( specWanted ), &ulActual );
    DosRead( hpipe, szPipeName, sizeof( szPipeName ), &ulActual );

    specWanted.pfnCallBack = kaisrvCallBack;
    specWanted.pCallBackData = strdup( szPipeName );
    if( !specWanted.pCallBackData )
        rc = KAIE_NOT_ENOUGH_MEMORY;
    else
        rc = kaiMixerStreamOpen( hkm, &specWanted, &specObtained, &hkms );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
    DosWrite( hpipe, &specObtained, sizeof( specObtained ), &ulActual );
    DosWrite( hpipe, &hkms, sizeof( hkms ), &ulActual );
}

static void kaisrvMixerStreamClose( HPIPE hpipe )
{
    HKAIMIXER hkm;
    HKAIMIXERSTREAM hkms;
    PVOID pCallBackData;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &hkm, sizeof( hkm ), &ulActual );
    DosRead( hpipe, &hkms, sizeof( hkms ), &ulActual );
    DosRead( hpipe, &pCallBackData, sizeof( pCallBackData ), &ulActual );

    rc = kaiMixerStreamClose( hkm, hkms );
    if( !rc )
        free( pCallBackData );

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvEnableSoftMixer( HPIPE hpipe )
{
    BOOL fEnable;
    PKAISPEC pks;
    KAISPEC ks;
    ULONG ulActual;
    ULONG rc;

    DosRead( hpipe, &fEnable, sizeof( fEnable ), &ulActual );
    DosRead( hpipe, &pks, sizeof( pks ), &ulActual );
    if( pks )
    {
        DosRead( hpipe, &ks, sizeof( ks ), &ulActual );
        pks = &ks;
    }

    if( fEnable )
    {
        // Soft mixer mode is always enabled but spec is ignored in server mode
        rc = KAIE_NO_ERROR;
    }
    else
    {
        // Soft mixer mode should be enabled in server mode
        rc = KAIE_INVALID_PARAMETER;
    }

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

static void kaisrvGetCardCount( HPIPE hpipe )
{
    ULONG ulActual;
    ULONG rc;

    rc = kaiGetCardCount();

    DosWrite( hpipe, &rc, sizeof( rc ), &ulActual );
}

int main( int argc, char *argv[])
{
    HPIPE hpipe;
    BOOL fQuit;
    ULONG ulCmd;
    ULONG ulActual;
    ULONG rc;

    if( argc > 1 && strcmp( argv[ 1 ], "-q") == 0 )
    {
        if( pipeOpen( KAISRV_PIPE_CMD, &hpipe ))
        {
            fprintf( stderr, "kaisrv is not running!!!\n");

            return 1;
        }

        ulCmd = KAISRV_QUIT;
        DosWrite( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );

        pipeClose( hpipe );

        return 0;
    }

    // Disable server mode
    putenv("KAI_NOSERVERMODE=1");

    // Enable soft mixer mode
    putenv("KAI_NOSOFTMIXER");

    // Enable soft volume mode
    putenv("KAI_NOSOFTVOLUME");

    if( kaiInit( KAIM_AUTO ))
    {
        fprintf( stderr, "kaiInit() failed!!!\n" );

        return 1;
    }

    if( DosCreateNPipe( KAISRV_PIPE_CMD, &hpipe,
                        NP_ACCESS_DUPLEX | NP_NOINHERIT,
                        NP_WAIT | NP_TYPE_BYTE | NP_READMODE_BYTE | 1,
                        32768, 32768, 0 ))
    {
        fprintf( stderr, "kaisrv is already running!!!\n");

        kaiDone();

        return 1;
    }

    boostThread();

    for( fQuit = FALSE; !fQuit;)
    {
        REINTR( DosConnectNPipe( hpipe ), rc );

        DosRead( hpipe, &ulCmd, sizeof( ulCmd ), &ulActual );

        switch( ulCmd )
        {
            case KAISRV_CHECK:
                kaisrvCheck( hpipe );
                break;

            case KAISRV_CAPS:
                kaisrvCaps( hpipe );
                break;

            case KAISRV_OPEN:
                kaisrvOpen( hpipe );
                break;

            case KAISRV_CLOSE:
                kaisrvClose( hpipe );
                break;

            case KAISRV_PLAY:
                kaisrvPlay( hpipe );
                break;

            case KAISRV_STOP:
                kaisrvStop( hpipe );
                break;

            case KAISRV_PAUSE:
                kaisrvPause( hpipe );
                break;

            case KAISRV_RESUME:
                kaisrvResume( hpipe );
                break;

            case KAISRV_SETSOUNDSTATE:
                kaisrvSetSoundState( hpipe );
                break;

            case KAISRV_SETVOLUME:
                kaisrvSetVolume( hpipe );
                break;

            case KAISRV_GETVOLUME:
                kaisrvGetVolume( hpipe );
                break;

            case KAISRV_CLEARBUFFER:
                kaisrvClearBuffer( hpipe );
                break;

            case KAISRV_STATUS:
                kaisrvStatus( hpipe );
                break;

            case KAISRV_ENABLESOFTVOLUME:
                kaisrvEnableSoftVolume( hpipe );
                break;

            case KAISRV_MIXEROPEN:
                kaisrvMixerOpen( hpipe );
                break;

            case KAISRV_MIXERCLOSE:
                kaisrvMixerClose( hpipe );
                break;

            case KAISRV_MIXERSTREAMOPEN:
                kaisrvMixerStreamOpen( hpipe );
                break;

            case KAISRV_MIXERSTREAMCLOSE:
                kaisrvMixerStreamClose( hpipe );
                break;

            case KAISRV_ENABLESOFTMIXER:
                kaisrvEnableSoftMixer( hpipe );
                break;

            case KAISRV_GETCARDCOUNT:
                kaisrvGetCardCount( hpipe );
                break;

            case KAISRV_QUIT:
                fQuit = TRUE;
                break;
        }

        // Wait for ack before disconnecting a pipe
        DosRead( hpipe, &rc, sizeof( rc ), &ulActual );

        DosDisConnectNPipe( hpipe );
    }

    DosClose( hpipe );

    kaiDone();

    return 0;
}
