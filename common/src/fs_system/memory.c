#include <stddef.h>
#include <stdint.h>

/* GCC's alias type permits words to copy arbitrary object representations.
   Never issue a word load/store until its address is actually aligned. */
/* GCC 的别名类型允许按字复制任意对象表示；仅在地址实际对齐时访问整字。 */
typedef uint32_t CopyWord __attribute__((may_alias));
_Static_assert(sizeof(CopyWord)==4,"copy word width");

/* Freestanding aggregate copies must not depend on either title's SDK ABI.
   Volatile destinations also prevent the compiler from re-emitting itself. */
/* 独立编译生成的结构复制不依赖两个软件的 SDK 地址；volatile 防止再次生成同名调用。 */
void *memcpy(void *destination,const void *source,size_t size)
{
    volatile unsigned char *out=destination;
    const unsigned char *in=source;
    if (!(((uintptr_t)out|(uintptr_t)in)&3u)) {
        volatile CopyWord *words=(volatile CopyWord *)out;
        const CopyWord *input=(const CopyWord *)in;
        while (size>=16) {
            words[0]=input[0];words[1]=input[1];
            words[2]=input[2];words[3]=input[3];
            words+=4;input+=4;size-=16;
        }
        while (size>=4) { *words++=*input++;size-=4; }
        out=(volatile unsigned char *)words;
        in=(const unsigned char *)input;
    }
    while (size--) *out++=*in++;
    return destination;
}

void *memset(void *destination,int value,size_t size)
{
    volatile unsigned char *out=destination;
    while (size && ((uintptr_t)out&3u)) { *out++=(unsigned char)value;size--; }
    CopyWord fill=(CopyWord)(unsigned char)value*0x01010101u;
    volatile CopyWord *words=(volatile CopyWord *)out;
    while (size>=16) {
        words[0]=fill;words[1]=fill;words[2]=fill;words[3]=fill;
        words+=4;size-=16;
    }
    while (size>=4) { *words++=fill;size-=4; }
    out=(volatile unsigned char *)words;
    while (size--) *out++=(unsigned char)value;
    return destination;
}

size_t strlen(const char *text) { const char *end=text;while(*end)end++;return (size_t)(end-text); }
int strcmp(const char *a,const char *b) { while(*a && *a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b; }
int strncmp(const char *a,const char *b,size_t n)
{ while(n && *a && *a==*b){a++;b++;n--;}return n?(unsigned char)*a-(unsigned char)*b:0; }
char *strcpy(char *out,const char *in) { char *first=out;while((*out++=*in++)){}return first; }
char *strcat(char *out,const char *in) { strcpy(out+strlen(out),in);return out; }
char *strchr(const char *text,int c) { do{if(*text==(char)c)return (char *)text;}while(*text++);return 0; }
char *strstr(const char *text,const char *part)
{ size_t n=strlen(part);for(;;text++){if(!strncmp(text,part,n))return (char *)text;if(!*text)return 0;} }
int memcmp(const void *left,const void *right,size_t n)
{ const unsigned char *a=left,*b=right;while(n--){if(*a!=*b)return *a-*b;a++;b++;}return 0; }
