/* Redirect the real Windows server formatter without Windows console APIs. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MAXPRINTMSG 4096
#define CON_WHITEMASK 0
static struct { float value; } sys_nostdout, sys_colorconsole;
static void *hconsoleout;
static int coninput_len;
static char coninput_text[256];
typedef uint32_t conchar_t;
static int putch(int c) { return c; }
static conchar_t *COM_ParseFunString(int flags, char *text, conchar_t *out, size_t size, int markup)
{
    (void)flags; (void)text; (void)size; (void)markup; return out;
}
static void Sys_PrintColouredChars(conchar_t *first, conchar_t *last)
{
    (void)first; (void)last;
}
/* ENGINE_FUNCTIONS */
int main(void)
{
    FILE *sink = tmpfile(); assert(sink);
    int saved = dup(STDOUT_FILENO); assert(saved >= 0);
    assert(dup2(fileno(sink), STDOUT_FILENO) >= 0);
    Sys_Printf("server ready %d\n", 1);
    char text[64] = {0};
    /* Read the descriptor directly: no harness flush may hide lost output. */
    ssize_t size = pread(fileno(sink), text, sizeof(text)-1, 0);
    assert(size == (ssize_t)strlen("server ready 1\n"));
    assert(!strcmp(text, "server ready 1\n"));
    sys_nostdout.value = 1;
    Sys_Printf("hidden\n");
    assert(pread(fileno(sink), text, sizeof(text)-1, 0) == size);
    assert(dup2(saved, STDOUT_FILENO) >= 0);
    close(saved); fclose(sink);
    puts("Windows server output: short redirected messages reach the descriptor before shutdown; quiet mode preserved");
    return 0;
}
