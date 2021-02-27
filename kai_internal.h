/*
    Internal interface for K Audio Interface
    Copyright (C) 2010-2015 by KO Myung-Hun <komh@chollian.net>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __KAI_INTERNAL_H__
#define __KAI_INTERNAL_H__

#include <float.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __IBMC__
#define DECLARE_PFN( ret, callconv, name, arg ) ret ( * callconv name )arg
#define DLLEXPORT _Export
#else
#define DECLARE_PFN( ret, callconv, name, arg ) ret ( callconv * name )arg
#define DLLEXPORT __declspec(dllexport)
#endif

#define THREAD_STACK_SIZE   ( 1024 * 1024 )

#define INITIAL_TIMEOUT ( 10 * 1000 )

typedef struct tagKAIAPIS
{
    DECLARE_PFN( APIRET, APIENTRY, pfnDone, ( VOID ));
    DECLARE_PFN( APIRET, APIENTRY, pfnOpen, ( PKAISPEC, PHKAI ));
    DECLARE_PFN( APIRET, APIENTRY, pfnClose, ( HKAI ));
    DECLARE_PFN( APIRET, APIENTRY, pfnPlay, ( HKAI ));
    DECLARE_PFN( APIRET, APIENTRY, pfnStop, ( HKAI ));
    DECLARE_PFN( APIRET, APIENTRY, pfnPause, ( HKAI ));
    DECLARE_PFN( APIRET, APIENTRY, pfnResume, ( HKAI ));
    DECLARE_PFN( APIRET, APIENTRY, pfnSetSoundState, ( HKAI, ULONG, BOOL ));
    DECLARE_PFN( APIRET, APIENTRY, pfnSetVolume, ( HKAI, ULONG, USHORT ));
    DECLARE_PFN( APIRET, APIENTRY, pfnGetVolume, ( HKAI, ULONG ));
    DECLARE_PFN( APIRET, APIENTRY, pfnClearBuffer, ( HKAI ));
    DECLARE_PFN( APIRET, APIENTRY, pfnStatus, ( HKAI ));
} KAIAPIS, *PKAIAPIS;

static INLINE
APIRET DosLoadModuleCW( PSZ pszName, ULONG cbName, PSZ pszModName,
                        PHMODULE phmod )
{
    unsigned saved_cw;
    APIRET   rc;

    saved_cw = _control87( 0, 0 );

    rc = DosLoadModule( pszName, cbName, pszModName, phmod );

    _control87( saved_cw, MCW_EM | MCW_IC | MCW_RC | MCW_PC );

    return rc;
}

#define DosLoadModule( a, b, c, d ) DosLoadModuleCW( a, b, c, d )

static INLINE
VOID boostThread( VOID )
{
    if( getenv("KAI_TIMECRITICAL"))
    {
        PTIB ptib;

        DosGetInfoBlocks( &ptib, NULL );

        if( ptib->tib_ptib2->tib2_ulpri < 0x3000 )
            DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, 0, 0 );
    }
}

#ifdef __cplusplus
}
#endif

#endif
