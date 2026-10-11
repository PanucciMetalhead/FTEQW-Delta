#include "quakedef.h"

	#include <SDL3/SDL.h>

#ifdef _WIN32
	#include <direct.h>
#else
	#include <fcntl.h>
	#include <sys/stat.h>
	#if defined(__unix__) || defined(__unix) ||defined(__HAIKU__) || (defined(__APPLE__) && defined(__MACH__))	//apple make everything painful.
		#include <unistd.h>
	#endif
#endif

extern SDL_Window *sdlwindow;

#ifndef isDedicated
qboolean isDedicated;
#endif

void Sys_Error (const char *error, ...)
{
	va_list argptr;
	char string[1024];

	va_start (argptr,error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);
	COM_WorkerAbort(string);
	fprintf(stderr, "Error: %s\n", string);

	Sys_Printf ("Quake Error: %s\n", string);

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Sys_Error", string, sdlwindow);

	if (COM_CheckParm("-crashonerror"))
		*(int*)-3 = 0;

	Host_Shutdown ();
	exit (1);
}

void Sys_RecentServer(char *command, char *target, char *title, char *desc)
{
}


#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
qboolean Sys_RandomBytes(qbyte *string, int len)
{
    HCRYPTPROV  prov;

    if(!CryptAcquireContext( &prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        return false;
    }

    if(!CryptGenRandom(prov, len, (BYTE *)string))
    {
        CryptReleaseContext( prov, 0);
        return false;
    }
    CryptReleaseContext(prov, 0);
    return true;
}
#elif defined(__linux__) || defined(BSD) || defined(__APPLE__)
qboolean Sys_RandomBytes(qbyte *string, int len)
{
	qboolean res = false;
	int fd = open("/dev/urandom", 0);
	if (fd != -1)
	{
		res = (read(fd, string, len) == len);
		close(fd);
	}
	return res;
}
#else
qboolean Sys_RandomBytes(qbyte *string, int len)
{
	return false;
}
#endif

static void ApplyColour(unsigned int chrflags)
{
//on win32, SDL usually redirected stdout to a file (as it won't get printed anyway.
//win32 doesn't do ascii escapes, and text editors like to show the gibberish too, so just don't bother emitting any.
#ifndef _WIN32
	static const int ansiremap[8] = {0, 4, 2, 6, 1, 5, 3, 7};
	static int oldflags = CON_WHITEMASK;
	int bg, fg;

	if (oldflags == chrflags)
		return;
	oldflags = chrflags;

	printf("\e[0;"); // reset

	if (chrflags & CON_BLINKTEXT)
		printf("5;"); // set blink

	bg = (chrflags & CON_BGMASK) >> CON_BGSHIFT;
	fg = (chrflags & CON_FGMASK) >> CON_FGSHIFT;

	// don't handle intensive bit for background
	// as terminals differ too much in displaying \e[1;7;3?m
	bg &= 0x7;

	if (chrflags & CON_NONCLEARBG)
	{
		if (fg & 0x8) // intensive bit set for foreground
		{
			printf("1;"); // set bold/intensity ansi flag
			fg &= 0x7; // strip intensive bit
		}

		// set foreground and background colors
		printf("3%i;4%im", ansiremap[fg], ansiremap[bg]);
	}
	else
	{
		switch(fg)
		{
		//to get around wierd defaults (like a white background) we have these special hacks for colours 0 and 7
		case COLOR_BLACK:
			printf("7m"); // set inverse
			break;
		case COLOR_GREY:
			printf("1;30m"); // treat as dark grey
			break;
		case COLOR_WHITE:
			printf("m"); // set nothing else
			break;
		default:
			if (fg & 0x8) // intensive bit set for foreground
			{
				printf("1;"); // set bold/intensity ansi flag
				fg &= 0x7; // strip intensive bit
			}

			printf("3%im", ansiremap[fg]); // set foreground
			break;
		}
	}
#endif
}
//#include <wchar.h>
void Sys_Printf (char *fmt, ...)
{
	va_list		argptr;
	char		text[2048];
	conchar_t	ctext[2048];
	conchar_t       *c, *e;
	wchar_t		w;
	unsigned int codeflags, codepoint;

//	if (nostdout)
//		return;

	va_start (argptr,fmt);
	vsnprintf (text,sizeof(text)-1, fmt,argptr);
	va_end (argptr);

#ifdef SUBSERVERS
	if (SSV_IsSubServer())
	{
		if (SSV_PrintToMaster(text))
			return;
	}
#endif

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

	e = COM_ParseFunString(CON_WHITEMASK, text, ctext, sizeof(ctext), false);

	for (c = ctext; c < e; )
	{
		c = Font_Decode(c, &codeflags, &codepoint);
		if (codeflags & CON_HIDDEN)
			continue;

		if (codepoint == '\n' && (codeflags&CON_NONCLEARBG))
			codeflags &= CON_WHITEMASK;
		ApplyColour(codeflags);
		w = codepoint;
		if (w >= 0xe000 && w < 0xe100)
		{
			/*not all quake chars are ascii compatible, so map those control chars to safe ones so we don't mess up anyone's xterm*/
			if ((w & 0x7f) > 0x20)
				putc(w&0x7f, stdout);
			else if (w & 0x80)
			{
				static char tab[32] = "---#@.@@@@ # >.." "[]0123456789.---";
				putc(tab[w&31], stdout);
			}
			else
			{
				static char tab[32] = ".####.#### # >.." "[]0123456789.---";
				putc(tab[w&31], stdout);
			}
		}
		else if (w < ' ' && w != '\t' && w != '\r' && w != '\n')
			putc('?', stdout);	//don't let anyone print escape codes or other things that could crash an xterm.
		else
		{
			/*putwc doesn't like me. force it in utf8*/
			if (w >= 0x80)
			{
				if (w > 0x800)
				{
					putc(0xe0 | ((w>>12)&0x0f), stdout);
					putc(0x80 | ((w>>6)&0x3f), stdout);
				}
				else
					putc(0xc0 | ((w>>6)&0x1f), stdout);
				putc(0x80 | (w&0x3f), stdout);
			}
			else
				putc(w, stdout);
		}
	}

	ApplyColour(CON_WHITEMASK);
	fflush(stdout);
}

//#define QCLOCK(enumname,readablename,query,frequency,initcode) //query must have t=

	#define CLOCKDEF_SDL_TICKSNS QCLOCK(TICKSNS, "ticksns", t=SDL_GetTicksNS(), SDL_NS_PER_SECOND,;)
	#define CLOCKDEF_SDL_TICKS QCLOCK(TICKS, "ticks", t=SDL_GetTicks(), 1000,;)

static quint64_t sdlperf_freq;
#define CLOCKDEF_SDL_PERF  QCLOCK(PERF,  "perf", t=SDL_GetPerformanceCounter(), sdlperf_freq,sdlperf_freq=SDL_GetPerformanceFrequency())

#if defined(CLOCK_MONOTONIC) && (_POSIX_C_SOURCE >= 200112L)
	#define CLOCKDEF_LINUX_MONOTONIC QCLOCK(MONOTONIC, "monotonic", {	\
				struct timespec ts;	\
				clock_gettime(CLOCK_MONOTONIC, &ts);	\
				t = (ts.tv_sec*(quint64_t)1000000000) + ts.tv_nsec;	\
			}, 1000000000,;)
