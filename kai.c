/*
    K Audio Interface library for OS/2
    Copyright (C) 2010 by KO Myung-Hun <komh@chollian.net>

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

#include <string.h>
#include <stdlib.h>

#include "kai.h"
#include "kai_internal.h"
#include "kai_dart.h"
#include "kai_uniaud.h"

static BOOL     m_fInited = FALSE;
static KAIAPIS  m_kai = { NULL, };
static KAICAPS  m_kaic = { 0, };

typedef struct tagINSTANCELIST INSTANCELIST, *PINSTANCELIST;
struct tagINSTANCELIST
{
    HKAI          hkai;
    PINSTANCELIST pilNext;
};

PINSTANCELIST   m_pilStart = NULL;

static VOID instanceAdd( HKAI hkai )
{
    PINSTANCELIST pilNew;

    pilNew          = malloc( sizeof( INSTANCELIST ));
    pilNew->hkai    = hkai;
    pilNew->pilNext = m_pilStart;
    m_pilStart      = pilNew;
}

static VOID instanceDel( HKAI hkai )
{
    PINSTANCELIST pil, pilPrev = NULL;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkai )
            break;

        pilPrev = pil;
    }

    if( !pil )
        return;

    if( pilPrev )
        pilPrev->pilNext = pil->pilNext;
    else
        m_pilStart = pil->pilNext;

    free( pil );
}

static VOID instanceDelAll( VOID )
{
    PINSTANCELIST pil, pilNext;

    for( pil = m_pilStart; pil; pil = pilNext )
    {
        pilNext = pil->pilNext;
        free( pil );
    }

    m_pilStart = NULL;
}

static PINSTANCELIST instanceVerify( HKAI hkai )
{
    PINSTANCELIST pil;

    for( pil = m_pilStart; pil; pil = pil->pilNext )
    {
        if( pil->hkai == hkai )
            break;
    }

    return pil;
}

APIRET APIENTRY kaiInit( ULONG ulMode )
{
    APIRET rc = KAIE_INVALID_PARAMETER;

    if( m_fInited )
        return KAIE_ALREADY_INITIALIZED;

    if( ulMode == KAIM_UNIAUD || ulMode == KAIM_AUTO )
    {
        rc = kaiUniaudInit( &m_kai, &m_kaic.ulMaxChannels );
        if( rc )
        {
            if( ulMode != KAIM_AUTO )
                return rc;
        }
        else
            ulMode = KAIM_UNIAUD;
    }

    if( ulMode == KAIM_DART || ulMode == KAIM_AUTO )
    {
        rc = kaiDartInit( &m_kai, &m_kaic.ulMaxChannels );
        if( rc )
        {
            if( ulMode != KAIM_AUTO )
                return rc;
        }
        else
            ulMode = KAIM_DART;
    }

    if( !rc )
    {
        m_kaic.ulMode = ulMode;
        kaiOSLibGetAudioPDDName( m_kaic.szPDDName );

        m_fInited = TRUE;
    }

    return rc;
}

APIRET APIENTRY kaiDone( VOID )
{
    APIRET rc;

    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    rc = m_kai.pfnDone();
    if( rc )
        return rc;

    instanceDelAll();

    m_fInited = FALSE;

    return KAIE_NO_ERROR;
}

APIRET APIENTRY kaiCaps( PKAICAPS pkc )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !pkc )
        return KAIE_INVALID_PARAMETER;

    memcpy( pkc, &m_kaic, sizeof( KAICAPS ));

    return KAIE_NO_ERROR;
}

APIRET APIENTRY kaiOpen( const PKAISPEC pksWanted, PKAISPEC pksObtained, PHKAI phkai )
{
    APIRET rc;

    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !pksWanted || !pksObtained || !phkai )
        return KAIE_INVALID_PARAMETER;

    if( !pksWanted->pfnCallBack )
        return KAIE_INVALID_PARAMETER;

    memcpy( pksObtained, pksWanted, sizeof( KAISPEC ));

    rc = m_kai.pfnOpen( pksObtained, phkai );
    if( rc )
        return rc;

    instanceAdd( *phkai );

    return KAIE_NO_ERROR;
}

APIRET APIENTRY kaiClose( HKAI hkai )
{
    APIRET rc;

    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    rc = m_kai.pfnClose( hkai );
    if( rc )
        return rc;

    instanceDel( hkai );

    return KAIE_NO_ERROR;
}

APIRET APIENTRY kaiPlay( HKAI hkai )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnPlay( hkai );
}

APIRET APIENTRY kaiStop( HKAI hkai )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnStop( hkai );
}

APIRET APIENTRY kaiPause( HKAI hkai )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnPause( hkai );
}

APIRET APIENTRY kaiResume( HKAI hkai )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnResume( hkai );
}

APIRET APIENTRY kaiSetSoundState( HKAI hkai, ULONG ulCh, BOOL fState )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnSetSoundState( hkai, ulCh, fState );
}

APIRET APIENTRY kaiSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnSetVolume( hkai, ulCh, usVol );
}

APIRET APIENTRY kaiGetVolume( HKAI hkai, ULONG ulCh )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnGetVolume( hkai, ulCh );
}

APIRET APIENTRY kaiClearBuffer( HKAI hkai )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnClearBuffer( hkai );
}

APIRET APIENTRY kaiStatus( HKAI hkai )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !instanceVerify( hkai ))
        return KAIE_INVALID_HANDLE;

    return m_kai.pfnStatus( hkai );
}
