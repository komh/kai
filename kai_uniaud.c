/*
    Uniaud Interface for K Audio Interface
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

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#define INCL_OS2MM
#include <os2me.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __WATCOMC__
#include <process.h>
#endif

#include "uniaud.h"

#include "kai.h"
#include "kai_internal.h"
#include "kai_uniaud.h"

// EAGAIN is different in according to a compiler
// So define macro for UNIAUD
#define UNIAUD_EAGAIN   11

typedef struct ReSampleContext1
{
    void   *resample_context;
    short  *temp[2];
    int     temp_len;
    float   ratio;
    /* channel convert */
    int     input_channels, output_channels, filter_channels;
} ReSampleContext1;

static volatile BOOL m_fPlaying   = FALSE;
static volatile BOOL m_fPaused    = FALSE;
static          BOOL m_fCompleted = FALSE;

static TID m_tidFillThread = 0;

static PFNKAICB  m_pfnUniaudCB = NULL;
static PVOID     m_pUniaudCBData = NULL;

static PCH          m_pchBuffer = NULL;
static uniaud_pcm  *m_pcm = NULL;

#define uniaud_mixer_get_power_state        m_pfnUniaudMixerGetPowerState
#define uniaud_mixer_set_power_state        m_pfnUniaudMixerSetPowerState
#define uniaud_mixer_get_ctls_number        m_pfnUniaudMixerGetCtlsNumber
#define uniaud_mixer_get_ctl_list           m_pfnUniaudMixerGetCtlList
#define uniaud_get_id_by_name               m_pfnUniaudGetIdByName
#define uniaud_mixer_get_ctl_info           m_pfnUniaudMixerGetCtlInfo
#define uniaud_mixer_get_ctl_val            m_pfnUniaudMixerGetCtlVal
#define uniaud_mixer_put_ctl_val            m_pfnUniaudMixerPutCtlVal
#define uniaud_mixer_wait                   m_pfnUniaudMixerWait
#define uniaud_mixer_get_min_max            m_pfnUniaudMixerGetMinMax
#define uniaud_mixer_get_count_of_values    m_pfnUniaudMixerGetCountOfValues
#define uniaud_mixer_put_value              m_pfnUniaudMixerPutValue
#define uniaud_mixer_get_value              m_pfnUniaudMixerGetValue
#define uniaud_mixer_put_value_by_name      m_pfnUniaudMixerPutValueByName
#define uniaud_mixer_get_value_by_name      m_pfnUniaudMixerGetValueByName
#define uniaud_get_pcm_instances            m_pfnUniaudGetPcmInstances
#define uniaud_pcm_get_caps                 m_pfnUniaudPcmGetCaps
#define uniaud_pcm_find_pcm_for_chan        m_pfnUniaudPcmFindPcmForChan
#define uniaud_pcm_set_pcm                  m_pfnUniaudPcmSetPcm
#define uniaud_pcm_find_max_chan            m_pfnUniaudPcmFindMaxChan
#define uniaud_get_max_channels             m_pfnUniaudGetMaxChannels
#define uniaud_pcm_format_size              m_pfnUniaudPcmFormatSize
#define uniaud_pcm_open                     m_pfnUniaudPcmOpen
#define uniaud_pcm_close                    m_pfnUniaudPcmClose
#define uniaud_pcm_write                    m_pfnUniaudPcmWrite
#define uniaud_pcm_read                     m_pfnUniaudPcmRead
#define uniaud_pcm_prepare                  m_pfnUniaudPcmPrepare
#define uniaud_pcm_resume                   m_pfnUniaudPcmResume
#define uniaud_pcm_status                   m_pfnUniaudPcmStatus
#define uniaud_pcm_state                    m_pfnUniaudPcmState
#define uniaud_pcm_wait                     m_pfnUniaudPcmWait
#define uniaud_pcm_pause                    m_pfnUniaudPcmPause
#define uniaud_pcm_start                    m_pfnUniaudPcmStart
#define uniaud_pcm_drop                     m_pfnUniaudPcmDrop
#define uniaud_close_all_pcms               m_pfnUniaudCloseAllPcms
#define uniaud_get_cards                    m_pfnUniaudGetCards
#define uniaud_get_card_info                m_pfnUniaudGetCardInfo
#define uniaud_get_version                  m_pfnUniaudGetVersion
#define _snd_pcm_hw_params_any              m_pfn_SndPcmHwParamsAny
#define snd_pcm_hw_params_any               m_pfnSndPcmHwParamsAny
#define _snd_pcm_hw_param_set               m_pfn_SndPcmHwParamSet
#define _uniaud_pcm_refine_hw_params        m_pfnUniaudPcmRefineHwParams
#define _uniaud_pcm_set_hw_params           m_pfnUniaudPcmSetHwParams
#define _uniaud_pcm_set_sw_params           m_pfnUniaudPcmSetSwParams
#define uniaud_mixer_put_spdif_status       m_pfnUniaudMixerPutSpdifStatus

