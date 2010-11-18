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

static volatile BOOL m_fQuit;
static volatile int  m_nThreads;

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
    HKAI            hkai;
    CBDATA          cd;

    /* Open the audio file.
     */
    memset( &mmioInfo, '\0', sizeof( MMIOINFO ));
    mmioInfo.fccIOProc = mmioFOURCC('W', 'A', 'V', 'E');
    hmmio = mmioOpen( name, &mmioInfo, MMIO_READ | MMIO_DENYNONE );
    if( !hmmio )
    {
        fprintf( stderr, "[%s] Failed to open a wave file!!!\n", name );

        goto exit_discount_threads;
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
    ksWanted.ulDataFormat       = mmAudioHeader.mmXWAVHeader.WAVEHeader.usFormatTag;
    ksWanted.ulChannels         = mmAudioHeader.mmXWAVHeader.WAVEHeader.usChannels;
    ksWanted.ulNumBuffers       = 2;
    ksWanted.ulBufferSize       = 0;
    ksWanted.fShareable         = TRUE;
    ksWanted.pfnCallBack        = kaiCallback;
    ksWanted.pCallBackData      = &cd;

    if( kaiOpen( &ksWanted, &ksObtained, &hkai ))
    {
        fprintf( stderr, "[%s] Failed to open audio device!!!\n", name );

        goto exit_mmio_close;
    }

    printf("[%s] hkai = %lx\n", name, hkai );

    kaiSetVolume( hkai, MCI_SET_AUDIO_ALL, 50 );

    kaiSetSoundState( hkai, MCI_SET_AUDIO_ALL, TRUE );

    printf("[%s] Trying to play...\n", name );

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );
    while( 1 )
    {
        switch( kaiPlay( hkai ))
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
    while( !m_fQuit && !( kaiStatus( hkai ) & KAIS_COMPLETED ))
        DosSleep( 1 );

    printf("[%s] Completed\n", name );

exit_kai_close :

    kaiClose( hkai );

exit_mmio_close :

    mmioClose( hmmio, 0 );

exit_discount_threads :

    m_nThreads--;
}

static int play( const char *name )
{
    m_nThreads++;

    return _beginthread( playThread, NULL, 256 * 1024, name );
}

int main( int argc, char *argv[])
{
    int             key;
    ULONG           ulMode;
    const char     *modeName[] = {"DART", "UNIAUD"};
    KAICAPS         kaic;
    TID             tid1, tid2, tid3;

    ulMode = ( argc < 2 ) ? KAIM_AUTO : atoi( argv[ 1 ]);
    if( kaiInit( ulMode ))
    {
        fprintf( stderr, "Failed to init kai\n");

        return 0;
    }

    kaiCaps( &kaic );

    printf("Mode = %s, Available channels = %ld, PDD Name = %s\n",
           modeName[ kaic.ulMode - 1 ], kaic.ulMaxChannels, kaic.szPDDName );

    printf("Press ESC to quit\n");

    m_fQuit    = FALSE;
    m_nThreads = 0;

    tid1 = play("demo1.wav");
    tid2 = play("demo2.wav");
    tid3 = play("demo3.wav");

    while( !m_fQuit && m_nThreads )
    {
        key = read_key();

        if( key == 27 )     /* ESC */
            m_fQuit = TRUE;

        DosSleep( 1 );
    }

    DosWaitThread( &tid1, DCWW_WAIT );
    DosWaitThread( &tid2, DCWW_WAIT );
    DosWaitThread( &tid3, DCWW_WAIT );

    kaiDone();

    return 0;
}
