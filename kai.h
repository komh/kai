/*
    K Audio Interface library for OS/2
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

/** @file kai.h
 */
#ifndef __KAI_H__
#define __KAI_H__

#include <os2.h>

/*
 * Some headers such as config.h of many projects define VERSION macro causing
 * a name clash with OS/2 multimedia extension headers
 */
#undef VERSION

#define INCL_OS2MM
#include <os2me.h>

#ifdef __cplusplus
extern "C" {
#endif

/** KAI version macro */
#define KAI_VERSION     "2.1.0"

/**
 * @defgroup kaimodes KAI modes
 * @{
 */
#define KAIM_AUTO       0
#define KAIM_DART       1
#define KAIM_UNIAUD     2
/** @} */

/**
 * @defgroup kaierrors KAI error codes
 * @{
 */
#define KAIE_NO_ERROR                  0
#define KAIE_NOT_INITIALIZED        ( -1 )
#define KAIE_ALREADY_INITIALIZED    ( -2 )
#define KAIE_INVALID_PARAMETER      ( -3 )
#define KAIE_CANNOT_LOAD_SUB_MODULE ( -4 )
#define KAIE_NOT_OPENED             ( -5 )
#define KAIE_ALREADY_OPENED         ( -6 )
#define KAIE_NOT_ENOUGH_MEMORY      ( -7 )
#define KAIE_INVALID_HANDLE         ( -8 )
#define KAIE_NOT_READY              ( -9 )
#define KAIE_STREAMS_NOT_CLOSED     ( -10 )
/** @} */

/** @defgroup kaitypes KAI types
 * @{
 */
#define KAIT_PLAY       0
#define KAIT_RECORD     1
/** @} */

/**
 * @defgroup kaistatus KAI status
 * @{
 */
#define KAIS_PLAYING    0x0001
#define KAIS_PAUSED     0x0002
#define KAIS_COMPLETED  0x0004
/** @} */

#ifndef BPS_32
/** macro for 32 bits per sample */
#define BPS_32  32
#endif

/** function pointer for a callback function */
typedef ULONG ( APIENTRY FNKAICB )( PVOID, PVOID, ULONG );
typedef FNKAICB *PFNKAICB;

#pragma pack( 1 )
/**
 * @brief KAI capability information obtaining through #kaiCaps()
 */
typedef struct tagKAICAPS
{
    ULONG   ulMode;             /**< KAI mode */
    ULONG   ulMaxChannels;      /**< maximum channels */
    CHAR    szPDDName[ 256 ];   /**< Audio device name */
} KAICAPS, *PKAICAPS;

/**
 * @brief KAI specification requested to/obtained from #kaiOpen()
 */
typedef struct tagKAISPEC
{
    USHORT      usDeviceIndex;   /**< IN : 0 = default, 1 = 1st, ... */
    ULONG       ulType;          /**< IN : support KAIT_PLAY only */
    ULONG       ulBitsPerSample; /**< IN */
    ULONG       ulSamplingRate;  /**< IN */
    ULONG       ulDataFormat;    /**< IN : ignored */
    ULONG       ulChannels;      /**< IN */
    ULONG       ulNumBuffers;    /**< IN/OUT */
    ULONG       ulBufferSize;    /**< IN/OUT */
    BOOL        fShareable;      /**< IN */
    PFNKAICB    pfnCallBack;     /**< IN */
    PVOID       pCallBackData;   /**< IN */
    BYTE        bSilence;        /**< OUT */
} KAISPEC, *PKAISPEC;
#pragma pack()

/** KAI handle */
typedef ULONG HKAI, *PHKAI;

/** KAI MIXER handle */
typedef ULONG HKAIMIXER, *PHKAIMIXER;

/** KAI MIXER STREAM handle */
typedef ULONG HKAIMIXERSTREAM, *PHKAIMIXERSTREAM;

/**
 * @brief Initialize KAI
 * @param[in] ulKaiMode KAI modes
 * @return KAIE_NO_ERROR on success, or error codes
 * @remark KAI_AUTOMODE env. var. overrides KAIM_AUTO mode to the specified
 *         mode. Available modes are DART and UNIAUD.
 */
APIRET APIENTRY kaiInit( ULONG ulKaiMode );

/**
 * @brief Terminate KAI
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiDone( VOID );

/**
 * @brief Get initialization count of KAI
 * @return Initialization count of KAI
 */
APIRET APIENTRY kaiGetInitCount( VOID );

/**
 * @brief Query KAI capabilities
 * @param[out] pkc Obtained capabilities
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiCaps( PKAICAPS pkc );

/**
 * @brief Open KAI instance
 * @param[in] pksWanted Requested specification
 * @param[out] pksObtained Obtained specification
 * @param[out] phkai Opened KAI instance
 * @return KAIE_NO_ERROR on success, or error codes
 * @remark KAI_NOSOFTVOLUME env. var. disables soft volume control. However,
 *         it is possible to enable with #kaiEnableSoftVolume() later.
 */
APIRET APIENTRY kaiOpen( const PKAISPEC pksWanted, PKAISPEC pksObtained, PHKAI phkai );

/**
 * @brief Close KAI instance
 * @param[in] hkai KAI instance
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiClose( HKAI hkai );

/**
 * @brief Start to play
 * @param[in] hkai KAI instance
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiPlay( HKAI hkai );

/**
 * @brief Stop playing
 * @param[in] hkai KAI instance
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiStop( HKAI hkai );

/**
 * @brief Pause the play
 * @param[in] hkai KAI instance
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiPause( HKAI hkai );

/**
 * @brief Resume the play
 * @param[in] hkai KAI instance
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiResume( HKAI hkai );

/**
 * @brief Set sound state of each channels
 * @param[in] hkai KAI instance
 * @param[in] ulCh Requested channel
 *            MCI_SET_AUDIO_LEFT for left channel
 *            MCI_SET_AUDIO_RIGHT for right channel
 *            MCI_SET_AUDIO_ALL for all channel
 * @param[in] fState Turn the channel on if TRUE. Turn off if FALSE, that is,
 *            mute it.
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiSetSoundState( HKAI hkai, ULONG ulCh, BOOL fState );

/**
 * @brief Set volume of each channels
 * @param[in] hkai KAI instance
 * @param[in] ulCh Requested channel
 *            MCI_SET_AUDIO_LEFT for left channel
 *            MCI_SET_AUDIO_RIGHT for right channel
 *            MCI_SET_AUDIO_ALL for all channel
 * @param[in] usVol Volume
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiSetVolume( HKAI hkai, ULONG ulCh, USHORT usVol );

/**
 * @brief Get volume of each channels
 * @param[in] hkai KAI instance
 * @param[in] ulCh Requested channel
 *            MCI_STATUS_AUDIO_LEFT for left channel
 *            MCI_STATUS_AUDIO_RIGHT for right channel
 *            MCI_STATUS_AUDIO_ALL for all channel
 * @return Volume of a requested channel in percent
 */
APIRET APIENTRY kaiGetVolume( HKAI hkai, ULONG ulCh );

/**
 * @brief Clear audio buffer
 * @param[in] hkai KAI instance
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiClearBuffer( HKAI hkai );

/**
 * @brief Query current playing status
 * @param[in] hkai KAI instance
 * @return KAIS_PLAYING if playing
 * @return KAIS_PAUSED if paused
 * @return KAIS_COMPLETED if completed
 */
APIRET APIENTRY kaiStatus( HKAI hkai );

/**
 * @brief Enable software volume control
 * @param[in] hkai KAI instance
 * @param[in] fEnable Enable flag
 *            TRUE to enable software volume control
 *            FALSE to disable soft volume control
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiEnableSoftVolume( HKAI hkai, BOOL fEnable );

/**
 * @brief Convert float samples to s16 samples
 * @param[out] dst Where to place converted s16 samples.
 * @param[in] dstLen Size of @a dst buffer in bytes
 * @param[in] src Where float samples to be converted are placed
 * @param[in] srcLen Size of @a src buffer in bytes
 * @return Converted s16 samples in bytes
 */
APIRET APIENTRY kaiFloatToS16( short *dst, int dstLen,
                               float *src, int srcLen );

/**
 * @brief Open KAIMIXER instance
 * @param[in] pksWanted Requested specification
 *            pksWanted->ulBitsPerSample should be 16
 *            pksWanted->ulChannels should be 2
 * @param[out] pksObtained Obtained specification
 * @param[out] phkm Opened KAIMIXER instance
 * @return KAIE_NO_ERROR on success, or error codes
 * @remark pfnCallBack and pCallBackData of @a pksWanted are ignored
 * @remark Only 16 bits stereo audio is supported
 */
APIRET APIENTRY kaiMixerOpen( const PKAISPEC pksWanted, PKAISPEC pksObtained,
                              PHKAIMIXER phkm );

/**
 * @brief Close KAIMIXER instance
 * @param[in] hkm KAIMIXER instance
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiMixerClose( HKAIMIXER hkm );

/**
 * @brief Open KAIMIXERSTREAM instance
 * @param[in] hkm KAIMIXER instance
 * @param[in] pksWanted Requested specification
 * @param[out] pksObtained Obtained specification
 * @param[out] phkms Opened KAIMIXERSTREAM instance
 * @return KAIE_NO_ERROR on success, or error codes
 * @remark usDeviceIndex, ulNumBuffer, ulBufferSize and fShareble of
 *         @a pksWanted are ignored
 * @remark fShareble of @a pksObtained is always TRUE
 */
APIRET APIENTRY kaiMixerStreamOpen( HKAIMIXER hkm,
                                    const PKAISPEC pksWanted,
                                    PKAISPEC pksObtained,
                                    PHKAIMIXERSTREAM phkms );

/**
 * @brief Close KAIMIXERSTREAM instance
 * @param[in] hkm KAIMIXER instance
 * @param[in] hkms KAIMIXERSTREAM instance
 * @return KAIE_NO_ERROR on success, or error codes
 */
APIRET APIENTRY kaiMixerStreamClose( HKAIMIXER hkm, HKAIMIXERSTREAM hkms );

/**
 * @brief Enable soft mixer mode.
 *        If enabled, #kaiOpen() and #kaiClose() behave like
 *        #kaiMixerStreamOpen() and #kaiMixerStreamClose(), respectively.
 * @param[in] fEnable Enable flag
 *            TRUE to enable soft mixer mode
 *            FALSE to disable soft mixer mode
 * @param[in] pks Specification for a mixer
 *            If @fEnable is TRUE and @pks is NULL, spec of a mixer does not
 *            change.
 * @return KAIE_NO_ERROR on success, or error codes
 * @remark pfnCallBack and pCallBackData of @a pks are ignored
 * @remark Default spec of a mixer is default device, 16 bits,
 *         48 KHz, stereo, 2 buffers, KAI_MINSAMPLES samples if specified,
 *         or 2048 samples, and shareable
 */
APIRET APIENTRY kaiEnableSoftMixer( BOOL fEnable, const PKAISPEC pks );

#ifdef __cplusplus
}
#endif

#endif