#else
	#define CLOCKDEF_LINUX_MONOTONIC
#endif

#if defined(CLOCK_REALTIME) && (_POSIX_C_SOURCE >= 200112L)
	#define CLOCKDEF_LINUX_REALTIME QCLOCK(REALTIME, "realtime", {	\
				struct timespec ts;	\
				clock_gettime(CLOCK_REALTIME, &ts);	\
				t = (ts.tv_sec*(quint64_t)1000000000) + ts.tv_nsec;	\
			}, 1000000000,;)
#else
	#define CLOCKDEF_LINUX_REALTIME
#endif

#if _POSIX_C_SOURCE >= 200112L
	#include <sys/time.h>
	#define CLOCKDEF_POSIX_GTOD QCLOCK(GTOD, "gettimeofday", {	\
			struct timeval tp;	\
			gettimeofday(&tp, NULL);	\
			t = tp.tv_sec*(quint64_t)1000000 + tp.tv_usec;	\
		}, 1000000,;)
#else
	#define CLOCKDEF_POSIX_GTOD
#endif

#define CLOCKDEF_ALL	\
					 CLOCKDEF_SDL_TICKSNS /*sdl*/\
					 CLOCKDEF_LINUX_MONOTONIC CLOCKDEF_LINUX_REALTIME /*linux-specific clocks*/\
					 CLOCKDEF_SDL_PERF CLOCKDEF_SDL_TICKS /*sdl clocks*/\
					 CLOCKDEF_POSIX_GTOD /*posix clocks*/

static quint64_t timer_basetime;    //used by all clocks to bias them to starting at 0
static quint64_t timer_nobacksies;  //used by all clocks to bias them to starting at 0
static void Sys_ClockType_Changed(cvar_t *var, char *oldval);
#define QCLOCK(e,n,q,f,i) n"\n"
static cvar_t sys_clocktype = CVARFCD("sys_clocktype", "", CVAR_NOTFROMSERVER, Sys_ClockType_Changed, "Controls which system clock to base timings from.\nAvailable Clocks:\n"
	CLOCKDEF_ALL);
#undef QCLOCK

static enum
{
#define QCLOCK(e,n,q,f,i) CLOCKID_##e,
	CLOCKDEF_ALL
#undef QCLOCK
	CLOCKID_INVALID
} timer_clocktype=CLOCKID_INVALID;
static quint64_t Sys_GetClock(quint64_t *freq)
{
	quint64_t t;
	switch(timer_clocktype)
	{
	default:
#define QCLOCK(e,n,q,f,i) case CLOCKID_##e: *freq = f; q; break;
	CLOCKDEF_ALL
#undef QCLOCK
	}
	t -= timer_basetime;
	if (t < timer_nobacksies && t-*freq>timer_nobacksies)
	{
		quint64_t back = timer_nobacksies-t;
		t = timer_nobacksies;
		if (back > 0)//*freq/1000)	//warn if the clock went backwards by more than 1ms.
			Con_Printf("Warning: clock went backwards by %g secs.\n", back/(double)*freq);
		if (back > 0x8000000 && *freq==1000)
		{	//32bit value wrapped?
			timer_basetime -= 0x8000000;
			t += 0x8000000;
		}
	}
	else
		timer_nobacksies = t;
	return t;
}
static void Sys_ClockType_Changed(cvar_t *var, char *oldval)
{
	int newtype;

#define QCLOCK(e,n,q,f,i) if (!strcasecmp(var->string, n)) newtype = CLOCKID_##e; else
	CLOCKDEF_ALL
#undef QCLOCK
	{
		quint64_t freq;
		if (!*var->string || !strcasecmp(var->string, "auto"))
			;
		else
		{
			Con_Printf("%s: Unknown clock name %s available clocks", var->name, var->string);
#define QCLOCK(e,n,q,f,i) Con_Printf(", %s", n);
	CLOCKDEF_ALL
#undef QCLOCK
			Con_Printf("\n");
		}

		//look for any that work.
		for(newtype = 0; ; newtype++)
		{
			switch(newtype)
			{
			default:
				freq = 0;
				Sys_Error("No usable clocks");
#define QCLOCK(e,n,q,f,i) case CLOCKID_##e: freq = f; break;
	CLOCKDEF_ALL
#undef QCLOCK
			}
			if (freq)
				break;	//this clock seems usable.
		}
	}

	if (newtype != timer_clocktype)
	{
		quint64_t oldtime, oldfreq;
		quint64_t newtime, newfreq;

		oldtime = Sys_GetClock(&oldfreq);
		timer_clocktype = newtype;
		timer_nobacksies = timer_basetime = 0;	//make sure we get the raw clock.
		newtime = Sys_GetClock(&newfreq);

		timer_basetime = newtime - (newfreq * ((long double)oldtime) / oldfreq);
		timer_nobacksies = newtime - timer_basetime;

		Sys_GetClock(&newfreq);
	}
}
static void Sys_InitClock(void)
{
	quint64_t freq;

	Cvar_Register(&sys_clocktype, "System vars");

#define QCLOCK(e,n,q,f,i) i;
	CLOCKDEF_ALL
#undef QCLOCK

	//calibrate it, and apply.
	timer_clocktype = CLOCKID_INVALID;
	Sys_ClockType_Changed(&sys_clocktype, NULL);
	timer_basetime = timer_nobacksies = 0;
	timer_basetime = Sys_GetClock(&freq);
	timer_nobacksies = 0;
}
double Sys_DoubleTime (void)
{
	quint64_t denum, num = Sys_GetClock(&denum);
	return num / (long double)denum;
}
unsigned int Sys_Milliseconds (void)
{
	quint64_t denum, num = Sys_GetClock(&denum);
	num *= 1000;
	return num / denum;
}

