#ifndef SETJMP_H
#define SETJMP_H
typedef int jmp_buf[1];
#define setjmp(env) 0
#define longjmp(env, val) do {} while(0)
#endif
