#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include <math.h> 

#include "../headers/png.h" 
#include "../headers/logger.h"

const int adam7_start_x[] = {0, 4, 0, 2, 0, 1, 0};
const int adam7_start_y[] = {0, 0, 4, 0, 2, 0, 1};
const int adam7_step_x[]  = {8, 8, 4, 4, 2, 2, 1};
const int adam7_step_y[]  = {8, 8, 8, 4, 4, 2, 2};
static int unfilter_pass(const PngImage* png, const unsigned char* src, unsigned char* dst, uint32_t pass_width, uint32_t pass_height);
static void place_pixels(PngImage* png, const unsigned char* pass_pixels, int pass_index, uint32_t pass_width, uint32_t pass_height, uint8_t pass_bpp);
static uint32_t ntohl_manual(uint32_t n);
void png_destroy(PngImage* png) {
    if (png != NULL) {
        free(png->final_pixel_data);
        free(png->palette);
        free(png);        
    }
}

PngImage* png_load_from_data(const unsigned char* data, size_t size) {
    const uint8_t png_sig_bytes[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8 || memcmp(data, png_sig_bytes, 8) != 0) return NULL;
    PngImage* png = calloc(1, sizeof(PngImage));
    if (!png) return NULL;
    unsigned char* compressed_data = NULL;
    size_t compressed_size = 0;
    const unsigned char* current_ptr = data + 8;
    while (current_ptr < data + size) {
        uint32_t chunk_length = ntohl_manual(*(uint32_t*)current_ptr);
        const char* chunk_type = (const char*)(current_ptr + 4);
        const unsigned char* chunk_data = current_ptr + 8;
        if (strncmp(chunk_type, "IHDR", 4) == 0) {
            png->width = ntohl_manual(*(uint32_t*)(chunk_data));
            png->height = ntohl_manual(*(uint32_t*)(chunk_data + 4));
            png->bit_depth = chunk_data[8];
            png->color_type = chunk_data[9];
            png->interlace_method = chunk_data[12];
            png->bytes_per_pixel = 3;
        } else if (strncmp(chunk_type, "PLTE", 4) == 0) {
            png->palette_size = chunk_length / 3;
            png->palette = malloc(png->palette_size * sizeof(RGBA));
            for(unsigned int i=0; i<png->palette_size; ++i) {
                png->palette[i].r = chunk_data[i*3 + 0];
                png->palette[i].g = chunk_data[i*3 + 1];
                png->palette[i].b = chunk_data[i*3 + 2];
                png->palette[i].a = 255;
            }
        } else if (strncmp(chunk_type, "tRNS", 4) == 0) {
            if (png->color_type == 3 && png->palette) {
                for (unsigned int i=0; i < chunk_length && i < png->palette_size; ++i) {
                    png->palette[i].a = chunk_data[i];
                }
            }
        } else if (strncmp(chunk_type, "IDAT", 4) == 0) {
            compressed_data = realloc(compressed_data, compressed_size + chunk_length);
            memcpy(compressed_data + compressed_size, chunk_data, chunk_length);
            compressed_size += chunk_length;
        } else if (strncmp(chunk_type, "IEND", 4) == 0) {
            break;
        }
        current_ptr += 12 + chunk_length;
    }
    size_t source_bpp_calc = 1;
    if (png->color_type == 2) source_bpp_calc = 3;
    else if (png->color_type == 6) source_bpp_calc = 4;
    else if (png->color_type == 4) source_bpp_calc = 2;
    size_t uncompressed_size = png->height * (1 + (png->width * png->bit_depth * source_bpp_calc + 7) / 8);
    unsigned char* uncompressed_data = malloc(uncompressed_size * 2); 
    uLongf dest_len = uncompressed_size * 2;
    uncompress(uncompressed_data, &dest_len, compressed_data, compressed_size);
    free(compressed_data);
    png->final_pixel_size = png->width * png->height * png->bytes_per_pixel;
    png->final_pixel_data = calloc(1, png->final_pixel_size); 
    if (png->interlace_method == 0) {
        size_t stride = (png->width * png->bit_depth * source_bpp_calc + 7) / 8;
        unsigned char* unfiltered_data = malloc(png->height * stride);
        unfilter_pass(png, uncompressed_data, unfiltered_data, png->width, png->height);
        place_pixels(png, unfiltered_data, -1, png->width, png->height, source_bpp_calc);
        free(unfiltered_data);
    } else {
        const unsigned char* data_ptr = uncompressed_data;
        for (int i = 0; i < 7; i++) {
            uint32_t pass_w = (png->width - adam7_start_x[i] + adam7_step_x[i] - 1) / adam7_step_x[i];
            uint32_t pass_h = (png->height - adam7_start_y[i] + adam7_step_y[i] - 1) / adam7_step_y[i];
            if (pass_w == 0 || pass_h == 0) continue;
            size_t pass_stride = (pass_w * png->bit_depth * source_bpp_calc + 7) / 8;
            size_t pass_size = pass_h * pass_stride;
            size_t filtered_pass_size = pass_h * (1 + pass_stride);
            unsigned char* unfiltered_pass = malloc(pass_size);
            unfilter_pass(png, data_ptr, unfiltered_pass, pass_w, pass_h);
            place_pixels(png, unfiltered_pass, i, pass_w, pass_h, source_bpp_calc);
            data_ptr += filtered_pass_size;
            free(unfiltered_pass);
        }
    }
    free(uncompressed_data);
    return png;
}