static DECLARE_PFN( int, _System, uniaud_mixer_get_power_state, ( int, ULONG * ));
static DECLARE_PFN( int, _System, uniaud_mixer_set_power_state, ( int, ULONG * ));
static DECLARE_PFN( int, _System, uniaud_mixer_get_ctls_number, ( int ));
static DECLARE_PFN( UniaudControl *, _System, uniaud_mixer_get_ctl_list, ( int ));
static DECLARE_PFN( int, _System, uniaud_get_id_by_name, ( int, char *, int ));
static DECLARE_PFN( int, _System, uniaud_mixer_get_ctl_info, ( int, ULONG, UniaudControlInfo * ));
static DECLARE_PFN( int, _System, uniaud_mixer_get_ctl_val, ( int, ULONG, UniaudControlValue * ));
static DECLARE_PFN( int, _System, uniaud_mixer_put_ctl_val, ( int, ULONG, UniaudControlValue * ));
static DECLARE_PFN( int, _System, uniaud_mixer_wait, ( int, int ));
static DECLARE_PFN( int, _System, uniaud_mixer_get_min_max, ( int, ULONG, int *, int * ));
static DECLARE_PFN( int, _System, uniaud_mixer_get_count_of_values, ( int, ULONG, int * ));
static DECLARE_PFN( int, _System, uniaud_mixer_put_value, ( int, ULONG, int, int ));
static DECLARE_PFN( int, _System, uniaud_mixer_get_value, ( int, ULONG, int ));
static DECLARE_PFN( int, _System, uniaud_mixer_put_value_by_name, ( int, char *, int, int, int ));
static DECLARE_PFN( int, _System, uniaud_mixer_get_value_by_name, ( int, char *, int, int ));
static DECLARE_PFN( int, _System, uniaud_get_pcm_instances, ( int ));
static DECLARE_PFN( int, _System, uniaud_pcm_get_caps, ( int, POSS32_DEVCAPS ));
static DECLARE_PFN( int, _System, uniaud_pcm_find_pcm_for_chan, ( POSS32_DEVCAPS, int, int, int ));
static DECLARE_PFN( int, _System, uniaud_pcm_set_pcm, ( int ));
static DECLARE_PFN( int, _System, uniaud_pcm_find_max_chan, ( POSS32_DEVCAPS, int, int ));
static DECLARE_PFN( int, _System, uniaud_get_max_channels, ( int ));
static DECLARE_PFN( ssize_t, _System, uniaud_pcm_format_size, ( snd_pcm_format_t, size_t ));
static DECLARE_PFN( int, _System, uniaud_pcm_open, ( int, int, int, int, int, int, int, uniaud_pcm ** ));
static DECLARE_PFN( int, _System, uniaud_pcm_close, ( uniaud_pcm * ));
static DECLARE_PFN( int, _System, uniaud_pcm_write, ( uniaud_pcm *, char *, int ));
static DECLARE_PFN( int, _System, uniaud_pcm_read, ( uniaud_pcm *, char *, int ));
static DECLARE_PFN( int, _System, uniaud_pcm_prepare, ( uniaud_pcm * ));
static DECLARE_PFN( int, _System, uniaud_pcm_resume, ( uniaud_pcm * ));
static DECLARE_PFN( int, _System, uniaud_pcm_status, ( uniaud_pcm *, snd_pcm_status_t * ));
static DECLARE_PFN( int, _System, uniaud_pcm_state, ( uniaud_pcm * ));
static DECLARE_PFN( int, _System, uniaud_pcm_wait, ( uniaud_pcm *, int ));
static DECLARE_PFN( int, _System, uniaud_pcm_pause, ( uniaud_pcm * ));
static DECLARE_PFN( int, _System, uniaud_pcm_start, ( uniaud_pcm * ));
static DECLARE_PFN( int, _System, uniaud_pcm_drop, ( uniaud_pcm * ));
static DECLARE_PFN( int, _System, uniaud_close_all_pcms, ( int ));
static DECLARE_PFN( int, _System, uniaud_get_cards, ( void ));
static DECLARE_PFN( int, _System, uniaud_get_card_info, ( int, UniaudCardInfo * ));
static DECLARE_PFN( int, _System, uniaud_get_version, ( void ));
static DECLARE_PFN( void, _System, _snd_pcm_hw_params_any, ( snd_pcm_hw_params_t * ));
static DECLARE_PFN( int, _System, snd_pcm_hw_params_any, ( uniaud_pcm *, snd_pcm_hw_params_t * ));
static DECLARE_PFN( int, _System, _snd_pcm_hw_param_set, ( snd_pcm_hw_params_t *, snd_pcm_hw_param_t, unsigned int, int ));
static DECLARE_PFN( int, _System, _uniaud_pcm_refine_hw_params, ( uniaud_pcm *, snd_pcm_hw_params_t * ));
static DECLARE_PFN( int, _System, _uniaud_pcm_set_hw_params, ( uniaud_pcm *, snd_pcm_hw_params_t * ));
static DECLARE_PFN( int, _System, _uniaud_pcm_set_sw_params, ( uniaud_pcm *, snd_pcm_sw_params_t * ));
static DECLARE_PFN( int, _System, uniaud_mixer_put_spdif_status, ( int, char *, int, int, int, int ));

