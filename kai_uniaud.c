/*
    Uniaud Interface for K Audio Interface
    Copyright (C) 2010-2015 by KO Myung-Hun <komh@chollian.net>

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

#ifdef __KLIBC__
#include <emx/umalloc.h>

#define calloc _lcalloc
#define malloc _lmalloc
#endif

// EAGAIN is different in according to a compiler
// So define macro for UNIAUD
#define UNIAUD_EAGAIN   11

#pragma pack( 1 )
typedef struct ReSampleContext1
{
    void   *resample_context;
    short  *temp[2];
    int     temp_len;
    float   ratio;
    /* channel convert */
    int     input_channels, output_channels, filter_channels;
} ReSampleContext1;

typedef struct tagUNIAUDINFO
{
    uniaud_pcm      *pcm;
    BYTE             bSilence;
    PFNKAICB         pfnUniaudCB;
    PVOID            pUniaudCBData;
    PCH              pchBuffer;
    HEV              hevFill;
    HEV              hevFillDone;
    TID              tidPlayThread;
    INT              nFillSize;
    ULONG   volatile ulCount;
    BOOL    volatile fPlaying;
    BOOL    volatile fPaused;
    BOOL    volatile fCompleted;
} UNIAUDINFO, *PUNIAUDINFO;
#pragma pack()

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

static BOOL m_fDebugMode = FALSE;

static APIRET APIENTRY uniaudDone( VOID );
static APIRET APIENTRY uniaudOpen( PKAISPEC pks, PHKAI phkai );
static APIRET APIENTRY uniaudClose( HKAI hkai );
static APIRET APIENTRY uniaudPlay( HKAI hkai );
static APIRET APIENTRY uniaudStop( HKAI hkai );
static APIRET APIENTRY uniaudPause( HKAI hkai );
static APIRET APIENTRY uniaudResume( HKAI hkai );
static APIRET APIENTRY uniaudSetSoundState( HKAI hkai, ULONG ulCh, BOOL fState );
static APIRET APIENTRY uniaudSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol );
static APIRET APIENTRY uniaudGetVolume( HKAI hkai, ULONG ulCh );
static APIRET APIENTRY uniaudChNum( VOID );
static APIRET APIENTRY uniaudClearBuffer( HKAI hkai );
static APIRET APIENTRY uniaudStatus( HKAI hkai );

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
        goto exit_error;

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

    switch( uniaud_get_version())
    {
        case -2 :   // uniaud not detected
        case -1 :   // uniaud error
            break;

        default :
            if( uniaud_get_cards()) // audio card detected ?
                return TRUE;
            break;
    }

exit_error :
    freeUniaud();

    return FALSE;
}

APIRET APIENTRY kaiUniaudInit( PKAIAPIS pkai, PKAICAPS pkc )
{
    UniaudCardInfo cardInfo;

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

    pkc->ulMode         = KAIM_UNIAUD;
    pkc->ulMaxChannels  = uniaudChNum();

    uniaud_get_card_info( 0, &cardInfo );
    strcpy( pkc->szPDDName, ( char * )cardInfo.name );

    m_fDebugMode = getenv("KAIDEBUG") != NULL;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudDone( VOID )
{
    freeUniaud();

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudStop( HKAI hkai )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    if( !pui->fPlaying )
        return KAIE_NO_ERROR;

    uniaud_pcm_drop( pui->pcm );

    pui->fPlaying = FALSE;
    pui->fPaused  = FALSE;

    while( DosWaitThread( &pui->tidPlayThread, DCWW_WAIT ) == ERROR_INTERRUPT );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudClearBuffer( HKAI hkai )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    memset( pui->pchBuffer, pui->bSilence, pui->pcm->bufsize );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudFreeBuffers( HKAI hkai )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    free( pui->pchBuffer );
    pui->pchBuffer = NULL;

    return KAIE_NO_ERROR;
}

static void uniaudFillThread( void *arg )
{
    PUNIAUDINFO pui = arg;
    ULONG       ulPost;

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );

    for(;;)
    {
        while( DosWaitEventSem( pui->hevFill, SEM_INDEFINITE_WAIT ) == ERROR_INTERRUPT );
        DosResetEventSem( pui->hevFill, &ulPost );

        if( !pui->fPlaying )
            break;

        pui->ulCount = pui->pfnUniaudCB( pui->pUniaudCBData, pui->pchBuffer, pui->nFillSize );

        DosPostEventSem( pui->hevFillDone );
    }
}