static void place_pixels(PngImage* png, const unsigned char* pass_pixels, int pass_index, uint32_t pass_width, uint32_t pass_height, uint8_t pass_bpp_source) {
    for (uint32_t py = 0; py < pass_height; py++) {
        for (uint32_t px = 0; px < pass_width; px++) {
            uint32_t final_x, final_y;
            if (pass_index == -1) { 
                final_x = px;
                final_y = py;
            } else { 
                final_x = px * adam7_step_x[pass_index] + adam7_start_x[pass_index];
                final_y = py * adam7_step_y[pass_index] + adam7_start_y[pass_index];
            }
            if (final_x >= png->width || final_y >= png->height) continue;
            unsigned char* out_pixel = png->final_pixel_data + (final_y * png->width + final_x) * png->bytes_per_pixel;
            if (png->color_type == 3) {
                size_t bit_pos = py * (pass_width * png->bit_depth) + px * png->bit_depth;
                size_t byte_pos = bit_pos / 8;
                int bit_in_byte = 7 - (bit_pos % 8);
                uint8_t index_mask = (1 << png->bit_depth) - 1;
                uint8_t index = (pass_pixels[byte_pos] >> (bit_in_byte - png->bit_depth + 1)) & index_mask;
                if (index < png->palette_size) {
                    out_pixel[0] = png->palette[index].r;
                    out_pixel[1] = png->palette[index].g;
                    out_pixel[2] = png->palette[index].b;
                }
            } else if (png->color_type == 2 || png->color_type == 6) {
                const unsigned char* in_pixel = pass_pixels + (py * pass_width + px) * pass_bpp_source;
                out_pixel[0] = in_pixel[0];
                out_pixel[1] = in_pixel[1];
                out_pixel[2] = in_pixel[2];
            } 
            else if (png->color_type == 0 || png->color_type == 4) {
                const unsigned char* in_pixel = pass_pixels + (py * pass_width + px) * pass_bpp_source;
                uint8_t gray_value = in_pixel[0];
                out_pixel[0] = gray_value;
                out_pixel[1] = gray_value;
                out_pixel[2] = gray_value;
            }
        }
    }
}

static int unfilter_pass(const PngImage* png, const unsigned char* src, unsigned char* dst, uint32_t pass_width, uint32_t pass_height) {
    size_t source_bpp_calc = 1;
    if (png->color_type == 2) source_bpp_calc = 3;
    else if (png->color_type == 6) source_bpp_calc = 4;
    else if (png->color_type == 4) source_bpp_calc = 2;
    size_t stride = (pass_width * png->bit_depth * source_bpp_calc + 7) / 8;
    if (stride == 0) return 0;
    size_t filter_bpp = (png->bit_depth < 8) ? 1 : source_bpp_calc * (png->bit_depth/8);
    for (uint32_t y = 0; y < pass_height; y++) {
        uint8_t filter_type = *src++;
        const unsigned char* prev_line = (y == 0) ? NULL : (dst - stride);
        for (uint32_t x = 0; x < stride; x++) {
            uint8_t raw = src[x];
            uint8_t left = (x >= filter_bpp) ? dst[x - filter_bpp] : 0;
            uint8_t up = (prev_line != NULL) ? prev_line[x] : 0;
            uint8_t up_left = (prev_line != NULL && x >= filter_bpp) ? prev_line[x - filter_bpp] : 0;
            switch (filter_type) {
                case 0: dst[x] = raw; break;
                case 1: dst[x] = raw + left; break;
                case 2: dst[x] = raw + up; break;
                case 3: dst[x] = raw + ((left + up) / 2); break;
                case 4: { 
                    int p = left + up - up_left;
                    int pa = abs(p - left); int pb = abs(p - up); int pc = abs(p - up_left);
                    dst[x] = raw + ((pa <= pb && pa <= pc) ? left : (pb <= pc ? up : up_left));
                    break;
                }
                default: return -1;
            }
        }
        src += stride;
        dst += stride;
    }
    return 0;
}

static uint32_t ntohl_manual(uint32_t n) {
    unsigned char *p = (unsigned char *)&n;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}