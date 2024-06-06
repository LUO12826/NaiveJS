#include "qjs_date.h"

#include <cstdint>
#include <sys/time.h>
#include <ctime>
#include <cassert>
#include "njs/utils/helper.h"
#include "njs/basic_types/String.h"

static int const month_days[] = {31, 28, 31, 30, 31, 30,
                                 31, 31, 30, 31, 30, 31};
static char const month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
static char const day_names[] = "SunMonTueWedThuFriSat";

static int64_t math_mod(int64_t a, int64_t b) {
  /* return positive modulo */
  int64_t m = a % b;
  return m + (m < 0) * b;
}

static int64_t floor_div(int64_t a, int64_t b) {
  /* integer division rounding toward -Infinity */
  int64_t m = a % b;
  return (a - (m + (m < 0) * b)) / b;
}

static int64_t days_in_year(int64_t y) {
  return 365 + !(y % 4) - !(y % 100) + !(y % 400);
}

static int64_t days_from_year(int64_t y) {
  return 365 * (y - 1970) + floor_div(y - 1969, 4) - floor_div(y - 1901, 100) +
         floor_div(y - 1601, 400);
}

/* return the year, update days */
static int64_t year_from_days(int64_t *days) {
  int64_t y, d1, nd, d = *days;
  y = floor_div(d * 10000, 3652425) + 1970;
  /* the initial approximation is very good, so only a few
     iterations are necessary */
  for (;;) {
    d1 = d - days_from_year(y);
    if (d1 < 0) {
      y--;
      d1 += days_in_year(y);
    } else {
      nd = days_in_year(y);
      if (d1 < nd)
        break;
      d1 -= nd;
      y++;
    }
  }
  *days = d1;
  return y;
}

/* OS dependent. d = argv[0] is in ms from 1970. Return the difference
   between UTC time and local time 'd' in minutes */
static int get_timezone_offset(int64_t time) {
#if defined(_WIN32)
  /* XXX: TODO */
  return 0;
#else
  time_t ti;
  struct tm tm;

  time /= 1000; /* convert to seconds */
  if (sizeof(time_t) == 4) {
    /* on 32-bit systems, we need to clamp the time value to the
       range of `time_t`. This is better than truncating values to
       32 bits and hopefully provides the same result as 64-bit
       implementation of localtime_r.
     */
    if ((time_t)-1 < 0) {
      if (time < INT32_MIN) {
        time = INT32_MIN;
      } else if (time > INT32_MAX) {
        time = INT32_MAX;
      }
    } else {
      if (time < 0) {
        time = 0;
      } else if (time > UINT32_MAX) {
        time = UINT32_MAX;
      }
    }
  }
  ti = time;
  localtime_r(&ti, &tm);
  return -tm.tm_gmtoff / 60;
#endif
}

static void get_date_fields(double dval, double fields[9], int is_local) {
  int64_t d, days, wd, y, i, md, h, m, s, ms, tz = 0;
  assert(not isnan(dval));

  d = dval;
  if (is_local) {
    tz = -get_timezone_offset(d);
    d += tz * 60000;
  }

  /* result is >= 0, we can use % */
  h = math_mod(d, 86400000);
  days = (d - h) / 86400000;
  ms = h % 1000;
  h = (h - ms) / 1000;
  s = h % 60;
  h = (h - s) / 60;
  m = h % 60;
  h = (h - m) / 60;
  wd = math_mod(days + 4, 7); /* week day */
  y = year_from_days(&days);

  for (i = 0; i < 11; i++) {
    md = month_days[i];
    if (i == 1)
      md += days_in_year(y) - 365;
    if (days < md)
      break;
    days -= md;
  }
  fields[0] = y;
  fields[1] = i;
  fields[2] = days + 1;
  fields[3] = h;
  fields[4] = m;
  fields[5] = s;
  fields[6] = ms;
  fields[7] = wd;
  fields[8] = tz;
}

/* fmt:
   0: toUTCString: "Tue, 02 Jan 2018 23:04:46 GMT"
   1: toString: "Wed Jan 03 2018 00:05:22 GMT+0100 (CET)"
   2: toISOString: "2018-01-02T23:02:56.927Z"
   3: toLocaleString: "1/2/2018, 11:40:40 PM"
   part: 1=date, 2=time 3=all
   XXX: should use a variant of strftime().
 */
njs::String get_date_string(double ts, int magic) {
  // _string(obj, fmt, part)
  char buf[64];
  double fields[9];
  int res, fmt, part, pos;
  int y, mon, d, h, m, s, ms, wd, tz;

  fmt = (magic >> 4) & 0x0F;
  part = magic & 0x0F;

  get_date_fields(ts, fields, fmt & 1);

  y = fields[0];
  mon = fields[1];
  d = fields[2];
  h = fields[3];
  m = fields[4];
  s = fields[5];
  ms = fields[6];
  wd = fields[7];
  tz = fields[8];

  pos = 0;

  if (part & 1) { /* date part */
    switch (fmt) {
      case 0:
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%.3s, %02d %.3s %0*d ",
                        day_names + wd * 3, d, month_names + mon * 3, 4 + (y < 0),
                        y);
        break;
      case 1:
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%.3s %.3s %02d %0*d",
                        day_names + wd * 3, month_names + mon * 3, d, 4 + (y < 0),
                        y);
        if (part == 3) {
          buf[pos++] = ' ';
        }
        break;
      case 2:
        if (y >= 0 && y <= 9999) {
          pos += snprintf(buf + pos, sizeof(buf) - pos, "%04d", y);
        } else {
          pos += snprintf(buf + pos, sizeof(buf) - pos, "%+07d", y);
        }
        pos += snprintf(buf + pos, sizeof(buf) - pos, "-%02d-%02dT", mon + 1, d);
        break;
      case 3:
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02d/%02d/%0*d", mon + 1,
                        d, 4 + (y < 0), y);
        if (part == 3) {
          buf[pos++] = ',';
          buf[pos++] = ' ';
        }
        break;
    }
  }
  if (part & 2) { /* time part */
    switch (fmt) {
      case 0:
        pos +=
            snprintf(buf + pos, sizeof(buf) - pos, "%02d:%02d:%02d GMT", h, m, s);
        break;
      case 1:
        pos +=
            snprintf(buf + pos, sizeof(buf) - pos, "%02d:%02d:%02d GMT", h, m, s);
        if (tz < 0) {
          buf[pos++] = '-';
          tz = -tz;
        } else {
          buf[pos++] = '+';
        }
        /* tz is >= 0, can use % */
        pos +=
            snprintf(buf + pos, sizeof(buf) - pos, "%02d%02d", tz / 60, tz % 60);
        /* XXX: tack the time zone code? */
        break;
      case 2:
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02d:%02d:%02d.%03dZ", h,
                        m, s, ms);
        break;
      case 3:
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02d:%02d:%02d %cM",
                        (h + 1) % 12 - 1, m, s, (h < 12) ? 'A' : 'P');
        break;
    }
  }

  char16_t u16buf[64];
  njs::u8_to_u16_buffer(buf, u16buf);
  return njs::String(u16buf);
}