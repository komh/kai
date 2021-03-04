/*
 * KAI DEMO3 for KAI Mixer
 * Copyright (C) 2010-2021 KO Myung-Hun <komh@chollian.net>
 *
 * This file is a part of K Audio Interface.
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

#define INCL_KBD
#define INCL_DOS
#include <os2.h>

#define INCL_OS2MM
#include <os2me.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __WATCOMC__
#include <process.h>
#endif

#include "kai.h"

#define BUF_SIZE    1024

static BOOL m_fQuit;

static HKAIMIXER m_hkm;


typedef struct tagCBDATA
{
    BYTE           abBuf[ BUF_SIZE ];
    int            iBufIndex;
    int            iBufLen;
    HMMIO          hmmio;
} CBDATA, *PCBDATA;

ULONG APIENTRY kaiCallback ( PVOID pCBData, PVOID Buffer, ULONG BufferSize )
{
    PCBDATA pcd = pCBData;
    PBYTE   pbBuffer = Buffer;
    LONG    lLen;

    while( BufferSize > 0 )
    {
        if( pcd->iBufIndex >= pcd->iBufLen )
        {
            pcd->iBufLen = mmioRead( pcd->hmmio, pcd->abBuf, BUF_SIZE );
            if( pcd->iBufLen == 0 )
                break;

            pcd->iBufIndex = 0;
        }

        lLen = pcd->iBufLen - pcd->iBufIndex;
        if( lLen > BufferSize )
            lLen = BufferSize;
        memcpy( pbBuffer, &pcd->abBuf[ pcd->iBufIndex ], lLen );
        pcd->iBufIndex += lLen;
        pbBuffer       += lLen;
        BufferSize     -= lLen;
    }

    return pbBuffer - ( PBYTE )Buffer;
}

int read_key( void )
{
    KBDKEYINFO Char;

    KbdCharIn(&Char, IO_NOWAIT, 0);

    if( Char.fbStatus )
        return Char.chChar;

    return 0;
}

void playThread( void *arg )
{
    char           *name = arg;
    HMMIO           hmmio;
    MMIOINFO        mmioInfo;
    MMAUDIOHEADER   mmAudioHeader;
    LONG            lBytesRead;
    KAISPEC         ksWanted, ksObtained;
    HKAIMIXERSTREAM hkms;
    CBDATA          cd;

    /* Open the audio file.
     */
    memset( &mmioInfo, '\0', sizeof( MMIOINFO ));
    mmioInfo.fccIOProc = mmioFOURCC('W', 'A', 'V', 'E');
    hmmio = mmioOpen( name, &mmioInfo, MMIO_READ | MMIO_DENYNONE );
    if( !hmmio )
    {
        fprintf( stderr, "[%s] Failed to open a wave file!!!\n", name );

        return;
    }

    /* Get the audio file header.
     */
    mmioGetHeader( hmmio,
                   &mmAudioHeader,
                   sizeof( MMAUDIOHEADER ),
                   &lBytesRead,
                   0,
                   0);

    memset( &cd, 0, sizeof( CBDATA ));
    cd.hmmio = hmmio;

    ksWanted.usDeviceIndex      = 0;
    ksWanted.ulType             = KAIT_PLAY;
    ksWanted.ulBitsPerSample    = mmAudioHeader.mmXWAVHeader.WAVEHeader.usBitsPerSample;
    ksWanted.ulSamplingRate     = mmAudioHeader.mmXWAVHeader.WAVEHeader.ulSamplesPerSec;
    ksWanted.ulDataFormat       = 0;
    ksWanted.ulChannels         = mmAudioHeader.mmXWAVHeader.WAVEHeader.usChannels;
    ksWanted.ulNumBuffers       = 0;
    ksWanted.ulBufferSize       = 0;
    ksWanted.fShareable         = TRUE;
    ksWanted.pfnCallBack        = kaiCallback;
    ksWanted.pCallBackData      = &cd;

    if( kaiMixerStreamOpen( m_hkm, &ksWanted, &ksObtained, &hkms ))
    {
        fprintf( stderr, "[%s] Failed to open a mixer stream!!!\n", name );

        goto exit_mmio_close;
    }

    printf("[%s] hkms = %lx\n", name, hkms );
    printf("[%s] Number of channels = %lu\n", name, ksObtained.ulChannels );
    printf("[%s] Number of buffers = %lu\n", name, ksObtained.ulNumBuffers );
    printf("[%s] Buffer size = %lu bytes\n", name, ksObtained.ulBufferSize );
    printf("[%s] Silence = %02x\n", name, ksObtained.bSilence );

    kaiSetVolume( hkms, MCI_SET_AUDIO_ALL, 50 );

    kaiSetSoundState( hkms, MCI_SET_AUDIO_ALL, TRUE );

    printf("[%s] Trying to play...\n", name );

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );
    while( 1 )
    {
        switch( kaiPlay( hkms ))
        {
            case KAIE_NO_ERROR :
                break;

#if 0
            // Wait for instance to be active
            case KAIE_NOT_READY :
                DosSleep( 1 );
                continue;
#endif

            default :
                fprintf( stderr, "[%s] Failed to play!!!\n", name );
                goto exit_kai_close;
        }

        break;
    }
    //DosSetPriority( PRTYS_THREAD, PRTYC_REGULAR, 0, 0 );

    printf("[%s] Playing...\n", name );
    while( !m_fQuit && !( kaiStatus( hkms ) & KAIS_COMPLETED ))
        DosSleep( 1 );

    printf("[%s] Completed\n", name );

