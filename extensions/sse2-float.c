/* babl - dynamically extendable universal pixel conversion library.
 * Copyright (C) 2013 Massimo Valentini
 * Copyright (C) 2013 Daniel Sabo
 * Copyright (C) 2013 Loren Merritt
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#if defined(USE_SSE2)

/* SSE 2 */
#include <emmintrin.h>

#include <stdint.h>
#include <stdlib.h>

#include "babl.h"
#include "babl-cpuaccel.h"
#include "base/util.h"
#include "extensions/util.h"

#define Q(a) { a, a, a, a }

static const float BABL_ALPHA_THRESHOLD_FLOAT = (float)BABL_ALPHA_THRESHOLD;

static long
conv_rgbaF_linear_rgbAF_linear (const float *src, float *dst, long samples)
{
  long i = 0;
  long remainder;

  if (((uintptr_t)src % 16) + ((uintptr_t)dst % 16) == 0)
    {
      const long    n = (samples / 2) * 2;
      const __v4sf *s = (const __v4sf*) src;
            __v4sf *d = (__v4sf*)dst;

      for ( ; i < n; i += 2)
        {
          __v4sf rbaa0, rbaa1;
        
          __v4sf rgba0 = *s++;
          __v4sf rgba1 = *s++;

          /* Expand alpha */
          __v4sf aaaa0 = (__v4sf)_mm_shuffle_epi32((__m128i)rgba0, _MM_SHUFFLE(3, 3, 3, 3));
          __v4sf aaaa1 = (__v4sf)_mm_shuffle_epi32((__m128i)rgba1, _MM_SHUFFLE(3, 3, 3, 3));
          
          /* Premultiply */
          rgba0 = rgba0 * aaaa0;
          rgba1 = rgba1 * aaaa1;
          
          /* Shuffle the original alpha value back in */
          rbaa0 = _mm_shuffle_ps(rgba0, aaaa0, _MM_SHUFFLE(0, 0, 2, 0));
          rbaa1 = _mm_shuffle_ps(rgba1, aaaa1, _MM_SHUFFLE(0, 0, 2, 0));
          
          rgba0 = _mm_shuffle_ps(rgba0, rbaa0, _MM_SHUFFLE(2, 1, 1, 0));
          rgba1 = _mm_shuffle_ps(rgba1, rbaa1, _MM_SHUFFLE(2, 1, 1, 0));
          
          *d++ = rgba0;
          *d++ = rgba1;
        }
      _mm_empty ();
    }

  dst += i * 4;
  src += i * 4;
  remainder = samples - i;
  while (remainder--)
  {
    const float a = src[3];
    dst[0] = src[0] * a;
    dst[1] = src[1] * a;
    dst[2] = src[2] * a;
    dst[3] = a;
    
    src += 4;
    dst += 4;
  }

  return samples;
}

static long
conv_rgbAF_linear_rgbaF_linear_shuffle (const float *src, float *dst, long samples)
{
  long i = 0;
  long remainder;

  if (((uintptr_t)src % 16) + ((uintptr_t)dst % 16) == 0)
    {
      const long    n = samples;
      const __v4sf *s = (const __v4sf*) src;
            __v4sf *d = (__v4sf*)dst;

      for ( ; i < n; i += 1)
        {
          __v4sf pre_rgba0, rgba0, rbaa0, raaaa0;
          
          float alpha0 = ((float *)s)[3];
          pre_rgba0 = *s;
          
          if (alpha0 <= 0.0f)
          {
            /* Zero RGB */
            rgba0 = _mm_setzero_ps();
          }
          else
          {
            float recip0 = 1.0f/alpha0;
            
            /* Expand reciprocal */
            raaaa0 = _mm_load1_ps(&recip0);
            
            /* Un-Premultiply */
            rgba0 = pre_rgba0 * raaaa0;
          }
            
          /* Shuffle the original alpha value back in */
          rbaa0 = _mm_shuffle_ps(rgba0, pre_rgba0, _MM_SHUFFLE(3, 3, 2, 0));
          rgba0 = _mm_shuffle_ps(rgba0, rbaa0, _MM_SHUFFLE(2, 1, 1, 0));
          
          s++;
          *d++ = rgba0;
        }
      _mm_empty ();
    }

  dst += i * 4;
  src += i * 4;
  remainder = samples - i;
  while (remainder--)
    {
      float alpha = src[3];
      float recip;
      if (alpha <= 0.0f)
        recip = 0.0f;
      else
        recip = 1.0f/alpha;
      dst[0] = src[0] * recip;
      dst[1] = src[1] * recip;
      dst[2] = src[2] * recip;
      dst[3] = alpha;
      
      src   += 4;
      dst   += 4;
    }

  return samples;
}

