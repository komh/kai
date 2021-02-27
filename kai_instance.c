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

#include <os2.h>

#include <stdlib.h>

#include "kai_instance.h"

static PINSTANCELIST m_pilStart = NULL;

PINSTANCELIST instanceNew( BOOL fStream, PKAISPEC pksMixer, PKAISPEC pks )
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
    // Use the soft volume mode unless KAI_NOSOFTVOLUME is specified
    pilNew->fSoftVol    = ( getenv("KAI_NOSOFTVOLUME") ? FALSE : TRUE ) ||
                          // Mixer stream always use the soft volume mode
                          fStream;

    return pilNew;
}

VOID instanceFree( PINSTANCELIST pil )
{
    if( pil )
    {
        PMIXERSTREAM pms = pil->pms;

        if( pms )
        {
            free( pms->buf.pch );
            free( pms->bufFill.pch );

            free( pms );
        }

        free( pil );
    }
}

VOID instanceAdd( ULONG id, HKAI hkai, PINSTANCELIST pil )
{
    pil->id      = id;
    pil->hkai    = hkai;
    pil->pilNext = m_pilStart;

    m_pilStart = pil;
}

VOID instanceDel( ULONG id )
{
    PINSTANCELIST pil, pilPrev = NULL;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->id == id )
            break;

        pilPrev = pil;
    }

    if( !pil )
        return;

    if( pilPrev )
        pilPrev->pilNext = pil->pilNext;
    else
        m_pilStart = pil->pilNext;

    instanceFree( pil );
}

VOID instanceDelAll( VOID )
{
    PINSTANCELIST pil, pilNext;

    for( pil = m_pilStart; pil; pil = pilNext )
    {
        pilNext = pil->pilNext;

        instanceFree( pil );
    }

    m_pilStart = NULL;
}

PINSTANCELIST instanceStart( VOID )
{
    return m_pilStart;
}

PINSTANCELIST instanceVerify( ULONG id, ULONG ivf )
{
    PINSTANCELIST pil;

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
            return NULL;
        }
    }

    return pil;
}

LONG instanceStreamCount( HKAIMIXER hkm )
{
    PINSTANCELIST pil;
    LONG count = 0;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkm && ISSTREAM( pil ))
            count++;
    }

    return count;
}

LONG instancePlayingStreamCount( HKAIMIXER hkm )
{
    PINSTANCELIST pil;
    LONG count = 0;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkm && ISSTREAM( pil ) && pil->pms->fPlaying )
            count++;
    }

    return count;
}
