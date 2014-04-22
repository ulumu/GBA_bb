/*============================================================
	BLIP BUFFER
============================================================ */

/* Blip_Buffer_Gba 0.4.1. http://www.slack.net/~ant */

#define FIXED_SHIFT 12
#define SAL_FIXED_SHIFT 4096
#define TO_FIXED( f )   int ((f) * SAL_FIXED_SHIFT)
#define FROM_FIXED( f ) ((f) >> FIXED_SHIFT)

Blip_Buffer_Gba::Blip_Buffer_Gba()
{
   factor_       = INT_MAX;
   buffer_       = 0;
   buffer_size_  = 0;
   sample_rate_  = 0;
   clock_rate_   = 0;
   length_       = 0;

   clear();
}

Blip_Buffer_Gba::~Blip_Buffer_Gba()
{
   free( buffer_ );
}

void Blip_Buffer_Gba::clear( void)
{
   offset_       = 0;
   reader_accum_ = 0;

   if ( buffer_ )
      memset( buffer_, 0, (buffer_size_ + BLIP_BUFFER_EXTRA_) * sizeof (int32_t) );
}

const char * Blip_Buffer_Gba::set_sample_rate( long new_rate, int msec )
{
   /* start with maximum length that resampled time can represent*/
   long new_size = (ULONG_MAX >> BLIP_BUFFER_ACCURACY) - BLIP_BUFFER_EXTRA_ - 64;

   if ( msec != 0)
   {
      long s = (new_rate * (msec + 1) + 999) / 1000;
      if ( s < new_size )
	      new_size = s;
   }

   if ( buffer_size_ != new_size )
   {
      void* p = realloc( buffer_, (new_size + BLIP_BUFFER_EXTRA_) * sizeof *buffer_ );
      if ( !p )
         return "Out of memory";
      buffer_ = (int32_t *) p;
   }

   buffer_size_ = new_size;

   /* update things based on the sample rate*/
   sample_rate_ = new_rate;
   length_ = new_size * 1000 / new_rate - 1;

   /* update these since they depend on sample rate*/
   if ( clock_rate_ )
      factor_ = clock_rate_factor( clock_rate_);

   clear();

   return 0;
}

/* Sets number of source time units per second */

uint32_t Blip_Buffer_Gba::clock_rate_factor( long rate ) const
{
   double ratio = (double) sample_rate_ / rate;
   int32_t factor = (int32_t) floor( ratio * (1L << BLIP_BUFFER_ACCURACY) + 0.5 );
   return (uint32_t) factor;
}

void Blip_Buffer_Gba::save_state( blip_buffer_statenew_t* out )
{
   out->offset_       = offset_;
   out->reader_accum_ = reader_accum_;
   memcpy( out->buf, &buffer_ [offset_ >> BLIP_BUFFER_ACCURACY], sizeof out->buf );
}

void Blip_Buffer_Gba::load_state( blip_buffer_statenew_t const& in )
{
   clear();

   offset_       = in.offset_;
   reader_accum_ = in.reader_accum_;
   memcpy( buffer_, in.buf, sizeof(in.buf));
}

/*============================================================
	BLIP SYNTH
============================================================ */

Blip_Synth::Blip_Synth()
{
   buf          = 0;
   delta_factor = 0;
}