//create a directory
void Sys_mkdir (const char *path)
{
	SDL_CreateDirectory(path);
}

qboolean Sys_rmdir (const char *path)
{
	return SDL_RemovePath(path);	//dir or file
}

//unlink a file
qboolean Sys_remove (const char *path)
{
	return SDL_RemovePath(path);	//file or dir
}

qboolean Sys_Rename (const char *oldfname, const char *newfname)
{
	return SDL_RenamePath(oldfname, newfname);
}

#ifndef _WIN32
#if _POSIX_C_SOURCE >= 200112L
	#include <sys/statvfs.h>
#endif
qboolean Sys_GetFreeDiskSpace(const char *path, quint64_t *freespace)
{
#if _POSIX_C_SOURCE >= 200112L
	//posix 2001
	struct statvfs inf;
	if(0==statvfs(path, &inf))
	{
		*freespace = inf.f_bsize*(quint64_t)inf.f_bavail;
		return true;
	}
#endif
	return false;
}
#endif

//someone used the 'quit' command
void Sys_Quit (void)
{
	Host_Shutdown();

	host_parms.binarydir = NULL;

	exit (0);
}

#if 1&&!defined(_WIN32)
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, qofs_t, time_t modtime, void *, searchpathfuncs_t *), void *parm, searchpathfuncs_t *spath)
{	//SDL_GlobDirectory does seem to do wildcards in parent directories properly, at least on loonix.
	//on wine it seems to do dumb slow shit ('*' crossing the '\\' directory seperator and enumerating the entire FS tree).
	char file[MAX_OSPATH];
	int i, count;
	SDL_PathInfo st;
	char **list = SDL_GlobDirectory(gpath, match, SDL_GLOB_CASEINSENSITIVE, &count);
	for(i = 0; i < count; i++)
	{
		if (list[i][0] != '.' && !strstr(list[i], "/."))	//unfortunately seems to include hidden files (we don't like creating hidden files by mistake, so avoid warnings later by refusing to find them here).
		{
			Q_snprintfz(file, sizeof(file), "%s/%s", gpath, list[i]);

			if (SDL_GetPathInfo(file, &st))
			{
				Q_snprintfz(file, sizeof(file), "%s%s", list[i], (st.type==SDL_PATHTYPE_DIRECTORY)?"/":"");

				if (!func(file, st.size, st.modify_time, parm, spath))
				{
					SDL_free(list);
					return false;
				}
			}
		}
	}
	SDL_free(list);
	return true;
}

