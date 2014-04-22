#ifndef SOUND_H
#define SOUND_H

#include "sound_blargg.h"

#define SGCNT0_H 0x82
#define FIFOA_L 0xa0
#define FIFOA_H 0xa2
#define FIFOB_L 0xa4
#define FIFOB_H 0xa6

#define BLIP_BUFFER_ACCURACY 16
#define BLIP_PHASE_BITS 8
#define BLIP_WIDEST_IMPULSE_ 16
#define BLIP_BUFFER_EXTRA_ 18
#define BLIP_RES 256
#define BLIP_RES_MIN_ONE 255
#define BLIP_SAMPLE_BITS 30
#define BLIP_READER_DEFAULT_BASS 9
#define BLIP_DEFAULT_LENGTH 250		/* 1/4th of a second */

#define BUFS_SIZE 3
#define STEREO 2

#define	CLK_MUL	GB_APU_OVERCLOCK
#define CLK_MUL_MUL_2 8
#define CLK_MUL_MUL_4 16
#define CLK_MUL_MUL_6 24
#define CLK_MUL_MUL_8 32
#define CLK_MUL_MUL_15 60
#define CLK_MUL_MUL_32 128
#define DAC_BIAS 7

#define PERIOD_MASK 0x70
#define SHIFT_MASK 0x07

#define PERIOD2_MASK 0x1FFFF

#define BANK40_MASK 0x40
#define BANK_SIZE 32
#define BANK_SIZE_MIN_ONE 31
#define BANK_SIZE_DIV_TWO 16

/* 11-bit frequency in NRx3 and NRx4*/
#define GB_OSC_FREQUENCY() (((regs[4] & 7) << 8) + regs[3])

#define	WAVE_TYPE	0x100
#define NOISE_TYPE	0x200
#define MIXED_TYPE	WAVE_TYPE | NOISE_TYPE
#define TYPE_INDEX_MASK	0xFF

struct blip_buffer_statenew_t
{
        uint32_t offset_;
        int32_t reader_accum_;
        int32_t buf [BLIP_BUFFER_EXTRA_];
};


/* Begins reading from buffer. Name should be unique to the current block.*/
#define BLIP_READER_BEGIN( name, blip_buffer ) \
        const int32_t * name##_reader_buf = (blip_buffer).buffer_;\
        int32_t name##_reader_accum = (blip_buffer).reader_accum_

/* Advances to next sample*/
#define BLIP_READER_NEXT( name, bass ) \
        (void) (name##_reader_accum += *name##_reader_buf++ - (name##_reader_accum >> (bass)))

/* Ends reading samples from buffer. The number of samples read must now be removed
   using Blip_Buffer_Gba::remove_samples(). */
#define BLIP_READER_END( name, blip_buffer ) \
        (void) ((blip_buffer).reader_accum_ = name##_reader_accum)

#define BLIP_READER_ADJ_( name, offset ) (name##_reader_buf += offset)

#define BLIP_READER_NEXT_IDX_( name, idx ) {\
        name##_reader_accum -= name##_reader_accum >> BLIP_READER_DEFAULT_BASS;\
        name##_reader_accum += name##_reader_buf [(idx)];\
}

#define BLIP_READER_NEXT_RAW_IDX_( name, idx ) {\
        name##_reader_accum -= name##_reader_accum >> BLIP_READER_DEFAULT_BASS; \
        name##_reader_accum += *(int32_t const*) ((char const*) name##_reader_buf + (idx)); \
}

#if defined (_M_IX86) || defined (_M_IA64) || defined (__i486__) || \
                defined (__x86_64__) || defined (__ia64__) || defined (__i386__)
        #define BLIP_CLAMP_( in ) in < -0x8000 || 0x7FFF < in
#else
        #define BLIP_CLAMP_( in ) (int16_t) in != in
#endif

/* Clamp sample to int16_t range */
#define BLIP_CLAMP( sample, out ) { if ( BLIP_CLAMP_( (sample) ) ) (out) = ((sample) >> 24) ^ 0x7FFF; }
#define GB_ENV_DAC_ENABLED() (regs[2] & 0xF8)	/* Non-zero if DAC is enabled*/
#define GB_NOISE_PERIOD2_INDEX()	(regs[3] >> 4)
#define GB_NOISE_PERIOD2(base)		(base << GB_NOISE_PERIOD2_INDEX())
#define GB_NOISE_LFSR_MASK()		((regs[3] & 0x08) ? ~0x4040 : ~0x4000)
#define GB_WAVE_DAC_ENABLED() (regs[0] & 0x80)	/* Non-zero if DAC is enabled*/

