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

#include "kai.h"
#include "kai_internal.h"
#include "kai_dart.h"
#include "kai_uniaud.h"

static BOOL     m_fInited = FALSE;
static KAIAPIS  m_kai = { NULL, };
static BOOL     m_fOpened = FALSE;

static ULONG m_ulMode;
static ULONG m_ulMaxChannels;
static CHAR  m_szPDDName[ 256 ];

APIRET APIENTRY kaiInit( ULONG ulMode )
{
    APIRET rc = KAIE_INVALID_PARAMETER;

    if( m_fInited )
        return KAIE_ALREADY_INITIALIZED;

    if( ulMode == KAIM_UNIAUD || ulMode == KAIM_AUTO )
    {
        rc = kaiUniaudInit( &m_kai, &m_ulMaxChannels );
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
        rc = kaiDartInit( &m_kai, &m_ulMaxChannels );
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
        m_ulMode = ulMode;
        kaiOSLibGetAudioPDDName( m_szPDDName );

        m_fOpened = FALSE;

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

    m_fInited = FALSE;

    return KAIE_NO_ERROR;
}

APIRET APIENTRY kaiCaps( PKAICAPS pkc )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !pkc )
        return KAIE_INVALID_PARAMETER;

    pkc->ulMode        = m_ulMode;
    pkc->ulMaxChannels = m_ulMaxChannels;
    strcpy( pkc->szPDDName, m_szPDDName );

    return KAIE_NO_ERROR;
}

APIRET APIENTRY kaiOpen( const PKAISPEC pksWanted, PKAISPEC pksObtained )
{
    APIRET rc;

    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( m_fOpened )
        return KAIE_ALREADY_OPENED;

    if( !pksWanted || !pksObtained )
        return KAIE_INVALID_PARAMETER;

    if( !pksWanted->pfnCallBack )
        return KAIE_INVALID_PARAMETER;

    memcpy( pksObtained, pksWanted, sizeof( KAISPEC ));

    rc = m_kai.pfnOpen( pksObtained );
    if( rc )
        return rc;

    m_fOpened = TRUE;

    return KAIE_NO_ERROR;
}

APIRET APIENTRY kaiClose( VOID )
{
    APIRET rc;

    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    rc = m_kai.pfnClose();
    if( rc )
        return rc;

    m_fOpened = FALSE;

    return KAIE_NO_ERROR;
}

APIRET APIENTRY kaiPlay( VOID )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnPlay();
}

APIRET APIENTRY kaiStop( VOID )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnStop();
}

APIRET APIENTRY kaiPause( VOID )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnPause();
}

APIRET APIENTRY kaiResume( VOID )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnResume();
}

APIRET APIENTRY kaiSetSoundState( ULONG ulCh, BOOL fState )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnSetSoundState( ulCh, fState );
}

APIRET APIENTRY kaiSetVolume( ULONG ulCh, USHORT usVol )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnSetVolume( ulCh, usVol );
}

APIRET APIENTRY kaiGetVolume( ULONG ulCh )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnGetVolume( ulCh );
}

APIRET APIENTRY kaiClearBuffer( VOID )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnClearBuffer();
}

APIRET APIENTRY kaiStatus( VOID )
{
    if( !m_fInited )
        return KAIE_NOT_INITIALIZED;

    if( !m_fOpened )
        return KAIE_NOT_OPENED;

    return m_kai.pfnStatus();
}
