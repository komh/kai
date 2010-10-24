#ifndef __UNIDEF__
#define __UNIDEF__

#include <sys/time.h>
#include <sys/types.h>
#include "errno.h"

#define TARGET_OS2

#define __BYTE_ORDER  __LITTLE_ENDIAN
#define SNDRV_LITTLE_ENDIAN

/*
 * errors definitions
 */

#define UNIAUD_NO_ERROR                  0
#define UNIAUD_ALSA32OPEN_ERROR          10001
#define UNIAUD_INVALID_PARAMETER         10002
#define UNIAUD_ERROR_IN_DRIVER           10003
#define UNIAUD_MEMORY_ERROR              10004
#define UNIAUD_INVALID_CARD              10005
#define UNIAUD_ERROR_INVALID_PCM_PARAMS  10006
#define UNIAUD_PCM_OPEN_ERROR            10007
#define UNIAUD_PCM_WRITE_ERROR           10008
#define UNIAUD_NOT_FOUND                 10009
#define UNIAUD_MIXER_ERROR               10010
#define UNIAUD_ERROR_RESAMPLE_INIT       10011

#define SIZE_DEVICE_NAME         32

#define MAX_CARDS                8

#define MAX_CHANNELS             8

#define PCM_TYPE_WRITE           0
#define PCM_TYPE_READ            1

typedef struct sndrv_ctl_card_info {
	int card;			/* card number */
	int pad;			/* reserved for future (was type) */
	unsigned char id[16];		/* ID of card (user selectable) */
	unsigned char driver[16];	/* Driver name */
	unsigned char name[32];		/* Short name of soundcard */
	unsigned char longname[80];	/* name + info text about soundcard */
	unsigned char reserved_[16];	/* reserved for future (was ID of mixer) */
	unsigned char mixername[80];	/* visual mixer identification */
	unsigned char components[80];	/* card components / fine identification, delimited with one space (AC97 etc..) */
	unsigned char reserved[48];	/* reserved for future */
} UniaudCardInfo;

enum sndrv_ctl_elem_iface {
        SNDRV_CTL_ELEM_IFACE_CARD = 0,          /* global control */
        SNDRV_CTL_ELEM_IFACE_HWDEP,             /* hardware dependent device */
        SNDRV_CTL_ELEM_IFACE_MIXER,             /* virtual mixer device */
        SNDRV_CTL_ELEM_IFACE_PCM,               /* PCM device */
        SNDRV_CTL_ELEM_IFACE_RAWMIDI,           /* RawMidi device */
        SNDRV_CTL_ELEM_IFACE_TIMER,             /* timer device */
        SNDRV_CTL_ELEM_IFACE_SEQUENCER,         /* sequencer client */
        SNDRV_CTL_ELEM_IFACE_LAST = SNDRV_CTL_ELEM_IFACE_SEQUENCER,
};

typedef struct sndrv_ctl_elem_id {
        unsigned int numid;             /* numeric identifier, zero = invalid */
        enum sndrv_ctl_elem_iface iface; /* interface identifier */
        unsigned int device;            /* device/client number */
        unsigned int subdevice;         /* subdevice (substream) number */
        unsigned char name[44];         /* ASCII name of item */
        unsigned int index;             /* index of item */
} UniaudControl;

enum sndrv_ctl_elem_type {
	SNDRV_CTL_ELEM_TYPE_NONE = 0,		/* invalid */
	SNDRV_CTL_ELEM_TYPE_BOOLEAN,		/* boolean type */
	SNDRV_CTL_ELEM_TYPE_INTEGER,		/* integer type */
	SNDRV_CTL_ELEM_TYPE_ENUMERATED,		/* enumerated type */
	SNDRV_CTL_ELEM_TYPE_BYTES,		/* byte array */
	SNDRV_CTL_ELEM_TYPE_IEC958,		/* IEC958 (S/PDIF) setup */
	SNDRV_CTL_ELEM_TYPE_INTEGER64,		/* 64-bit integer type */
	SNDRV_CTL_ELEM_TYPE_LAST = SNDRV_CTL_ELEM_TYPE_INTEGER64,
};

typedef struct sndrv_ctl_elem_info {
	UniaudControl id;	/* W: element ID */
	enum sndrv_ctl_elem_type type;	/* R: value type - SNDRV_CTL_ELEM_TYPE_* */
	unsigned int access;		/* R: value access (bitmask) - SNDRV_CTL_ELEM_ACCESS_* */
	unsigned int count;		/* count of values */
	int owner;			/* owner's PID of this control */
	union {
		struct {
			long min;		/* R: minimum value */
			long max;		/* R: maximum value */
			long step;		/* R: step (0 variable) */
		} integer;
#ifndef NO_LONGLONG
		struct {
			long long min;		/* R: minimum value */
			long long max;		/* R: maximum value */
			long long step;		/* R: step (0 variable) */
		} integer64;
#endif
		struct {
			unsigned int items;	/* R: number of items */
			unsigned int item;	/* W: item number */
			char name[64];		/* R: value name */
		} enumerated;
		unsigned char reserved[128];
	} value;
	union {
		unsigned short d[4];		/* dimensions */
		unsigned short *d_ptr;		/* indirect */
	} dimen;
	unsigned char reserved[64-4*sizeof(unsigned short)];
} UniaudControlInfo;

struct sndrv_aes_iec958 {
        unsigned char status[24];       /* AES/IEC958 channel status bits */
        unsigned char subcode[147];     /* AES/IEC958 subcode bits */
        unsigned char pad;              /* nothing */
        unsigned char dig_subframe[4];  /* AES/IEC958 subframe bits */
};

struct timespec1 {
        long    tv_sec;         /* seconds */
        long    tv_usec;        /* and microseconds */
};

