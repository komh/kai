/*
 * KAI DEMO
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

#include "kai.h"

#define BUF_SIZE    1024

static BYTE m_abBuf[ BUF_SIZE ];
static int  m_iBufIndex = 0;
static int  m_iBufLen = 0;

static HMMIO m_hmmio;

static ULONG m_ulStatus = 0;

ULONG APIENTRY kaiCallback ( PVOID pCBData, PVOID Buffer, ULONG BufferSize )
{
    PBYTE   pbBuffer = Buffer;
    LONG    lLen;

    while( BufferSize > 0 )
    {
        if( m_iBufIndex >= m_iBufLen )
        {
            m_iBufLen = mmioRead( m_hmmio, m_abBuf, BUF_SIZE );
            if( m_iBufLen == 0 )
                break;

            m_iBufIndex = 0;
        }

        lLen = m_iBufLen - m_iBufIndex;
        if( lLen > BufferSize )
            lLen = BufferSize;
        memcpy( pbBuffer, &m_abBuf[ m_iBufIndex ], lLen );
        m_iBufIndex  += lLen;
        pbBuffer     += lLen;
        BufferSize   -= lLen;
    }

    if( BufferSize > 0 )
        mmioSeek( m_hmmio, 0, SEEK_SET );

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

const char *getStatusName( ULONG m_ulStatus )
{
    if( m_ulStatus & KAIS_COMPLETED )
        return "COMPLETED";

    if( m_ulStatus & KAIS_PAUSED )
        return "PAUSED";

    if( m_ulStatus & KAIS_PLAYING )
        return "PLAYING";

    return "STOPPED";
}

int main( int argc, char *argv[])
{
    MMIOINFO        mmioInfo;
    MMAUDIOHEADER   mmAudioHeader;
    LONG            lBytesRead;
    KAICAPS         kaic;
    KAISPEC         ksWanted, ksObtained;
    HKAI            hkai;
    int             key;
    ULONG           ulMode;
    USHORT          usVol = 50;
    const char     *modeName[] = {"DART", "UNIAUD"};

    if( argc < 2 )
    {
        fprintf( stderr, "Usage : kaidemo WAVE-file [mode] (mode : 0 = auto, 1 = dart, 2 = uniaud )\n");

        return 1;
    }

    ulMode = argc > 2 ? atoi( argv[ 2 ]) : KAIM_AUTO;
    if( kaiInit( ulMode ))
    {
        fprintf( stderr, "Failed to init kai\n");

        return 1;
    }

    kaiCaps( &kaic );

    printf("Mode = %s, Available channels = %ld, PDD Name = %s\n",
           modeName[ kaic.ulMode - 1 ], kaic.ulMaxChannels, kaic.szPDDName );

    /* Open the audio file.
     */
    memset( &mmioInfo, '\0', sizeof( MMIOINFO ));
    mmioInfo.fccIOProc = mmioFOURCC( 'W', 'A', 'V', 'E' );
    m_hmmio = mmioOpen( argv[ 1 ], &mmioInfo, MMIO_READ | MMIO_DENYNONE );
    if( !m_hmmio )
    {
        fprintf( stderr, "Failed to open wave file\n");

        return 1;
    }

    /* Get the audio file header.
     */
    mmioGetHeader( m_hmmio,
                   &mmAudioHeader,
                   sizeof( MMAUDIOHEADER ),
                   &lBytesRead,
                   0,
                   0);

    ksWanted.usDeviceIndex      = 0;
    ksWanted.ulType             = KAIT_PLAY;
    ksWanted.ulBitsPerSample    = mmAudioHeader.mmXWAVHeader.WAVEHeader.usBitsPerSample;
    ksWanted.ulSamplingRate     = mmAudioHeader.mmXWAVHeader.WAVEHeader.ulSamplesPerSec;
    ksWanted.ulDataFormat       = 0;
    ksWanted.ulChannels         = mmAudioHeader.mmXWAVHeader.WAVEHeader.usChannels;
    ksWanted.ulNumBuffers       = 2;
    ksWanted.ulBufferSize       = 0;
    ksWanted.fShareable         = FALSE;
    ksWanted.pfnCallBack        = kaiCallback;
    ksWanted.pCallBackData      = NULL;

    if( kaiOpen( &ksWanted, &ksObtained, &hkai ))
    {
        printf("Failed to open audio device!!!\n");
        goto exit_mmio_close;
    }

    printf("Number of channels = %lu\n", ksObtained.ulChannels );
    printf("Number of buffers = %lu\n", ksObtained.ulNumBuffers );
    printf("Buffer size = %lu\n", ksObtained.ulBufferSize );
    printf("Silence = %02x\n", ksObtained.bSilence );

    printf("ESC = quit, q = stop, w = play, e = pause, r = resume, +/- = volume up/down\n");

    kaiSetVolume( hkai, MCI_SET_AUDIO_ALL, usVol );
    kaiSetSoundState( hkai, MCI_SET_AUDIO_ALL, TRUE );

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );
    kaiPlay( hkai );
    //DosSetPriority( PRTYS_THREAD, PRTYC_REGULAR, 0, 0 );

    while( 1 )
    {
        m_ulStatus = kaiStatus( hkai );
        printf("Status : [%9s], vol = [%3ld%%]\r",
               getStatusName( m_ulStatus ),
               kaiGetVolume( hkai, MCI_STATUS_AUDIO_ALL ));
        fflush( stdout );

        key = read_key();

        if( key == 27 )    /* ESC */
            break;

        if( key == 'q')
            kaiStop( hkai );

        if( key == 'w')
            kaiPlay( hkai );

        if( key == 'e')
            kaiPause( hkai );

        if( key == 'r')
            kaiResume( hkai );

        if( key == '+')
        {
            usVol += 10;
            if( usVol > 100 )
                usVol = 100;

            kaiSetVolume( hkai, MCI_SET_AUDIO_ALL, usVol );
        }

        if( key == '-')
        {
            if( usVol >= 10 )
                usVol -= 10;
            else
                usVol = 0;

            kaiSetVolume( hkai, MCI_SET_AUDIO_ALL, usVol );
        }

        DosSleep( 1 );
    }

    printf("\n");

    kaiClose( hkai );

exit_mmio_close :

    mmioClose( m_hmmio, 0 );

    kaiDone();

    return 0;
}
