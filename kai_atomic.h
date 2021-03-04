/*
 * Atomic functions
 *
 * Copyright (C) 2021 KO Myung-Hun <komh@chollian.net>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

/** @file kai_atomic.h */

#ifndef __KAI_ATOMIC_H__
#define __KAI_ATOMIC_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Atomic load
 *
 * @param[in] p Pointer to the place where is loaded atomically
 * @return Atomically loaded value of the place pointed by @a p
 */
int _System _kaiAtomicLoad( volatile int *p );

#define LOAD( p )   _kaiAtomicLoad(( int * )( p ))
#define LOADP( p )  (( void * )LOAD(( p )))

/**
 * Atomic exchange
 *
 * @param[out] p Pointer to the place where to store a new value @a v
 * @param[in] v New value
 * @return Old value pointed by @a p
 */
int _System _kaiAtomicExchange( volatile int *p, int v );

#define XCHG( p, v )    _kaiAtomicExchange(( int * )( p ), ( int )( v ))
#define XCHGP( p, v )   (( void * )XCHG(( p ), ( v )))
#define STORE( p, v )   XCHG(( p ), ( v ))
#define STOREP( p, v )  XCHG(( p ), ( v ))

/**
 * Atomic compare and exchange
 *
 * @param[in, out] p Pointer to the place where to compare to @a e
 *                   and to exchange with @a d if the value pointed by @a p
                     and @a e are same
 * @param[in] d Desired value
 * @param[in] e Expected value
 * @return 1 if the value pointed by @a p and @a e are same, otherwise 0
 */
int _System _kaiAtomicCompareExchange( volatile int *p, int d, int e );

#define CMPXCHG( p, d, e ) \
    _kaiAtomicCompareExchange(( int * )( p ), ( int )( d ), ( int )( e ))
#define CMPXCHGP( p, d, e ) CMPXCHG(( p ), ( v ), ( e ))

#ifdef __cplusplus
}
#endif

#endif
