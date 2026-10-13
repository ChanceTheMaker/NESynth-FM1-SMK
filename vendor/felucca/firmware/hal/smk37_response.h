/* SPDX-License-Identifier: GPL-3.0-only */
/* Pure SMK-37 response conversion, recovered from Pro 016 control flow.
 * Does not read hardware or substitute defaults for unit calibration.
 * Input is the calibrated 0..1023 response, NOT a raw ADC/contact duration.
 */
#pragma once
#include <stdint.h>

enum smk37_response_curve {
    SMK37_RESPONSE_LINEAR,
    SMK37_RESPONSE_BOOST_32,
    SMK37_RESPONSE_BOOST_64,
    SMK37_RESPONSE_FIXED
};

static inline uint8_t smk37_response_velocity(unsigned response,
                                            enum smk37_response_curve curve)
{
    unsigned velocity;
    if (!response) return 0;
    if (response > 1023) response = 1023;
    switch (curve) {
    case SMK37_RESPONSE_BOOST_32: velocity = ((response * 3) >> 5) + 32; break;
    case SMK37_RESPONSE_BOOST_64: velocity = (response >> 4) + 64; break;
    case SMK37_RESPONSE_FIXED: velocity = 127; break;
    default: velocity = response >> 3; break;
    }
    return (uint8_t)(velocity ? velocity : 1);
}

/* Stock preset velocity endpoints may be reversed. Preserve release before
 * applying the range; stock callers dispatch release separately. */
static inline uint8_t smk37_velocity_range(uint8_t velocity,
                                         uint8_t first, uint8_t last)
{
    unsigned result;
    if (!velocity) return 0;
    if (velocity > 127) velocity = 127;
    if (first > 127) first = 127;
    if (last > 127) last = 127;
    if (last > first)
        result = first + (((unsigned)velocity + 1) * (last - first) >> 7);
    else
        result = last + ((128u - velocity) * (first - last) >> 7);
    return (uint8_t)(result ? result : 1);
}

/* Stock normal-key path: 48 + contact index + 12*octave + transpose.
 * Stock clips at 120. Invalid indices are rejected, never wrapped. */
static inline int smk37_key_note(unsigned index, int8_t octave, int8_t transpose)
{
    int note;
    if (index >= 37) return -1;
    note = 48 + (int)index + 12 * (int)octave + (int)transpose;
    return note < 0 ? 0 : note > 120 ? 120 : note;
}