//enumerate the files in a directory (of both gpath and match - match may not contain ..)
//calls the callback for each one until the callback returns 0
//SDL provides no file enumeration facilities.
#elif defined(_WIN32)
static int Sys_EnumerateFiles2 (const char *match, int matchstart, int neststart, int (QDECL *func)(const char *fname, qofs_t fsize, time_t mtime, void *parm, searchpathfuncs_t *spath), void *parm, searchpathfuncs_t *spath)
{
	qboolean go;

	HANDLE r;
	WIN32_FIND_DATAW fd;
	int nest = neststart;	//neststart refers to just after a /
	qboolean wild = false;

	while(match[nest] && match[nest] != '/')
	{
		if (match[nest] == '?' || match[nest] == '*')
			wild = true;
		nest++;
	}
	if (match[nest] == '/')
	{
		char submatch[MAX_OSPATH];
		char tmproot[MAX_OSPATH];

		if (!wild)
			return Sys_EnumerateFiles2(match, matchstart, nest+1, func, parm, spath);

		if (nest-neststart+1> MAX_OSPATH)
			return 1;
		memcpy(submatch, match+neststart, nest - neststart);
		submatch[nest - neststart] = 0;
		nest++;

		if (neststart+4 > MAX_OSPATH)
			return 1;
		memcpy(tmproot, match, neststart);
		strcpy(tmproot+neststart, "*.*");

		{
			wchar_t wroot[MAX_OSPATH];
			r = FindFirstFileExW(widen(wroot, sizeof(wroot), tmproot), FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0);
		}
		strcpy(tmproot+neststart, "");
		if (r==(HANDLE)-1)
			return 1;
		go = true;
		do
		{
			char utf8[MAX_OSPATH];
			char file[MAX_OSPATH];
			narrowen(utf8, sizeof(utf8), fd.cFileName);
			if (*utf8 == '.');	//don't ever find files with a name starting with '.'
			else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)	//is a directory
			{
				if (wildcmp(submatch, utf8))
				{
					int newnest;
					if (strlen(tmproot) + strlen(utf8) + strlen(match+nest) + 2 < MAX_OSPATH)
					{
						Q_snprintfz(file, sizeof(file), "%s%s/", tmproot, utf8);
						newnest = strlen(file);
						strcpy(file+newnest, match+nest);
						go = Sys_EnumerateFiles2(file, matchstart, newnest, func, parm, spath);
					}
				}
			}
		} while(FindNextFileW(r, &fd) && go);
		FindClose(r);
	}
	else
	{
		const char *submatch = match + neststart;
		char tmproot[MAX_OSPATH];

		if (neststart+4 > MAX_OSPATH)
			return 1;
		memcpy(tmproot, match, neststart);
		strcpy(tmproot+neststart, "*.*");

		{
			wchar_t wroot[MAX_OSPATH];
			r = FindFirstFileExW(widen(wroot, sizeof(wroot), tmproot), FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0);
		}
		strcpy(tmproot+neststart, "");
		if (r==(HANDLE)-1)
			return 1;
		go = true;
		do
		{
			char utf8[MAX_OSPATH];
			char file[MAX_OSPATH];

			narrowen(utf8, sizeof(utf8), fd.cFileName);
			if (*utf8 == '.')
				;	//don't ever find files with a name starting with '.' (includes .. and . directories, and unix hidden files)
			else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)	//is a directory
			{
				if (wildcmp(submatch, utf8))
				{
					if (strlen(tmproot+matchstart) + strlen(utf8) + 2 < MAX_OSPATH)
					{
						Q_snprintfz(file, sizeof(file), "%s%s/", tmproot+matchstart, utf8);
						go = func(file, qofs_Make(fd.nFileSizeLow, fd.nFileSizeHigh), 0, parm, spath);
					}
				}
			}
			else
			{
				if (wildcmp(submatch, utf8))
				{
					if (strlen(tmproot+matchstart) + strlen(utf8) + 1 < MAX_OSPATH)
					{
						Q_snprintfz(file, sizeof(file), "%s%s", tmproot+matchstart, utf8);
						go = func(file, qofs_Make(fd.nFileSizeLow, fd.nFileSizeHigh), 0, parm, spath);
					}
				}
			}
		} while(FindNextFileW(r, &fd) && go);
		FindClose(r);
	}
	return go;
}
int Sys_EnumerateFiles (const char *gpath, const char *match, int (QDECL *func)(const char *fname, qofs_t fsize, time_t modtime, void *parm, searchpathfuncs_t *spath), void *parm, searchpathfuncs_t *spath)
{
	char fullmatch[MAX_OSPATH];
	int start;
	if (strlen(gpath) + strlen(match) + 2 > MAX_OSPATH)
		return 1;

	strcpy(fullmatch, gpath);
	start = strlen(fullmatch);
	if (start && fullmatch[start-1] != '/')
		fullmatch[start++] = '/';
	fullmatch[start] = 0;
	strcat(fullmatch, match);
	return Sys_EnumerateFiles2(fullmatch, start, start, func, parm, spath);
}
#elif defined(linux) || defined(__unix__) || defined(__MACH__) || defined(__HAIKU__)
#include <dirent.h>
#include <errno.h>
static int Sys_EnumerateFiles2 (const char *truepath, int apathofs, const char *match, int (*func)(const char *, qofs_t, time_t modtime, void *, searchpathfuncs_t *), void *parm, searchpathfuncs_t *spath)
{
	DIR *dir;
	char file[MAX_OSPATH];
	const char *s;
	struct dirent *ent;
	struct stat st;
	const char *wild;
	const char *apath = truepath+apathofs;

	//if there's a * in a system path, then we need to scan its parent directory to figure out what the * expands to.
	//we can just recurse quicklyish to try to handle it.
	wild = strchr(apath, '*');
	if (!wild)
		wild = strchr(apath, '?');
	if (wild)
	{
		char subdir[MAX_OSPATH];
		for (s = wild+1; *s && *s != '/'; s++)
			;
		while (wild > truepath)
		{
			if (*(wild-1) == '/')
				break;
			wild--;
		}
		memcpy(file, truepath, wild-truepath);
		file[wild-truepath] = 0;

		dir = opendir(file);
		memcpy(subdir, wild, s-wild);
		subdir[s-wild] = 0;
		if (dir)
		{
			do
			{
				ent = readdir(dir);
				if (!ent)
					break;
				if (*ent->d_name != '.')
				{
#ifdef _DIRENT_HAVE_D_TYPE
					if (ent->d_type != DT_DIR && ent->d_type != DT_UNKNOWN)
						continue;
#endif
					if (wildcmp(subdir, ent->d_name))
					{
						memcpy(file, truepath, wild-truepath);
						Q_snprintfz(file+(wild-truepath), sizeof(file)-(wild-truepath), "%s%s", ent->d_name, s);
						if (!Sys_EnumerateFiles2(file, apathofs, match, func, parm, spath))
						{
							closedir(dir);
							return false;
						}
					}
				}
			} while(1);
			closedir(dir);
		}
		return true;
	}


	dir = opendir(truepath);
	if (!dir)
	{
		Con_DLPrintf((errno==ENOENT)?2:1, "Failed to open dir %s\n", truepath);
		return true;
	}
	do
	{
		ent = readdir(dir);
		if (!ent)
			break;
		if (*ent->d_name != '.')
		{
			if (wildcmp(match, ent->d_name))
			{
				Q_snprintfz(file, sizeof(file), "%s/%s", truepath, ent->d_name);

				if (stat(file, &st) == 0 || lstat(file, &st) == 0)
				{
					Q_snprintfz(file, sizeof(file), "%s%s%s", apath, ent->d_name, S_ISDIR(st.st_mode)?"/":"");

					if (!func(file, st.st_size, st.st_mtime, parm, spath))
					{
//						Con_DPrintf("giving up on search after finding %s\n", file);
						closedir(dir);
						return false;
					}
				}
//				else
//					printf("Stat failed for \"%s\"\n", file);
			}
		}
	} while(1);
	closedir(dir);
	return true;
}
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, qofs_t, time_t modtime, void *, searchpathfuncs_t *), void *parm, searchpathfuncs_t *spath)
{
	char apath[MAX_OSPATH];
	char truepath[MAX_OSPATH];
	char *s;

	if (!gpath)
		gpath = "";
	*apath = '\0';

	Q_strncpyz(apath, match, sizeof(apath));
	for (s = apath+strlen(apath)-1; s >= apath; s--)
	{
		if (*s == '/')
		{
			s[1] = '\0';
			match += s - apath+1;
			break;
		}
	}
	if (s < apath)	//didn't find a '/'
		*apath = '\0';

	Q_snprintfz(truepath, sizeof(truepath), "%s/%s", gpath, apath);
	return Sys_EnumerateFiles2(truepath, strlen(gpath)+1, match, func, parm, spath);
}

#else
int Sys_EnumerateFiles (const char *gpath, const char *match, int (*func)(const char *, qofs_t, time_t mtime, void *, void *), void *parm, void *spath)
{
	Con_Printf("Warning: Sys_EnumerateFiles not implemented\n");
	return false;
}
#endif

#include "fs.h"

