#ifndef __UNIAUD__
#define __UNIAUD__

#include "unidef.h"

struct ReSampleContext;

typedef struct ReSampleContext ReSampleContext;

/*
 * Mixer API
 */

typedef struct uniaud_pcm {
    ULONG uni_handle; // handle, returned from UNIAUD32
    ULONG uni_file;   // handle, returned from DosOpen
    int card_id;      // card ID
    card_type_ids card_type;    // card type
    int type;         // type of PCM. 0 - read, 1 - write
    int instance;
    int state;        // state of stream
    int channels;     // number of channels
    int multi_channels;     // number of channels when multichannel
    int rate;         // sample rate
    int format;       // format of sample
    int bufsize;      // buffer size in driver
    int bufptr;       // current buffer position
    int period_size;  // period size in byte
    int periods;      // number of periods per buffer
    char *multi_buf;  // buffer for multichannel
    struct uniaud_pcm *pcm_rear; // pointer to rear channels pcm for multichannel
    struct uniaud_pcm *pcm_center_lfe; // pointer to center&lfe channels pcm for multichannel
    ReSampleContext *resample;
    char *resample_buf;
    int orig_channels; // original number of channels
    int orig_rate; // original sample rate
    int orig_format; // original sample rate
    int resample_size; // total latest resample size in bytes
    int resample_written; // written bytes from resample buffer to card
} uniaud_pcm;

static INLINE snd_interval_t *hw_param_interval(snd_pcm_hw_params_t *params,
					     snd_pcm_hw_param_t var)
{
	return &params->intervals[var - SNDRV_PCM_HW_PARAM_FIRST_INTERVAL];
}

/*
 * Get current power state for given card
 * if <= 0 - error
 */
int _System uniaud_mixer_get_power_state(int card_id, ULONG *state);

/*
 * Set power state for given card
 * if <= 0 - error
 */
int _System uniaud_mixer_set_power_state(int card_id, ULONG *state);

/*
 * Get number of controls for given card
 * if <= 0 - error
 */
int _System uniaud_mixer_get_ctls_number(int card_id);

/*
 * Get controls list for given card
 * if NULL - error
 */
UniaudControl * _System uniaud_mixer_get_ctl_list(int card_id);

/*
 * Get control ID by given name
 * if < 0 - error
 */
int _System uniaud_get_id_by_name(int card_id, char *name, int index);

/*
 * Get control info for given card and control
 * if < 0 - error
 */
int _System uniaud_mixer_get_ctl_info(int card_id, ULONG list_id, UniaudControlInfo *ctl_info);

/*
 * Get current value for given control
 * if < 0 - error
 */
int _System uniaud_mixer_get_ctl_val(int card_id, ULONG list_id, UniaudControlValue *ctl_val);

/*
 * Set new value for given control
 * if < 0 - error
 */
int _System uniaud_mixer_put_ctl_val(int card_id, ULONG list_id, UniaudControlValue *ctl_val);

/*
 * Wait for any control change for given card
 * if <= 0 - error, otherwise - changed control number
 * if timeout expired, returns -ETIME (see. errno.h)
 */
int _System uniaud_mixer_wait(int card_id, int timeout);

/*
 * Get range for given control ID
 * checks min/max parameters for NULL
 * if < 0 - error
 */
int _System uniaud_mixer_get_min_max(int card_id, ULONG ctl_id, int *min, int*max);

/*
 * Get count of values for given control ID
 * checks values_cnt parameters for NULL
 * if < 0 - error
 */
int _System uniaud_mixer_get_count_of_values(int card_id, ULONG ctl_id, int *values_cnt);

/*
 * Simplified version of set new value for given control.
 * Useful for simple types of data (integers, integer64, boolean, enum)
 * if < 0 - error
 */
int _System uniaud_mixer_put_value(int card_id, ULONG put_id, int put_val, int put_cnt);

/*
 * Simplified version of get value for given control.
 * Useful for simple types of data (integers, integer64, boolean, enum)
 * if < 0 - error
 */
int _System uniaud_mixer_get_value(int card_id, ULONG put_id, int get_cnt);

/*
 * Simplified version of set new value for given control.
 * Useful for simple types of data (integers, integer64, boolean, enum)
 * Control is referenses by name
 * if < 0 - error
 */
int _System uniaud_mixer_put_value_by_name(int card_id, char *name, int put_val, int put_cnt, int index);

/*
 * Simplified version of get value for given control.
 * Useful for simple types of data (integers, integer64, boolean, enum)
 * Control is referenses by name
 * if < 0 - error
 */
