#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include <math.h> 

#include "../headers/png.h" 
#include "../headers/logger.h"

const int adam7_start_x[] = {0, 4, 0, 2, 0, 1, 0}; const int adam7_start_y[] = {0, 0, 4, 0, 2, 0, 1};
const int adam7_step_x[]  = {8, 8, 4, 4, 2, 2, 1}; const int adam7_step_y[]  = {8, 8, 8, 4, 4, 2, 2};
static int unfilter_pass(const PngImage* png, const unsigned char* src, unsigned char* dst, uint32_t pass_width, uint32_t pass_height);
static void place_pixels(PngImage* png, const unsigned char* pass_pixels, int pass_index, uint32_t pass_width, uint32_t pass_height, const uint8_t gamma_lut[256]);
static uint16_t get_pixel_value_at(const unsigned char* data, uint32_t x, uint32_t y, uint32_t pass_width, uint8_t bit_depth);
static uint32_t ntohl_manual(uint32_t n); static uint16_t ntohs_manual(uint16_t n);

void png_destroy(PngImage* png) {
    if (png != NULL) { free(png->final_pixel_data); free(png->palette); free(png); }
}

PngImage*
png_load_from_data(const unsigned char* data, size_t size)
{
    const uint8_t png_sig_bytes[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8 || memcmp(data, png_sig_bytes, 8) != 0) return NULL;
    PngImage* png = calloc(1, sizeof(PngImage));
    if (!png) return NULL;
    png->file_gamma = 1.0f / 2.2f;
    png->has_transparency_key = false;
    unsigned char* compressed_data = NULL;
    size_t compressed_size = 0;
    const unsigned char* current_ptr = data + 8;
    while (current_ptr < data + size) {
        uint32_t chunk_length = ntohl_manual(*(uint32_t*)current_ptr);
        const unsigned char* chunk_type = current_ptr + 4;
        const unsigned char* chunk_data = current_ptr + 8;
        if (chunk_data + chunk_length + 4 > data + size) { png_destroy(png); return NULL; }
        uint32_t stored_crc = ntohl_manual(*(uint32_t*)(chunk_data + chunk_length));
        uLong calculated_crc = crc32(crc32(0L, chunk_type, 4), chunk_data, chunk_length);
        if (calculated_crc != stored_crc) { png_destroy(png); return NULL; }

        if (strncmp((const char*)chunk_type, "IHDR", 4) == 0) {
            png->width = ntohl_manual(*(uint32_t*)(chunk_data)); png->height = ntohl_manual(*(uint32_t*)(chunk_data + 4));
            png->bit_depth = chunk_data[8]; png->color_type = chunk_data[9]; png->interlace_method = chunk_data[12];
            png->bytes_per_pixel = 3;
        } else if (strncmp((const char*)chunk_type, "PLTE", 4) == 0) {
            png->palette_size = chunk_length / 3;
            png->palette = malloc(png->palette_size * sizeof(RGBA));
            for(unsigned int i=0; i<png->palette_size; ++i) { png->palette[i].r = chunk_data[i*3]; png->palette[i].g = chunk_data[i*3+1]; png->palette[i].b = chunk_data[i*3+2]; png->palette[i].a = 255; }
        } else if (strncmp((const char*)chunk_type, "gAMA", 4) == 0) {
            uint32_t gamma_int = ntohl_manual(*(uint32_t*)chunk_data);
            if (gamma_int != 0) { png->file_gamma = 1.0f / (gamma_int / 100000.0f); }
        } else if (strncmp((const char*)chunk_type, "tRNS", 4) == 0) {
            png->has_transparency_key = true;
            if (png->color_type == 0) {
                png->transparency_key.r = png->transparency_key.g = png->transparency_key.b = ntohs_manual(*(uint16_t*)chunk_data);
            } else if (png->color_type == 2) {
                png->transparency_key.r = ntohs_manual(*(uint16_t*)(chunk_data));
                png->transparency_key.g = ntohs_manual(*(uint16_t*)(chunk_data + 2));
                png->transparency_key.b = ntohs_manual(*(uint16_t*)(chunk_data + 4));
            } else if (png->color_type == 3 && png->palette) {
                png->has_transparency_key = false;
                for (unsigned int i=0; i < chunk_length && i < png->palette_size; ++i) png->palette[i].a = chunk_data[i];
            }
        } else if (strncmp((const char*)chunk_type, "IDAT", 4) == 0) {
            compressed_data = realloc(compressed_data, compressed_size + chunk_length);
            memcpy(compressed_data + compressed_size, chunk_data, chunk_length);
            compressed_size += chunk_length;
        } else if (strncmp((const char*)chunk_type, "IEND", 4) == 0) { break; }
        current_ptr += 12 + chunk_length;
    }
    uint8_t gamma_lut[256];
    for(int i=0; i<256; ++i) {
        gamma_lut[i] = (uint8_t)roundf(powf(i / 255.0f, png->file_gamma) * 255.0f);
    }
    size_t source_bpp_calc = (png->color_type==2)?3:(png->color_type==6)?4:(png->color_type==4)?2:1;
    size_t uncompressed_size = png->height * (1 + (png->width * png->bit_depth * source_bpp_calc + 7) / 8);
    unsigned char* uncompressed_data = malloc(uncompressed_size * 2);
    uLongf dest_len = uncompressed_size * 2;
    uncompress(uncompressed_data, &dest_len, compressed_data, compressed_size);
    free(compressed_data);
    png->final_pixel_size = png->width * png->height * png->bytes_per_pixel;
    png->final_pixel_data = calloc(1, png->final_pixel_size);
    if (!png->final_pixel_data) {
        return NULL;
    }
    if (png->interlace_method == 0) {
        size_t stride = (png->width * png->bit_depth * source_bpp_calc + 7) / 8;
        unsigned char* unfiltered_data = malloc(png->height * stride);
        unfilter_pass(png, uncompressed_data, unfiltered_data, png->width, png->height);
        place_pixels(png, unfiltered_data, -1, png->width, png->height, gamma_lut);
        free(unfiltered_data);
    } else {
        const unsigned char* data_ptr = uncompressed_data;
        for (int i=0; i<7; i++) {
            uint32_t pass_w = (png->width - adam7_start_x[i] + adam7_step_x[i] - 1) / adam7_step_x[i];
            uint32_t pass_h = (png->height - adam7_start_y[i] + adam7_step_y[i] - 1) / adam7_step_y[i];
            if (pass_w==0||pass_h==0) continue;
            size_t pass_stride = (pass_w * png->bit_depth * source_bpp_calc + 7) / 8;
            unsigned char* unfiltered_pass = malloc(pass_h * pass_stride);
            unfilter_pass(png, data_ptr, unfiltered_pass, pass_w, pass_h);
            place_pixels(png, unfiltered_pass, i, pass_w, pass_h, gamma_lut);
            data_ptr += pass_h * (1 + pass_stride);
            free(unfiltered_pass);
        }
    }
    free(uncompressed_data);
    return png;
}

