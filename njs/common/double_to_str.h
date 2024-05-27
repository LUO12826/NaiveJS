/*
* This file collects double-to-string code from quickjs.
*/
#ifndef NJS_DOUBLE_TO_STR_H
#define NJS_DOUBLE_TO_STR_H

#include <cmath>
#include <cstdio>
#include <cfenv>
#include <cassert>
#include <cstring>

/* radix != 10 is only supported with flags = JS_DTOA_VAR_FORMAT */
/* use as many digits as necessary */
#define JS_DTOA_VAR_FORMAT (0 << 0)
/* use n_digits significant digits (1 <= n_digits <= 101) */
#define JS_DTOA_FIXED_FORMAT (1 << 0)
/* force fractional format: [-]dd.dd with n_digits fractional digits */
#define JS_DTOA_FRAC_FORMAT (2 << 0)
/* force exponential notation either in fixed or variable format */
#define JS_DTOA_FORCE_EXP (1 << 2)

#define JS_DTOA_BUF_SIZE 128
#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)

namespace njs {

static int js_fcvt(char *buf, int buf_size, double d, int n_digits,
                    int rounding_mode) {
  int n;
  if (rounding_mode != FE_TONEAREST)
    fesetround(rounding_mode);
  n = snprintf(buf, buf_size, "%.*f", n_digits, d);
  if (rounding_mode != FE_TONEAREST)
    fesetround(FE_TONEAREST);
  assert(n < buf_size);
  return n;
}

/* buf1 contains the printf result */
static void js_ecvt1(double d, int n_digits, int *decpt, int *sign, char *buf,
                     int rounding_mode, char *buf1, int buf1_size) {
  if (rounding_mode != FE_TONEAREST)
    fesetround(rounding_mode);
  snprintf(buf1, buf1_size, "%+.*e", n_digits - 1, d);
  if (rounding_mode != FE_TONEAREST)
    fesetround(FE_TONEAREST);
  *sign = (buf1[0] == '-');
  /* mantissa */
  buf[0] = buf1[1];
  if (n_digits > 1)
    memcpy(buf + 1, buf1 + 3, n_digits - 1);
  buf[n_digits] = '\0';
  /* exponent */
  *decpt = atoi(buf1 + n_digits + 2 + (n_digits > 1)) + 1;
}

static int js_ecvt(double d, int n_digits, int *decpt, int *sign, char *buf,
                   bool is_fixed) {
  int rounding_mode;
  char buf_tmp[JS_DTOA_BUF_SIZE];

  if (!is_fixed) {
    unsigned int n_digits_min, n_digits_max;
    /* find the minimum amount of digits (XXX: inefficient but simple) */
    n_digits_min = 1;
    n_digits_max = 17;
    while (n_digits_min < n_digits_max) {
      n_digits = (n_digits_min + n_digits_max) / 2;
      js_ecvt1(d, n_digits, decpt, sign, buf, FE_TONEAREST, buf_tmp,
               sizeof(buf_tmp));
      if (strtod(buf_tmp, NULL) == d) {
        /* no need to keep the trailing zeros */
        while (n_digits >= 2 && buf[n_digits - 1] == '0')
          n_digits--;
        n_digits_max = n_digits;
      } else {
        n_digits_min = n_digits + 1;
      }
    }
    n_digits = n_digits_max;
    rounding_mode = FE_TONEAREST;
  } else {
    rounding_mode = FE_TONEAREST;
#ifdef CONFIG_PRINTF_RNDN
    {
      char buf1[JS_DTOA_BUF_SIZE], buf2[JS_DTOA_BUF_SIZE];
      int decpt1, sign1, decpt2, sign2;
      /* The JS rounding is specified as round to nearest ties away
         from zero (RNDNA), but in printf the "ties" case is not
         specified (for example it is RNDN for glibc, RNDNA for
         Windows), so we must round manually. */
      js_ecvt1(d, n_digits + 1, &decpt1, &sign1, buf1, FE_TONEAREST, buf_tmp,
               sizeof(buf_tmp));
      /* XXX: could use 2 digits to reduce the average running time */
      if (buf1[n_digits] == '5') {
        js_ecvt1(d, n_digits + 1, &decpt1, &sign1, buf1, FE_DOWNWARD, buf_tmp,
                 sizeof(buf_tmp));
        js_ecvt1(d, n_digits + 1, &decpt2, &sign2, buf2, FE_UPWARD, buf_tmp,
                 sizeof(buf_tmp));
        if (memcmp(buf1, buf2, n_digits + 1) == 0 && decpt1 == decpt2) {
          /* exact result: round away from zero */
          if (sign1)
            rounding_mode = FE_DOWNWARD;
          else
            rounding_mode = FE_UPWARD;
        }
      }
    }
#endif /* CONFIG_PRINTF_RNDN */
  }
  js_ecvt1(d, n_digits, decpt, sign, buf, rounding_mode, buf_tmp,
           sizeof(buf_tmp));
  return n_digits;
}

/* 2 <= base <= 36 */
static char *i64toa(char *buf_end, int64_t n, unsigned int base) {
  char *q = buf_end;
  int digit, is_neg;

  is_neg = 0;
  if (n < 0) {
    is_neg = 1;
    n = -n;
  }
  *--q = '\0';
  do {
    digit = (uint64_t)n % base;
    n = (uint64_t)n / base;
    if (digit < 10)
      digit += '0';
    else
      digit += 'a' - 10;
    *--q = digit;
  } while (n != 0);
  if (is_neg)
    *--q = '-';
  return q;
}

inline void js_dtoa(char *buf, double d, int radix, int n_digits, int flags) {
  char *q;

  if (!isfinite(d)) {
    if (isnan(d)) {
      strcpy(buf, "NaN");
    } else {
      q = buf;
      if (d < 0)
        *q++ = '-';
      strcpy(q, "Infinity");
    }
  } else if (flags == JS_DTOA_VAR_FORMAT) {
    int64_t i64;
    char buf1[70], *ptr;
    i64 = (int64_t)d;
    if (d != i64 || i64 > MAX_SAFE_INTEGER || i64 < -MAX_SAFE_INTEGER)
      goto generic_conv;
    /* fast path for integers */
    ptr = i64toa(buf1 + sizeof(buf1), i64, radix);
    strcpy(buf, ptr);
  } else {
    if (d == 0.0)
      d = 0.0; /* convert -0 to 0 */
    if (flags == JS_DTOA_FRAC_FORMAT) {
      js_fcvt(buf, JS_DTOA_BUF_SIZE, d, n_digits, FE_TONEAREST);
    } else {
      char buf1[JS_DTOA_BUF_SIZE];
      int sign, decpt, k, n, i, p, n_max;
      bool is_fixed;
    generic_conv:
      is_fixed = ((flags & 3) == JS_DTOA_FIXED_FORMAT);
      if (is_fixed) {
        n_max = n_digits;
      } else {
        n_max = 21;
      }
      /* the number has k digits (k >= 1) */
      k = js_ecvt(d, n_digits, &decpt, &sign, buf1, is_fixed);
      n = decpt; /* d=10^(n-k)*(buf1) i.e. d= < x.yyyy 10^(n-1) */
      q = buf;
      if (sign)
        *q++ = '-';
      if (flags & JS_DTOA_FORCE_EXP)
        goto force_exp;
      if (n >= 1 && n <= n_max) {
        if (k <= n) {
          memcpy(q, buf1, k);
          q += k;
          for (i = 0; i < (n - k); i++)
            *q++ = '0';
          *q = '\0';
        } else {
          /* k > n */
          memcpy(q, buf1, n);
          q += n;
          *q++ = '.';
          for (i = 0; i < (k - n); i++)
            *q++ = buf1[n + i];
          *q = '\0';
        }
      } else if (n >= -5 && n <= 0) {
        *q++ = '0';
        *q++ = '.';
        for (i = 0; i < -n; i++)
          *q++ = '0';
        memcpy(q, buf1, k);
        q += k;
        *q = '\0';
      } else {
      force_exp:
        /* exponential notation */
        *q++ = buf1[0];
        if (k > 1) {
          *q++ = '.';
          for (i = 1; i < k; i++)
            *q++ = buf1[i];
        }
        *q++ = 'e';
        p = n - 1;
        if (p >= 0)
          *q++ = '+';
        sprintf(q, "%d", p);
      }
    }
  }
}

}

#endif // NJS_DOUBLE_TO_STR_H
