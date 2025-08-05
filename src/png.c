#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include <math.h> 

#include "../headers/png.h" 
#include "../headers/logger.h"

typedef struct {
    unsigned char* data;
    size_t size;
} IdatData;

static uint8_t calculate_bytes_per_pixel(uint8_t color_type, uint8_t bit_depth) {
    int bpp = 0;
    switch (color_type) {
        case 0: bpp = (bit_depth + 7) / 8; break; 
        case 2: bpp = (bit_depth / 8) * 3; break; 
        case 3: bpp = 1; break;                  
        case 4: bpp = (bit_depth / 8) * 2; break;
        case 6: bpp = (bit_depth / 8) * 4; break;
    }
    return bpp;
}

uint32_t ntohl_manual(uint32_t n) {
    unsigned char *p = (unsigned char *)&n;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

void png_destroy(PngImage* png) {
    if (png != NULL) {
        free(png->raw_data);
        free(png);        
    }
}

PngImage* png_load_from_data(Logger* logger, const unsigned char* data, size_t size) {
    const uint8_t png_sig_bytes[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8 || memcmp(data, png_sig_bytes, 8) != 0) {
        log_error(logger, "Signature PNG invalide.");
        return NULL;
    }
    log_message(logger, "Signature PNG valide.");
    PngImage* png = calloc(1, sizeof(PngImage));
    if (!png) {
        log_error(logger, "Allocation mémoire échouée pour la structure PngImage.");
        return NULL;
    }
    IdatData idat_store = { .data = NULL, .size = 0 };
    bool iend_found = false;
    bool ihdr_found = false;
    const unsigned char* current_ptr = data + 8; 
    const unsigned char* end_ptr = data + size;
    while (current_ptr + 8 <= end_ptr && !iend_found) {
        uint32_t chunk_length;
        memcpy(&chunk_length, current_ptr, 4);
        chunk_length = ntohl_manual(chunk_length);
        char chunk_type[5] = {0};
        memcpy(chunk_type, current_ptr + 4, 4);
        const unsigned char* chunk_data_ptr = current_ptr + 8;
        if (chunk_data_ptr + chunk_length + 4 > end_ptr) {
            log_error(logger, "Chunk '%s' dépasse la taille du fichier.", chunk_type);
            break;
        }
        if (!ihdr_found && strncmp(chunk_type, "IHDR", 4) != 0) {
            log_error(logger, "Le premier chunk n'est pas IHDR.");
            png_destroy(png);
            return NULL;
        }
        if (strncmp(chunk_type, "IHDR", 4) == 0) {
            ihdr_found = true;
            if (chunk_length != 13) {
                log_error(logger, "Taille incorrecte pour le chunk IHDR (%u octets).", chunk_length);
                png_destroy(png);
                return NULL;
            }
            const unsigned char* p = chunk_data_ptr;
            png->width = ntohl_manual(*(uint32_t*)(p));
            png->height = ntohl_manual(*(uint32_t*)(p + 4));
            png->bit_depth = *(p + 8);
            png->color_type = *(p + 9);
            png->bytes_per_pixel = calculate_bytes_per_pixel(png->color_type, png->bit_depth);
            if (png->bytes_per_pixel == 0) {
                 log_error(logger, "Type de couleur (%u) ou profondeur de bit (%u) non supporté.", png->color_type, png->bit_depth);
                 png_destroy(png);
                 return NULL;
            }
            log_message(logger, "--- En-tête PNG (IHDR) ---");
            log_message(logger, "Largeur                : %u pixels", png->width);
            log_message(logger, "Hauteur                : %u pixels", png->height);
            log_message(logger, "Profondeur de bit      : %u", png->bit_depth);
            log_message(logger, "Type de couleur        : %u", png->color_type);
            log_message(logger, "Octets par pixel       : %u", png->bytes_per_pixel);
            log_message(logger, "--------------------------");
        }
        else if (strncmp(chunk_type, "IDAT", 4) == 0) {
            if (chunk_length > 0) {
                unsigned char* new_data_block = realloc(idat_store.data, idat_store.size + chunk_length);
                if (new_data_block == NULL) {
                    log_error(logger, "Allocation memoire echouee pour les donnees IDAT.");
                    free(idat_store.data);
                    png_destroy(png);
                    return NULL;
                }
                idat_store.data = new_data_block;
                memcpy(idat_store.data + idat_store.size, chunk_data_ptr, chunk_length);
                idat_store.size += chunk_length;
            }
        }
        else if (strncmp(chunk_type, "IEND", 4) == 0) {
            iend_found = true;
        }
        current_ptr += 8 + chunk_length + 4;
    }
    if (idat_store.data != NULL) {
        log_message(logger, "\n--- Décompression des données IDAT ---");
        log_message(logger, "Taille totale des données IDAT concaténées : %zu octets.", idat_store.size);
        size_t expected_size = (size_t)png->height * (png->width * png->bytes_per_pixel + 1);
        png->raw_data = malloc(expected_size);
        if (!png->raw_data) {
            log_error(logger, "Allocation mémoire échouée pour la décompression.");
            free(idat_store.data);
            png_destroy(png);
            return NULL;
        }
        uLongf dest_len = expected_size;
        int result = uncompress(png->raw_data, &dest_len, idat_store.data, idat_store.size);
        free(idat_store.data); 
        if (result != Z_OK) {
            log_error(logger, "Erreur de décompression zlib : %d (%s)", result, zError(result));
            png_destroy(png);
            return NULL;
        }
        if (dest_len != expected_size) {
            log_message(logger, "Avertissement: La taille décompressée (%lu) ne correspond pas à la taille attendue (%zu).", dest_len, expected_size);
        }
        png->raw_data_size = dest_len;
        log_message(logger, "Décompression réussie. Taille des données brutes (filtrées) : %zu octets.\n", png->raw_data_size);
    }
    return png;
}

int unfilter_png(Logger* logger, const PngImage* png, unsigned char* out_pixels) {
    if (!png || !png->raw_data || !out_pixels) {
        return -1;
    }
    uint32_t stride = png->width * png->bytes_per_pixel;
    const unsigned char* src = png->raw_data;
    unsigned char* dst = out_pixels;
    for (uint32_t y = 0; y < png->height; y++) {
        uint8_t filter_type = *src++;
        const unsigned char* prev_line = (y == 0) ? NULL : (dst - stride);
        for (uint32_t x = 0; x < stride; x++) {
            uint8_t raw = src[x];
            uint8_t left = (x >= png->bytes_per_pixel) ? dst[x - png->bytes_per_pixel] : 0;
            uint8_t up = (prev_line != NULL) ? prev_line[x] : 0;
            uint8_t up_left = (prev_line != NULL && x >= png->bytes_per_pixel) ? prev_line[x - png->bytes_per_pixel] : 0;
            switch (filter_type) {
                case 0: dst[x] = raw; break;
                case 1: dst[x] = raw + left; break;
                case 2: dst[x] = raw + up; break;
                case 3: dst[x] = raw + ((left + up) >> 1); break; 
                case 4: { 
                    int p = left + up - up_left;
                    int pa = abs(p - left);
                    int pb = abs(p - up);
                    int pc = abs(p - up_left);
                    uint8_t paeth = (pa <= pb && pa <= pc) ? left : (pb <= pc ? up : up_left);
                    dst[x] = raw + paeth;
                    break;
                }
                default:
                    log_error(logger, "Type de filtre inconnu : %d sur la ligne %u", filter_type, y);
                    return -1;
            }
        }
        src += stride;
        dst += stride;
    }
    return 0;
}