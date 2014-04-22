/*============================================================
	GB APU
============================================================ */

/* 0: Square 1, 1: Square 2, 2: Wave, 3: Noise */
#define OSC_COUNT 4

/* Resets hardware to initial power on state BEFORE boot ROM runs. Mode selects*/
/* sound hardware. Additional AGB wave features are enabled separately.*/
#define MODE_AGB	2

#define START_ADDR	0xFF10
#define END_ADDR	0xFF3F

/* Reads and writes must be within the START_ADDR to END_ADDR range, inclusive.*/
/* Addresses outside this range are not mapped to the sound hardware.*/
#define REGISTER_COUNT	48
#define REGS_SIZE 64

/* Clock rate that sound hardware runs at.
 * formula: 4194304 * 4 
 * */
#define CLOCK_RATE 16777216

struct gb_apu_t
{
   bool		reduce_clicks_;
   uint8_t	regs[REGS_SIZE]; // last values written to registers
   int32_t	last_time;	// time sound emulator has been run to
   int32_t	frame_time;	// time of next frame sequencer action
   int32_t	frame_period;       // clocks between each frame sequencer step
   int32_t      frame_phase;    // phase of next frame sequencer step
   double	volume_;
   Gba_Osc*	oscs [OSC_COUNT];
   Gba_Sweep_Square square1;
   Gba_Square    square2;
   Gba_Wave      wave;
   Gba_Noise     noise;
   Blip_Synth	good_synth;
   Blip_Synth	med_synth;
} gba_apu;

// Format of save state. Should be stable across versions of the library,
// with earlier versions properly opening later save states. Includes some
// room for expansion so the state size shouldn't increase.
struct gb_apu_state_t
{
   // Values stored as plain int so your code can read/write them easily.
   // Structure can NOT be written to disk, since format is not portable.
   typedef int val_t;

   enum { format0 = 0x50414247 };

   val_t format;   // format of all following data
   val_t version;  // later versions just add fields to end

   unsigned char regs [0x40];
   val_t frame_time;
   val_t frame_phase;

   val_t sweep_freq;
   val_t sweep_delay;
   val_t sweep_enabled;
   val_t sweep_neg;
   val_t noise_divider;
   val_t wave_buf;

   val_t delay      [4];
   val_t length_ctr [4];
   val_t phase      [4];
   val_t enabled    [4];

   val_t env_delay   [3];
   val_t env_volume  [3];
   val_t env_enabled [3];

   val_t unused  [13]; // for future expansion
};

#define VOL_REG 0xFF24
#define STEREO_REG 0xFF25
#define STATUS_REG 0xFF26
#define WAVE_RAM 0xFF30
#define POWER_MASK 0x80

#define OSC_COUNT 4

static void gb_apu_reduce_clicks( bool reduce )
{
   gba_apu.reduce_clicks_ = reduce;

   /* Click reduction makes DAC off generate same output as volume 0*/
   int dac_off_amp = 0;

   gba_apu.oscs[0]->dac_off_amp = dac_off_amp;
   gba_apu.oscs[1]->dac_off_amp = dac_off_amp;
   gba_apu.oscs[2]->dac_off_amp = dac_off_amp;
   gba_apu.oscs[3]->dac_off_amp = dac_off_amp;

   /* AGB always eliminates clicks on wave channel using same method*/
   gba_apu.wave.dac_off_amp = -DAC_BIAS;
}

static void gb_apu_synth_volume( int iv )
{
   double v = gba_apu.volume_ * 0.60 / OSC_COUNT / 15 /*steps*/ / 8 /*master vol range*/ * iv;
   gba_apu.good_synth.volume( v );
   gba_apu.med_synth .volume( v );
}

static void gb_apu_apply_volume (void)
{
   int data, left, right, vol_tmp;

   data  = gba_apu.regs [VOL_REG - START_ADDR];
   left  = data >> 4 & 7;
   right = data & 7;
   vol_tmp = left < right ? right : left;
   gb_apu_synth_volume( vol_tmp + 1 );
}

static void gb_apu_silence_osc( Gba_Osc& o )
{
   int delta = -o.last_amp;

   if (delta)
   {
      o.last_amp = 0;

      if (o.output)
         gba_apu.med_synth.offset( gba_apu.last_time, delta, o.output );
   }
}