typedef struct sndrv_ctl_elem_value {
	UniaudControl id;	/* W: element ID */
	unsigned int indirect: 1;	/* W: use indirect pointer (xxx_ptr member) */
        union {
		union {
			long value[128];
			long *value_ptr;
		} integer;
#ifndef NO_LONGLONG
		union {
			long long value[64];
			long long *value_ptr;
		} integer64;
#endif
		union {
			unsigned int item[128];
			unsigned int *item_ptr;
		} enumerated;
		union {
			unsigned char data[512];
			unsigned char *data_ptr;
		} bytes;
		struct sndrv_aes_iec958 iec958;
        } value;                /* RO */
	struct timespec1 tstamp;
        unsigned char reserved[128-sizeof(struct timespec1)];
} UniaudControlValue;

typedef struct {
    ULONG  nrStreams;            //nr of activate wave streams supported
    ULONG  ulMinChannels;        //min nr of channels
    ULONG  ulMaxChannels;        //max nr of channels
    ULONG  ulChanFlags;          //channel flags
    ULONG  ulMinRate;            //min sample rate
    ULONG  ulMaxRate;            //max sample rate
    ULONG  ulRateFlags;          //sample rate flags
    ULONG  ulDataFormats;        //supported wave formats
} WAVE_CAPS, *PWAVE_CAPS, FAR *LPWAVE_CAPS;

typedef struct {
    ULONG     nrDevices;                   //total nr of audio devices
    ULONG     ulCaps; 	   	           //device caps
    char      szDeviceName[SIZE_DEVICE_NAME];
    char      szMixerName[SIZE_DEVICE_NAME];
    WAVE_CAPS waveOutCaps;
    WAVE_CAPS waveInCaps;
} OSS32_DEVCAPS, *POSS32_DEVCAPS, FAR *LPOSS32_DEVCAPS;


typedef struct sndrv_pcm_hw_params snd_pcm_hw_params_t;
typedef struct sndrv_pcm_sw_params snd_pcm_sw_params_t;
typedef struct sndrv_mask snd_mask_t;
typedef struct sndrv_interval snd_interval_t;
/** Unsigned frames quantity */
typedef unsigned long snd_pcm_uframes_t;
typedef long snd_pcm_sframes_t;
typedef struct sndrv_pcm_status snd_pcm_status_t;


enum sndrv_pcm_stream {
	SNDRV_PCM_STREAM_PLAYBACK = 0,
	SNDRV_PCM_STREAM_CAPTURE,
	SNDRV_PCM_STREAM_LAST = SNDRV_PCM_STREAM_CAPTURE,
};

enum sndrv_pcm_access {
	SNDRV_PCM_ACCESS_MMAP_INTERLEAVED = 0,	/* interleaved mmap */
	SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED, 	/* noninterleaved mmap */
	SNDRV_PCM_ACCESS_MMAP_COMPLEX,		/* complex mmap */
	SNDRV_PCM_ACCESS_RW_INTERLEAVED,	/* readi/writei */
	SNDRV_PCM_ACCESS_RW_NONINTERLEAVED,	/* readn/writen */
	SNDRV_PCM_ACCESS_LAST = SNDRV_PCM_ACCESS_RW_NONINTERLEAVED,
};

enum sndrv_pcm_format {
	SNDRV_PCM_FORMAT_S8 = 0,
	SNDRV_PCM_FORMAT_U8,
	SNDRV_PCM_FORMAT_S16_LE,
	SNDRV_PCM_FORMAT_S16_BE,
	SNDRV_PCM_FORMAT_U16_LE,
	SNDRV_PCM_FORMAT_U16_BE,
	SNDRV_PCM_FORMAT_S24_LE,	/* low three bytes */
	SNDRV_PCM_FORMAT_S24_BE,	/* low three bytes */
	SNDRV_PCM_FORMAT_U24_LE,	/* low three bytes */
	SNDRV_PCM_FORMAT_U24_BE,	/* low three bytes */
	SNDRV_PCM_FORMAT_S32_LE,
	SNDRV_PCM_FORMAT_S32_BE,
	SNDRV_PCM_FORMAT_U32_LE,
	SNDRV_PCM_FORMAT_U32_BE,
	SNDRV_PCM_FORMAT_FLOAT_LE,	/* 4-byte float, IEEE-754 32-bit, range -1.0 to 1.0 */
	SNDRV_PCM_FORMAT_FLOAT_BE,	/* 4-byte float, IEEE-754 32-bit, range -1.0 to 1.0 */
	SNDRV_PCM_FORMAT_FLOAT64_LE,	/* 8-byte float, IEEE-754 64-bit, range -1.0 to 1.0 */
	SNDRV_PCM_FORMAT_FLOAT64_BE,	/* 8-byte float, IEEE-754 64-bit, range -1.0 to 1.0 */
	SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE,	/* IEC-958 subframe, Little Endian */
	SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE,	/* IEC-958 subframe, Big Endian */
	SNDRV_PCM_FORMAT_MU_LAW,
	SNDRV_PCM_FORMAT_A_LAW,
	SNDRV_PCM_FORMAT_IMA_ADPCM,
	SNDRV_PCM_FORMAT_MPEG,
	SNDRV_PCM_FORMAT_GSM,
	SNDRV_PCM_FORMAT_SPECIAL = 31,
	SNDRV_PCM_FORMAT_S24_3LE = 32,	/* in three bytes */
	SNDRV_PCM_FORMAT_S24_3BE,	/* in three bytes */
	SNDRV_PCM_FORMAT_U24_3LE,	/* in three bytes */
	SNDRV_PCM_FORMAT_U24_3BE,	/* in three bytes */
	SNDRV_PCM_FORMAT_S20_3LE,	/* in three bytes */
	SNDRV_PCM_FORMAT_S20_3BE,	/* in three bytes */
	SNDRV_PCM_FORMAT_U20_3LE,	/* in three bytes */
	SNDRV_PCM_FORMAT_U20_3BE,	/* in three bytes */
	SNDRV_PCM_FORMAT_S18_3LE,	/* in three bytes */
	SNDRV_PCM_FORMAT_S18_3BE,	/* in three bytes */
	SNDRV_PCM_FORMAT_U18_3LE,	/* in three bytes */
	SNDRV_PCM_FORMAT_U18_3BE,	/* in three bytes */
	SNDRV_PCM_FORMAT_LAST = SNDRV_PCM_FORMAT_U18_3BE,

#ifdef SNDRV_LITTLE_ENDIAN
	SNDRV_PCM_FORMAT_S16 = SNDRV_PCM_FORMAT_S16_LE,
	SNDRV_PCM_FORMAT_U16 = SNDRV_PCM_FORMAT_U16_LE,
	SNDRV_PCM_FORMAT_S24 = SNDRV_PCM_FORMAT_S24_LE,
	SNDRV_PCM_FORMAT_U24 = SNDRV_PCM_FORMAT_U24_LE,
	SNDRV_PCM_FORMAT_S32 = SNDRV_PCM_FORMAT_S32_LE,
	SNDRV_PCM_FORMAT_U32 = SNDRV_PCM_FORMAT_U32_LE,
	SNDRV_PCM_FORMAT_FLOAT = SNDRV_PCM_FORMAT_FLOAT_LE,
	SNDRV_PCM_FORMAT_FLOAT64 = SNDRV_PCM_FORMAT_FLOAT64_LE,
	SNDRV_PCM_FORMAT_IEC958_SUBFRAME = SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE,
#endif
#ifdef SNDRV_BIG_ENDIAN
	SNDRV_PCM_FORMAT_S16 = SNDRV_PCM_FORMAT_S16_BE,
	SNDRV_PCM_FORMAT_U16 = SNDRV_PCM_FORMAT_U16_BE,
	SNDRV_PCM_FORMAT_S24 = SNDRV_PCM_FORMAT_S24_BE,
	SNDRV_PCM_FORMAT_U24 = SNDRV_PCM_FORMAT_U24_BE,
	SNDRV_PCM_FORMAT_S32 = SNDRV_PCM_FORMAT_S32_BE,
	SNDRV_PCM_FORMAT_U32 = SNDRV_PCM_FORMAT_U32_BE,
	SNDRV_PCM_FORMAT_FLOAT = SNDRV_PCM_FORMAT_FLOAT_BE,
	SNDRV_PCM_FORMAT_FLOAT64 = SNDRV_PCM_FORMAT_FLOAT64_BE,
	SNDRV_PCM_FORMAT_IEC958_SUBFRAME = SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE,
#endif
};

