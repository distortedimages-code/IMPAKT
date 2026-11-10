/* Minimal runtime for the freestanding wasm build (compiled with -fno-builtin). */
typedef __SIZE_TYPE__ size_t;

void* memset (void* d, int c, size_t n)
{
    unsigned char* p = (unsigned char*) d;
    while (n--) *p++ = (unsigned char) c;
    return d;
}

void* memcpy (void* d, const void* s, size_t n)
{
    unsigned char* a = (unsigned char*) d;
    const unsigned char* b = (const unsigned char*) s;
    while (n--) *a++ = *b++;
    return d;
}

void* memmove (void* d, const void* s, size_t n)
{
    unsigned char* a = (unsigned char*) d;
    const unsigned char* b = (const unsigned char*) s;
    if (a < b) while (n--) *a++ = *b++;
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
}
