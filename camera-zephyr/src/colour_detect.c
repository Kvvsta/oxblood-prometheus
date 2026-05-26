/*
 * colour_detect.c -> HSV majority vote 
 *
 * only the centre SAMPLE_SIZE×SAMPLE_SIZE region is examined.
 *
 * Target colours:
 *   Orange H = roughly 27 degrees (15–45 degreees) (british paints wordly YEL 119 paint sample card)
 *   Purple H = roughly 233 degrees (210–260 degrees) (british paints royal tuplip EB 252 paint sample card)
 *   Red H = roughly 2 degrees (345–360 degrees and 0–15 degrees)  (british paints red bauble RED 186 paint sample card)
 */

#include <math.h>
#include <stddef.h>
#include <zephyr/sys/util.h>
#include "colour_detect.h"

#define DEG2RAD (3.14159265f / 180.f)
#define RAD2DEG (180.f / 3.14159265f)

#define SAMPLE_SIZE 20  // side length of centre region in pixels
#define MIN_SATURATION 0.22f // below this is grey/colourless
#define MIN_VALUE 0.12f  // below this is too dark
#define VOTE_THRESHOLD 0.50f  // fraction of pixels that must vote to agree 

// HSV thresholds 
#define HUE_ORANGE_LO 15.0f
#define HUE_ORANGE_HI 45.0f
#define HUE_PURPLE_LO 210.0f
#define HUE_PURPLE_HI 260.0f
//red wraps around 0degrees: 345–360degrees and 0–HUE_ORANGE_LO 

/*
 * The OV2640 via the ESP32 DVP stores RGB565 big-endian (high byte — RRRRRGGG — at the lower address).  
 * On the little-endian ESP32, casting two consecutive bytes to uint16_t reverses them.  
 * Undo that swap before extracting channels.
 */
static inline void rgb565_unpack(uint16_t px, uint8_t *r, uint8_t *g, uint8_t *b)
{
    px = (uint16_t)((px >> 8) | (px << 8));
    *r = (uint8_t)(((px >> 11) & 0x1Fu) * 255u / 31u);
    *g = (uint8_t)(((px >> 5) & 0x3Fu) * 255u / 63u);
    *b = (uint8_t)(( px & 0x1Fu) * 255u / 31u);
}

// RGB -> HSV 
static void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float *h, float *s, float *v)
{
    float rf = (float)r / 255.0f;
    float gf = (float)g / 255.0f;
    float bf = (float)b / 255.0f;

    float cmax = MAX(MAX(rf, gf), bf);
    float cmin = MIN(MIN(rf, gf), bf);
    float delta = cmax - cmin;

    *v = cmax;
    *s = (cmax < 0.001f) ? 0.0f : (delta / cmax);

    if (delta < 0.001f) {
        *h = 0.0f;
    } else if (cmax == rf) {
        *h = 60.0f * fmodf((gf - bf) / delta, 6.0f);
    } else if (cmax == gf) {
        *h = 60.0f * ((bf - rf) / delta + 2.0f);
    } else {
        *h = 60.0f * ((rf - gf) / delta + 4.0f);
    }

    if (*h < 0.0f) {
        *h += 360.0f;
    }
}

// pixel classifier based on HSV thresholds
static colour_t classify_pixel(float h, float s, float v)
{
    if (s < MIN_SATURATION || v < MIN_VALUE) {
        return COLOUR_NONE;
    }

    if (h >= 345.0f || h < HUE_ORANGE_LO) return COLOUR_RED;
    if (h >= HUE_ORANGE_LO && h < HUE_ORANGE_HI) return COLOUR_ORANGE;
    if (h >= HUE_PURPLE_LO && h < HUE_PURPLE_HI) return COLOUR_PURPLE;

    return COLOUR_NONE;
}


colour_t colour_detect_verbose(const uint8_t *buf, uint32_t width, uint32_t height, struct colour_result *out)
{
    const uint16_t *pixels = (const uint16_t *)buf;

    int cx = (int)width / 2;
    int cy = (int)height / 2;
    int half = SAMPLE_SIZE / 2;

    int votes[4] = {0, 0, 0, 0};

    // sin/cos accumulators used to calculate circular-mean of hue for each colour
    // this avoids wrap bias caused by red wrapping around the 0 & 360 degree boundary.
    float h_sin[4] = {0.f, 0.f, 0.f, 0.f};
    float h_cos[4] = {0.f, 0.f, 0.f, 0.f};
    float s_sum[4] = {0.f, 0.f, 0.f, 0.f};
    float v_sum[4] = {0.f, 0.f, 0.f, 0.f};
    int total = 0;

    // extract the centre pixel for debugging 
    uint8_t ctr_r = 0, ctr_g = 0, ctr_b = 0;
    rgb565_unpack(pixels[cy * (int)width + cx], &ctr_r, &ctr_g, &ctr_b);

    for (int y = cy - half; y < cy + half; y++) {
        for (int x = cx - half; x < cx + half; x++) {
            uint16_t px = pixels[y * (int)width + x];
            uint8_t r, g, b;
            rgb565_unpack(px, &r, &g, &b);

            float h, s, v;
            rgb_to_hsv(r, g, b, &h, &s, &v);

            colour_t c = classify_pixel(h, s, v);
            votes[c]++;
            // circular mean again
            h_sin[c] += sinf(h * DEG2RAD);
            h_cos[c] += cosf(h * DEG2RAD);
            s_sum[c] += s;
            v_sum[c] += v;
            total++;
        }
    }

    colour_t best = COLOUR_NONE;
    int best_count = 0;

    for (int i = 1; i < 4; i++) {
        if (votes[i] > best_count) {
            best_count = votes[i];
            best = (colour_t)i;
        }
    }

    bool pass = (total > 0) && ((float)best_count / (float)total >= VOTE_THRESHOLD) && (best != COLOUR_NONE);

    colour_t result = pass ? best : COLOUR_NONE;

    if (out) {
        out->colour   = result;
        for (int i = 0; i < 4; i++) {
            out->votes[i] = votes[i];
        }
        out->total = total;
        // report HSV averages only for the winning colour
        colour_t bucket = (result != COLOUR_NONE) ? result : COLOUR_NONE;
        int bv = votes[bucket];
        if (bv > 0) {
            // circular mean again
            float ch = atan2f(h_sin[bucket] / bv, h_cos[bucket] / bv) * RAD2DEG;
            if (ch < 0.f) ch += 360.f;
            out->avg_h = ch;
        } else {
            out->avg_h = 0.f;
        }
        out->avg_s = (bv > 0) ? s_sum[bucket] / (float)bv : 0.f;
        out->avg_v = (bv > 0) ? v_sum[bucket] / (float)bv : 0.f;
        out->center_r = ctr_r;
        out->center_g = ctr_g;
        out->center_b = ctr_b;
    }

    return result;
}

const char *colour_name(colour_t c)
{
    switch (c) {
        case COLOUR_ORANGE: return "ORANGE";
        case COLOUR_PURPLE: return "PURPLE";
        case COLOUR_RED: return "RED";
        default: return "NONE";
    }
}
