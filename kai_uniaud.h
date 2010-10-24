/*
    Uniaud Interface for OS/2
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

#ifndef __KAI_UNIAUD_H__
#define __KAI_UNIAUD_H__

#include <os2.h>

#ifdef __cplusplus
extern "C" {
#endif

APIRET APIENTRY kaiUniaudInit( PKAIAPIS pkai, PULONG pulMaxChannels );

#ifdef __cpluslus
}
#endif

#endif