typedef struct
{
	vfsfile_t f;
	SDL_IOStream *s;
} sdlfile_t;
static int IOF_ReadBytes(struct vfsfile_s *file, void *buffer, int bytestoread)
{
	sdlfile_t *f = (void*)file;
	int r = SDL_ReadIO(f->s, buffer, bytestoread);
	if (!r && bytestoread)
	{
		SDL_IOStatus s = SDL_GetIOStatus(f->s);
		if (s == SDL_IO_STATUS_NOT_READY)
			return 0;
		else if (s == SDL_IO_STATUS_EOF)
			return VFS_ERROR_EOF;
		else
			return VFS_ERROR_UNSPECIFIED;
	}
	return r;
}
static int IOF_WriteBytes(struct vfsfile_s *file,const  void *buffer, int bytestowrite)
{
	sdlfile_t *f = (void*)file;
	int r = SDL_WriteIO(f->s, buffer, bytestowrite);
	if (!r && bytestowrite)
	{
		SDL_IOStatus s = SDL_GetIOStatus(f->s);
		if (s == SDL_IO_STATUS_NOT_READY)
			return 0;
		else if (s == SDL_IO_STATUS_EOF)
			return VFS_ERROR_EOF;
		else
			return VFS_ERROR_UNSPECIFIED;
	}
	return r;
}
static qboolean IOF_Seek(struct vfsfile_s *file, qofs_t pos)
{
	sdlfile_t *f = (void*)file;
	Sint64 r = SDL_SeekIO(f->s, pos, SDL_IO_SEEK_SET);
	return r == pos;
}
static qofs_t IOF_Tell(struct vfsfile_s *file)
{
	sdlfile_t *f = (void*)file;
	return SDL_TellIO(f->s);
}
static qofs_t IOF_GetLen(struct vfsfile_s *file)
{
	sdlfile_t *f = (void*)file;
	return SDL_GetIOSize(f->s);
}
static qboolean IOF_Close(struct vfsfile_s *file)
{
	sdlfile_t *f = (void*)file;
	qboolean ret = SDL_CloseIO(f->s);
	Z_Free(f);
	return ret;
}
static void IOF_Flush(struct vfsfile_s *file)
{
	sdlfile_t *f = (void*)file;
	SDL_FlushIO(f->s);
}
static vfsfile_t *IOF_Setup(sdlfile_t *r, SDL_IOStream *f)
{
	if (f)
	{
		r->s = f;
		r->f.ReadBytes = IOF_ReadBytes;
		r->f.WriteBytes = IOF_WriteBytes;
		r->f.Seek = IOF_Seek;
		r->f.Tell = IOF_Tell;
		r->f.GetLen = IOF_GetLen;
		r->f.Close = IOF_Close;
		r->f.Flush = IOF_Flush;
		r->f.seekstyle = SS_SEEKABLE;
		return &r->f;
	}
	else
		Z_Free(r);	//oops.
	return NULL;
}
static vfsfile_t *IOF_AsVFS(SDL_IOStream *f)
{
	return IOF_Setup(Z_Malloc(sizeof(sdlfile_t)), f);
}
static vfsfile_t *VFSSDL_Open(const char *fname, char *mode)
{
	return IOF_AsVFS(SDL_IOFromFile(fname, mode));
}

static void SDLCALL FS_OpenFilePicked (void *userdata, const char * const *filelist, int filter)
{
	int i;
	for(i = 0; filelist && filelist[i]; i++)
	{
		vfsfile_t *f = VFSSDL_Open(filelist[i], "rb");	//using this instead of VFSOS_Open, to handle platform weirdness here.
		if (f)
			Host_RunFile(filelist[i],strlen(filelist[i]), f);
	}
}
static void FS_OpenFilePicker_f(void)
{
	SDL_DialogFileFilter exts[] = {
		{"Packages", "pk3;pk4;pak"},
		{"Demos", "mvd.gz;mvd;qwd.gz;qwd;dem.gz;dem"},
		{"Maps", "bsp.gz;bsp;map"},
		{"Mods", "fmf"},
		{"All Files", "*"},
	};
	SDL_ShowOpenFileDialog(FS_OpenFilePicked, NULL, sdlwindow, exts, countof(exts), NULL, true);
}


//blink window if possible (it's not)
void Sys_ServerActivity(void)
{
	SDL_FlashWindow(sdlwindow, SDL_FLASH_BRIEFLY);
}

void Sys_CloseLibrary(dllhandle_t *lib)
{
	SDL_UnloadObject((void*)lib);
}
dllhandle_t *Sys_LoadLibrary(const char *name, dllfunction_t *funcs)
{
	int i;
	void *lib;

	lib = SDL_LoadObject(name);
	if (!lib)
	{
		char libpath[MAX_OSPATH];
		Q_snprintfz(libpath, sizeof(libpath), "%s"ARCH_DL_POSTFIX, name);
		lib = SDL_LoadObject(libpath);
	}
	if (!lib)
		return NULL;

	if (funcs)
	{
		for (i = 0; funcs[i].name; i++)
		{
			*funcs[i].funcptr = SDL_LoadFunction(lib, funcs[i].name);
			if (!*funcs[i].funcptr)
				break;
		}
		if (funcs[i].name)
		{
			Sys_CloseLibrary((dllhandle_t*)lib);
			lib = NULL;
		}
	}

	return (dllhandle_t*)lib;
}
void *Sys_GetAddressForName(dllhandle_t *module, const char *exportname)
{
	if (!module)
		return NULL;
	return SDL_LoadFunction((void *)module, exportname);
}





//used to see if a file exists or not.
int	Sys_FileTime (char *path)
{
	FILE	*f;
	
	f = fopen(path, "rb");
	if (f)
	{
		fclose(f);
		return 1;
	}
	
	return -1;
}

char *Sys_URIScheme_NeedsRegistering(void)
{   //no support.
    return NULL;
}

void Sys_Init(void)
{
//TODO:	SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition,candidates");

	Cmd_AddCommandD("sys_openfile", FS_OpenFilePicker_f,	"Select a file to open/install/etc.");

	SDL_Init(SDL_INIT_EVENTS);
	Sys_InitClock();
}
void Sys_Shutdown(void)
{
	SDL_Quit();
}



int VARGS Sys_DebugLog(char *file, char *fmt, ...)
{
	FILE *fd;
	va_list argptr; 
	static char data[1024];
    
	va_start(argptr, fmt);
	vsnprintf(data, sizeof(data)-1, fmt, argptr);
	va_end(argptr);

#if defined(CRAZYDEBUGGING) && CRAZYDEBUGGING > 1
	{
		static int sock;
		if (!sock)
		{
			struct sockaddr_in sa;
			netadr_t na;
			int _true = true;
			int listip;
			listip = COM_CheckParm("-debugip");
			NET_StringToAdr(listip?com_argv[listip+1]:"127.0.0.1", &na);
			NetadrToSockadr(&na, (struct sockaddr_qstorage*)&sa);
			sa.sin_port = htons(10000);
			sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (-1==connect(sock, (struct sockaddr*)&sa, sizeof(sa)))
				Sys_Error("Couldn't send debug log lines\n");
			setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&_true, sizeof(_true));
		}
		send(sock, data, strlen(data), 0);
	}