static void gb_apu_run_until_( int32_t end_time )
{
	int32_t time;

	do
	{
		/* run oscillators*/
		time = end_time;
		if ( time > gba_apu.frame_time )
			time = gba_apu.frame_time;

		gba_apu.square1.run( gba_apu.last_time, time );
		gba_apu.square2.run( gba_apu.last_time, time );
		gba_apu.wave   .run( gba_apu.last_time, time );
		gba_apu.noise  .run( gba_apu.last_time, time );
		gba_apu.last_time = time;

		if ( time == end_time )
			break;

		/* run frame sequencer*/
		gba_apu.frame_time += gba_apu.frame_period * CLK_MUL;

		switch ( gba_apu.frame_phase++ )
		{
		case 2:
		case 6:
			/* 128 Hz*/
			gba_apu.square1.clock_sweep();
		case 0:
		case 4:
			/* 256 Hz*/
			gba_apu.square1.clock_length();
			gba_apu.square2.clock_length();
			gba_apu.wave   .clock_length();
			gba_apu.noise  .clock_length();
			break;
		case 7:
			/* 64 Hz*/
			gba_apu.frame_phase = 0;
			gba_apu.square1.clock_envelope();
			gba_apu.square2.clock_envelope();
			gba_apu.noise  .clock_envelope();
			break;
		}
	}while(1);
}

INLINE void Gba_Sweep_Square::write_register( int frame_phase, int reg, int old_data, int data )
{
   if ( reg == 0 && sweep_enabled && sweep_neg && !(data & 0x08) )
      enabled = false; // sweep negate disabled after used

   if ( Gba_Square::write_register( frame_phase, reg, old_data, data ) )
   {
      sweep_freq = GB_OSC_FREQUENCY();
      sweep_neg = false;
      reload_sweep_timer();
      sweep_enabled = (regs [0] & (PERIOD_MASK | SHIFT_MASK)) != 0;

      if ( regs [0] & SHIFT_MASK )
         calc_sweep( false );
   }
}

INLINE void Gba_Wave::write_register( int frame_phase, int reg, int old_data, int data )
{
	switch (reg)
	{
	case 0:
		if (!GB_WAVE_DAC_ENABLED())
			enabled = false;
		break;
	case 1:
		length_ctr = 256 - data;
		break;
	case 4:
		if (write_trig( frame_phase, 256, old_data))
		{
			if ( !GB_WAVE_DAC_ENABLED() )
				enabled = false;
			phase = 0;
			delay    = period() + CLK_MUL_MUL_6;
		}
	}
}

INLINE void Gba_Noise::write_register( int frame_phase, int reg, int old_data, int data )
{
   if ( Gba_Env::write_register( frame_phase, reg, old_data, data ) )
   {
      phase = 0x7FFF;
      delay += CLK_MUL_MUL_8;
   }
}

static void gb_apu_write_osc( int index, int reg, int old_data, int data )
{
   reg -= index * 5;

   switch ( index )
   {
      case 0:
         gba_apu.square1.write_register( gba_apu.frame_phase, reg, old_data, data );
	 break;
      case 1:
	 gba_apu.square2.write_register( gba_apu.frame_phase, reg, old_data, data );
	 break;
      case 2:
	 gba_apu.wave.write_register( gba_apu.frame_phase, reg, old_data, data );
	 break;
      case 3:
	 gba_apu.noise.write_register( gba_apu.frame_phase, reg, old_data, data );
	 break;
   }
}

static INLINE int gb_apu_calc_output( int osc )
{
   int bits = gba_apu.regs [STEREO_REG - START_ADDR] >> osc;
   return (bits >> 3 & 2) | (bits & 1);
}

