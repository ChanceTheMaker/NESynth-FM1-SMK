#include <stddef.h>
void *memset(void *p,int v,size_t n) {unsigned char *d=p;while(n--)*d++=(unsigned char)v;return p;}
void *memcpy(void *p,const void *q,size_t n) {unsigned char *d=p;const unsigned char*s=q;while(n--)*d++=*s++;return p;}
int memcmp(const void *p,const void*q,size_t n) {const unsigned char*a=p,*b=q;while(n--){if(*a!=*b)return *a-*b;a++;b++;}return 0;}
void abort(void) {__builtin_trap();}
