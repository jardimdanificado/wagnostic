#ifndef STDIO_H
#define STDIO_H

typedef unsigned long size_t;
#define NULL ((void*)0)

typedef struct FILE FILE;

int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int sscanf(const char *str, const char *format, ...);

#endif