int _System uniaud_mixer_get_value_by_name(int card_id, char *name, int get_cnt, int index);

/*
 * Set status bytes for SPDIF
 * Control is referenses by name
 * if < 0 - error
 */
int _System uniaud_mixer_put_spdif_status(int card_id, char *name, int aes0, int aes1, int aes2, int aes3);


/*
 * PCM API
 */

/*
 * Return number of PCM instances for given card
 * PCM instance is an abstraction layer of an card
 * Each card may have one or more PCM instance
 * Each PCM instance may have different properties
 * Each PCM instance can have one or more PLAY or/and RECORD
 * substreams
 * Each PCM instance allowed open so many times as many substreams
 * it have
 */
int _System uniaud_get_pcm_instances(int card_id);
/*
 *
 */
int _System uniaud_pcm_get_caps(int card_id, POSS32_DEVCAPS pcaps);
/*
 *
 */
int _System uniaud_pcm_find_pcm_for_chan(POSS32_DEVCAPS pcaps, int pcms, int type, int channels);
/*
 *
 */
int _System uniaud_pcm_set_pcm(int pcm);
/*
 *
 */
int _System uniaud_pcm_find_max_chan(POSS32_DEVCAPS pcaps, int pcms, int type);

int _System uniaud_get_max_channels(int card_id);

/**
 * \brief Return bytes needed to store a quantity of PCM sample
 * \param format Sample format
 * \param samples Samples count
 * \return bytes needed, a negative error code if not integer or unknown
 */
ssize_t _System uniaud_pcm_format_size(snd_pcm_format_t format, size_t samples);

/*
 * Open an PCM.
 * card_id - logical ID of card from 0 to 7
 * type    - PCM type: 0 - PLAY, 1 - RECORD
 * pcm_instance - instance of PCM
 * access_flag - exclusive OPEN or not
 * sample_rate - sample rate
 * channels - number of wanted channels
 * pcm_format - see snd_pcm_format_t
 * on successful open returns 0 and pointer to uniaud_pcm structure
 */
int _System uniaud_pcm_open(int card_id, int type, int pcm_instance, int access_flag,
                    int sample_rate, int channels, int pcm_format,
                    uniaud_pcm **pcm);
/*
 *
 */
int _System uniaud_pcm_close(uniaud_pcm *pcm);

/*
 *
 */
int _System uniaud_pcm_write(uniaud_pcm *pcm, char* buffer, int size);

/*
 *
 */
int _System uniaud_pcm_read(uniaud_pcm *pcm, char* buffer, int size);

/*
 *
 */
int _System uniaud_pcm_prepare(uniaud_pcm *pcm);
/*
 *
 */
int _System uniaud_pcm_resume(uniaud_pcm *pcm);
/*
 *
 */
int _System uniaud_pcm_status(uniaud_pcm *pcm, snd_pcm_status_t *status);
/*
 *
 */
int _System uniaud_pcm_state(uniaud_pcm *pcm);
/*
 *
 */
int _System uniaud_pcm_wait(uniaud_pcm *pcm, int timeout);
/*
 *
 */
int _System uniaud_pcm_pause(uniaud_pcm *pcm);
/*
 *
 */
int _System uniaud_pcm_start(uniaud_pcm *pcm);
/*
 *
 */
int _System uniaud_pcm_drop(uniaud_pcm *pcm);

/*
 *
 */
int _System uniaud_close_all_pcms(int unlock);

/*
 * Common API
 */

/*
 *
 */
int _System uniaud_get_cards(void);
/*
 *
 */
int _System uniaud_get_card_info(int card_id, UniaudCardInfo *info);
/*
 *
 */
int _System uniaud_get_version(void);

void _System _snd_pcm_hw_params_any(snd_pcm_hw_params_t *params);

/**
 * \brief Fill params with a full configuration space for a PCM
 * \param pcm PCM handle
 * \param params Configuration space
 */
int _System snd_pcm_hw_params_any(uniaud_pcm *pcm, snd_pcm_hw_params_t *params);

int _System _snd_pcm_hw_param_set(snd_pcm_hw_params_t *params,
			  snd_pcm_hw_param_t var, unsigned int val, int dir);

int _System _uniaud_pcm_refine_hw_params(uniaud_pcm *pcm, snd_pcm_hw_params_t *pparams);
int _System _uniaud_pcm_set_hw_params(uniaud_pcm *pcm, snd_pcm_hw_params_t *pparams);
int _System _uniaud_pcm_set_sw_params(uniaud_pcm *pcm, snd_pcm_sw_params_t *pparams);

#endif //__UNIAUD__
