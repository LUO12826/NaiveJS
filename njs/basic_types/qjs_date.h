#ifndef NJS_QJS_DATE_H
#define NJS_QJS_DATE_H

#include <math.h>
#include <string>

inline double time_clip(double t) {
  if (t >= -8.64e15 && t <= 8.64e15)
    return trunc(t) + 0.0; /* convert -0 to +0 */
  else
    return NAN;
}

std::u16string get_date_string(double ts, int magic);

#endif // NJS_QJS_DATE_H
