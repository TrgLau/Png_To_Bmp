#ifndef PNG_TO_BMP_H
#define PNG_TO_BMP_H

#include "png.h"
#include "logger.h"

int png_save_to_bmp(Logger* logger, const char* output_filename, const PngImage* png, const unsigned char* pixel_data);

#endif 