#define reload_sweep_timer() \
        sweep_delay = (regs [0] & PERIOD_MASK) >> 4; \
        if ( !sweep_delay ) \
                sweep_delay = 8;


#define NR10 0x60
#define NR11 0x62
#define NR12 0x63
#define NR13 0x64
#define NR14 0x65
#define NR21 0x68
#define NR22 0x69
#define NR23 0x6c
#define NR24 0x6d
#define NR30 0x70
#define NR31 0x72
#define NR32 0x73
#define NR33 0x74
#define NR34 0x75
#define NR41 0x78
#define NR42 0x79
#define NR43 0x7c
#define NR44 0x7d
#define NR50 0x80
#define NR51 0x81
#define NR52 0x84

/* 1/100th of a second */
#define SOUND_CLOCK_TICKS_ 167772
#define SOUNDVOLUME 0.5f
#define SOUNDVOLUME_ -1
static float soundVolume     = 1.0f;

/*============================================================
	CLASS DECLS
============================================================ */

class Blip_Buffer_Gba
{
	public:
		const char * set_sample_rate( long samples_per_sec, int msec_length = 1000 / 4 );
		void clear( void);
		void save_state( blip_buffer_statenew_t* out );
		void load_state( blip_buffer_statenew_t const& in );
		uint32_t clock_rate_factor( long clock_rate ) const;
		long clock_rate_;
		int length_;		/* Length of buffer in milliseconds*/
		long sample_rate_;	/* Current output sample rate*/
		uint32_t factor_;
		uint32_t offset_;
		int32_t * buffer_;
		int32_t buffer_size_;
		int32_t reader_accum_;
		Blip_Buffer_Gba();
		~Blip_Buffer_Gba();
	private:
		Blip_Buffer_Gba( const Blip_Buffer_Gba& );
		Blip_Buffer_Gba& operator = ( const Blip_Buffer_Gba& );
};

class Blip_Synth
{
	public:
	Blip_Buffer_Gba* buf;
	int delta_factor;

	Blip_Synth();

	void volume( double v ) { delta_factor = int ((v * 1.0) * (1L << BLIP_SAMPLE_BITS) + 0.5); }
	void offset( int32_t, int delta, Blip_Buffer_Gba* ) const;
	void offset_resampled( uint32_t, int delta, Blip_Buffer_Gba* ) const;
	void offset_inline( int32_t t, int delta, Blip_Buffer_Gba* buf ) const {
		offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
	}
	void offset_inline( int32_t t, int delta ) const {
		offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
	}
};

class Gba_Osc
{
	public:
	Blip_Buffer_Gba* outputs [4];	/* NULL, right, left, center*/
	Blip_Buffer_Gba* output;		/* where to output sound*/
	uint8_t * regs;			/* osc's 5 registers*/
	int mode;			/* mode_dmg, mode_cgb, mode_agb*/
	int dac_off_amp;		/* amplitude when DAC is off*/
	int last_amp;			/* current amplitude in Blip_Buffer_Gba*/
	Blip_Synth const* good_synth;
	Blip_Synth  const* med_synth;

	int delay;			/* clocks until frequency timer expires*/
	int length_ctr;			/* length counter*/
	unsigned phase;			/* waveform phase (or equivalent)*/
	bool enabled;			/* internal enabled flag*/

	void clock_length();
	void reset();
	protected:
	void update_amp( int32_t, int new_amp );
	int write_trig( int frame_phase, int max_len, int old_data );
};

class Gba_Env : public Gba_Osc
{
	public:
	int  env_delay;
	int  volume;
	bool env_enabled;

	void clock_envelope();
	bool write_register( int frame_phase, int reg, int old_data, int data );

	void reset()
	{
		env_delay = 0;
		volume    = 0;
		Gba_Osc::reset();
	}
	private:
	void zombie_volume( int old, int data );
	int reload_env_timer();
};

class Gba_Square : public Gba_Env
{
	public:
	bool write_register( int frame_phase, int reg, int old_data, int data );
	void run( int32_t, int32_t );

	void reset()
	{
		Gba_Env::reset();
		delay = 0x40000000; /* TODO: something less hacky (never clocked until first trigger)*/
	}
	private:
	/* Frequency timer period*/
	int period() const { return (2048 - GB_OSC_FREQUENCY()) * (CLK_MUL_MUL_4); }
};

class Gba_Sweep_Square : public Gba_Square
{
	public:
	int  sweep_freq;
	int  sweep_delay;
	bool sweep_enabled;
	bool sweep_neg;