static HMODULE m_hmodUniaud = NULLHANDLE;

static APIRET APIENTRY uniaudDone( VOID );
static APIRET APIENTRY uniaudOpen( PKAISPEC pks );
static APIRET APIENTRY uniaudClose( VOID );
static APIRET APIENTRY uniaudPlay( VOID );
static APIRET APIENTRY uniaudStop( VOID );
static APIRET APIENTRY uniaudPause( VOID );
static APIRET APIENTRY uniaudResume( VOID );
static APIRET APIENTRY uniaudSetSoundState( ULONG ulCh, BOOL fState );
static APIRET APIENTRY uniaudSetVolume( ULONG ulCh, USHORT usVol );
static APIRET APIENTRY uniaudGetVolume( ULONG );
static APIRET APIENTRY uniaudChNum( VOID );
static APIRET APIENTRY uniaudClearBuffer( VOID );
static APIRET APIENTRY uniaudStatus( VOID );

static VOID freeUniaud( VOID )
{
    DosFreeModule( m_hmodUniaud );

    m_hmodUniaud = NULLHANDLE;
}

static BOOL loadUniaud( VOID )
{
    char szTempStr[ 255 ];

    if( m_hmodUniaud )
        return TRUE;

    if( DosLoadModule( szTempStr, sizeof( szTempStr ), "uniaud", &m_hmodUniaud ))
        return FALSE;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_power_state", ( PFN * )&uniaud_mixer_get_power_state ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_set_power_state", ( PFN * )&uniaud_mixer_set_power_state ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_ctls_number", ( PFN * )&uniaud_mixer_get_ctls_number ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_ctl_list", ( PFN * )&uniaud_mixer_get_ctl_list ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_get_id_by_name", ( PFN * )&uniaud_get_id_by_name ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_ctl_info", ( PFN * )&uniaud_mixer_get_ctl_info ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_ctl_val", ( PFN * )&uniaud_mixer_get_ctl_val ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_put_ctl_val", ( PFN * )&uniaud_mixer_put_ctl_val ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_wait", ( PFN * )&uniaud_mixer_wait ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_min_max", ( PFN * )&uniaud_mixer_get_min_max ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_count_of_values", ( PFN * )&uniaud_mixer_get_count_of_values ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_put_value", ( PFN * )&uniaud_mixer_put_value ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_value", ( PFN * )&uniaud_mixer_get_value ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_put_value_by_name", ( PFN * )&uniaud_mixer_put_value_by_name ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_get_value_by_name", ( PFN * )&uniaud_mixer_get_value_by_name ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_get_pcm_instances", ( PFN * )&uniaud_get_pcm_instances ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_get_caps", ( PFN * )&uniaud_pcm_get_caps ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_find_pcm_for_chan", ( PFN * )&uniaud_pcm_find_pcm_for_chan ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_set_pcm", ( PFN * )&uniaud_pcm_set_pcm ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_find_max_chan", ( PFN * )&uniaud_pcm_find_max_chan ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_get_max_channels", ( PFN * )&uniaud_get_max_channels ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_format_size", ( PFN * )&uniaud_pcm_format_size ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_open", ( PFN * )&uniaud_pcm_open ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_close", ( PFN * )&uniaud_pcm_close ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_write", ( PFN * )&uniaud_pcm_write ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_read", ( PFN * )&uniaud_pcm_read ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_prepare", ( PFN * )&uniaud_pcm_prepare ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_resume", ( PFN * )&uniaud_pcm_resume ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_status", ( PFN * )&uniaud_pcm_status ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_state", ( PFN * )&uniaud_pcm_state ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_wait", ( PFN * )&uniaud_pcm_wait ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_pause", ( PFN * )&uniaud_pcm_pause ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_start", ( PFN * )&uniaud_pcm_start ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_pcm_drop", ( PFN * )&uniaud_pcm_drop ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_close_all_pcms", ( PFN * )&uniaud_close_all_pcms ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_get_cards", ( PFN * )&uniaud_get_cards ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_get_card_info", ( PFN * )&uniaud_get_card_info ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_get_version", ( PFN * )&uniaud_get_version ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "_snd_pcm_hw_params_any", ( PFN * )&_snd_pcm_hw_params_any ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "snd_pcm_hw_params_any", ( PFN * )&snd_pcm_hw_params_any ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "_snd_pcm_hw_param_set", ( PFN * )&_snd_pcm_hw_param_set ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "_uniaud_pcm_refine_hw_params", ( PFN * )&_uniaud_pcm_refine_hw_params ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "_uniaud_pcm_set_hw_params", ( PFN * )&_uniaud_pcm_set_hw_params ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "_uniaud_pcm_set_sw_params", ( PFN * )&_uniaud_pcm_set_sw_params ))
        goto exit_error;

    if( DosQueryProcAddr( m_hmodUniaud, 0, "uniaud_mixer_put_spdif_status", ( PFN * )&uniaud_mixer_put_spdif_status ))
        goto exit_error;

    return TRUE;

