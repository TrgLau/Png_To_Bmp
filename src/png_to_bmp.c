#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "../headers/png_to_bmp.h"
#include "../headers/png.h"
#include "../headers/logger.h"

#pragma pack(push, 1)

typedef struct {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BITMAPINFOHEADER;

#pragma pack(pop)

int png_save_to_bmp(Logger* logger, const char* output_filename, const PngImage* png, const unsigned char* pixel_data) {
    if (!png || !pixel_data) {
        log_error(logger, "Données d'image invalides pour la conversion BMP.");
        return -1;
    }
    if (png->bytes_per_pixel < 3) {
        log_error(logger, "Conversion BMP non supportée pour les images avec moins de 3 octets par pixel.");
        return -1;
    }
    FILE* bmp_file = fopen(output_filename, "wb");
    if (!bmp_file) {
        log_error(logger, "Erreur lors de la création du fichier BMP '%s': %s", output_filename, strerror(errno));
        return -1;
    }
    const int width = png->width;
    const int height = png->height;
    const int bpp_png = png->bytes_per_pixel;
    int padding = (4 - (width * 3) % 4) % 4;
    uint32_t row_size_bmp = width * 3 + padding;
    uint32_t pixel_data_size = row_size_bmp * height;
    BITMAPFILEHEADER file_header;
    file_header.bfType = 0x4D42;
    file_header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + pixel_data_size;
    file_header.bfReserved1 = 0;
    file_header.bfReserved2 = 0;
    file_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    BITMAPINFOHEADER info_header;
    info_header.biSize = sizeof(BITMAPINFOHEADER);
    info_header.biWidth = width;
    info_header.biHeight = height;
    info_header.biPlanes = 1;
    info_header.biBitCount = 24; 
    info_header.biCompression = 0; 
    info_header.biSizeImage = pixel_data_size;
    info_header.biXPelsPerMeter = 2835; 
    info_header.biYPelsPerMeter = 2835; 
    info_header.biClrUsed = 0;
    info_header.biClrImportant = 0;
    fwrite(&file_header, sizeof(file_header), 1, bmp_file);
    fwrite(&info_header, sizeof(info_header), 1, bmp_file);
    unsigned char padding_bytes[3] = {0, 0, 0};
    for (int y = height - 1; y >= 0; y--) {
        const unsigned char* png_row = pixel_data + (y * width * bpp_png);
        for (int x = 0; x < width; x++) {
            const unsigned char* png_pixel = png_row + (x * bpp_png);
            unsigned char bmp_pixel[3] = { png_pixel[2], png_pixel[1], png_pixel[0] };
            fwrite(bmp_pixel, sizeof(bmp_pixel), 1, bmp_file);
        }
        if (padding > 0) {
            fwrite(padding_bytes, 1, padding, bmp_file);
        }
    }
    fclose(bmp_file);
    log_message(logger, "Image sauvegardée avec succès sous : %s", output_filename);
    return 0;
}