enum sndrv_pcm_subformat {
	SNDRV_PCM_SUBFORMAT_STD = 0,
	SNDRV_PCM_SUBFORMAT_LAST = SNDRV_PCM_SUBFORMAT_STD,
};

#define SNDRV_PCM_INFO_MMAP		0x00000001	/* hardware supports mmap */
#define SNDRV_PCM_INFO_MMAP_VALID	0x00000002	/* period data are valid during transfer */
#define SNDRV_PCM_INFO_DOUBLE		0x00000004	/* Double buffering needed for PCM start/stop */
#define SNDRV_PCM_INFO_BATCH		0x00000010	/* double buffering */
#define SNDRV_PCM_INFO_INTERLEAVED	0x00000100	/* channels are interleaved */
#define SNDRV_PCM_INFO_NONINTERLEAVED	0x00000200	/* channels are not interleaved */
#define SNDRV_PCM_INFO_COMPLEX		0x00000400	/* complex frame organization (mmap only) */
#define SNDRV_PCM_INFO_BLOCK_TRANSFER	0x00010000	/* hardware transfer block of samples */
#define SNDRV_PCM_INFO_OVERRANGE	0x00020000	/* hardware supports ADC (capture) overrange detection */
#define SNDRV_PCM_INFO_RESUME		0x00040000	/* hardware supports stream resume after suspend */
#define SNDRV_PCM_INFO_PAUSE		0x00080000	/* pause ioctl is supported */
#define SNDRV_PCM_INFO_HALF_DUPLEX	0x00100000	/* only half duplex */
#define SNDRV_PCM_INFO_JOINT_DUPLEX	0x00200000	/* playback and capture stream are somewhat correlated */
#define SNDRV_PCM_INFO_SYNC_START	0x00400000	/* pcm support some kind of sync go */
#define SNDRV_PCM_INFO_NONATOMIC_OPS	0x00800000	/* non-atomic prepare callback */

enum sndrv_pcm_state {
	SNDRV_PCM_STATE_OPEN = 0,		/* stream is open */
	SNDRV_PCM_STATE_SETUP,		/* stream has a setup */
	SNDRV_PCM_STATE_PREPARED,	/* stream is ready to start */
	SNDRV_PCM_STATE_RUNNING,	/* stream is running */
	SNDRV_PCM_STATE_XRUN,		/* stream reached an xrun */
	SNDRV_PCM_STATE_DRAINING,	/* stream is draining */
        SNDRV_PCM_STATE_PAUSED,		/* stream is paused */
        SNDRV_PCM_STATE_SUSPENDED,      /* hardware is suspended */
        SNDRV_PCM_STATE_DISCONNECTED,   /* hardware is disconnected */
        SNDRV_PCM_STATE_LAST = SNDRV_PCM_STATE_DISCONNECTED,
};

/* for further details see the ACPI and PCI power management specification */
#define SNDRV_CTL_POWER_D0              0x0000  /* full On */
#define SNDRV_CTL_POWER_D1              0x0100  /* partial On */
#define SNDRV_CTL_POWER_D2              0x0200  /* partial On */
#define SNDRV_CTL_POWER_D3              0x0300  /* Off */
#define SNDRV_CTL_POWER_D3hot           (SNDRV_CTL_POWER_D3|0x0000)     /* Off,with power */
#define SNDRV_CTL_POWER_D3cold          (SNDRV_CTL_POWER_D3|0x0001)     /* Off,without power */

