/* Real Quake 3 script escapes; arbitrarily long digits must clamp without UB. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { char *script_p; } script_t;
static int warnings,errors;
static void ScriptWarning(script_t *s,const char *format,...) { (void)s;(void)format;warnings++; }
static void ScriptError(script_t *s,const char *format,...) { (void)s;(void)format;errors++; }
/* ENGINE_FUNCTIONS */

static void check(const char *text,int success,unsigned char expected,int warning,int error)
{
    size_t len=strlen(text); char *copy=malloc(len+1); assert(copy); memcpy(copy,text,len+1);
    script_t script={.script_p=copy}; char out=0;
    warnings=errors=0;
    assert(PS_ReadEscapeCharacter(&script,&out)==success);
    assert(script.script_p>=copy && script.script_p<=copy+len);
    assert(warnings==warning && errors==error);
    if (success) assert((unsigned char)out==expected);
    free(copy);
}
int main(void)
{
    check("\\n",1,'\n',0,0); check("\\r",1,'\r',0,0); check("\\t",1,'\t',0,0);
    check("\\\\",1,'\\',0,0); check("\\\"",1,'"',0,0);
    check("\\x41",1,'A',0,0); check("\\255",1,255,0,0); check("\\256",1,255,1,0);
    check("\\xFF",1,255,0,0); check("\\x100",1,255,1,0);
    check("\\",0,0,0,1); check("\\q",0,0,0,1);
    char big[4096]; big[0]='\\'; memset(big+1,'9',sizeof(big)-2); big[4095]=0; check(big,1,255,1,0);
    big[1]='x'; memset(big+2,'F',sizeof(big)-3); check(big,1,255,1,0);
    for (int i=0; i<256; i++) {
        char number[32]; snprintf(number,sizeof(number),"\\%d",i); check(number,1,(unsigned char)i,0,0);
        snprintf(number,sizeof(number),"\\x%X",i); check(number,1,(unsigned char)i,0,0);
    }
    puts("Q3 script escapes: all byte values, 4K numeric escapes, saturation and unterminated input passed");
}