#endif
	fd = fopen(file, "wt");
	if (fd)
	{
		fwrite(data, 1, strlen(data), fd);
		fclose(fd);
		return 0;
	}
	return 1;
};


static qbyte nostdin;
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#endif

#ifdef _WIN32
static qboolean gotconsole;
static HANDLE con_stdin;
qboolean Sys_InitTerminal(void)
{
	gotconsole = AllocConsole();	//failure is okay if we already had one.
	con_stdin = CreateFile("CONIN$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	freopen("CON", "w", stdout);	//unfuck the stdout too.
    return true;
}
char *Sys_ConsoleInput(void)
{	//NET_Sleep won't sleep on this handle, so expect it to be sluggish.
	DWORD numevents, i;
	while (GetNumberOfConsoleInputEvents(con_stdin, &numevents) && numevents>0)
	{
		static char text[256];
		static int textlen;
		INPUT_RECORD event[1];	//longer might miss presses. especially with delays.
		if (numevents > countof(event))
			numevents = countof(event);
		if (ReadConsoleInputW(con_stdin, event, numevents, &numevents))
		{
			for(i = 0; i < numevents; i++)
			{
				if (event[i].EventType == KEY_EVENT && event[i].Event.KeyEvent.bKeyDown && event[i].Event.KeyEvent.uChar.UnicodeChar)
				{
					if (textlen >= countof(text))
						textlen = countof(text)-1;	//don't overflow.
					text[textlen] = event[i].Event.KeyEvent.uChar.UnicodeChar;
					if (text[textlen] == '\r')
					{
						text[textlen] = 0;	//caller will add its own \n
						printf("\r]%s\n", text);
						textlen = 0; //start from the start
						fflush(stdout);
						return text;
					}
					textlen++;
					text[textlen] = 0;	//caller will add its own \n
					printf("\r]%s", text);
					fflush(stdout);
				}
			}
		}
	}
	return NULL;
}
void Sys_CloseTerminal (void)
{
	if (gotconsole)
	{	//don't close our initial one. don't detach that way.
		FreeConsole();
		gotconsole = false;
	}
	if (con_stdin)
	{
		CloseHandle(con_stdin);
		con_stdin = NULL;
	}
}
#elif defined(__unix__) && !defined(__ANDROID__)
#include <errno.h>
qboolean Sys_InitTerminal(void)
{
	if (COM_CheckParm("-nostdin"))
		nostdin = true;
	if (nostdin)
		return true;	//they okayed it, let it start regardless.
	if (isatty(STDIN_FILENO))
		return true;
	Con_Printf(CON_WARNING"Sys_InitTerminal: not started from a tty\n"); //no easy way to kill it otherwise.
	return false;
}
char *Sys_ConsoleInput(void)
{
	static char text[256];
	char *nl;

	if (nostdin)
		return NULL;

#if defined(__linux__) && defined(_DEBUG)
	{
		int fl = fcntl (STDIN_FILENO, F_GETFL, 0);
		if (!(fl & FNDELAY))
		{
			fcntl(STDIN_FILENO, F_SETFL, fl | FNDELAY);
//          Sys_Printf(CON_WARNING "stdin flags became blocking - gdb bug?\n");
		}
	}
#endif

	if (!fgets(text, sizeof(text), stdin))
	{
		if (errno == EIO)
		{
			Sys_Printf(CON_WARNING "Backgrounded, ignoring stdin\n");
			nostdin |= 2;
		}
		return NULL;
	}
	nl = strchr(text, '\n');
	if (!nl)    //err? wut?
		return NULL;
	*nl = 0;

	return text;
}
void Sys_CloseTerminal (void)
{
}
#else
qboolean Sys_InitTerminal(void)
{
	Con_Printf(CON_WARNING"Sys_InitTerminal: not implemented in this build.\n");
    return false;	//Sys_ConsoleInput cannot work, so return false here.
}
char *Sys_ConsoleInput(void)
{
	return NULL;
}
void Sys_CloseTerminal (void)
{
}
#endif

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_main.h>

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
	Host_Shutdown();
}

static double oldtime;
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
	quakeparms_t	parms;

	memset(&parms, 0, sizeof(parms));

	parms.basedir = SDL_GetCurrentDirectory();
	parms.binarydir = SDL_GetBasePath();

	parms.argc = argc;
	parms.argv = (const char**)argv;
#ifdef CONFIG_MANIFEST_TEXT
	parms.manifest = CONFIG_MANIFEST_TEXT;
#endif

#if !defined(WIN32)
	fcntl(0, F_SETFL, fcntl (0, F_GETFL, 0) | O_NDELAY);
#endif

	COM_InitArgv (parms.argc, parms.argv);

#ifdef SUBSERVERS
	if (COM_CheckParm("-clusterslave"))
	{
		isDedicated = true;
		isClusterSlave = true;
		SSV_SetupControlPipe(Sys_GetStdInOutStream(), false);
	}
#endif

	TL_InitLanguages(parms.basedir);

	if (parms.binarydir)
		Sys_Printf("Binary is located at \"%s\"\n", parms.binarydir);

#if defined(HAVE_CLIENT) && !defined(CLIENTONLY)
	if (COM_CheckParm ("-dedicated"))
		isDedicated = true;
	if (isDedicated)    //compleate denial to switch to anything else - many of the client structures are not initialized.
	{
		float delay;

		SV_Init (&parms);

		if (!Sys_InitTerminal())
			Con_Printf(CON_WARNING"Stdin unavailable\n");

		delay = SV_Frame();
		while (1)
		{
			if (!isDedicated)
				Sys_Error("Dedicated was cleared");
			NET_Sleep(delay, false);
			delay = SV_Frame();
		}
		return EXIT_FAILURE;
	}
#endif


	Host_Init (&parms);

	oldtime = Sys_DoubleTime ();
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	double time, newtime, sleeptime;

#ifndef CLIENTONLY
		if (isDedicated)
		{
		// find time passed since last cycle
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
			oldtime = newtime;
			
			sleeptime = SV_Frame();
			NET_Sleep(sleeptime, false);
		}
		else