size_t
fweight(FILE *fptr)
{
    long fsize;
    fseek(fptr, 0, SEEK_END);
    fsize = ftell(fptr);
    rewind(fptr);
    if (fsize < 0) {
        return 0;
    }
    return (size_t)fsize;
}

unsigned char *
_fread(FILE *fptr, size_t fsize)
{
    unsigned char *buffer;
    size_t buffer_size;
    buffer = malloc(fsize);
    if (!buffer) {
        return NULL;
    }
    buffer_size = fread(buffer, 1, fsize, fptr);
    if (buffer_size != fsize) {
        free(buffer);
        return NULL;
    }
    fclose(fptr);
    return buffer;
}

PngImage*
png_load_from_file(const char *fname)
{
    FILE *fptr;
    size_t fsize;
    unsigned char *buffer;

    PngImage* img;

    fptr = fopen(fname, "rb");
    if (!fptr) {
        return NULL;
    }
    fsize = fweight(fptr);
    if (!fsize) {
        fclose(fptr);
        return NULL;
    }
    buffer = _fread(fptr, fsize);
    if (!buffer) {
        fclose(fptr);
        return NULL;
    }

    img = png_load_from_data(buffer, fsize);
    return img;
}

static void place_pixels(PngImage* png, const unsigned char* pass_pixels, int pass_index, uint32_t pass_width, uint32_t pass_height, const uint8_t gamma_lut[256]) {
    size_t source_bpp = (png->bit_depth < 8) ? 1 : (png->bit_depth / 8) * ((png->color_type==2)?3:(png->color_type==6)?4:(png->color_type==4)?2:1);

    for (uint32_t py = 0; py < pass_height; py++) {
        for (uint32_t px = 0; px < pass_width; px++) {
            uint32_t final_x, final_y;
            if (pass_index == -1) { final_x = px; final_y = py; }
            else { final_x = px * adam7_step_x[pass_index] + adam7_start_x[pass_index]; final_y = py * adam7_step_y[pass_index] + adam7_start_y[pass_index]; }
            if (final_x >= png->width || final_y >= png->height) continue;
            unsigned char* out_pixel = png->final_pixel_data + (final_y * png->width + final_x) * png->bytes_per_pixel;
            uint8_t r=0, g=0, b=0;
            bool is_transparent = false;
            if (png->color_type == 3) {
                uint16_t index = get_pixel_value_at(pass_pixels, px, py, pass_width, png->bit_depth);
                if (index < png->palette_size) {
                    if (png->palette[index].a < 128) { is_transparent = true; }
                    else { r = png->palette[index].r; g = png->palette[index].g; b = png->palette[index].b; }
                }
            } else if (png->color_type == 0 || png->color_type == 4) {
                uint16_t gray16 = get_pixel_value_at(pass_pixels, px, py, pass_width * source_bpp, png->bit_depth);
                if (png->has_transparency_key && gray16 == png->transparency_key.r) { is_transparent = true; }
                else { r = g = b = (png->bit_depth == 16) ? (gray16 >> 8) : (gray16 * (255.0f / ((1 << png->bit_depth) - 1))); }
            } else if (png->color_type == 2 || png->color_type == 6) {
                if (png->bit_depth == 16) {
                    const uint16_t* in16 = (const uint16_t*)(pass_pixels + (py * pass_width + px) * source_bpp);
                    uint16_t r16 = ntohs_manual(in16[0]), g16 = ntohs_manual(in16[1]), b16 = ntohs_manual(in16[2]);
                    if (png->has_transparency_key && r16==png->transparency_key.r && g16==png->transparency_key.g && b16==png->transparency_key.b) { is_transparent = true; }
                    else { r = r16 >> 8; g = g16 >> 8; b = b16 >> 8; }
                } else {
                    const uint8_t* in8 = pass_pixels + (py * pass_width + px) * source_bpp;
                    r = in8[0]; g = in8[1]; b = in8[2];
                }
            }
            if (is_transparent) {
                out_pixel[0] = out_pixel[1] = out_pixel[2] = 128;
            } else {
                out_pixel[0] = gamma_lut[r];
                out_pixel[1] = gamma_lut[g];
                out_pixel[2] = gamma_lut[b];
            }
        }
    }
}

