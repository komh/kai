/*
    Recursive Spinlock for K Audio Interface
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

#ifndef __KAI_SPINLOCK_H__
#define __KAI_SPINLOCK_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SPINLOCK_INIT { 0, 0 }

typedef struct SPINLOCK
{
    int owner;
    int count;
} SPINLOCK, *PSPINLOCK;

VOID _kaiSpinLockInit( PSPINLOCK pLock );
VOID _kaiSpinLock( PSPINLOCK pLock );
VOID _kaiSpinUnlock( PSPINLOCK pLock );

#define spinLockInit _kaiSpinLockInit
#define spinLock     _kaiSpinLock
#define spinUnlock   _kaiSpinUnlock

#ifdef __cplusplus
}
#endif

#endif