#endif
		{

	// yield the CPU for a little while when paused, minimized, or not the focus
			//using the callbacks. let sdl deal with it. also sleeping sucks on windows anyway.

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
			sleeptime = Host_Frame (time);
			oldtime = newtime;

			if (sleeptime)
				Sys_Sleep(sleeptime);
		}
	return SDL_APP_CONTINUE;
}

qboolean Sys_GetDesktopParameters(int *width, int *height, int *bpp, int *refreshrate)
{
	const SDL_DisplayMode *mode;
	COM_AssertMainThread("SDL_GetDesktopDisplayMode");
	mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
	if (mode)
	{
		*width = mode->w;
		*height = mode->h;
		*bpp = (SDL_PIXELTYPE(mode->format) == SDL_PIXELTYPE_PACKED32)?32:16;
		*refreshrate = mode->refresh_rate;
		return true;
	}
	return false;
}



void Sys_Clipboard_PasteText(clipboardtype_t cbt, void (*callback)(void *cb, const char *utf8), void *ctx)
{
	char *txt;
	if (cbt == CBT_SELECTION)
		txt = SDL_GetPrimarySelectionText();
	else
		txt = SDL_GetClipboardText();
	callback(ctx, txt);
	SDL_free(txt);
}
void Sys_SaveClipboard(clipboardtype_t cbt, const char *text)
{
	if (cbt == CBT_SELECTION)
		SDL_SetPrimarySelectionText(text);
	else
		SDL_SetClipboardText(text);
}

#ifdef MULTITHREAD

/*Thread management stuff*/
static SDL_ThreadID mainthread;
static SDL_TLSID tls_threadinfo;
struct threadinfo_s
{
	jmp_buf jmpbuf;	//so we can actually abort our threads...
	int (*threadfunc)(void *args);
	void *args;
};

void Sys_ThreadsInit(void)
{
	mainthread = SDL_GetCurrentThreadID();
}
qboolean Sys_IsThread(void *thread)
{
	return SDL_GetThreadID(thread) == SDL_GetCurrentThreadID();
}
qboolean Sys_IsMainThread(void)
{
	return mainthread == SDL_GetCurrentThreadID();
}
void Sys_ThreadAbort(void)
{
	//SDL_KillThread(NULL) got removed... so we have to do things the shitty way.
	struct threadinfo_s *tinfo = SDL_GetTLS(&tls_threadinfo);
	if (!tinfo)
	{	//erk... not created via Sys_CreateThread?!?
		SDL_Delay(10*1000);
		exit(0);
	}
	longjmp(tinfo->jmpbuf, 1);
}
static int FTESDLThread(void *args)
{	//all for Sys_ThreadAbort
	struct threadinfo_s *tinfo = args;
	int r;
	SDL_SetTLS(&tls_threadinfo, tinfo, NULL);
	if (setjmp(tinfo->jmpbuf))
		r = 0;	//aborted...
	else
		r = tinfo->threadfunc(tinfo->args);
	SDL_SetTLS(&tls_threadinfo, NULL, NULL);
	Z_Free(tinfo);
	return r;
}
void *Sys_CreateThread(char *name, int (*func)(void *), void *args, int priority, int stacksize)
{
	struct threadinfo_s *tinfo = Z_Malloc(sizeof(*tinfo));
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_Thread *thread;
	tinfo->threadfunc = func;
	tinfo->args = args;
	SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void *)FTESDLThread);
	SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, tinfo);
	SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, name);
	/* Optimized model loaders can exceed the old 256 KB worker hint after
	 * inlining. SDL2 ignored that hint; keep enough stack when SDL3 honors it. */
	if (stacksize > 0) SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, max(stacksize, 1024*1024));
	thread = SDL_CreateThreadWithProperties(props);
	SDL_DestroyProperties(props);
	if (!thread) Z_Free(tinfo);
	return thread;
}

void Sys_WaitOnThread(void *thread)
{
	SDL_WaitThread((SDL_Thread *)thread, NULL);
}


/* Mutex calls */
void *Sys_CreateMutex(void)
{
	return (void *)SDL_CreateMutex();
}

qboolean Sys_TryLockMutex(void *mutex)
{
	return SDL_TryLockMutex(mutex);
}

qboolean Sys_LockMutex(void *mutex)
{
	SDL_LockMutex(mutex);
	return true;
}

qboolean Sys_UnlockMutex(void *mutex)
{
	SDL_UnlockMutex(mutex);
	return true;
}

void Sys_DestroyMutex(void *mutex)
{
	SDL_DestroyMutex(mutex);
}

/* Conditional wait calls */
typedef struct condvar_s
{
	SDL_Mutex *mutex;
	SDL_Condition *cond;
} condvar_t;

void *Sys_CreateConditional(void)
{
	condvar_t *condv;
	SDL_Mutex *mutex;
	SDL_Condition *cond;

	condv = (condvar_t *)malloc(sizeof(condvar_t));
	if (!condv)
		return NULL;

	mutex = SDL_CreateMutex();
	cond = SDL_CreateCondition();

	if (mutex)
	{
		if (cond)
		{
			condv->cond = cond;
			condv->mutex = mutex;

			return (void *)condv;
		}
		else
			SDL_DestroyMutex(mutex);
	}

	free(condv);
	return NULL;
}

qboolean Sys_LockConditional(void *condv)
{
	SDL_LockMutex(((condvar_t *)condv)->mutex);
	return true;
}

qboolean Sys_UnlockConditional(void *condv)
{
	SDL_UnlockMutex(((condvar_t *)condv)->mutex);
	return true;
}

qboolean Sys_ConditionWait(void *condv)
{
	SDL_WaitCondition(((condvar_t *)condv)->cond, ((condvar_t *)condv)->mutex);
	return true;
}

qboolean Sys_ConditionSignal(void *condv)
{
	SDL_SignalCondition(((condvar_t *)condv)->cond);
	return true;
}

qboolean Sys_ConditionBroadcast(void *condv)
{
	SDL_BroadcastCondition(((condvar_t *)condv)->cond);
	return true;
}

void Sys_DestroyConditional(void *condv)
{
	condvar_t *cv = (condvar_t *)condv;

	SDL_DestroyCondition(cv->cond);
	SDL_DestroyMutex(cv->mutex);
	free(cv);
}
#endif

