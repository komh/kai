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

typedef struct KAIAUDIOBUFFER KAIAUDIOBUFFER, *PKAIAUDIOBUFFER;

PKAIAUDIOBUFFER _kaiBufCreate( ULONG ulNum, ULONG ulSize );
VOID _kaiBufDestroy( PKAIAUDIOBUFFER pbuf );
LONG _kaiBufReadLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulLength );
LONG _kaiBufReadUnlock( PKAIAUDIOBUFFER pbuf );
VOID _kaiBufReadWaitDone( PKAIAUDIOBUFFER pbuf, ULONG ulTimeout );
LONG _kaiBufWriteLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulSize );
LONG _kaiBufWriteUnlock( PKAIAUDIOBUFFER pbuf, ULONG ulLength );
VOID _kaiBufWritePostFill( PKAIAUDIOBUFFER pbuf );
VOID _kaiBufClear( PKAIAUDIOBUFFER pbuf, UCHAR uch );

#define bufCreate           _kaiBufCreate
#define bufDestroy          _kaiBufDestroy
#define bufReadLock         _kaiBufReadLock
#define bufReadUnlock       _kaiBufReadUnlock
#define bufReadWaitDone     _kaiBufReadWaitDone
#define bufWriteLock        _kaiBufWriteLock
#define bufWriteUnlock      _kaiBufWriteUnlock
#define bufWritePostFill    _kaiBufWritePostFill
#define bufClear            _kaiBufClear

#ifdef __cplusplus
}
#endif

#endif
