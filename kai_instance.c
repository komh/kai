/*
    Instances for K Audio Interface
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

#include <stdlib.h>

#include "kai_internal.h"
#include "kai_mixer.h"
#include "kai_spinlock.h"
#include "kai_instance.h"

static PINSTANCELIST m_pilStart = NULL;
static SPINLOCK m_lock = SPINLOCK_INIT;

PINSTANCELIST _kaiInstanceNew( BOOL fStream, PKAISPEC pksMixer, PKAISPEC pks )
{
    PINSTANCELIST pilNew;

    pilNew = calloc( 1, sizeof( INSTANCELIST ));
    if( !pilNew )
        return NULL;

    if( fStream )
    {
        ULONG ulBufSize = pksMixer->ulBufferSize * pksMixer->ulSamplingRate /
                          pks->ulSamplingRate;
        PMIXERSTREAM pms;

        if( pksMixer->ulSamplingRate % pks->ulSamplingRate )
            ulBufSize += pksMixer->ulBufferSize;

        pms = pilNew->pms = calloc( 1, sizeof( MIXERSTREAM ));
        if( pms )
        {
            pms->buf.pch = malloc( ulBufSize );
            pms->bufFill.pch = malloc( ulBufSize );
        }

        if( !pms || !pms->buf.pch || !pms->bufFill.pch )
        {
            instanceFree( pilNew );

            return NULL;
        }

        pms->pksMixer = pksMixer;
        pms->buf.ulSize = ulBufSize;
        pms->bufFill.ulSize = ulBufSize;
    }

    pilNew->lLeftVol    = 100;
    pilNew->lRightVol   = 100;
    pilNew->fLeftState  = TRUE;
    pilNew->fRightState = TRUE;
    pilNew->fSoftVol    = _kaiIsSoftVolume() ||
                          // Mixer stream always use the soft volume mode
                          fStream;
    spinLockInit( &pilNew->lock );

    return pilNew;
}

VOID _kaiInstanceFree( PINSTANCELIST pil )
{
    if( pil )
    {
        PMIXERSTREAM pms = pil->pms;

        if( pms )
        {
            DosCloseEventSem( pms->hevFill );
            DosCloseEventSem( pms->hevFillDone );

            if( pms->srs )
                speex_resampler_destroy( pms->srs );

            free( pms->buf.pch );
            free( pms->bufFill.pch );

            free( pms );
        }

        free( pil );
    }
}

VOID _kaiInstanceAdd( ULONG id, HKAI hkai, PINSTANCELIST pil )
{
    spinLock( &m_lock );

    pil->id      = id;
    pil->hkai    = hkai;
    pil->pilNext = m_pilStart;

    m_pilStart = pil;

    spinUnlock( &m_lock );
}

VOID _kaiInstanceDel( ULONG id )
{
    PINSTANCELIST pil, pilPrev = NULL;

    spinLock( &m_lock );

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->id == id )
            break;

        pilPrev = pil;
    }

    if( pil )
    {
        if( pilPrev )
            pilPrev->pilNext = pil->pilNext;
        else
            m_pilStart = pil->pilNext;

        instanceFree( pil );
    }

    spinUnlock( &m_lock );
}

VOID _kaiInstanceDelAll( VOID )
{
    PINSTANCELIST pil, pilNext;

    spinLock( &m_lock );

    for( pil = m_pilStart; pil; pil = pilNext )
    {
        pilNext = pil->pilNext;

        instanceFree( pil );
    }

    m_pilStart = NULL;

    spinUnlock( &m_lock );
}

VOID _kaiInstanceLoop( void ( *callback )( PINSTANCELIST, void * ),
                       void *arg )
{
    PINSTANCELIST pil;

    spinLock( &m_lock );

    for( pil = m_pilStart; pil; pil = pil->pilNext )
        callback( pil, arg );

    spinUnlock( &m_lock );
}

PINSTANCELIST _kaiInstanceVerify( ULONG id, ULONG ivf )
{
    PINSTANCELIST pil;

    spinLock( &m_lock );

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->id == id)
        {
            if(( ivf & IVF_NORMAL ) && ISNORMAL( pil ))
                break;

            if(( ivf & IVF_MIXER ) && ISMIXER( pil ))
                break;

            if(( ivf & IVF_STREAM ) && ISSTREAM( pil ))
                break;

            /* Oooops... not matched! */
            pil = NULL;
            break;
        }
    }

    spinUnlock( &m_lock );

    return pil;
}

LONG _kaiInstanceStreamCount( HKAIMIXER hkm )
{
    PINSTANCELIST pil;
    LONG count = 0;

    spinLock( &m_lock );

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkm && ISSTREAM( pil ))
            count++;
    }

    spinUnlock( &m_lock );

    return count;
}

LONG _kaiInstancePlayingStreamCount( HKAIMIXER hkm )
{
    PINSTANCELIST pil;
    LONG count = 0;

    spinLock( &m_lock );

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkm && ISSTREAM( pil ) && pil->pms->fPlaying )
            count++;
    }

    spinUnlock( &m_lock );

    return count;
}