void Sys_Sleep (double seconds)
{
	SDL_Delay(seconds * 1000);
}

#ifdef WEBCLIENT
qboolean Sys_RunInstaller(void)
{       //not implemented
    return false;
}
#endif

#ifdef HAVEAUTOUPDATE
//returns true if we could sucessfull overwrite the engine binary.
qboolean Sys_SetUpdatedBinary(const char *fname)
{
	return false;
}
//says whether the system code is able/allowed to overwrite itself.
//(ie: return false if we don't know the binary name or if its write-protected etc)
qboolean Sys_EngineMayUpdate(void)
{
	return false;	//sorry
}
#endif

#ifdef SUBSERVERS
#include <errno.h>
static int QDECL Sys_StdoutWrite (struct vfsfile_s *file, const void *buffer, int bytestowrite)
{
	ssize_t r = write(STDOUT_FILENO, buffer, bytestowrite);
	if (r == 0 && bytestowrite)
		return -1;	//eof
	if (r < 0)
	{
		int e = errno;
		if (e == EINTR || e == EAGAIN || e == EWOULDBLOCK)
			return 0;
	}
	return r;
}
static int QDECL Sys_StdinRead (struct vfsfile_s *file, void *buffer, int bytestoread)
{
#ifdef _WIN32
	DWORD avail;
	HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
	if (!PeekNamedPipe(input, NULL, 0, NULL, &avail, NULL))
		return -1;	//some kind of error? EOF? Hangup? just report it as an error.
	if (avail)
	{
		if (avail > bytestoread)
			avail = bytestoread;
		if (!ReadFile(input, buffer, avail, &avail, NULL))
			return -1;
	}
	return avail;
#else
	//standard posix
	ssize_t r;
#if defined(__linux__) && defined(_DEBUG)	//this tends to g
	int fl = fcntl (STDIN_FILENO, F_GETFL, 0);
	if (!(fl & O_NONBLOCK))
	{
		fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);
		Sys_Printf(CON_WARNING "stdin flags became blocking - gdb bug?\n");
	}
#endif
	r = read(STDIN_FILENO, buffer, bytestoread);
	if (r == 0 && bytestoread)
		return VFS_ERROR_EOF;	//eof
	if (r < 0)
	{
		int e = errno;
		if (e == EINTR || e == EAGAIN || e == EWOULDBLOCK)
			return 0;
	}
	return r;
#endif
}
static qboolean QDECL Sys_StdinOutClose(vfsfile_t *fs)
{
	Cbuf_AddText("\nquit\n", RESTRICT_LOCAL);
	Z_Free(fs);
	return true;
}
vfsfile_t *Sys_GetStdInOutStream(void)
{
	vfsfile_t *stream = Z_Malloc(sizeof(*stream));	//not using extra state, so no need to subclass.

	nostdin = true;	//detatch from stdin, read no more commands.

#ifdef _WIN32
#else
	//make sure nothing bad is going to happen.
	fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0)|O_NONBLOCK);
	fcntl(STDOUT_FILENO, F_SETFL, fcntl(STDOUT_FILENO, F_GETFL, 0)|O_NONBLOCK);
#endif

	stream->WriteBytes = Sys_StdoutWrite;
	stream->ReadBytes = Sys_StdinRead;
	stream->Close = Sys_StdinOutClose;
	stream->seekstyle = SS_UNSEEKABLE;
	return stream;
}

typedef struct
{
	vfsfile_t pub;
	SDL_IOStream *in;	//attached to the stdin of the process	(writable)
	SDL_IOStream *out;	//attached to the stdout of the process (readable)
	SDL_Process *proc;
} sdlsubserver_t;
static int Sys_MSV_ReadBytes(struct vfsfile_s *file, void *buffer, int bytestoread)
{
	sdlsubserver_t *f = (void*)file;
	int r = SDL_ReadIO(f->out, buffer, bytestoread);
	if (!r && bytestoread)
	{
		SDL_IOStatus s = SDL_GetIOStatus(f->out);
		if (s == SDL_IO_STATUS_NOT_READY)
			return 0;
		else if (s == SDL_IO_STATUS_EOF)
			return VFS_ERROR_EOF;
		else
			return VFS_ERROR_UNSPECIFIED;
	}
	return r;
}
static int Sys_MSV_WriteBytes(struct vfsfile_s *file,const  void *buffer, int bytestowrite)
{
	sdlsubserver_t *f = (void*)file;
	int r = SDL_WriteIO(f->in, buffer, bytestowrite);
	if (!r && bytestowrite)
	{
		SDL_IOStatus s = SDL_GetIOStatus(f->in);
		if (s == SDL_IO_STATUS_NOT_READY)
			return 0;
		else if (s == SDL_IO_STATUS_EOF)
			return VFS_ERROR_EOF;
		else
			return VFS_ERROR_UNSPECIFIED;
	}
	return r;
}
static qboolean Sys_MSV_Close(struct vfsfile_s *file)
{
	sdlsubserver_t *f = (void*)file;
	SDL_KillProcess(f->proc, false);	//let it die on its own...
	SDL_DestroyProcess(f->proc);	//clear up any state (does not force-terminate though!).
	Z_Free(f);
	return true;
}
vfsfile_t *Sys_ForkServer(void)
{
	sdlsubserver_t *ctx;
	const char *argv[64];
	int argc = 0;

	argv[argc++] = com_argv[0];	//best guess. not exactly reliable.
	argv[argc++] = "-clusterslave";
	argc += FS_GetManifestArgv(argv+argc, countof(argv)-argc-1);
	argv[argc++] = NULL;

	Con_DPrintf("Execing %s\n", argv[0]);

	ctx = Z_Malloc(sizeof(*ctx));

	ctx->proc = SDL_CreateProcess(argv, true);
    if (!ctx->proc) {
        Con_Printf("Could not create server process: %s\n", SDL_GetError());
        Z_Free(ctx);
        return NULL;
    }
	ctx->in = SDL_GetProcessInput(ctx->proc);
	ctx->out = SDL_GetProcessOutput(ctx->proc);

	ctx->pub.ReadBytes = Sys_MSV_ReadBytes;
	ctx->pub.WriteBytes = Sys_MSV_WriteBytes;
	ctx->pub.Close = Sys_MSV_Close;
	ctx->pub.seekstyle = SS_UNSEEKABLE;
	return &ctx->pub;
}
#endif