static void gb_apu_write_register( int32_t time, unsigned addr, int data )
{
   int reg = addr - START_ADDR;

   if ((unsigned) reg >= REGISTER_COUNT)
      return;

   if ( addr < STATUS_REG && !(gba_apu.regs [STATUS_REG - START_ADDR] & POWER_MASK) )
      return;	/* Power is off*/

   if ( time > gba_apu.last_time )
      gb_apu_run_until_( time );

   if ( addr >= WAVE_RAM )
      gba_apu.wave.write( addr, data );
   else
   {
	   int old_data = gba_apu.regs [reg];
	   gba_apu.regs [reg] = data;

	   if ( addr < VOL_REG )
		   gb_apu_write_osc( reg / 5, reg, old_data, data );	/* Oscillator*/
	   else if ( addr == VOL_REG && data != old_data )
	   {
		   /* Master volume*/
		   for ( int i = OSC_COUNT; --i >= 0; )
			   gb_apu_silence_osc( *gba_apu.oscs [i] );

		   gb_apu_apply_volume();
	   }
	   else if ( addr == STEREO_REG )
	   {
		   /* Stereo panning*/
		   for ( int i = OSC_COUNT; --i >= 0; )
		   {
			   Gba_Osc& o = *gba_apu.oscs [i];
			   Blip_Buffer_Gba* out = o.outputs [gb_apu_calc_output( i )];
			   if ( o.output != out )
			   {
				   gb_apu_silence_osc( o );
				   o.output = out;
			   }
		   } }
	   else if ( addr == STATUS_REG && (data ^ old_data) & POWER_MASK )
	   {
		   /* Power control*/
		   gba_apu.frame_phase = 0;
		   for ( int i = OSC_COUNT; --i >= 0; )
			   gb_apu_silence_osc( *gba_apu.oscs [i] );

		   for ( int i = 0; i < 32; i++ )
			   gba_apu.regs [i] = 0;

		   gba_apu.square1.reset();
		   gba_apu.square2.reset();
		   gba_apu.wave   .reset();
		   gba_apu.noise  .reset();

		   gb_apu_apply_volume();

		   gba_apu.square1.length_ctr = 64;
		   gba_apu.square2.length_ctr = 64;
		   gba_apu.wave   .length_ctr = 256;
		   gba_apu.noise  .length_ctr = 64;

		   gba_apu.regs [STATUS_REG - START_ADDR] = data;
	   }
   }
}

static void gb_apu_reset( uint32_t mode, bool agb_wave )
{
	/* Hardware mode*/
	mode = MODE_AGB; /* using AGB wave features implies AGB hardware*/
	gba_apu.wave.agb_mask = 0xFF;
	gba_apu.oscs [0]->mode = mode;
	gba_apu.oscs [1]->mode = mode;
	gba_apu.oscs [2]->mode = mode;
	gba_apu.oscs [3]->mode = mode;
	gb_apu_reduce_clicks( gba_apu.reduce_clicks_ );

	/* Reset state*/
	gba_apu.frame_time  = 0;
	gba_apu.last_time   = 0;
	gba_apu.frame_phase = 0;

	for ( int i = 0; i < 32; i++ )
		gba_apu.regs [i] = 0;

	gba_apu.square1.reset();
	gba_apu.square2.reset();
	gba_apu.wave   .reset();
	gba_apu.noise  .reset();

	gb_apu_apply_volume();

	gba_apu.square1.length_ctr = 64;
	gba_apu.square2.length_ctr = 64;
	gba_apu.wave   .length_ctr = 256;
	gba_apu.noise  .length_ctr = 64;

	/* Load initial wave RAM*/
	static unsigned char const initial_wave [2] [16] = {
		{0x84,0x40,0x43,0xAA,0x2D,0x78,0x92,0x3C,0x60,0x59,0x59,0xB0,0x34,0xB8,0x2E,0xDA},
		{0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF},
	};
	for ( int b = 2; --b >= 0; )
	{
		/* Init both banks (does nothing if not in AGB mode)*/
		gb_apu_write_register( 0, 0xFF1A, b * 0x40 );
		for ( unsigned i = 0; i < sizeof initial_wave [0]; i++ )
			gb_apu_write_register( 0, i + WAVE_RAM, initial_wave [1] [i] );
	}
}

static void gb_apu_new(void)
{
	int i;

	gba_apu.wave.wave_ram = &gba_apu.regs [WAVE_RAM - START_ADDR];

	gba_apu.oscs [0] = &gba_apu.square1;
	gba_apu.oscs [1] = &gba_apu.square2;
	gba_apu.oscs [2] = &gba_apu.wave;
	gba_apu.oscs [3] = &gba_apu.noise;

	for ( i = OSC_COUNT; --i >= 0; )
	{
		Gba_Osc& o = *gba_apu.oscs [i];
		o.regs        = &gba_apu.regs [i * 5];
		o.output      = 0;
		o.outputs [0] = 0;
		o.outputs [1] = 0;
		o.outputs [2] = 0;
		o.outputs [3] = 0;
		o.good_synth  = &gba_apu.good_synth;
		o.med_synth   = &gba_apu.med_synth;
	}

	gba_apu.reduce_clicks_ = false;
	gba_apu.frame_period = 4194304 / 512; /* 512 Hz*/

	gba_apu.volume_ = 1.0;
	gb_apu_reset(MODE_AGB, false);
}



