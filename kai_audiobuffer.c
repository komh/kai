#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include <stdlib.h>
#include <string.h>

#include "kai_audiobuffer.h"

typedef struct KAIAUDIOBUFFERELEMENT {
    PVOID pBuffer;
    ULONG ulSize;
    ULONG ulLength;

    HEV hevFill;
    HEV hevDone;
} KAIAUDIOBUFFERELEMENT, *PKAIAUDIOBUFFERELEMENT;

struct KAIAUDIOBUFFER {
    ULONG ulNumBuffers;
    ULONG volatile ulReadPos;
    ULONG volatile ulWritePos;

    KAIAUDIOBUFFERELEMENT abufelm[ 1 ];
};

PKAIAUDIOBUFFER bufCreate( ULONG ulNum, ULONG ulSize )
{
    PKAIAUDIOBUFFER pbuf;
    int i;

    pbuf = calloc( 1, sizeof( *pbuf ) +
                      ( ulSize - 1 ) * sizeof( KAIAUDIOBUFFERELEMENT ));
    if( !pbuf )
        return NULL;

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

VOID bufDestroy( PKAIAUDIOBUFFER pbuf )
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

    free( pbuf );
}

LONG bufReadLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulLength )
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

LONG bufReadUnlock( PKAIAUDIOBUFFER pbuf )
{
    PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ pbuf->ulReadPos ];

    pbuf->ulReadPos = ( pbuf->ulReadPos + 1 ) % pbuf->ulNumBuffers;

    DosPostEventSem( pbufelm->hevFill );

    return 0;
}

VOID bufReadWaitDone( PKAIAUDIOBUFFER pbuf, ULONG ulTimeout )
{
    PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ pbuf->ulReadPos ];

    DosWaitEventSem( pbufelm->hevDone, ulTimeout );
}

LONG bufWriteLock( PKAIAUDIOBUFFER pbuf, PPVOID ppBuffer, PULONG pulSize )
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

LONG bufWriteUnlock( PKAIAUDIOBUFFER pbuf, ULONG ulLength )
{
    PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ pbuf->ulWritePos ];

    pbufelm->ulLength = ulLength;

    pbuf->ulWritePos = ( pbuf->ulWritePos + 1 ) % pbuf->ulNumBuffers;

    DosPostEventSem( pbufelm->hevDone );

    return 0;
}

VOID bufWritePostFill( PKAIAUDIOBUFFER pbuf )
{
    DosPostEventSem( pbuf->abufelm[ pbuf->ulWritePos ].hevFill );
}

VOID bufClear( PKAIAUDIOBUFFER pbuf, UCHAR uch )
{
    int i;

    for( i = pbuf->ulNumBuffers - 1; i >= 0; i-- )
    {
        PKAIAUDIOBUFFERELEMENT pbufelm = &pbuf->abufelm[ i ];

        memset( pbufelm->pBuffer, uch, pbufelm->ulSize );
    }
}