exit_error :
    freeUniaud();

    return FALSE;
}

APIRET APIENTRY kaiUniaudInit( PKAIAPIS pkai, PULONG pulMaxChannels )
{
    if( !loadUniaud())
        return KAIE_CANNOT_LOAD_SUB_MODULE;

    pkai->pfnDone          = uniaudDone;
    pkai->pfnOpen          = uniaudOpen;
    pkai->pfnClose         = uniaudClose;
    pkai->pfnPlay          = uniaudPlay;
    pkai->pfnStop          = uniaudStop;
    pkai->pfnPause         = uniaudPause;
    pkai->pfnResume        = uniaudResume;
    pkai->pfnSetSoundState = uniaudSetSoundState;
    pkai->pfnSetVolume     = uniaudSetVolume;
    pkai->pfnGetVolume     = uniaudGetVolume;
    pkai->pfnClearBuffer   = uniaudClearBuffer;
    pkai->pfnStatus        = uniaudStatus;

    *pulMaxChannels  = uniaudChNum();

    return KAIE_NO_ERROR;

}

static APIRET APIENTRY uniaudDone( VOID )
{
    freeUniaud();

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudStop(void)
{
    if( !m_fPlaying )
        return KAIE_NO_ERROR;

    m_fPlaying = FALSE;
    m_fPaused  = FALSE;

    while( DosWaitThread( &m_tidFillThread, DCWW_WAIT ) == ERROR_INTERRUPT );

    return uniaud_pcm_drop( m_pcm );
}

static APIRET APIENTRY uniaudClearBuffer( VOID )
{
    memset( m_pchBuffer,0, m_pcm->bufsize );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudFreeBuffers( VOID )
{
    free( m_pchBuffer );
    m_pchBuffer = NULL;

    return KAIE_NO_ERROR;
}

static void uniaudFillThread( void *arg )
{
    int   count, timeout = 20;
    int   err, ret, state;
    int   written;
    int   fill_size;

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );

    timeout *= m_pcm->channels;

    while( m_fPlaying )
    {
        if( m_pcm->multi_channels > 0 )
            fill_size = m_pcm->bufsize;
        else
        {
            if( m_pcm->resample && (( ReSampleContext1 * )( m_pcm->resample ))->ratio >= 2.0f )
                fill_size = m_pcm->period_size;
            else
                fill_size = m_pcm->period_size * 2;
        }

        count = m_pfnUniaudCB( m_pUniaudCBData, m_pchBuffer, fill_size );
        if( count == 0 )
        {
            // stop playing
            m_fPlaying = FALSE;
            m_fPaused  = FALSE;
            uniaud_pcm_drop( m_pcm );

            m_fCompleted = TRUE;
            break;
        }

        written = 0;
        while( m_fPlaying && written < count )
        {
            if( m_fPaused )
            {
                DosSleep( 1 );
                continue;
            }

            err = uniaud_pcm_write( m_pcm, m_pchBuffer + written, count - written );
            ret = uniaud_pcm_wait( m_pcm, timeout );
            if (ret == -77)
                uniaud_pcm_prepare( m_pcm );

            if (err == -UNIAUD_EAGAIN)
            {
                state = uniaud_pcm_state( m_pcm );
                printf("eagain. written: %i of %i. state: %i\n", written, count, state );
                if( written > 0 )
                    break;

                DosSleep( 1 );

                uniaud_pcm_prepare( m_pcm );
                if(( state != SND_PCM_STATE_PREPARED ) &&
                   ( state != SND_PCM_STATE_RUNNING ) &&
                   ( state != SND_PCM_STATE_DRAINING ))
                    uniaud_pcm_prepare(m_pcm);

                continue;
            }

            if( err > 0 )
                written += err;

            if( err < 0 )
            {
                if( err < -10000 )
                {
                    DosSleep( 1 );

                    printf("part written %i from %i.err: %i\n", written, count, err );
                    break; // internal uniaud error
                }

                DosSleep( 1 );

                state = uniaud_pcm_state( m_pcm );
                if((( state != SND_PCM_STATE_PREPARED ) &&
                    ( state != SND_PCM_STATE_RUNNING ) &&
                    ( state != SND_PCM_STATE_DRAINING )) || err == -32 )
                {
                    if(( ret = uniaud_pcm_prepare( m_pcm )) < 0 )
                        break;
                }
            }
        }
    }
}

static APIRET APIENTRY uniaudChNum( VOID )
{
    return uniaud_get_max_channels( 0 );
}

static APIRET APIENTRY uniaudOpen( PKAISPEC pks )
{
    int format;
    int err;

    m_fPlaying   = FALSE;
    m_fPaused    = FALSE;
    m_fCompleted = FALSE;

    switch( pks->ulBitsPerSample )
    {
        case BPS_8 :
            format = SNDRV_PCM_FORMAT_U8;
            break;

        case BPS_16 :
            format = SNDRV_PCM_FORMAT_S16_LE;
            break;

        case BPS_32 :
            format = SNDRV_PCM_FORMAT_S32_LE;
            break;

        default :
            format = SNDRV_PCM_FORMAT_S16_LE;
            break;
    }

    err = uniaud_pcm_open( pks->usDeviceIndex, PCM_TYPE_WRITE, 0, pks->fShareable,
                           pks->ulSamplingRate, pks->ulChannels, format, &m_pcm);

    if( !m_pcm || err )
    {
        printf("m_pcm open error %d\n", err );

        m_pcm = NULL;

        return err;
    }

    m_pchBuffer = malloc( m_pcm->bufsize );
    if( !m_pchBuffer )
    {
        uniaud_pcm_close( m_pcm );

        m_pcm = NULL;

        return KAIE_NOT_ENOUGH_MEMORY;
    }

    memset( m_pchBuffer, 0, m_pcm->bufsize );

    m_pfnUniaudCB   = pks->pfnCallBack;
    m_pUniaudCBData = pks->pCallBackData;

    pks->ulNumBuffers = m_pcm->periods;
    pks->ulBufferSize = m_pcm->period_size;
    pks->bSilence     = ( pks->ulBitsPerSample == BPS_8 ) ? 0x80 : 0x00;

    uniaud_pcm_prepare( m_pcm );

    return KAIE_NO_ERROR;
}


static APIRET APIENTRY uniaudClose( VOID )
{
    uniaudStop();
    uniaudFreeBuffers();

    uniaud_pcm_close( m_pcm );

    m_pcm = NULL;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudPlay( VOID )
{
    if( m_fPlaying )
        return KAIE_NO_ERROR;

    uniaud_pcm_prepare( m_pcm );

    m_fPlaying   = TRUE;
    m_fPaused    = FALSE;
    m_fCompleted = FALSE;

    m_tidFillThread = _beginthread( uniaudFillThread, NULL, 256 * 1024, NULL );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudPause( VOID )
{
    if( m_fPaused )
        return KAIE_NO_ERROR;

    m_fPaused = TRUE;
    uniaud_pcm_drop( m_pcm );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudResume( VOID )
{
    if( !m_fPaused )
        return KAIE_NO_ERROR;

    uniaud_pcm_start( m_pcm );
    m_fPaused = FALSE;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudSetSoundState( ULONG ulCh, BOOL fState )
{
    uniaud_mixer_put_value_by_name( m_pcm->card_id, "PCM Playback Switch", fState, 1, 0 );
    uniaud_mixer_put_value_by_name( m_pcm->card_id, "PCM Playback Switch", fState, 0, 0 );
    uniaud_mixer_put_value_by_name( m_pcm->card_id, "Front Playback Switch", fState, 1, 0 );
    uniaud_mixer_put_value_by_name( m_pcm->card_id, "Front Playback Switch", fState, 0, 0 );
    uniaud_mixer_put_value_by_name( m_pcm->card_id, "Surround Playback Switch", fState, 1, 0 );
    uniaud_mixer_put_value_by_name( m_pcm->card_id, "Surround Playback Switch", fState, 0, 0 );
    uniaud_mixer_put_value_by_name( m_pcm->card_id, "LFE Playback Switch", fState, 0, 0 );
    uniaud_mixer_put_value_by_name( m_pcm->card_id, "Center Playback Switch", fState, 0, 0 );

    return KAIE_NO_ERROR;
}

static void set_perc_vol( char *name, int vol, int stereo )
{
    int ctl_id, min, max, new_vol;

    ctl_id = uniaud_get_id_by_name( m_pcm->card_id, name, 0 );
    if( ctl_id > 0 )
    {
        uniaud_mixer_get_min_max( m_pcm->card_id, ctl_id, &min, &max );
        new_vol = (( float )( max - min ) / 100.0f ) * vol;

        uniaud_mixer_put_value( m_pcm->card_id, ctl_id, new_vol, 0 );
        if( stereo )
            uniaud_mixer_put_value( m_pcm->card_id, ctl_id, new_vol, 1 );
    }
}

static APIRET APIENTRY uniaudSetVolume( ULONG ulCh, USHORT usVol )
{
    set_perc_vol("PCM Playback Volume", usVol, 1 );
    set_perc_vol("Front Playback Volume", usVol, 1 );
    set_perc_vol("Wave Playback Volume", usVol, 1 );
    set_perc_vol("Wave Surround Playback Volume", usVol, 1 );
    set_perc_vol("Wave Center Playback Volume", usVol, 0 );
    set_perc_vol("Wave LFE Playback Volume", usVol, 0 );
    set_perc_vol("Playback Volume", usVol, 1 );
    set_perc_vol("Surround Playback Volume", usVol, 1 );
    set_perc_vol("Center Playback Volume", usVol, 0 );
    set_perc_vol("LFE Playback Volume", usVol, 0 );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudGetVolume( ULONG ulCh )
{
    int ctl_id, min, max, new_vol;

    ctl_id = uniaud_get_id_by_name( m_pcm->card_id, "PCM Playback Volume", 0 );

    uniaud_mixer_get_min_max( m_pcm->card_id, ctl_id, &min, &max );
    new_vol = uniaud_mixer_get_value( m_pcm->card_id, ctl_id, 0 );

    return new_vol * ( 100.0f / ( max - min ));
}

static APIRET APIENTRY uniaudStatus( VOID )
{
    ULONG ulStatus = 0;

    if( m_fPlaying )
        ulStatus |= KAIS_PLAYING;

    if( m_fPaused )
        ulStatus |= KAIS_PAUSED;

    if( m_fCompleted )
        ulStatus |= KAIS_COMPLETED;

    return ulStatus;
}
