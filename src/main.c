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
    unsigned long long size = ftell(f);
    rewind(f);
    if (size <= 0) {
        log_error(logger, "Fichier '%s' vide ou erreur de lecture de la taille.", path);
        fclose(f);
        return NULL;
    }
    unsigned char *buffer = malloc(size);
    if (!buffer) {
        log_error(logger, "Erreur d'allocation mémoire (malloc) pour le fichier: %s", strerror(errno));
        fclose(f);
        return NULL;
    }
    if (fread(buffer, 1, size, f) != size) {
        log_error(logger, "Erreur de lecture du contenu du fichier '%s': %s", path, strerror(errno));
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
        log_error(&logger, "Utilisation incorrecte du programme.");
        log_message(&logger, "Usage: %s <fichier_source.png> <fichier_destination.bmp>", argv[0]);
        log_close(&logger);
        return EXIT_FAILURE;
    }
    const char* png_filename = argv[1];
    const char* bmp_filename = argv[2];
    char current_path[PATH_MAX];
    if (getcwd(current_path, sizeof(current_path)) != NULL) {
        log_message(&logger, "Le répertoire courant est : %s", current_path);
    } else {
        log_error(&logger, "Erreur lors de la récupération du répertoire courant: %s", strerror(errno));
        log_close(&logger);
        return 1;
    }
    long file_size = 0;
    log_message(&logger, "\n--- Traitement du fichier PNG : %s ---", png_filename);
    unsigned char* file_buffer = readFile(&logger, png_filename, &file_size);
    if (file_buffer == NULL) {
        log_close(&logger);
        return EXIT_FAILURE;
    }
    PngImage* my_png = png_load_from_data(&logger, file_buffer, file_size);
    free(file_buffer);
    if (my_png == NULL) {
        log_error(&logger, "Echec du chargement du fichier PNG. Arrêt.");
        log_close(&logger);
        return EXIT_FAILURE;
    }
    log_message(&logger, "\n--- Application du défiltrage ---");
    size_t final_pixel_size = (size_t)my_png->width * my_png->height * my_png->bytes_per_pixel;
    unsigned char* final_pixels = malloc(final_pixel_size);
    if (!final_pixels) {
        log_error(&logger, "Erreur d'allocation pour les pixels finaux: %s", strerror(errno));
        png_destroy(my_png);
        log_close(&logger);
        return EXIT_FAILURE;
    }
    if (unfilter_png(&logger, my_png, final_pixels) == 0) {
        log_message(&logger, "Défiltrage terminé avec succès !");
        log_message(&logger, "Les %zu octets de pixels finaux sont prêts.", final_pixel_size);
        
        log_message(&logger, "\n--- Conversion en BMP ---");
        if (png_save_to_bmp(&logger, bmp_filename, my_png, final_pixels) != 0) {
        log_error(&logger, "La conversion en BMP a échoué.");
    }
    } else {
        log_error(&logger, "Une erreur est survenue durant le défiltrage.");
    }
    log_message(&logger, "\n--- Nettoyage de la mémoire ---");
    free(final_pixels);
    png_destroy(my_png);
    log_message(&logger, "Mémoire libérée.");
    log_close(&logger);
    return EXIT_SUCCESS;
}