exit_kai_close :

    if( kaiMixerStreamClose( m_hkm, hkms ))
        fprintf( stderr, "Failed to close a mixer stream!!!\n");

exit_mmio_close :

    mmioClose( hmmio, 0 );
}

static int play( const char *name )
{
    return _beginthread( playThread, NULL, 256 * 1024, ( void * )name );
}

int main( int argc, char *argv[])
{
    int             key;
    ULONG           ulMode;
    const char     *modeName[] = {"DART", "UNIAUD"};
    KAICAPS         kaic;
    KAISPEC         ksWanted, ksObtained;
    TID             tid1, tid2, tid3;

    ulMode = ( argc < 2 ) ? KAIM_AUTO : atoi( argv[ 1 ]);
    if( kaiInit( ulMode ))
    {
        fprintf( stderr, "Failed to init kai\n");

        return 1;
    }

    kaiCaps( &kaic );

    printf("Mode = %s, Available channels = %ld, PDD Name = %s\n",
           modeName[ kaic.ulMode - 1 ], kaic.ulMaxChannels, kaic.szPDDName );

    printf("Press ESC to quit\n");

    ksWanted.usDeviceIndex      = 0;
    ksWanted.ulType             = KAIT_PLAY;
    ksWanted.ulBitsPerSample    = 16;
    ksWanted.ulSamplingRate     = 44100;
    ksWanted.ulDataFormat       = 0;
    ksWanted.ulChannels         = 2;
    ksWanted.ulNumBuffers       = 2;
    ksWanted.ulBufferSize       = 512 * 2/* 16bits */ * 2/* channels */;
    ksWanted.fShareable         = TRUE;
    ksWanted.pfnCallBack        = NULL;
    ksWanted.pCallBackData      = NULL;

    if( kaiMixerOpen( &ksWanted, &ksObtained, &m_hkm ))
    {
        fprintf( stderr, "Failed to open a mixer!!!\n");

        return 1;
    }

    m_fQuit    = FALSE;

    tid1 = play("demo1.wav");
    tid2 = play("demo2.wav");
    tid3 = play("demo3.wav");

    while( !m_fQuit & !( kaiStatus( m_hkm ) & KAIS_COMPLETED ))
    {
        key = read_key();

        if( key == 27 )     /* ESC */
            m_fQuit = TRUE;

        DosSleep( 1 );
    }

    DosWaitThread( &tid1, DCWW_WAIT );
    DosWaitThread( &tid2, DCWW_WAIT );
    DosWaitThread( &tid3, DCWW_WAIT );

    if( kaiMixerClose( m_hkm ))
        fprintf( stderr, "Failed to close a mixer!!!\n");

    kaiDone();

    return 0;
}
