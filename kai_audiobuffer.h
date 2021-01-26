#ifndef __KAI_AUDIO_BUFFER_H_
#define __KAI_AUDIO_BUFFER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KAIAUDIOBUFFER KAIAUDIOBUFFER, *PKAIAUDIOBUFFER;

PKAIAUDIOBUFFER bufCreate( ULONG ulNum, ULONG ulSize );
VOID bufDestroy( PKAIAUDIOBUFFER pbuf );
LONG bufReadLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulLength );
LONG bufReadUnlock( PKAIAUDIOBUFFER pbuf );
LONG bufWriteLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulSize );
LONG bufWriteUnlock( PKAIAUDIOBUFFER pbuf, ULONG ulLength );
VOID bufWritePostFill( PKAIAUDIOBUFFER pbuf );
VOID bufClear( PKAIAUDIOBUFFER pbuf, UCHAR uch );

#ifdef __cplusplus
}
#endif

#endif
