/*
    Audio Server Interface for K Audio Interface
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

#ifndef KAI_SERVER_H
#define KAI_SERVER_H

#define KAISRV_PIPE_CMD     "\\PIPE\\KAISRV\\CMD"
#define KAISRV_PIPE_CB_BASE "\\PIPE\\KAISRV\\CALLBACK"

#define KAISRV_CHECK                0
#define KAISRV_CAPS                 1
#define KAISRV_OPEN                 2
#define KAISRV_CLOSE                3
#define KAISRV_PLAY                 4
#define KAISRV_STOP                 5
#define KAISRV_PAUSE                6
#define KAISRV_RESUME               7
#define KAISRV_SETSOUNDSTATE        8
#define KAISRV_SETVOLUME            9
#define KAISRV_GETVOLUME            10
#define KAISRV_CLEARBUFFER          11
#define KAISRV_STATUS               12
#define KAISRV_ENABLESOFTVOLUME     13
#define KAISRV_MIXEROPEN            14
#define KAISRV_MIXERCLOSE           15
#define KAISRV_MIXERSTREAMOPEN      16
#define KAISRV_MIXERSTREAMCLOSE     17
#define KAISRV_ENABLESOFTMIXER      18
#define KAISRV_GETCARDCOUNT         19
#define KAISRV_CAPSEX               20
#define KAISRV_QUIT                 99

#define REINTR( expr, rc ) \
    do ( rc ) = ( expr ); while(( rc ) == ERROR_INTERRUPT )

APIRET _kaiPipeTimedOpen( const char *name, PHPIPE phpipe, ULONG ms );
APIRET _kaiPipeOpen( const char *name, PHPIPE phpipe );
APIRET _kaiPipeClose( HPIPE hpipe );

APIRET _kaiServerCheck( void );
APIRET _kaiServerCaps( PKAICAPS pkaic );
APIRET _kaiServerOpen( const PKAISPEC pksWanted, PKAISPEC pksObtained,
                       PHKAI phkai );
APIRET _kaiServerClose( PINSTANCELIST pil );
APIRET _kaiServerPlay( PINSTANCELIST pil );
APIRET _kaiServerStop( PINSTANCELIST pil );
APIRET _kaiServerPause( PINSTANCELIST pil );
APIRET _kaiServerResume( PINSTANCELIST pil );
APIRET _kaiServerSetSoundState( PINSTANCELIST pil, ULONG ulCh, BOOL fState );
APIRET _kaiServerSetVolume( PINSTANCELIST pil, ULONG ulCh, USHORT usVol );
APIRET _kaiServerGetVolume( PINSTANCELIST pil, ULONG ulCh );
APIRET _kaiServerClearBuffer( PINSTANCELIST pil );
APIRET _kaiServerStatus( PINSTANCELIST pil );
APIRET _kaiServerEnableSoftVolume( PINSTANCELIST pil, BOOL fEnable );
APIRET _kaiServerMixerOpen( const PKAISPEC pksWanted, PKAISPEC pksObtained,
                            PHKAIMIXER phkm );
APIRET _kaiServerMixerClose( const PINSTANCELIST pil );
APIRET _kaiServerMixerStreamOpen( const PINSTANCELIST pilMixer,
                                  const PKAISPEC pksWanted,
                                  PKAISPEC pksObtained,
                                  PHKAIMIXERSTREAM phkms );
APIRET _kaiServerMixerStreamClose( const PINSTANCELIST pilMixer,
                                   const PINSTANCELIST pil );
APIRET _kaiServerEnableSoftMixer( BOOL fEnable, const PKAISPEC pks );
APIRET _kaiServerGetCardCount( VOID );
APIRET _kaiServerCapsEx( ULONG ulDeviceIndex, PKAICAPS pkaic );

#define pipeTimedOpen   _kaiPipeTimedOpen
#define pipeOpen        _kaiPipeOpen
#define pipeClose       _kaiPipeClose

#define serverCheck             _kaiServerCheck
#define serverCaps              _kaiServerCaps
#define serverOpen              _kaiServerOpen
#define serverClose             _kaiServerClose
#define serverPlay              _kaiServerPlay
#define serverStop              _kaiServerStop
#define serverPause             _kaiServerPause
#define serverResume            _kaiServerResume
#define serverSetSoundState     _kaiServerSetSoundState
#define serverSetVolume         _kaiServerSetVolume
#define serverGetVolume         _kaiServerGetVolume
#define serverClearBuffer       _kaiServerClearBuffer
#define serverStatus            _kaiServerStatus
#define serverEnableSoftVolume  _kaiServerEnableSoftVolume
#define serverMixerOpen         _kaiServerMixerOpen
#define serverMixerClose        _kaiServerMixerClose
#define serverMixerStreamOpen   _kaiServerMixerStreamOpen
#define serverMixerStreamClose  _kaiServerMixerStreamClose
#define serverEnableSoftMixer   _kaiServerEnableSoftMixer
#define serverGetCardCount      _kaiServerGetCardCount
#define serverCapsEx            _kaiServerCapsEx

#endif
