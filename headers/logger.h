#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef struct Logger {
    FILE* file;
    char filename[256];
} Logger;

int log_init(Logger* logger, const char* filename);
void log_close(Logger* logger);
void log_message(Logger* logger, const char* format, ...);
void log_error(Logger* logger, const char* format, ...);

#endif 