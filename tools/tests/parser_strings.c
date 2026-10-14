/* Real engine console/config quoted strings, including exact heap boundaries. */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char com_token[1024];
static void COM_AssertMainThread(const char *reason) { (void)reason; }
static int ishexcode(int c) { return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'); }
static int dehex(int c) { return c<='9' ? c-'0' : (c|32)-'a'+10; }
/* ENGINE_FUNCTIONS */

static void check(const char *input)
{
    size_t length=strlen(input);
    char *source=malloc(length+1);
    assert(source); memcpy(source,input,length+1);
    for (size_t capacity=0; capacity<20; capacity++) {
        char *output=capacity ? malloc(capacity) : NULL;
        assert(!capacity || output);
        if (capacity) memset(output,0xa5,capacity);
        size_t written=999;
        char *end=COM_ParseCString(source,output,capacity,&written);
        if (!capacity) assert(!end && written==0);
        else {
            assert(written<capacity && output[written]==0);
            assert(!end || (end>=source && end<=source+length));
        }
        free(output);
    }
    free(source);
}

int main(void)
{
    const char *cases[]={"", " ", "//", "// comment", "word", "\"\"", "\"a\"", "\"long word with spaces\"",
        "\"unterminated", "\"\\", "\"\\x", "\"\\xF", "\"\\xFF", "\"\\q", "\"\\\r", "\"\\\r\n",
        "\"a\\n\\t\\r\\\\\\\"z\" tail", "// comment\n\"value\"", "\"\\x00\""};
    for (unsigned int i=0; i<sizeof(cases)/sizeof(cases[0]); i++) check(cases[i]);
    char out[32]; size_t written=0;
    char *end=COM_ParseCString("\"a\\n\\x42\\\"z\" tail",out,sizeof(out),&written);
    assert(!strcmp(out,"a\nB\"z") && written==5 && !strcmp(end," tail"));
    for (int byte=1; byte<256; byte++) {
        char input[]={'"','\\',(char)byte,0};
        check(input);
    }
    char longword[2048]; memset(longword,'a',sizeof(longword)-1); longword[2047]=0; check(longword);
    puts("Config strings: capacities 0-19, all escape bytes, truncation, comments and decoded content passed");
}