static void uniaudPlayThread( void *arg )
{
    PUNIAUDINFO pui = arg;
    int         count, timeout = 20;
    int         err, ret, state;
    int         written;
    TID         tidFillThread;
    ULONG       ulPost;
    PCH         pchBuffer;

    //DosSetPriority( PRTYS_THREAD, PRTYC_TIMECRITICAL, PRTYD_MAXIMUM, 0 );

    if( pui->pcm->multi_channels > 0 )
        pui->nFillSize = pui->pcm->bufsize;
    else
    {
        if( pui->pcm->resample && (( ReSampleContext1 * )( pui->pcm->resample ))->ratio >= 2.0f )
            pui->nFillSize = pui->pcm->period_size;
        else
            pui->nFillSize = pui->pcm->period_size * 2;
    }

    pchBuffer = malloc( pui->nFillSize );
    if( !pchBuffer )
    {
        pui->fPlaying = FALSE;

        return;
    }

    DosCreateEventSem( NULL, &pui->hevFill, 0, TRUE );
    DosCreateEventSem( NULL, &pui->hevFillDone, 0, FALSE );
    tidFillThread = _beginthread( uniaudFillThread, NULL, 256 * 1024, pui );

    timeout *= pui->pcm->channels;

    while( pui->fPlaying )
    {
        if( DosWaitEventSem( pui->hevFillDone, SEM_IMMEDIATE_RETURN ) == NO_ERROR )
        {
            DosResetEventSem( pui->hevFillDone, &ulPost );

            memcpy( pchBuffer, pui->pchBuffer, pui->nFillSize );
            count = pui->ulCount;

            if( pui->ulCount == pui->nFillSize )
                DosPostEventSem( pui->hevFill );
        }
        else
        {
            memset( pchBuffer, pui->bSilence, pui->nFillSize );
            count = pui->nFillSize;
        }

        written = 0;
        while( pui->fPlaying && written < count )
        {
            if( pui->fPaused )
            {
                DosSleep( 1 );
                continue;
            }

            err = uniaud_pcm_write( pui->pcm, pchBuffer + written, count - written );
            ret = uniaud_pcm_wait( pui->pcm, timeout );
            if( ret == -77 )
                uniaud_pcm_prepare( pui->pcm );

            if( err == -UNIAUD_EAGAIN )
            {
                state = uniaud_pcm_state( pui->pcm );
                if (m_fDebugMode)
                    fprintf( stderr, "EAGAIN : written = %i of %i, state = %i\n", written, count, state );

                uniaud_pcm_drop( pui->pcm );

                continue;
            }

            if( err > 0 )
                written += err;

            if( err < 0 )
            {
                if( err < -10000 )
                {
                    if (m_fDebugMode)
                        fprintf( stderr, "part written = %i from %i, err = %i\n", written, count, err );

                    break; // internal uniaud error
                }

                state = uniaud_pcm_state( pui->pcm );
                if((( state != SND_PCM_STATE_PREPARED ) &&
                    ( state != SND_PCM_STATE_RUNNING ) &&
                    ( state != SND_PCM_STATE_DRAINING )) || err == -32 )
                {
                    ret = uniaud_pcm_prepare( pui->pcm );
                    if( ret < 0 )
                    {
                        uniaud_pcm_drop( pui->pcm );
                        break;
                    }
                }
            }
        }

        if( count < pui->nFillSize )
        {
            // stop playing
            pui->fPlaying = FALSE;
            pui->fPaused  = FALSE;
            uniaud_pcm_drop( pui->pcm );

            pui->fCompleted = TRUE;
            break;
        }
    }

    DosPostEventSem( pui->hevFill );
    while( DosWaitThread( &tidFillThread, DCWW_WAIT ) == ERROR_INTERRUPT );
    DosCloseEventSem( pui->hevFill );

    DosCloseEventSem( pui->hevFillDone );

    free( pchBuffer );
}

static APIRET APIENTRY uniaudChNum( VOID )
{
    return uniaud_get_max_channels( 0 );
}

