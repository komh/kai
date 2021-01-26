#ifndef __KAI_AUDIO_BUFFER_H_
#define __KAI_AUDIO_BUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define INITIAL_TIMEOUT ( 10 * 1000 )

typedef struct KAIAUDIOBUFFER KAIAUDIOBUFFER, *PKAIAUDIOBUFFER;

PKAIAUDIOBUFFER bufCreate( ULONG ulNum, ULONG ulSize );
VOID bufDestroy( PKAIAUDIOBUFFER pbuf );
LONG bufReadLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulLength );
LONG bufReadUnlock( PKAIAUDIOBUFFER pbuf );
VOID bufReadWaitDone( PKAIAUDIOBUFFER pbuf, ULONG ulTimeout );
LONG bufWriteLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulSize );
LONG bufWriteUnlock( PKAIAUDIOBUFFER pbuf, ULONG ulLength );
VOID bufWritePostFill( PKAIAUDIOBUFFER pbuf );
VOID bufClear( PKAIAUDIOBUFFER pbuf, UCHAR uch );

#ifdef __cplusplus
}
#endif

#endif