static uint16_t get_pixel_value_at(const unsigned char* data, uint32_t x, uint32_t y, uint32_t pass_width, uint8_t bit_depth) {
    if (bit_depth >= 8) {
        size_t bpp = (bit_depth == 8) ? 1 : 2;
        if (bpp == 1) return data[y * pass_width + x];
        else { const uint16_t* data16 = (const uint16_t*)data; return ntohs_manual(data16[y * pass_width + x]); }
    }
    size_t bit_pos = (y * pass_width + x) * bit_depth;
    size_t byte_pos = bit_pos / 8;
    int bit_in_byte = 7 - (bit_pos % 8);
    int shift_amount = bit_in_byte - bit_depth + 1;
    return (data[byte_pos] >> shift_amount) & ((1 << bit_depth) - 1);
}
static size_t get_source_bytes_per_pixel(uint8_t color_type, uint8_t bit_depth) { if (bit_depth < 8) return 1; size_t bytes = bit_depth / 8; switch(color_type){ case 2: return bytes * 3; case 4: return bytes * 2; case 6: return bytes * 4; default: return bytes; }}
static int unfilter_pass(const PngImage* png, const unsigned char* src, unsigned char* dst, uint32_t pass_width, uint32_t pass_height) {
    size_t stride = (pass_width * png->bit_depth * get_source_bytes_per_pixel(png->color_type, 8) + 7) / 8;
    if (stride == 0) return 0;
    size_t filter_bpp = get_source_bytes_per_pixel(png->color_type, png->bit_depth);
    for (uint32_t y = 0; y < pass_height; y++) {
        uint8_t filter_type = *src++;
        const unsigned char* prev_line = (y == 0) ? NULL : (dst - stride);
        for (uint32_t x = 0; x < stride; x++) {
            uint8_t raw = src[x];
            uint8_t left = (x >= filter_bpp) ? dst[x - filter_bpp] : 0;
            uint8_t up = (prev_line != NULL) ? prev_line[x] : 0;
            uint8_t up_left = (prev_line != NULL && x >= filter_bpp) ? prev_line[x - filter_bpp] : 0;
            switch (filter_type) {
                case 0: dst[x] = raw; break; case 1: dst[x] = raw + left; break; case 2: dst[x] = raw + up; break;
                case 3: dst[x] = raw + ((left + up) / 2); break;
                case 4: { int p = left + up - up_left; int pa = abs(p - left), pb = abs(p - up), pc = abs(p - up_left); dst[x] = raw + ((pa <= pb && pa <= pc) ? left : (pb <= pc ? up : up_left)); break; }
                default: return -1;
            }
        }
        src += stride; dst += stride;
    }
    return 0;
}
static uint32_t ntohl_manual(uint32_t n) { unsigned char *p = (unsigned char *)&n; return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t ntohs_manual(uint16_t n) { unsigned char *p = (unsigned char *)&n; return ((uint16_t)p[0] << 8) | p[1]; }