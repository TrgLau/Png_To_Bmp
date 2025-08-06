#ifndef PNG_H
#define PNG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "logger.h"

typedef struct {
    uint8_t r, g, b, a;
} RGBA;

typedef struct {
    uint16_t r, g, b;
} RGB16;

typedef struct PngImage {
    uint32_t width;
    uint32_t height;
    uint8_t bit_depth;
    uint8_t color_type;
    uint8_t bytes_per_pixel; 
    uint8_t interlace_method;
    unsigned char* final_pixel_data; 
    size_t final_pixel_size;
    RGBA* palette;
    unsigned int palette_size;
    float file_gamma;
    bool has_transparency_key;
    RGB16 transparency_key;
} PngImage;

PngImage* png_load_from_data(const unsigned char* data, size_t size);
PngImage* png_load_from_file(const char *fname);
void png_destroy(PngImage* png);

#endif