static APIRET APIENTRY uniaudOpen( PKAISPEC pks, PHKAI phkai )
{
    PUNIAUDINFO pui;
    USHORT      usDeviceIndex;
    int         format;
    int         err;

    pui = calloc( 1, sizeof( UNIAUDINFO ));
    if( !pui )
        return KAIE_NOT_ENOUGH_MEMORY;

    usDeviceIndex = pks->usDeviceIndex > 0 ? ( pks->usDeviceIndex - 1 ) : 0;

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

    err = uniaud_pcm_open( usDeviceIndex, PCM_TYPE_WRITE, 0, pks->fShareable,
                           pks->ulSamplingRate, pks->ulChannels, format, &pui->pcm );

    if( !pui->pcm || err )
    {
        if (m_fDebugMode)
            fprintf( stderr, "pcm open error %d\n", err );

        goto exit_free;
    }

    pui->pchBuffer = malloc( pui->pcm->bufsize );
    if( !pui->pchBuffer )
    {
        err = KAIE_NOT_ENOUGH_MEMORY;

        goto exit_close;
    }

    pui->pfnUniaudCB   = pks->pfnCallBack;
    pui->pUniaudCBData = pks->pCallBackData;
    pui->bSilence      = ( pks->ulBitsPerSample == BPS_8 ) ? 0x80 : 0x00;

    memset( pui->pchBuffer, pui->bSilence, pui->pcm->bufsize );

    pks->ulNumBuffers = pui->pcm->periods;
    pks->ulBufferSize = pui->pcm->period_size;
    pks->bSilence     = pui->bSilence;

    uniaud_pcm_prepare( pui->pcm );

    *phkai = ( HKAI )pui;

    return KAIE_NO_ERROR;

exit_close :
    uniaud_pcm_close( pui->pcm );

exit_free :
    free( pui );

    return err;
}

static APIRET APIENTRY uniaudClose( HKAI hkai )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    uniaudStop( hkai );
    uniaudFreeBuffers( hkai );

    uniaud_pcm_close( pui->pcm );

    free( pui );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudPlay( HKAI hkai )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    if( pui->fPlaying )
        return KAIE_NO_ERROR;

    uniaud_pcm_prepare( pui->pcm );

    pui->fPlaying   = TRUE;
    pui->fPaused    = FALSE;
    pui->fCompleted = FALSE;

    pui->tidPlayThread = _beginthread( uniaudPlayThread, NULL, 256 * 1024, pui );

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudPause( HKAI hkai )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    if( pui->fPaused )
        return KAIE_NO_ERROR;

    uniaud_pcm_drop( pui->pcm );
    pui->fPaused = TRUE;

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudResume( HKAI hkai )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    if( !pui->fPaused )
        return KAIE_NO_ERROR;

    uniaud_pcm_prepare( pui->pcm );
    pui->fPaused = FALSE;

    return KAIE_NO_ERROR;
}

/* List of controls from uniaud32 trunk */
static const char *m_volumeControls[] = {
// Front
    "",
    "AC97",
    "Analog Front",
    "Front",
    "HD Analog Front",
    "HP Playback",
    "PCM",
    "PCM Front",
    "PCM Stream",
    "SB PCM",
    "Speaker",
    "Wave",
    "Wavetable",
    "Headphone",
// Rear
    "Analog Rear",
    "HD Analog Rear",
    "Rear",
// Surround/Side
    "Analog Side",
    "HD Analog Side",
    "PCM Surround",
    "Surround",
    "PCM Side",
    "Side",
    "Wave Surround",
// Center/LFE
    "Analog Center/LFE",
    "Center",
    "CLFE",
    "HD Analog Center/LFE",
    "LFE",
    "PCM Center",
    "PCM LFE",
    "Wave Center",
    "Wave LFE",
    "Headphone Center",
    "Headphone LFE",
// I don't know if these are related to PCM playback.
#if 0
    "ADC",
    "Analog Mix",
    "Beep",
    "DSP",
    "HDMI",
    "I2S",
    "Mono",
    "Mono Output",
    "Mobile Front",
    "Node 11",
    "PC Beep",
    "PC Speaker",
    "PCM Chorus",
    "PCM Reverb",
    "SB",
    "Sigmatel 4-Speaker Stereo",
    "Surround Phase Inversion",
    "VIA DXS",
    "Video",
#endif
    NULL,
};

#define MAKE_CONTROL_NAME( name, base, suffix) \
    sprintf(( name ), "%s%s%s", ( base ), *( base ) ? " " : "", ( suffix ))

