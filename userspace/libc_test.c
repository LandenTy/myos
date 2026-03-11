#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "lib/string.h"

int main() {
    // ── printf test ───────────────────────────────────────────────────────────
    printf("=== libc test ===\n");
    printf("Hello %s! The answer is %d (0x%x)\n", "world", 42, 42);
    printf("Padding: [%5d] [%-5d] [%05d]\n", 7, 7, 7);

    // ── malloc / free test ────────────────────────────────────────────────────
    printf("\n-- malloc/free --\n");
    char *buf = (char*)malloc(64);
    if (!buf) { printf("malloc failed!\n"); }
    else {
        strcpy(buf, "Hello from heap!");
        printf("buf = \"%s\"\n", buf);
        free(buf);
        printf("free OK\n");
    }

    // ── realloc test ──────────────────────────────────────────────────────────
    int *arr = (int*)malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) arr[i] = i * 10;
    arr = (int*)realloc(arr, 8 * sizeof(int));
    for (int i = 4; i < 8; i++) arr[i] = i * 10;
    printf("realloc arr: ");
    for (int i = 0; i < 8; i++) { printf("%d", arr[i]); if (i<7) printf(","); }
    printf("\n");
    free(arr);

    // ── string functions test ─────────────────────────────────────────────────
    printf("\n-- string --\n");
    char s[32];
    strcpy(s, "Hello");
    strcat(s, ", Mars!");
    printf("strcat: \"%s\"\n", s);
    printf("strlen: %d\n", (int)strlen(s));
    printf("strcmp(\"abc\",\"abc\") = %d\n", strcmp("abc", "abc"));
    printf("strcmp(\"abc\",\"abd\") = %d\n", strcmp("abc", "abd"));

    // ── fopen / fread test ────────────────────────────────────────────────────
    printf("\n-- file I/O --\n");
    FILE *f = fopen("/readme.txt", "r");
    if (!f) {
        printf("fopen failed!\n");
    } else {
        printf("size = %d bytes\n", (int)f->size);
        char rbuf[64];
        size_t n = fread(rbuf, 1, 63, f);
        rbuf[n] = '\0';
        printf("contents: \"%s\"\n", rbuf);
        fclose(f);
    }

    printf("\n=== all tests done ===\n");
    return 0;
}