static void gb_apu_set_output( Blip_Buffer_Gba* center, Blip_Buffer_Gba* left,
   Blip_Buffer_Gba* right, int osc )
{
   int i = osc;

   do
   {
      Gba_Osc& o = *gba_apu.oscs [i];
      o.outputs [1] = right;
      o.outputs [2] = left;
      o.outputs [3] = center;
      o.output = o.outputs [gb_apu_calc_output( i )];
      ++i;
   }while (i < osc);
}

static void gb_apu_volume( double v )
{
   if (gba_apu.volume_ != v)
   {
      gba_apu.volume_ = v;
      gb_apu_apply_volume();
   }
}

static void gb_apu_apply_stereo (void)
{
   int i;

   for (i = OSC_COUNT; --i >= 0;)
   {
      Gba_Osc& o = *gba_apu.oscs [i];
      Blip_Buffer_Gba* out = o.outputs [gb_apu_calc_output( i )];

      if (o.output != out)
      {
         gb_apu_silence_osc( o );
	 o.output = out;
      }
   }
}

#define REFLECT( x, y ) (save ?       (io->y) = (x) :         (x) = (io->y)          )

static INLINE const char* gb_apu_save_load( gb_apu_state_t* io, bool save )
{
   int format, version;

   format = io->format0;

   REFLECT( format, format );

   if ( format != io->format0 )
      return "Unsupported sound save state format";

   version = 0;
   REFLECT( version, version );

   /* Registers and wave RAM*/
   if (save)
      memcpy( io->regs, gba_apu.regs, sizeof io->regs );
   else
      memcpy( gba_apu.regs, io->regs, sizeof(gba_apu.regs) );

   /* Frame sequencer*/
   REFLECT( gba_apu.frame_time,  frame_time  );
   REFLECT( gba_apu.frame_phase, frame_phase );

   REFLECT( gba_apu.square1.sweep_freq,    sweep_freq );
   REFLECT( gba_apu.square1.sweep_delay,   sweep_delay );
   REFLECT( gba_apu.square1.sweep_enabled, sweep_enabled );
   REFLECT( gba_apu.square1.sweep_neg,     sweep_neg );

   REFLECT( gba_apu.noise.divider,         noise_divider );
   REFLECT( gba_apu.wave.sample_buf,       wave_buf );

   return 0;
}

/* second function to avoid inline limits of some compilers*/
static INLINE void gb_apu_save_load2( gb_apu_state_t* io, bool save )
{
	int i;

	for (i = OSC_COUNT; --i >= 0;)
	{
		Gba_Osc& osc = *gba_apu.oscs [i];
		REFLECT( osc.delay,      delay      [i] );
		REFLECT( osc.length_ctr, length_ctr [i] );
		REFLECT( osc.phase,      phase      [i] );
		REFLECT( osc.enabled,    enabled    [i] );

		if (i != 2)
		{
			int j = 2 < i ? 2 : i;
			Gba_Env& env = STATIC_CAST(Gba_Env&,osc);
			REFLECT( env.env_delay,   env_delay   [j] );
			REFLECT( env.volume,      env_volume  [j] );
			REFLECT( env.env_enabled, env_enabled [j] );
		}
	}
}

static void gb_apu_save_state( gb_apu_state_t* out )
{
   (void) gb_apu_save_load( out, true );
   gb_apu_save_load2( out, true );
}

static const char * gb_apu_load_state( gb_apu_state_t const& in )
{
   RETURN_ERR( gb_apu_save_load( CONST_CAST(gb_apu_state_t*,&in), false));
   gb_apu_save_load2( CONST_CAST(gb_apu_state_t*,&in), false );

   gb_apu_apply_stereo();
   gb_apu_synth_volume( 0 );          /* suppress output for the moment*/
   gb_apu_run_until_( gba_apu.last_time );    /* get last_amp updated*/
   gb_apu_apply_volume();             /* now use correct volume*/

   return 0;
}
