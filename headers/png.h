#ifndef PNG_H
#define PNG_H

#include <stdint.h>
#include <stddef.h>

#include "logger.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t bytes_per_pixel;
    
    unsigned char* raw_data; 
    size_t raw_data_size;
} PngImage;

PngImage* png_load_from_data(Logger* logger, const unsigned char* data, size_t size);
int unfilter_png(Logger* logger, const PngImage* png, unsigned char* out_pixels);
void png_destroy(PngImage* png);

#endif 