static void set_control_state( int card_id, char *name,
                               ULONG ulCh, BOOL fState )
{
    int ctl_id, values_cnt;

    ctl_id = uniaud_get_id_by_name( card_id, name, 0 );
    if( ctl_id > 0 )
    {
        uniaud_mixer_get_count_of_values( card_id, ctl_id, &values_cnt );

        if ( values_cnt == 1 ||
            ulCh == MCI_SET_AUDIO_LEFT || ulCh == MCI_SET_AUDIO_ALL )
            uniaud_mixer_put_value( card_id, ctl_id, fState, 0 );

        if ( values_cnt > 1 &&
             ( ulCh == MCI_SET_AUDIO_RIGHT || ulCh == MCI_SET_AUDIO_ALL ))
            uniaud_mixer_put_value( card_id, ctl_id, fState, 1 );
    }
}

static APIRET APIENTRY uniaudSetSoundState( HKAI hkai, ULONG ulCh, BOOL fState )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    const char **control;
    char         name[ 80 ];

    for( control = m_volumeControls; *control; control++ )
    {
        MAKE_CONTROL_NAME( name, *control, "Playback Switch");
        set_control_state( pui->pcm->card_id, name, ulCh, fState );
    }

    return KAIE_NO_ERROR;
}

static void set_perc_vol( int card_id, char *name, ULONG ulCh, USHORT usVol)
{
    int ctl_id, min, max, new_vol, values_cnt;

    ctl_id = uniaud_get_id_by_name( card_id, name, 0 );
    if( ctl_id > 0 )
    {
        uniaud_mixer_get_min_max( card_id, ctl_id, &min, &max );
        new_vol = (( float )( max - min ) / 100.0f ) * usVol + min;

        uniaud_mixer_get_count_of_values( card_id, ctl_id, &values_cnt );

        if( values_cnt == 1 ||
            ulCh == MCI_SET_AUDIO_LEFT || ulCh == MCI_SET_AUDIO_ALL )
            uniaud_mixer_put_value( card_id, ctl_id, new_vol, 0 );

        if( values_cnt > 1 &&
            ( ulCh == MCI_SET_AUDIO_RIGHT || ulCh == MCI_SET_AUDIO_ALL ))
            uniaud_mixer_put_value( card_id, ctl_id, new_vol, 1 );
    }
}

static APIRET APIENTRY uniaudSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;

    const char **control;
    char         name[ 80 ];

    for( control = m_volumeControls; *control; control++ )
    {
        MAKE_CONTROL_NAME( name, *control, "Playback Volume");
        set_perc_vol( pui->pcm->card_id, name, ulCh, usVol );
    }

    return KAIE_NO_ERROR;
}

static APIRET APIENTRY uniaudGetVolume( HKAI hkai, ULONG ulCh )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;
    int         ctl_id, min, max, vol, left_vol, right_vol, values_cnt;

    const char **control;
    char         name[ 80 ];

    ctl_id = -1;    /* Suppress uninitialized warning */
    for( control = m_volumeControls; *control; control++ )
    {
        MAKE_CONTROL_NAME( name, *control, "Playback Volume");
        ctl_id = uniaud_get_id_by_name( pui->pcm->card_id, name, 0 );
        if( ctl_id > 0 )
            break;
    }

    /* Ooops. Could not found a volume control */
    if(!*control )
        return 0;

    uniaud_mixer_get_min_max( pui->pcm->card_id, ctl_id, &min, &max );
    left_vol = uniaud_mixer_get_value( pui->pcm->card_id, ctl_id, 0 );
    uniaud_mixer_get_count_of_values( pui->pcm->card_id, ctl_id, &values_cnt );
    right_vol = values_cnt > 1 ?
                uniaud_mixer_get_value( pui->pcm->card_id, ctl_id, 1 ) :
                left_vol;

    if( ulCh == MCI_STATUS_AUDIO_LEFT )
        vol = left_vol;
    else if( ulCh == MCI_STATUS_AUDIO_RIGHT )
        vol = right_vol;
    else /* if( ulCh == MCI_STATUS_AUDIO_ALL ) */
        vol = ( left_vol + right_vol ) / 2;

    return (vol - min) * ( 100.0f / ( max - min ));
}

static APIRET APIENTRY uniaudStatus( HKAI hkai )
{
    PUNIAUDINFO pui = ( PUNIAUDINFO )hkai;
    ULONG       ulStatus = 0;

    if( pui->fPlaying )
        ulStatus |= KAIS_PLAYING;

    if( pui->fPaused )
        ulStatus |= KAIS_PAUSED;

    if( pui->fCompleted )
        ulStatus |= KAIS_COMPLETED;

    return ulStatus;
}
