#include <assert.h>
#include <stdio.h>
#include <string.h>
enum { CVAR_ARCHIVE=1, CVAR_USERINFO=2, CVAR_SERVERINFO=4 };
typedef struct { int ival, modifiedcount; float value; char *string; } cvar_t;
typedef struct { int handle, integer, modificationCount; float value; char string[256]; } q3vmcvar_t;
static cvar_t vars[2048];
static cvar_t *get(const char *name, const char *value, int flags, const char *description, const char *group)
{
	int index=-1; assert(sscanf(name,"cv%d",&index)==1 && index>=0 && index<2048);
	assert(flags==(CVAR_ARCHIVE|CVAR_USERINFO) && !description && !strcmp(group,"Q3VM cvars"));
	vars[index]=(cvar_t){index,1,(float)index,(char *)value}; return &vars[index];
}
static struct { cvar_t *(*GetNVFDG)(const char *,const char *,int,const char *,const char *); } cv_api={get}, *cvarfuncs=&cv_api;
#define Q_strncpyz(out,text,size) snprintf(out,size,"%s",text)
static void Con_Printf(const char *format) { (void)format; assert(!"unexpected cvar handle exhaustion"); }
int VMQ3_Cvar_Update(q3vmcvar_t *v);
/* CVAR_STORAGE */
/* ENGINE_FUNCTIONS */
int main(void)
{
	q3vmcvar_t cv={0}; char name[32];
	for (int i=0;i<2048;i++) {
		snprintf(name,sizeof(name),"cv%d",i);
		assert(VMQ3_Cvar_Register(&cv,name,"value",CVAR_ARCHIVE|CVAR_USERINFO|128)==i+1);
		assert(cv.integer==i && cv.value==i && !strcmp(cv.string,"value"));
	}
	assert(VMQ3_Cvar_Register(&cv,"cv20","updated",CVAR_ARCHIVE|CVAR_USERINFO)==21);
	assert(!strcmp(cv.string,"updated"));
	cv.handle=0;assert(!VMQ3_Cvar_Update(&cv));
	cv.handle=MAX_VMQ3_CVARS+1;assert(!VMQ3_Cvar_Update(&cv));
	assert(!VMQ3_Cvar_Update(NULL));
	puts("Q3 cvars: large mod registrations, shared handles, updates and invalid handles passed");
	return 0;
}