#define SNDRV_MASK_BITS 64      /* we use so far 64bits only */
#define SNDRV_MASK_SIZE (SNDRV_MASK_BITS / 32)
#define SND_MASK_MAX 64

#define MASK_INLINE static inline

#define MASK_MAX SND_MASK_MAX
#define MASK_SIZE (MASK_MAX / 32)

#define INTERVAL_INLINE static inline

#define MASK_OFS(i)     ((i) >> 5)
#define MASK_BIT(i)     (1U << ((i) & 31))

#define SND_PCM_HW_PARAM_ACCESS SNDRV_PCM_HW_PARAM_ACCESS
#define SND_PCM_HW_PARAM_FIRST_MASK SNDRV_PCM_HW_PARAM_FIRST_MASK
#define SND_PCM_HW_PARAM_FORMAT SNDRV_PCM_HW_PARAM_FORMAT
#define SND_PCM_HW_PARAM_SUBFORMAT SNDRV_PCM_HW_PARAM_SUBFORMAT
#define SND_PCM_HW_PARAM_LAST_MASK SNDRV_PCM_HW_PARAM_LAST_MASK
#define SND_PCM_HW_PARAM_SAMPLE_BITS SNDRV_PCM_HW_PARAM_SAMPLE_BITS
#define SND_PCM_HW_PARAM_FIRST_INTERVAL SNDRV_PCM_HW_PARAM_FIRST_INTERVAL
#define SND_PCM_HW_PARAM_FRAME_BITS SNDRV_PCM_HW_PARAM_FRAME_BITS
#define SND_PCM_HW_PARAM_CHANNELS SNDRV_PCM_HW_PARAM_CHANNELS
#define SND_PCM_HW_PARAM_RATE SNDRV_PCM_HW_PARAM_RATE
#define SND_PCM_HW_PARAM_PERIOD_TIME SNDRV_PCM_HW_PARAM_PERIOD_TIME
#define SND_PCM_HW_PARAM_PERIOD_SIZE SNDRV_PCM_HW_PARAM_PERIOD_SIZE
#define SND_PCM_HW_PARAM_PERIOD_BYTES SNDRV_PCM_HW_PARAM_PERIOD_BYTES
#define SND_PCM_HW_PARAM_PERIODS SNDRV_PCM_HW_PARAM_PERIODS
#define SND_PCM_HW_PARAM_BUFFER_TIME SNDRV_PCM_HW_PARAM_BUFFER_TIME
#define SND_PCM_HW_PARAM_BUFFER_SIZE SNDRV_PCM_HW_PARAM_BUFFER_SIZE
#define SND_PCM_HW_PARAM_BUFFER_BYTES SNDRV_PCM_HW_PARAM_BUFFER_BYTES
#define SND_PCM_HW_PARAM_TICK_TIME SNDRV_PCM_HW_PARAM_TICK_TIME
#define SND_PCM_HW_PARAM_LAST_INTERVAL SNDRV_PCM_HW_PARAM_LAST_INTERVAL
#define SND_PCM_HW_PARAMS_RUNTIME SNDRV_PCM_HW_PARAMS_RUNTIME
#define SND_PCM_HW_PARAM_LAST_MASK SNDRV_PCM_HW_PARAM_LAST_MASK
#define SND_PCM_HW_PARAM_FIRST_MASK SNDRV_PCM_HW_PARAM_FIRST_MASK
#define SND_PCM_HW_PARAM_LAST_INTERVAL SNDRV_PCM_HW_PARAM_LAST_INTERVAL
#define SND_PCM_HW_PARAM_FIRST_INTERVAL SNDRV_PCM_HW_PARAM_FIRST_INTERVAL

typedef unsigned long sndrv_pcm_uframes_t;
typedef long sndrv_pcm_sframes_t;

