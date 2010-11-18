/*
    Internal interface for K Audio Interface
    Copyright (C) 2010 by KO Myung-Hun <komh@chollian.net>

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

#ifdef __cplusplus
exter "C" {
#endif

#ifdef __IBMC__
#define DECLARE_PFN( ret, callconv, name, arg ) ret ( * callconv name )arg
#else
#define DECLARE_PFN( ret, callconv, name, arg ) ret ( callconv * name )arg
#endif

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

#ifdef __cplusplus
}
#endif

#endif
