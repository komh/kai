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

#include "kai_internal.h"

#include <stddef.h>

VOID _kaiSpinLockInit( PSPINLOCK pLock )
{
    static SPINLOCK lockInit = SPINLOCK_INIT;

    *pLock = lockInit;
}

VOID _kaiSpinLock( PSPINLOCK pLock )
{
    int tid = *_threadid;

    while( !CMPXCHG( &pLock->owner, tid, 0 ) && pLock->owner != tid )
        DosSleep( 1 );

    pLock->count++;
}

INT _kaiSpinTryLock( PSPINLOCK pLock )
{
    int tid = *_threadid;

    if( !CMPXCHG( &pLock->owner, tid, 0 ) && pLock->owner != tid )
        return -1;

    pLock->count++;

    return 0;
}

VOID _kaiSpinUnlock( PSPINLOCK pLock )
{
    if( pLock->owner == *_threadid )
    {
        if( --pLock->count == 0 )
            STORE( &pLock->owner, 0 );
    }
}
