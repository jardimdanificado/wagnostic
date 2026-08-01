#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void  free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);

double strtod(const char *nptr, char **endptr);
long   strtol(const char *nptr, char **endptr, int base);

void  qsort(void *base, size_t nmemb, size_t size,
            int (*compar)(const void *, const void *));
int   abs(int x);
void  abort(void);
void  exit(int status);

#endif