/** PCM sample format */
typedef enum _snd_pcm_format {
	/** Unknown */
	SND_PCM_FORMAT_UNKNOWN = -1,
	/** Signed 8 bit */
	SND_PCM_FORMAT_S8 = 0,
	/** Unsigned 8 bit */
	SND_PCM_FORMAT_U8,
	/** Signed 16 bit Little Endian */
	SND_PCM_FORMAT_S16_LE,
	/** Signed 16 bit Big Endian */
	SND_PCM_FORMAT_S16_BE,
	/** Unsigned 16 bit Little Endian */
	SND_PCM_FORMAT_U16_LE,
	/** Unsigned 16 bit Big Endian */
	SND_PCM_FORMAT_U16_BE,
	/** Signed 24 bit Little Endian */
	SND_PCM_FORMAT_S24_LE,
	/** Signed 24 bit Big Endian */
	SND_PCM_FORMAT_S24_BE,
	/** Unsigned 24 bit Little Endian */
	SND_PCM_FORMAT_U24_LE,
	/** Unsigned 24 bit Big Endian */
	SND_PCM_FORMAT_U24_BE,
	/** Signed 32 bit Little Endian */
	SND_PCM_FORMAT_S32_LE,
	/** Signed 32 bit Big Endian */
	SND_PCM_FORMAT_S32_BE,
	/** Unsigned 32 bit Little Endian */
	SND_PCM_FORMAT_U32_LE,
	/** Unsigned 32 bit Big Endian */
	SND_PCM_FORMAT_U32_BE,
	/** Float 32 bit Little Endian, Range -1.0 to 1.0 */
	SND_PCM_FORMAT_FLOAT_LE,
	/** Float 32 bit Big Endian, Range -1.0 to 1.0 */
	SND_PCM_FORMAT_FLOAT_BE,
	/** Float 64 bit Little Endian, Range -1.0 to 1.0 */
	SND_PCM_FORMAT_FLOAT64_LE,
	/** Float 64 bit Big Endian, Range -1.0 to 1.0 */
	SND_PCM_FORMAT_FLOAT64_BE,
	/** IEC-958 Little Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME_LE,
	/** IEC-958 Big Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME_BE,
	/** Mu-Law */
	SND_PCM_FORMAT_MU_LAW,
	/** A-Law */
	SND_PCM_FORMAT_A_LAW,
	/** Ima-ADPCM */
	SND_PCM_FORMAT_IMA_ADPCM,
	/** MPEG */
	SND_PCM_FORMAT_MPEG,
	/** GSM */
	SND_PCM_FORMAT_GSM,
	/** Special */
	SND_PCM_FORMAT_SPECIAL = 31,
	/** Signed 24bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_S24_3LE = 32,
	/** Signed 24bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_S24_3BE,
	/** Unsigned 24bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_U24_3LE,
	/** Unsigned 24bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_U24_3BE,
	/** Signed 20bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_S20_3LE,
	/** Signed 20bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_S20_3BE,
	/** Unsigned 20bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_U20_3LE,
	/** Unsigned 20bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_U20_3BE,
	/** Signed 18bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_S18_3LE,
	/** Signed 18bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_S18_3BE,
	/** Unsigned 18bit Little Endian in 3bytes format */
	SND_PCM_FORMAT_U18_3LE,
	/** Unsigned 18bit Big Endian in 3bytes format */
	SND_PCM_FORMAT_U18_3BE,
	SND_PCM_FORMAT_LAST = SND_PCM_FORMAT_U18_3BE,

#if __BYTE_ORDER == __LITTLE_ENDIAN
	/** Signed 16 bit CPU endian */
	SND_PCM_FORMAT_S16 = SND_PCM_FORMAT_S16_LE,
	/** Unsigned 16 bit CPU endian */
	SND_PCM_FORMAT_U16 = SND_PCM_FORMAT_U16_LE,
	/** Signed 24 bit CPU endian */
	SND_PCM_FORMAT_S24 = SND_PCM_FORMAT_S24_LE,
	/** Unsigned 24 bit CPU endian */
	SND_PCM_FORMAT_U24 = SND_PCM_FORMAT_U24_LE,
	/** Signed 32 bit CPU endian */
	SND_PCM_FORMAT_S32 = SND_PCM_FORMAT_S32_LE,
	/** Unsigned 32 bit CPU endian */
	SND_PCM_FORMAT_U32 = SND_PCM_FORMAT_U32_LE,
	/** Float 32 bit CPU endian */
	SND_PCM_FORMAT_FLOAT = SND_PCM_FORMAT_FLOAT_LE,
	/** Float 64 bit CPU endian */
	SND_PCM_FORMAT_FLOAT64 = SND_PCM_FORMAT_FLOAT64_LE,
	/** IEC-958 CPU Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME = SND_PCM_FORMAT_IEC958_SUBFRAME_LE
#elif __BYTE_ORDER == __BIG_ENDIAN
	/** Signed 16 bit CPU endian */
	SND_PCM_FORMAT_S16 = SND_PCM_FORMAT_S16_BE,
	/** Unsigned 16 bit CPU endian */
	SND_PCM_FORMAT_U16 = SND_PCM_FORMAT_U16_BE,
	/** Signed 24 bit CPU endian */
	SND_PCM_FORMAT_S24 = SND_PCM_FORMAT_S24_BE,
	/** Unsigned 24 bit CPU endian */
	SND_PCM_FORMAT_U24 = SND_PCM_FORMAT_U24_BE,
	/** Signed 32 bit CPU endian */
	SND_PCM_FORMAT_S32 = SND_PCM_FORMAT_S32_BE,
	/** Unsigned 32 bit CPU endian */
	SND_PCM_FORMAT_U32 = SND_PCM_FORMAT_U32_BE,
	/** Float 32 bit CPU endian */
	SND_PCM_FORMAT_FLOAT = SND_PCM_FORMAT_FLOAT_BE,
	/** Float 64 bit CPU endian */
	SND_PCM_FORMAT_FLOAT64 = SND_PCM_FORMAT_FLOAT64_BE,
	/** IEC-958 CPU Endian */
	SND_PCM_FORMAT_IEC958_SUBFRAME = SND_PCM_FORMAT_IEC958_SUBFRAME_BE
#else
#error "Unknown endian"
#endif
} snd_pcm_format_t;

/** PCM state */
typedef enum _snd_pcm_state {
	/** Open */
	SND_PCM_STATE_OPEN = 0,
	/** Setup installed */
	SND_PCM_STATE_SETUP,
	/** Ready to start */
	SND_PCM_STATE_PREPARED,
	/** Running */
	SND_PCM_STATE_RUNNING,
	/** Stopped: underrun (playback) or overrun (capture) detected */
	SND_PCM_STATE_XRUN,
	/** Draining: running (playback) or stopped (capture) */
	SND_PCM_STATE_DRAINING,
	/** Paused */
	SND_PCM_STATE_PAUSED,
	/** Hardware is suspended */
	SND_PCM_STATE_SUSPENDED,
	/** Hardware is disconnected */
	SND_PCM_STATE_DISCONNECTED,
	SND_PCM_STATE_LAST = SND_PCM_STATE_DISCONNECTED
} snd_pcm_state_t;

enum sndrv_pcm_hw_param {
	SNDRV_PCM_HW_PARAM_ACCESS = 0,	/* Access type */
	SNDRV_PCM_HW_PARAM_FIRST_MASK = SNDRV_PCM_HW_PARAM_ACCESS,
#ifdef TARGET_OS2
        SNDRV_PCM_HW_PARAM_RATE_MASK,
#endif
        SNDRV_PCM_HW_PARAM_FORMAT,	/* Format */
	SNDRV_PCM_HW_PARAM_SUBFORMAT,	/* Subformat */
	SNDRV_PCM_HW_PARAM_LAST_MASK = SNDRV_PCM_HW_PARAM_SUBFORMAT,

