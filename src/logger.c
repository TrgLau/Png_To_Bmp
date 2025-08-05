#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#include "../headers/logger.h"

static void get_timestamp(char* buffer, size_t buffer_size) {
    time_t now = time(NULL);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", localtime(&now));
}

int log_init(Logger* logger, const char* filename) {
    if (logger == NULL || filename == NULL) {
        return -1;
    }
    errno_t err = fopen_s(&logger->file, filename, "w");
    if (err != 0) {
        perror("FATAL ERROR: Unable to open log file");
        logger->file = NULL;
        return -1;
    }
    strncpy(logger->filename, filename, sizeof(logger->filename) - 1);
    logger->filename[sizeof(logger->filename) - 1] = '\0';
    char time_buffer[30];
    get_timestamp(time_buffer, sizeof(time_buffer));
    fprintf(logger->file, "Log started at: %s\n\n", time_buffer);
    fflush(logger->file);
    return 0;
}

void log_close(Logger* logger) {
    if (logger != NULL && logger->file != NULL) {
        fprintf(logger->file, "\n--- End of log ---\n");
        fclose(logger->file);
        logger->file = NULL;
    }
}

static void log_generic(Logger* logger, const char* level, const char* format, va_list args) {
    if (logger == NULL || logger->file == NULL) {
        return;
    }
    char time_buffer[30];
    get_timestamp(time_buffer, sizeof(time_buffer));
    fprintf(logger->file, "[%s] [%s] ", time_buffer, level);
    vfprintf(logger->file, format, args);
    size_t len = strlen(format);
    if (len == 0 || (len > 0 && format[len - 1] != '\n')) {
        fputc('\n', logger->file);
    }
    fflush(logger->file);
}

void log_message(Logger* logger, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_generic(logger, "MESSAGE", format, args);
    va_end(args);
}

void log_error(Logger* logger, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_generic(logger, "ERROR", format, args);
    va_end(args);
}