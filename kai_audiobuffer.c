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

#include "kai_internal.h"

#include "kai_audiobuffer.h"

#include <string.h>

typedef struct KAIAUDIOBUFFERELEMENT {
    PVOID pBuffer;
    ULONG ulSize;
    ULONG ulLength;

    HEV hevFill;
    HEV hevDone;
} KAIAUDIOBUFFERELEMENT, *PKAIAUDIOBUFFERELEMENT;

struct KAIAUDIOBUFFER {
    ULONG ulNumBuffers;
    ULONG ulReadPos;
    ULONG ulWritePos;
    HEV   hevFull;

    KAIAUDIOBUFFERELEMENT abufelm[ 1 ];
};

PKAIAUDIOBUFFER _kaiBufCreate( ULONG ulNum, ULONG ulSize )
{
    PKAIAUDIOBUFFER pbuf;
    int i;

    pbuf = calloc( 1, sizeof( *pbuf ) +
                      ( ulNum - 1 ) * sizeof( KAIAUDIOBUFFERELEMENT ));
    if( !pbuf )
        return NULL;

    if( DosCreateEventSem( NULL, &pbuf->hevFull, 0, FALSE ))
    {
        free( pbuf);

        return NULL;
    }

    for( i = 0; i < ulNum; i++ )
    {
        PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ i ];

        if( !( pbufelm->pBuffer = malloc( ulSize ))
            || DosCreateEventSem( NULL, &pbufelm->hevFill, 0, TRUE )
            || DosCreateEventSem( NULL, &pbufelm->hevDone, 0, FALSE ))
            goto exit_error;

        pbufelm->ulSize = ulSize;
    }

    pbuf->ulNumBuffers = ulNum;

    return pbuf;

exit_error:
    bufDestroy( pbuf );

    return NULL;
}

VOID _kaiBufDestroy( PKAIAUDIOBUFFER pbuf )
{
    int i;

    if( !pbuf )
        return;

    for( i = pbuf->ulNumBuffers - 1; i >= 0; i-- )
    {
        PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ i ];

        free( pbufelm->pBuffer );

        DosCloseEventSem( pbufelm->hevFill );
        DosCloseEventSem( pbufelm->hevDone );
    }

    DosCloseEventSem( pbuf->hevFull );
    free( pbuf );
}

LONG _kaiBufReadLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulLength )
{
    PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ pbuf->ulReadPos ];

    if( DosWaitEventSem( pbufelm->hevDone, SEM_IMMEDIATE_RETURN ) == NO_ERROR )
    {
        ULONG ulCount;

        DosResetEventSem( pbufelm->hevDone, &ulCount );

        *ppBuffer = pbufelm->pBuffer;
        *pulLength = pbufelm->ulLength;

        return 0;
    }

    return -1;
}

LONG _kaiBufReadUnlock( PKAIAUDIOBUFFER pbuf )
{
    PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ pbuf->ulReadPos ];

    pbuf->ulReadPos = ( pbuf->ulReadPos + 1 ) % pbuf->ulNumBuffers;

    DosPostEventSem( pbufelm->hevFill );

    return 0;
}

VOID _kaiBufReadWaitFull( PKAIAUDIOBUFFER pbuf )
{
    while( DosWaitEventSem( pbuf->hevFull, SEM_INDEFINITE_WAIT )
           == ERROR_INTERRUPT )
        /* nothing */;
}

LONG _kaiBufWriteLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulSize )
{
    PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ pbuf->ulWritePos ];
    ULONG ulPost;

    while( DosWaitEventSem( pbufelm->hevFill, SEM_INDEFINITE_WAIT )
           == ERROR_INTERRUPT )
        /* nothing */;

    DosResetEventSem( pbufelm->hevFill, &ulPost );

    *ppBuffer = pbufelm->pBuffer;
    *pulSize = pbufelm->ulSize;

    return 0;
}

LONG _kaiBufWriteUnlock( PKAIAUDIOBUFFER pbuf, ULONG ulLength )
{
    PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ pbuf->ulWritePos ];
    PKAIAUDIOBUFFERELEMENT pbufelmNext;

    pbufelm->ulLength = ulLength;

    pbuf->ulWritePos = ( pbuf->ulWritePos + 1 ) % pbuf->ulNumBuffers;

    pbufelmNext = &pbuf->abufelm[ pbuf->ulWritePos ];
    if( ulLength < pbufelm->ulSize  /* Last buffer */
        || DosWaitEventSem( pbufelmNext->hevFill, SEM_IMMEDIATE_RETURN )
           != NO_ERROR /* Buffer full */)
        DosPostEventSem( pbuf->hevFull );

    DosPostEventSem( pbufelm->hevDone );

    return 0;
}

VOID _kaiBufWriteCancel( PKAIAUDIOBUFFER pbuf )
{
    DosPostEventSem( pbuf->abufelm[ pbuf->ulWritePos ].hevFill );
}

VOID _kaiBufClear( PKAIAUDIOBUFFER pbuf, UCHAR uch )
{
    int i;

    for( i = pbuf->ulNumBuffers - 1; i >= 0; i-- )
    {
        PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ i ];

        memset( pbufelm->pBuffer, uch, pbufelm->ulSize );
    }
}
