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

BYTE abBuf[ BUF_SIZE ];
int  iBufIndex = 0;
int  iBufLen = 0;

HMMIO hmmio;
int   switch_sign = FALSE;

volatile ULONG ulStatus;

ULONG APIENTRY kaiCallback ( PVOID pCBData, PVOID Buffer, ULONG BufferSize )
{
  PBYTE pbBuffer = Buffer;
  LONG  len;

  if( ulStatus & KAIS_COMPLETED )
    mmioSeek( hmmio, 0, SEEK_SET );

  while( BufferSize > 0 )
  {
    if( iBufIndex >= iBufLen )
    {
        iBufLen = mmioRead( hmmio, abBuf, BUF_SIZE );
        if( iBufLen == 0 )
            break;
        iBufIndex = 0;
    }

    len = iBufLen - iBufIndex;
    if( len > BufferSize )
        len = BufferSize;
    memcpy( pbBuffer, &abBuf[ iBufIndex ], len );
    iBufIndex += len;
    pbBuffer += len;
    BufferSize -= len;
  }

  if (switch_sign)
  {
    char *sample = (char *)Buffer;
    char *lastsample = pbBuffer;
    while (sample < lastsample)
    {
      sample++;
      *sample ^= 0x80;
      sample++;
    } /* endwhile */
  } /* endif */

  return pbBuffer - ( PBYTE )Buffer;
}

int read_key(void)
{
    KBDKEYINFO Char;

    KbdCharIn(&Char, IO_NOWAIT, 0);

    if( Char.fbStatus )
        return Char.chChar;

    return 0;
}

const char *getStatusName( ULONG ulStatus )
{
    if( ulStatus & KAIS_COMPLETED )
        return "COMPLETED";

    if( ulStatus & KAIS_PAUSED )
        return "PAUSED";

    if( ulStatus & KAIS_PLAYING )
        return "PLAYING";

    return "STOPPED";
}

int main( int argc, char **argv )
{
    MMIOINFO        mmioInfo;
    MMAUDIOHEADER   mmAudioHeader;
    LONG            lBytesRead;
    HKAI            hkai;
    int             key;
    APIRET          rc;
    const char     *modeName[] = {"DART", "UNIAUD"};
    KAICAPS         kaic;
    KAISPEC         ksWanted, ksObtained;

    if( argc < 2 )
    {
        fprintf( stderr, "Usage : kaidemo WAVE-file [audio-mode=0:auto, 1:dart, 2:uniaud]\r\n" );

        return 0;
    }

   /* Open the audio file.
    */
   memset( &mmioInfo, '\0', sizeof( MMIOINFO ));
   mmioInfo.fccIOProc = mmioFOURCC( 'W', 'A', 'V', 'E' );
   hmmio = mmioOpen( argv[ 1 ], &mmioInfo, MMIO_READ | MMIO_DENYNONE );

   if( !hmmio )
   {
      fprintf( stderr, "Unable to open wave file\r\n" );

      return 0;
   }

   /* Get the audio file header.
    */
   mmioGetHeader( hmmio,
                  &mmAudioHeader,
                  sizeof( MMAUDIOHEADER ),
                  &lBytesRead,
                  0,
                  0);

    if( kaiInit( argc > 2 ? atoi( argv[ 2 ]) : KAIM_AUTO ))
    {
        fprintf( stderr, "Unable to init kai\n");

        return 0;
    }

    kaiCaps( &kaic );

    printf("Mode = %s, Available channels = %ld, PDD Name = %s\n",
           modeName[ kaic.ulMode - 1 ], kaic.ulMaxChannels, kaic.szPDDName );

    ksWanted.usDeviceIndex      = 0;
    ksWanted.ulType             = KAIT_PLAY;
    ksWanted.ulBitsPerSample    = mmAudioHeader.mmXWAVHeader.WAVEHeader.usBitsPerSample;
    ksWanted.ulSamplingRate     = mmAudioHeader.mmXWAVHeader.WAVEHeader.ulSamplesPerSec;
    ksWanted.ulDataFormat       = mmAudioHeader.mmXWAVHeader.WAVEHeader.usFormatTag;
    ksWanted.ulChannels         = mmAudioHeader.mmXWAVHeader.WAVEHeader.usChannels;
    ksWanted.ulNumBuffers       = 2;
    ksWanted.ulBufferSize       = 0;
    ksWanted.fShareable         = FALSE;
    ksWanted.pfnCallBack        = kaiCallback;
    ksWanted.pCallBackData      = NULL;

    rc = kaiOpen( &ksWanted, &ksObtained, &hkai );

    printf("Number of buffers = %lu\n", ksObtained.ulNumBuffers );
    printf("Buffer size = %lu\n", ksObtained.ulBufferSize );
    printf("Silence = %02x\n", ksObtained.bSilence );

    printf("ESC = quit, q = stop, w = play, e = pause, r = resume\n");

    kaiSetVolume( hkai, MCI_SET_AUDIO_ALL, 50 );
    kaiSetSoundState( hkai, MCI_SET_AUDIO_ALL, TRUE );

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );
    kaiPlay( hkai );
    //DosSetPriority( PRTYS_THREAD, PRTYC_REGULAR, 0, 0 );

    while( 1 )
    {
        ulStatus = kaiStatus( hkai );
        printf("Status = %04lx [%9s]\r", ulStatus, getStatusName( ulStatus ));
        fflush( stdout );

        key = read_key();

        if (key == 27)    /* ESC */
            break;

        if (key == 113)   /* q */
            kaiStop( hkai );

        if (key == 119)   /* w */
            kaiPlay( hkai );

        if (key == 101)   /* e */
            kaiPause( hkai );

        if (key == 114)   /* r */
            kaiResume( hkai );

        DosSleep( 1 );
    }

    printf("\n");

    kaiClose( hkai );

    kaiDone();

    mmioClose( hmmio, 0 );

    return 0;
}