static long
conv_rgbAF_linear_rgbaF_linear_spin (const float *src, float *dst, long samples)
{
  long i = 0;
  long remainder;

  if (((uintptr_t)src % 16) + ((uintptr_t)dst % 16) == 0)
    {
      const long    n = samples;
      const __v4sf *s = (const __v4sf*) src;
            __v4sf *d = (__v4sf*)dst;
      const __v4sf zero = _mm_setzero_ps();
      const __v4sf one = _mm_set_ss(1.0f);

      for ( ; i < n; i += 1)
        {
          __v4sf pre_abgr0, abgr0, rgba0, raaaa0;
          
          
          rgba0 = *s;
          /* Rotate to ABGR */
          pre_abgr0 = (__v4sf)_mm_shuffle_epi32((__m128i)rgba0, _MM_SHUFFLE(0, 1, 2, 3));
          
          if (_mm_ucomile_ss(pre_abgr0, zero))
          {
            /* Zero RGB */
            abgr0 = zero;
          }
          else
          {
            /* Un-Premultiply */
            raaaa0 = _mm_div_ss(one, pre_abgr0);
            
            /* Expand reciprocal */
            raaaa0 = (__v4sf)_mm_shuffle_epi32((__m128i)raaaa0, _MM_SHUFFLE(0, 0, 0, 0));
            
            /* Un-Premultiply */
            abgr0 = pre_abgr0 * raaaa0;
          }
          
          /* Move the original alpha value back in */
          abgr0 = _mm_move_ss(abgr0, pre_abgr0);
          
          /* Rotate to ABGR */
          rgba0 = (__v4sf)_mm_shuffle_epi32((__m128i)abgr0, _MM_SHUFFLE(0, 1, 2, 3));
          
          *d++ = rgba0;
          s++;
        }
      _mm_empty ();
    }

  dst += i * 4;
  src += i * 4;
  remainder = samples - i;
  while (remainder--)
    {
      float alpha = src[3];
      float recip;
      if (alpha <= 0.0f)
        recip = 0.0f;
      else
        recip = 1.0f/alpha;
      dst[0] = src[0] * recip;
      dst[1] = src[1] * recip;
      dst[2] = src[2] * recip;
      dst[3] = alpha;
      
      src   += 4;
      dst   += 4;
    }

  return samples;
}

#define splat4f(x) ((__v4sf){x,x,x,x})
#define splat4i(x) ((__v4si){x,x,x,x})
#define FLT_ONE 0x3f800000 // ((union {float f; int i;}){1.0f}).i
#define FLT_MANTISSA (1<<23)

static inline __v4sf
init_newton (__v4sf x, double exponent, double c0, double c1, double c2)
{
    double norm = exponent*M_LN2/FLT_MANTISSA;
    __v4sf y = _mm_cvtepi32_ps((__m128i)((__v4si)x - splat4i(FLT_ONE)));
    return splat4f(c0) + splat4f(c1*norm)*y + splat4f(c2*norm*norm)*y*y;
}

static inline __v4sf
pow_1_24 (__v4sf x)
{
  __v4sf y, z;
  y = init_newton (x, -1./12, 0.9976800269, 0.9885126933, 0.5908575383);
  x = _mm_sqrt_ps (x);
  /* newton's method for x^(-1/6) */
  z = splat4f (1.f/6.f) * x;
  y = splat4f (7.f/6.f) * y - z * ((y*y)*(y*y)*(y*y*y));
  y = splat4f (7.f/6.f) * y - z * ((y*y)*(y*y)*(y*y*y));
  return x*y;
}

static inline __v4sf
pow_24 (__v4sf x)
{
  __v4sf y, z;
  y = init_newton (x, -1./5, 0.9953189663, 0.9594345146, 0.6742970332);
  /* newton's method for x^(-1/5) */
  z = splat4f (1.f/5.f) * x;
  y = splat4f (6.f/5.f) * y - z * ((y*y*y)*(y*y*y));
  y = splat4f (6.f/5.f) * y - z * ((y*y*y)*(y*y*y));
  x *= y;
  return x*x*x;
}