	SNDRV_PCM_HW_PARAM_SAMPLE_BITS = 8,	/* Bits per sample */
	SNDRV_PCM_HW_PARAM_FIRST_INTERVAL = SNDRV_PCM_HW_PARAM_SAMPLE_BITS,
	SNDRV_PCM_HW_PARAM_FRAME_BITS,	/* Bits per frame */
	SNDRV_PCM_HW_PARAM_CHANNELS,	/* Channels */
	SNDRV_PCM_HW_PARAM_RATE,	/* Approx rate */
	SNDRV_PCM_HW_PARAM_PERIOD_TIME,	/* Approx distance between interrupts
					   in us */
	SNDRV_PCM_HW_PARAM_PERIOD_SIZE,	/* Approx frames between interrupts */
	SNDRV_PCM_HW_PARAM_PERIOD_BYTES, /* Approx bytes between interrupts */
	SNDRV_PCM_HW_PARAM_PERIODS,	/* Approx interrupts per buffer */
	SNDRV_PCM_HW_PARAM_BUFFER_TIME,	/* Approx duration of buffer in us */
	SNDRV_PCM_HW_PARAM_BUFFER_SIZE,	/* Size of buffer in frames */
	SNDRV_PCM_HW_PARAM_BUFFER_BYTES, /* Size of buffer in bytes */
	SNDRV_PCM_HW_PARAM_TICK_TIME,	/* Approx tick duration in us */
	SNDRV_PCM_HW_PARAM_LAST_INTERVAL = SNDRV_PCM_HW_PARAM_TICK_TIME
};

typedef enum sndrv_pcm_hw_param snd_pcm_hw_param_t;

#define SNDRV_PCM_HW_PARAMS_RUNTIME		(1<<0)

struct sndrv_interval {
	unsigned int min, max;
	unsigned int openmin:1,
		     openmax:1,
		     integer:1,
		     empty:1;
};

#define SNDRV_MASK_MAX  256

struct sndrv_mask {
        unsigned long int bits[(SNDRV_MASK_MAX+31)/32];
};

struct sndrv_pcm_hw_params {
	unsigned int flags;
        struct sndrv_mask masks[SNDRV_PCM_HW_PARAM_LAST_MASK -
                               SNDRV_PCM_HW_PARAM_FIRST_MASK + 1];
        struct sndrv_mask mres[5];      /* reserved masks */
	struct sndrv_interval intervals[SNDRV_PCM_HW_PARAM_LAST_INTERVAL -
                                        SNDRV_PCM_HW_PARAM_FIRST_INTERVAL + 1];
        struct sndrv_interval ires[9];  /* reserved intervals */
	unsigned int rmask;		/* W: requested masks */
	unsigned int cmask;		/* R: changed masks */
	unsigned int info;		/* R: Info flags for returned setup */
	unsigned int msbits;		/* R: used most significant bits */
	unsigned int rate_num;		/* R: rate numerator */
	unsigned int rate_den;		/* R: rate denominator */
	sndrv_pcm_uframes_t fifo_size;	/* R: chip FIFO size in frames */
	unsigned char reserved[64];	/* reserved for future */
};

enum sndrv_pcm_tstamp {
	SNDRV_PCM_TSTAMP_NONE = 0,
	SNDRV_PCM_TSTAMP_MMAP,
	SNDRV_PCM_TSTAMP_LAST = SNDRV_PCM_TSTAMP_MMAP,
};

struct sndrv_pcm_sw_params {
	enum sndrv_pcm_tstamp tstamp_mode;	/* timestamp mode */
	unsigned int period_step;
	unsigned int sleep_min;			/* min ticks to sleep */
	sndrv_pcm_uframes_t avail_min;		/* min avail frames for wakeup */
	sndrv_pcm_uframes_t xfer_align;		/* xfer size need to be a multiple */
	sndrv_pcm_uframes_t start_threshold;	/* min hw_avail frames for automatic start */
	sndrv_pcm_uframes_t stop_threshold;	/* min avail frames for automatic stop */
	sndrv_pcm_uframes_t silence_threshold;	/* min distance from noise for silence filling */
	sndrv_pcm_uframes_t silence_size;	/* silence block size */
	sndrv_pcm_uframes_t boundary;		/* pointers wrap point */
	unsigned char reserved[64];		/* reserved for future */
};

struct sndrv_pcm_status {
	enum sndrv_pcm_state state;	/* stream state */
	struct timespec trigger_tstamp;	/* time when stream was started/stopped/paused */
	struct timespec tstamp;		/* reference timestamp */
        sndrv_pcm_uframes_t appl_ptr;	/* appl ptr */
	sndrv_pcm_uframes_t hw_ptr;	/* hw ptr */
	sndrv_pcm_sframes_t delay;	/* current delay in frames */
	sndrv_pcm_uframes_t avail;	/* number of frames available */
	sndrv_pcm_uframes_t avail_max;	/* max frames available on hw since last status */
        sndrv_pcm_uframes_t overrange;	/* count of ADC (capture) overrange detections from last status */
        enum sndrv_pcm_state suspended_state; /* suspended stream state */
	unsigned char reserved[60];	/* must be filled with zero */
};

enum sndrv_pcm_class {
	SNDRV_PCM_CLASS_GENERIC = 0,	/* standard mono or stereo device */
	SNDRV_PCM_CLASS_MULTI,		/* multichannel device */
	SNDRV_PCM_CLASS_MODEM,		/* software modem class */
	SNDRV_PCM_CLASS_DIGITIZER,	/* digitizer class */
	/* Don't forget to change the following: */
	SNDRV_PCM_CLASS_LAST = SNDRV_PCM_CLASS_DIGITIZER,
};

