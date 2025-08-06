#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <limits.h> 
#include <errno.h>

#include "../headers/png.h" 
#include "../headers/png_to_bmp.h"
#include "../headers/logger.h"

int main(int argc, char *argv[]) 
{
    Logger logger;

    char *input;
    char *output;

    PngImage* img;

    if (log_init(&logger, "conversion.log") != 0) {
        fprintf(stderr, "err\n");
        return EXIT_FAILURE;
    }

    if (argc != 3) {
        log_error(&logger, "Usage: %s <source.png> <destination.bmp>", argv[0]);
        log_close(&logger);
        return EXIT_FAILURE;
    }
    input = argv[1];
    output = argv[2];

    log_message(&logger, "\n--- Traitement du fichier PNG : %s ---", input);
    img = png_load_from_file(input);
    if (!img) {
        log_error(&logger, "Echec du chargement ou du traitement du fichier PNG. Arrêt.");
        png_destroy(img);
        log_close(&logger);
        return EXIT_FAILURE;
    }
    log_message(&logger, "Données d'image préparées avec succès.");

    log_message(&logger, "\n--- Conversion en BMP ---");
    if (png_save_to_bmp(&logger, output, img, img->final_pixel_data) != 0) {
        log_error(&logger, "La sauvegarde en BMP a échoué.");
    }

    log_message(&logger, "\n--- Nettoyage de la mémoire ---");
    png_destroy(img);
    log_message(&logger, "Mémoire libérée.");
    log_close(&logger);
    return EXIT_SUCCESS;
}