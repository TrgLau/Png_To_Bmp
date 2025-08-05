#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <limits.h> 
#include <errno.h>

#include "../headers/png.h" 
#include "../headers/png_to_bmp.h"
#include "../headers/logger.h"

unsigned char *readFile(Logger* logger, const char *path, long *fileSize) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        log_error(logger, "Erreur ouverture du fichier '%s': %s", path, strerror(errno));
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size <= 0) {
        log_error(logger, "Fichier '%s' vide ou erreur.", path);
        fclose(f);
        return NULL;
    }
    unsigned char *buffer = malloc(size);
    if (!buffer) {
        log_error(logger, "Erreur malloc: %s", strerror(errno));
        fclose(f);
        return NULL;
    }
    if (fread(buffer, 1, size, f) != (size_t)size) {
        log_error(logger, "Erreur lecture du fichier '%s': %s", path, strerror(errno));
        free(buffer);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *fileSize = size;
    return buffer;
}

int main(int argc, char *argv[]) {
    Logger logger;
    if (log_init(&logger, "conversion.log") != 0) {
        return EXIT_FAILURE;
    }
    if (argc != 3) {
        log_error(&logger, "Usage: %s <source.png> <destination.bmp>", argv[0]);
        log_close(&logger);
        return EXIT_FAILURE;
    }
    const char* png_filename = argv[1];
    const char* bmp_filename = argv[2];
    log_message(&logger, "\n--- Traitement du fichier PNG : %s ---", png_filename);
    long file_size = 0;
    unsigned char* file_buffer = readFile(&logger, png_filename, &file_size);
    if (file_buffer == NULL) {
        log_close(&logger);
        return EXIT_FAILURE;
    }
    PngImage* my_png = png_load_from_data(file_buffer, file_size);
    free(file_buffer);
    if (my_png == NULL || my_png->final_pixel_data == NULL) {
        log_error(&logger, "Echec du chargement ou du traitement du fichier PNG. Arrêt.");
        png_destroy(my_png);
        log_close(&logger);
        return EXIT_FAILURE;
    }
    log_message(&logger, "Données d'image préparées avec succès.");
    log_message(&logger, "\n--- Conversion en BMP ---");
    if (png_save_to_bmp(&logger, bmp_filename, my_png, my_png->final_pixel_data) != 0) {
        log_error(&logger, "La sauvegarde en BMP a échoué.");
    }
    log_message(&logger, "\n--- Nettoyage de la mémoire ---");
    png_destroy(my_png);
    log_message(&logger, "Mémoire libérée.");
    log_close(&logger);
    return EXIT_SUCCESS;
}