static inline __v4sf
linear_to_gamma_2_2_sse2 (__v4sf x)
{
  __v4sf curve = pow_1_24 (x) * splat4f (1.055f) - splat4f (0.055f);
  __v4sf line = x * splat4f (12.92f);
  __v4sf mask = _mm_cmpgt_ps (x, splat4f (0.003130804954f));
  return _mm_or_ps (_mm_and_ps (mask, curve), _mm_andnot_ps (mask, line));
}

static inline __v4sf
gamma_2_2_to_linear_sse2 (__v4sf x)
{
  __v4sf curve = pow_24 ((x + splat4f (0.055f)) * splat4f (1/1.055f));
  __v4sf line = x * splat4f (1/12.92f);
  __v4sf mask = _mm_cmpgt_ps (x, splat4f (0.04045f));
  return _mm_or_ps (_mm_and_ps (mask, curve), _mm_andnot_ps (mask, line));
}

#define GAMMA_RGBA(func, munge) \
static long \
func (const float *src, float *dst, long samples)\
{\
  int i = samples;\
  if (((uintptr_t)src % 16) + ((uintptr_t)dst % 16) == 0)\
    {\
      for (; i > 3; i -= 4, src += 16, dst += 16)\
        {\
          /* Pack the rgb components from 4 pixels into 3 vectors, gammafy, unpack. */\
          __v4sf x0 = _mm_load_ps (src);\
          __v4sf x1 = _mm_load_ps (src+4);\
          __v4sf x2 = _mm_load_ps (src+8);\
          __v4sf x3 = _mm_load_ps (src+12);\
          __v4sf y0 = _mm_movelh_ps (x0, x1);\
          __v4sf y1 = _mm_movelh_ps (x2, x3);\
          __v4sf z0 = _mm_unpackhi_ps (x0, x1);\
          __v4sf z1 = _mm_unpackhi_ps (x2, x3);\
          __v4sf y2 = _mm_movelh_ps (z0, z1);\
          __v4sf y3 = _mm_movehl_ps (z1, z0);\
          y0 = munge (y0);\
          _mm_storel_pi ((__m64*)(dst), y0);\
          _mm_storeh_pi ((__m64*)(dst+4), y0);\
          y1 = munge (y1);\
          _mm_storel_pi ((__m64*)(dst+8), y1);\
          _mm_storeh_pi ((__m64*)(dst+12), y1);\
          y2 = munge (y2);\
          z0 = _mm_unpacklo_ps (y2, y3);\
          z1 = _mm_unpackhi_ps (y2, y3);\
          _mm_storel_pi ((__m64*)(dst+2), z0);\
          _mm_storeh_pi ((__m64*)(dst+6), z0);\
          _mm_storel_pi ((__m64*)(dst+10), z1);\
          _mm_storeh_pi ((__m64*)(dst+14), z1);\
        }\
      for (; i > 0; i--, src += 4, dst += 4)\
        {\
          __v4sf x = munge (_mm_load_ps (src));\
          float a = src[3];\
          _mm_store_ps (dst, x);\
          dst[3] = a;\
        }\
    }\
  else\
    {\
      for (; i > 0; i--, src += 4, dst += 4)\
        {\
          __v4sf x = munge (_mm_loadu_ps (src));\
          float a = src[3];\
          _mm_storeu_ps (dst, x);\
          dst[3] = a;\
        }\
    }\
  return samples;\
}

GAMMA_RGBA(conv_rgbaF_linear_rgbaF_gamma, linear_to_gamma_2_2_sse2)
GAMMA_RGBA(conv_rgbaF_gamma_rgbaF_linear, gamma_2_2_to_linear_sse2)

#define YA_APPLY(load, store, convert) \
{ \
  __v4sf yyaa0, yyaa1; \
  __v4sf yaya0  = load ((float *)s++); \
  __v4sf yaya1  = load ((float *)s++); \
  __v4sf yyyy01 = _mm_shuffle_ps (yaya0, yaya1, _MM_SHUFFLE(0, 2, 0, 2)); \
\
  yyyy01 = convert (yyyy01); \
\
  yyaa0 = _mm_shuffle_ps (yyyy01, yaya0, _MM_SHUFFLE(3, 1, 0, 1)); \
  yaya0 = (__v4sf)_mm_shuffle_epi32((__m128i)yyaa0, _MM_SHUFFLE(3, 1, 2, 0)); \
  yyaa1 = _mm_shuffle_ps (yyyy01, yaya1, _MM_SHUFFLE(3, 1, 2, 3)); \
  yaya1 = (__v4sf)_mm_shuffle_epi32((__m128i)yyaa1, _MM_SHUFFLE(3, 1, 2, 0)); \
\
  store ((float *)d++, yaya0); \
  store ((float *)d++, yaya1); \
}\