	void clock_sweep();
	void write_register( int frame_phase, int reg, int old_data, int data );

	void reset()
	{
		sweep_freq    = 0;
		sweep_delay   = 0;
		sweep_enabled = false;
		sweep_neg     = false;
		Gba_Square::reset();
	}
	private:
	void calc_sweep( bool update );
};

class Gba_Noise : public Gba_Env
{
	public:
	int divider; /* noise has more complex frequency divider setup*/

	void run( int32_t, int32_t );
	void write_register( int frame_phase, int reg, int old_data, int data );

	void reset()
	{
		divider = 0;
		Gba_Env::reset();
		delay = CLK_MUL_MUL_4; /* TODO: remove?*/
	}
};

class Gba_Wave : public Gba_Osc
{
	public:
	int sample_buf;		/* last wave RAM byte read (hardware has this as well)*/
	int agb_mask;		/* 0xFF if AGB features enabled, 0 otherwise*/
	uint8_t* wave_ram;	/* 32 bytes (64 nybbles), stored in APU*/

	void write_register( int frame_phase, int reg, int old_data, int data );
	void run( int32_t, int32_t );

	/* Reads/writes wave RAM*/
	int read( unsigned addr ) const;
	void write( unsigned addr, int data );

	void reset()
	{
		sample_buf = 0;
		Gba_Osc::reset();
	}

	private:
	friend class Gb_Apu;

	/* Frequency timer period*/
	int period() const { return (2048 - GB_OSC_FREQUENCY()) * (CLK_MUL_MUL_2); }

	void corrupt_wave();

	/* Wave index that would be accessed, or -1 if no access would occur*/
	int access( unsigned addr ) const;
};

/*============================================================
	INLINE CLASS FUNCS
============================================================ */

INLINE void Blip_Synth::offset_resampled( uint32_t time, int delta, Blip_Buffer_Gba* blip_buf ) const
{
	int32_t left, right, phase;
	int32_t *buf;

	delta *= delta_factor;
	buf = blip_buf->buffer_ + (time >> BLIP_BUFFER_ACCURACY);
	phase = (int) (time >> (BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS) & BLIP_RES_MIN_ONE);

	left = buf [0] + delta;

	right = (delta >> BLIP_PHASE_BITS) * phase;

	left  -= right;
	right += buf [1];

	buf [0] = left;
	buf [1] = right;
}

INLINE void Blip_Synth::offset( int32_t t, int delta, Blip_Buffer_Gba* buf ) const
{
   offset_resampled( t * buf->factor_ + buf->offset_, delta, buf );
}

INLINE int Gba_Wave::read( unsigned addr ) const
{
	int index;

	if(enabled)
		index = access( addr );
	else
		index = addr & 0x0F;

	unsigned char const * wave_bank = &wave_ram[(~regs[0] & BANK40_MASK) >> 2 & agb_mask];

	return (index < 0 ? 0xFF : wave_bank[index]);
}

INLINE void Gba_Wave::write( unsigned addr, int data )
{
	int index;

	if(enabled)
		index = access( addr );
	else
		index = addr & 0x0F;

	unsigned char * wave_bank = &wave_ram[(~regs[0] & BANK40_MASK) >> 2 & agb_mask];

	if ( index >= 0 )
		wave_bank[index] = data;;
}



void soundSetVolume( float );

// Manages muting bitmask. The bits control the following channels:
// 0x001 Pulse 1
// 0x002 Pulse 2
// 0x004 Wave
// 0x008 Noise
// 0x100 PCM 1
// 0x200 PCM 2
void soundPause (void);
void soundResume (void);
void soundSetSampleRate(long sampleRate);
void soundReset (void);
void soundEvent_u8( uint32_t addr, uint8_t  data );
void soundEvent_u8_parallel(int gb_addr[], uint32_t address[], uint8_t data[]);
void soundEvent_u16( uint32_t addr, uint16_t data );
void soundTimerOverflow( int which );
void process_sound_tick_fn (void);
void soundSaveGameMem(uint8_t *& data);
void soundReadGameMem(const uint8_t *& data, int version);
void soundSetEnable(int channels);
int  soundGetEnable();
void soundSetVolume( float volume );
float soundGetVolume();

extern int SOUND_CLOCK_TICKS;   // Number of 16.8 MHz clocks between calls to soundTick()
extern int soundTicks;          // Number of 16.8 MHz clocks until soundTick() will be called

#endif // SOUND_H
