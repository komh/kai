/*
    Audio Buffer Interface for K Audio Interface
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