static long
conv_yaF_linear_yaF_gamma (const float *src, float *dst, long samples)
{
  long total = samples;

  const __v4sf *s = (const __v4sf*)src;
        __v4sf *d = (__v4sf*)dst;

  if (((uintptr_t)src % 16) + ((uintptr_t)dst % 16) == 0)
    {
      while (samples > 4)
        {
          YA_APPLY (_mm_load_ps, _mm_store_ps, linear_to_gamma_2_2_sse2);
          samples -= 4;
        }
    }
  else
    {
      while (samples > 4)
        {
          YA_APPLY (_mm_loadu_ps, _mm_storeu_ps, linear_to_gamma_2_2_sse2);
          samples -= 4;
        }
    }

  src = (const float *)s;
  dst = (float *)d;

  while (samples--)
    {
      *dst++ = babl_linear_to_gamma_2_2 (*src++);
      *dst++ = *src++;
    }

  return total;
}


static long
conv_yaF_gamma_yaF_linear (const float *src, float *dst, long samples)
{
  long total = samples;

  const __v4sf *s = (const __v4sf*)src;
        __v4sf *d = (__v4sf*)dst;

  if (((uintptr_t)src % 16) + ((uintptr_t)dst % 16) == 0)
    {
      while (samples > 4)
        {
          YA_APPLY (_mm_load_ps, _mm_store_ps, gamma_2_2_to_linear_sse2);
          samples -= 4;
        }
    }
  else
    {
      while (samples > 4)
        {
          YA_APPLY (_mm_loadu_ps, _mm_storeu_ps, gamma_2_2_to_linear_sse2);
          samples -= 4;
        }
    }

  src = (const float *)s;
  dst = (float *)d;

  while (samples--)
    {
      *dst++ = babl_gamma_2_2_to_linear (*src++);
      *dst++ = *src++;
    }

  return total;
}

static inline long
conv_yF_linear_yF_gamma (const float *src, float *dst, long samples)
{
  long total = samples;

  const __v4sf *s = (const __v4sf*)src;
        __v4sf *d = (__v4sf*)dst;

  if (((uintptr_t)src % 16) + ((uintptr_t)dst % 16) == 0)
    {
      while (samples > 4)
        {
          __v4sf rgba0 = _mm_load_ps ((float *)s++);
          rgba0 = linear_to_gamma_2_2_sse2 (rgba0);
          _mm_store_ps ((float *)d++, rgba0);
          samples -= 4;
        }
    }
  else
    {
      while (samples > 4)
        {
          __v4sf rgba0 = _mm_loadu_ps ((float *)s++);
          rgba0 = linear_to_gamma_2_2_sse2 (rgba0);
          _mm_storeu_ps ((float *)d++, rgba0);
          samples -= 4;
        }
    }

  src = (const float *)s;
  dst = (float *)d;

  while (samples--)
    {
      *dst++ = babl_linear_to_gamma_2_2 (*src++);
    }

  return total;
}

static inline long
conv_yF_gamma_yF_linear (const float *src, float *dst, long samples)
{
  long total = samples;

  const __v4sf *s = (const __v4sf*)src;
        __v4sf *d = (__v4sf*)dst;

  if (((uintptr_t)src % 16) + ((uintptr_t)dst % 16) == 0)
    {
      while (samples > 4)
        {
          __v4sf rgba0 = _mm_load_ps ((float *)s++);
          rgba0 = gamma_2_2_to_linear_sse2 (rgba0);
          _mm_store_ps ((float *)d++, rgba0);
          samples -= 4;
        }
    }
  else
    {
      while (samples > 4)
        {
          __v4sf rgba0 = _mm_loadu_ps ((float *)s++);
          rgba0 = gamma_2_2_to_linear_sse2 (rgba0);
          _mm_storeu_ps ((float *)d++, rgba0);
          samples -= 4;
        }
    }

  src = (const float *)s;
  dst = (float *)d;

  while (samples--)
    {
      *dst++ = babl_gamma_2_2_to_linear (*src++);
    }

  return total;
}


static long
conv_rgbF_linear_rgbF_gamma (const float *src, float *dst, long samples)
{
  return conv_yF_linear_yF_gamma (src, dst, samples * 3) / 3;
}


