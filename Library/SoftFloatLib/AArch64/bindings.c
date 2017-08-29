#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "softfloat.h"

float128_t __floatunditf(uint64_t a)
{
  return ui64_to_f128(a);
}

uint64_t __fixunstfdi(float128_t a)
{
  return f128_to_ui64_r_minMag(a, false);
}