enum sndrv_pcm_subclass {
	SNDRV_PCM_SUBCLASS_GENERIC_MIX = 0,	/* mono or stereo subdevices are mixed together */
	SNDRV_PCM_SUBCLASS_MULTI_MIX,	/* multichannel subdevices are mixed together */
	/* Don't forget to change the following: */
	SNDRV_PCM_SUBCLASS_LAST = SNDRV_PCM_SUBCLASS_MULTI_MIX,
};

union sndrv_pcm_sync_id {
	unsigned char id[16];
	unsigned short id16[8];
	unsigned int id32[4];
};


struct sndrv_pcm_info {
	unsigned int device;		/* RO/WR (control): device number */
	unsigned int subdevice;		/* RO/WR (control): subdevice number */
	enum sndrv_pcm_stream stream;	/* RO/WR (control): stream number */
	int card;			/* R: card number */
	unsigned char id[64];		/* ID (user selectable) */
	unsigned char name[80];		/* name of this device */
	unsigned char subname[32];	/* subdevice name */
	enum sndrv_pcm_class dev_class;	/* SNDRV_PCM_CLASS_* */
	enum sndrv_pcm_subclass dev_subclass; /* SNDRV_PCM_SUBCLASS_* */
	unsigned int subdevices_count;
	unsigned int subdevices_avail;
	union sndrv_pcm_sync_id sync;	/* hardware synchronization ID */
	unsigned char reserved[64];	/* reserved for future... */
};

/****************************************************************************
 *                                                                          *
 *        Digital audio interface					    *
 *                                                                          *
 ****************************************************************************/

/* AES/IEC958 channel status bits */
#define IEC958_AES0_PROFESSIONAL	(1<<0)	/* 0 = consumer, 1 = professional */
#define IEC958_AES0_NONAUDIO		(1<<1)	/* 0 = audio, 1 = non-audio */
#define IEC958_AES0_PRO_EMPHASIS	(7<<2)	/* mask - emphasis */
#define IEC958_AES0_PRO_EMPHASIS_NOTID	(0<<2)	/* emphasis not indicated */
#define IEC958_AES0_PRO_EMPHASIS_NONE	(1<<2)	/* none emphasis */
#define IEC958_AES0_PRO_EMPHASIS_5015	(3<<2)	/* 50/15us emphasis */
#define IEC958_AES0_PRO_EMPHASIS_CCITT	(7<<2)	/* CCITT J.17 emphasis */
#define IEC958_AES0_PRO_FREQ_UNLOCKED	(1<<5)	/* source sample frequency: 0 = locked, 1 = unlocked */
#define IEC958_AES0_PRO_FS		(3<<6)	/* mask - sample frequency */
#define IEC958_AES0_PRO_FS_NOTID	(0<<6)	/* fs not indicated */
#define IEC958_AES0_PRO_FS_44100	(1<<6)	/* 44.1kHz */
#define IEC958_AES0_PRO_FS_48000	(2<<6)	/* 48kHz */
#define IEC958_AES0_PRO_FS_32000	(3<<6)	/* 32kHz */
#define IEC958_AES0_CON_NOT_COPYRIGHT	(1<<2)	/* 0 = copyright, 1 = not copyright */
#define IEC958_AES0_CON_EMPHASIS	(7<<3)	/* mask - emphasis */
#define IEC958_AES0_CON_EMPHASIS_NONE	(0<<3)	/* none emphasis */
#define IEC958_AES0_CON_EMPHASIS_5015	(1<<3)	/* 50/15us emphasis */
#define IEC958_AES0_CON_MODE		(3<<6)	/* mask - mode */
#define IEC958_AES1_PRO_MODE		(15<<0)	/* mask - channel mode */
#define IEC958_AES1_PRO_MODE_NOTID	(0<<0)	/* not indicated */
#define IEC958_AES1_PRO_MODE_STEREOPHONIC (2<<0) /* stereophonic - ch A is left */
#define IEC958_AES1_PRO_MODE_SINGLE	(4<<0)	/* single channel */
#define IEC958_AES1_PRO_MODE_TWO	(8<<0)	/* two channels */
#define IEC958_AES1_PRO_MODE_PRIMARY	(12<<0)	/* primary/secondary */
#define IEC958_AES1_PRO_MODE_BYTE3	(15<<0)	/* vector to byte 3 */
#define IEC958_AES1_PRO_USERBITS	(15<<4)	/* mask - user bits */
#define IEC958_AES1_PRO_USERBITS_NOTID	(0<<4)	/* not indicated */
#define IEC958_AES1_PRO_USERBITS_192	(8<<4)	/* 192-bit structure */
#define IEC958_AES1_PRO_USERBITS_UDEF	(12<<4)	/* user defined application */
#define IEC958_AES1_CON_CATEGORY	0x7f
#define IEC958_AES1_CON_GENERAL		0x00
#define IEC958_AES1_CON_EXPERIMENTAL	0x40
#define IEC958_AES1_CON_SOLIDMEM_MASK	0x0f
#define IEC958_AES1_CON_SOLIDMEM_ID	0x08
#define IEC958_AES1_CON_BROADCAST1_MASK 0x07
#define IEC958_AES1_CON_BROADCAST1_ID	0x04
#define IEC958_AES1_CON_DIGDIGCONV_MASK 0x07
#define IEC958_AES1_CON_DIGDIGCONV_ID	0x02
#define IEC958_AES1_CON_ADC_COPYRIGHT_MASK 0x1f
#define IEC958_AES1_CON_ADC_COPYRIGHT_ID 0x06
#define IEC958_AES1_CON_ADC_MASK	0x1f
#define IEC958_AES1_CON_ADC_ID		0x16
#define IEC958_AES1_CON_BROADCAST2_MASK 0x0f
#define IEC958_AES1_CON_BROADCAST2_ID	0x0e
#define IEC958_AES1_CON_LASEROPT_MASK	0x07
#define IEC958_AES1_CON_LASEROPT_ID	0x01
#define IEC958_AES1_CON_MUSICAL_MASK	0x07
#define IEC958_AES1_CON_MUSICAL_ID	0x05
#define IEC958_AES1_CON_MAGNETIC_MASK	0x07
#define IEC958_AES1_CON_MAGNETIC_ID	0x03
#define IEC958_AES1_CON_IEC908_CD	(IEC958_AES1_CON_LASEROPT_ID|0x00)
#define IEC958_AES1_CON_NON_IEC908_CD	(IEC958_AES1_CON_LASEROPT_ID|0x08)
#define IEC958_AES1_CON_PCM_CODER	(IEC958_AES1_CON_DIGDIGCONV_ID|0x00)
#define IEC958_AES1_CON_SAMPLER		(IEC958_AES1_CON_DIGDIGCONV_ID|0x20)
#define IEC958_AES1_CON_MIXER		(IEC958_AES1_CON_DIGDIGCONV_ID|0x10)
#define IEC958_AES1_CON_RATE_CONVERTER	(IEC958_AES1_CON_DIGDIGCONV_ID|0x18)
#define IEC958_AES1_CON_SYNTHESIZER	(IEC958_AES1_CON_MUSICAL_ID|0x00)
#define IEC958_AES1_CON_MICROPHONE	(IEC958_AES1_CON_MUSICAL_ID|0x08)
#define IEC958_AES1_CON_DAT		(IEC958_AES1_CON_MAGNETIC_ID|0x00)
#define IEC958_AES1_CON_VCR		(IEC958_AES1_CON_MAGNETIC_ID|0x08)
#define IEC958_AES1_CON_ORIGINAL	(1<<7)	/* this bits depends on the category code */
#define IEC958_AES2_PRO_SBITS		(7<<0)	/* mask - sample bits */
#define IEC958_AES2_PRO_SBITS_20	(2<<0)	/* 20-bit - coordination */
#define IEC958_AES2_PRO_SBITS_24	(4<<0)	/* 24-bit - main audio */
#define IEC958_AES2_PRO_SBITS_UDEF	(6<<0)	/* user defined application */
#define IEC958_AES2_PRO_WORDLEN		(7<<3)	/* mask - source word length */
#define IEC958_AES2_PRO_WORDLEN_NOTID	(0<<3)	/* not indicated */
#define IEC958_AES2_PRO_WORDLEN_22_18	(2<<3)	/* 22-bit or 18-bit */
#define IEC958_AES2_PRO_WORDLEN_23_19	(4<<3)	/* 23-bit or 19-bit */
#define IEC958_AES2_PRO_WORDLEN_24_20	(5<<3)	/* 24-bit or 20-bit */
#define IEC958_AES2_PRO_WORDLEN_20_16	(6<<3)	/* 20-bit or 16-bit */
#define IEC958_AES2_CON_SOURCE		(15<<0)	/* mask - source number */
#define IEC958_AES2_CON_SOURCE_UNSPEC	(0<<0)	/* unspecified */
#define IEC958_AES2_CON_CHANNEL		(15<<4)	/* mask - channel number */
#define IEC958_AES2_CON_CHANNEL_UNSPEC	(0<<4)	/* unspecified */
#define IEC958_AES3_CON_FS		(15<<0)	/* mask - sample frequency */
#define IEC958_AES3_CON_FS_44100	(0<<0)	/* 44.1kHz */
#define IEC958_AES3_CON_FS_48000	(2<<0)	/* 48kHz */
#define IEC958_AES3_CON_FS_32000	(3<<0)	/* 32kHz */
#define IEC958_AES3_CON_CLOCK		(3<<4)	/* mask - clock accuracy */
#define IEC958_AES3_CON_CLOCK_1000PPM	(0<<4)	/* 1000 ppm */
#define IEC958_AES3_CON_CLOCK_50PPM	(1<<4)	/* 50 ppm */
#define IEC958_AES3_CON_CLOCK_VARIABLE	(2<<4)	/* variable pitch */

