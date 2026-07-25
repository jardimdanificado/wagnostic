// math.h — minimal libc shim for -nostdlib WASM builds
//
// The transcoding target (wasm32) lowers sin/cos/exp/log/pow to
// `env.*` imports. The native host links them from glibc via
// m3_LinkRawFunction. sqrt/fabs/floor lower to native WASM
// instructions and need no import.

#ifndef _MATH_H
#define _MATH_H

#define M_PI   3.14159265358979323846
#define M_E    2.71828182845904523536
#define HUGE_VAL (__builtin_huge_val())

double sin(double x);
double cos(double x);
double exp(double x);
double log(double x);
double pow(double x, double y);
double sqrt(double x);
double fabs(double x);
double floor(double x);
double ceil(double x);
double ldexp(double x, int exp);
double frexp(double x, int *exp);

#endif
