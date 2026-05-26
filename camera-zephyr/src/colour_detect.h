#ifndef COLOUR_DETECT_H
#define COLOUR_DETECT_H

#include <stdint.h>

typedef enum {
    COLOUR_NONE,
    COLOUR_ORANGE,
    COLOUR_PURPLE,
    COLOUR_RED,
} colour_t;

struct colour_result {
    colour_t colour;  // final verdict of colour
    int votes[4];   // vote counts for each colour            
    int total;   // total pixels sampled                      
    float avg_h; // average hue: over winning colour pixels only
    float avg_s;  // average sat 
    float avg_v;  // average value  

    //center pixel — raw extracted channels for byte-order diagnostics  
    uint8_t center_r;
    uint8_t center_g;
    uint8_t center_b;
};

colour_t colour_detect_verbose(const uint8_t *buf, uint32_t width, uint32_t height, struct colour_result *out);

const char *colour_name(colour_t c);

#endif /* COLOUR_DETECT_H */