static long
conv_rgbF_gamma_rgbF_linear (const float *src, float *dst, long samples)
{
  return conv_yF_gamma_yF_linear (src, dst, samples * 3) / 3;
}

#endif /* defined(USE_SSE2) */

#define o(src, dst) \
  babl_conversion_new (src, dst, "linear", conv_ ## src ## _ ## dst, NULL)

int init (void);

int
init (void)
{
#if defined(USE_SSE2)

  const Babl *rgbaF_linear = babl_format_new (
    babl_model ("RGBA"),
    babl_type ("float"),
    babl_component ("R"),
    babl_component ("G"),
    babl_component ("B"),
    babl_component ("A"),
    NULL);
  const Babl *rgbAF_linear = babl_format_new (
    babl_model ("RaGaBaA"),
    babl_type ("float"),
    babl_component ("Ra"),
    babl_component ("Ga"),
    babl_component ("Ba"),
    babl_component ("A"),
    NULL);
  const Babl *rgbAF_gamma = babl_format_new (
    babl_model ("R'aG'aB'aA"),
    babl_type ("float"),
    babl_component ("R'a"),
    babl_component ("G'a"),
    babl_component ("B'a"),
    babl_component ("A"),
    NULL);
  const Babl *rgbaF_gamma = babl_format_new (
    babl_model ("R'G'B'A"),
    babl_type ("float"),
    babl_component ("R'"),
    babl_component ("G'"),
    babl_component ("B'"),
    babl_component ("A"),
    NULL);
  const Babl *rgbF_linear = babl_format_new (
    babl_model ("RGB"),
    babl_type ("float"),
    babl_component ("R"),
    babl_component ("G"),
    babl_component ("B"),
    NULL);
  const Babl *rgbF_gamma = babl_format_new (
    babl_model ("R'G'B'"),
    babl_type ("float"),
    babl_component ("R'"),
    babl_component ("G'"),
    babl_component ("B'"),
    NULL);
  const Babl *yaF_linear = babl_format_new (
    babl_model ("YA"),
    babl_type ("float"),
    babl_component ("Y"),
    babl_component ("A"),
    NULL);
  const Babl *yaF_gamma = babl_format_new (
    babl_model ("Y'A"),
    babl_type ("float"),
    babl_component ("Y'"),
    babl_component ("A"),
    NULL);
  const Babl *yF_linear = babl_format_new (
    babl_model ("Y"),
    babl_type ("float"),
    babl_component ("Y"),
    NULL);
  const Babl *yF_gamma = babl_format_new (
    babl_model ("Y'"),
    babl_type ("float"),
    babl_component ("Y'"),
    NULL);

  if ((babl_cpu_accel_get_support () & BABL_CPU_ACCEL_X86_SSE) &&
      (babl_cpu_accel_get_support () & BABL_CPU_ACCEL_X86_SSE2))
      
    {
      babl_conversion_new(rgbaF_linear, 
                          rgbAF_linear,
                          "linear",
                          conv_rgbaF_linear_rgbAF_linear,
                          NULL);

      babl_conversion_new(rgbaF_gamma, 
                          rgbAF_gamma,
                          "linear",
                          conv_rgbaF_linear_rgbAF_linear,
                          NULL);
                          
      /* Which of these is faster varies by CPU, and the difference
       * is big enough that it's worthwhile to include both and
       * let them fight it out in the babl benchmarks.
       */
      babl_conversion_new(rgbAF_linear, 
                          rgbaF_linear,
                          "linear",
                          conv_rgbAF_linear_rgbaF_linear_shuffle,
                          NULL);
      babl_conversion_new(rgbAF_gamma, 
                          rgbaF_gamma,
                          "linear",
                          conv_rgbAF_linear_rgbaF_linear_shuffle,
                          NULL);

      babl_conversion_new(rgbAF_linear, 
                          rgbaF_linear,
                          "linear",
                          conv_rgbAF_linear_rgbaF_linear_spin,
                          NULL);

      o (yF_linear, yF_gamma);
      o (yF_gamma,  yF_linear);

      o (yaF_linear, yaF_gamma);
      o (yaF_gamma,  yaF_linear);

      o (rgbF_linear, rgbF_gamma);
      o (rgbF_gamma,  rgbF_linear);

      o (rgbaF_linear, rgbaF_gamma);
      o (rgbaF_gamma, rgbaF_linear);
    }

#endif /* defined(USE_SSE2) */

  return 0;
}