/*
 * stupid hardware vendors !!!
 * regarding to Dolby docs:
 *
 *  0       1        2      3        4            5
 * LEFT   RIGHT   CENTER   LFE   LEFT SURR   RIGHT SURR
 *
 * for other stupid morons we need to use conversion table
 * actually, I wonna to talk with their drug dealers
 *
 */

typedef struct chan_convert_table_t {
    int src[MAX_CHANNELS];
    int dst[MAX_CHANNELS];
} chan_convert_table_t;

typedef enum card_type_ids {
    UNIAUD_TYPE_ID_UNKNOWN = 0,
    UNIAUD_TYPE_ID_EMU10K1,
    UNIAUD_TYPE_ID_AUDIGY,
    UNIAUD_TYPE_ID_AUDIGY2,
    UNIAUD_TYPE_ID_CMI8738,
    UNIAUD_TYPE_ID_CMI8338,
    UNIAUD_TYPE_ID_CMI,
    UNIAUD_TYPE_ID_YMF724,
    UNIAUD_TYPE_ID_YMF724F,
    UNIAUD_TYPE_ID_YMF740,
    UNIAUD_TYPE_ID_YMF740C,
    UNIAUD_TYPE_ID_YMF744,
    UNIAUD_TYPE_ID_YMF754,
    UNIAUD_TYPE_ID_AUDIGYLS,
    UNIAUD_TYPE_ID_FM801,
    UNIAUD_TYPE_ID_NFORCE,
    UNIAUD_TYPE_ID_ICH,
    UNIAUD_TYPE_ID_ICH4,
    UNIAUD_TYPE_ID_ICH5,
    UNIAUD_TYPE_ID_VIA8233A,
    UNIAUD_TYPE_ID_VIA8237,
    UNIAUD_TYPE_ID_VIA8233
} card_type_ids;


#endif
