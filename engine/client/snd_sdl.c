#include "quakedef.h"

#if defined(HAVE_MIXER) || defined(VOICECHAT)
#include "winquake.h"

#include <SDL3/SDL.h>
#define SDRVNAME "SDL"

//SDL calls a callback each time it needs to repaint the 'hardware' buffers
//This results in extra latency due it needing to buffer that much data.
//So we tell it a fairly pathetically sized buffer and try and get it to copy often
//hopefully this lowers sound latency, and has no suddenly starting sounds and stuff.
//It still has greater latency than direct access, of course.
//On the plus side, SDL calls the callback from another thread. this means we can set up some tiny buffer size, and if we're mixing inside the callback then you can actually get lower latency than waiting for an entire frame (yay rtlights)

static qboolean SSDL_InitAudio(void)
{
	static qboolean inited = false;
	if (COM_CheckParm("-nosdlsnd") || COM_CheckParm("-nosdl"))
		return false;

	if (!inited)
	{
		if(SDL_InitSubSystem(SDL_INIT_AUDIO))
			inited = true;
		else
			Con_Printf("Couldn't initialize SDL audio subsystem (%s)\n", SDL_GetError());
	}
	return inited;
}
#endif

#ifdef HAVE_MIXER
#define SELFPAINT
static void SSDL_Shutdown(soundcardinfo_t *sc)
{
	Con_DPrintf("Shutdown SDL sound\n");

	if (sc->handle)
	{
		SDL_PauseAudioStreamDevice(sc->handle);
		SDL_DestroyAudioStream(sc->handle);
        sc->handle = NULL;
	}
#ifndef SELFPAINT
	if (sc->sn.buffer)
		free(sc->sn.buffer);
#endif
	sc->sn.buffer = NULL;
}
static unsigned int SSDL_Callback_GetDMAPos(soundcardinfo_t *sc)
{
	sc->sn.samplepos = sc->snd_sent / sc->sn.samplebytes;
	return sc->sn.samplepos;
}

//this function is called from inside SDL.
//transfer the 'dma' buffer into the buffer it requests.
static void SDLCALL SSDL3_Callback_Paint(void *userdata, SDL_AudioStream *stream, int additional, int total)
{
	soundcardinfo_t *sc = userdata;
    /* SDL may request arbitrarily large fills. Mix bounded, whole sample frames. */
    Uint8 buffer[16384];
    const int framebytes = sc->sn.samplebytes * sc->sn.numchannels;
    (void)total;
    if (additional <= 0 || framebytes <= 0) return;
    while (additional > 0) {
        int bytes = min(additional, (int)sizeof(buffer));
        bytes = ((bytes + framebytes - 1) / framebytes) * framebytes;
        if (bytes > (int)sizeof(buffer)) bytes -= framebytes;
        sc->sn.buffer = buffer;
        sc->sn.samples = bytes / sc->sn.samplebytes;
        sc->samplequeue = sc->sn.samples;
        S_MixerThread(sc);
        if (!SDL_PutAudioStreamData(stream, buffer, bytes)) break;
        sc->snd_sent += bytes;
        additional -= bytes;
    }
    sc->sn.buffer = NULL;
}

static void *SSDL_Callback_LockBuffer(soundcardinfo_t *sc, unsigned int *sampidx)
{
	SDL_LockAudioStream(sc->handle);

	return sc->sn.buffer;
}

static void SSDL_Callback_UnlockBuffer(soundcardinfo_t *sc, void *buffer)
{
	SDL_UnlockAudioStream(sc->handle);
}

static void SSDL_Callback_Submit(soundcardinfo_t *sc, int start, int end)
{
	//SDL will call SSDL_Paint to paint when it's time, and the sound buffer is always there...
}


static qboolean QDECL SDL_InitCard(soundcardinfo_t *sc, const char *devicename)
{
	SDL_AudioSpec desired, obtained;

	if(!SSDL_InitAudio())
	{
		Con_Printf("Couldn't initialize SDL audio subsystem\n");
		return false;
	}

	{
		SDL_AudioDeviceID devid = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
		SDL_AudioDeviceID *dev;
		int count, i;

		//figure out which device to use.
		if (!devicename || !*devicename)
			dev = NULL, count = 0;	//not specified, just go with the default.
		else
			dev = SDL_GetAudioPlaybackDevices(&count);
		for (i = 0; i < count; i++)
		{
			const char *devname = SDL_GetAudioDeviceName(dev[i]);
			if (devname && !strcmp(devname, devicename))
			{	//this one matched
				devid = dev[i];
				break;
			}
		}

		//set up what we desire...
		sc->sn.samplebytes = 2;
		sc->sn.sampleformat = QSF_S16;
		desired.channels = sc->sn.numchannels;
		desired.freq = sc->sn.speed;
		switch(sc->sn.sampleformat)
		{
		case QSF_U8:	desired.format = SDL_AUDIO_U8; break;
		case QSF_S8:	desired.format = SDL_AUDIO_S8; break;
		default:
		case QSF_S16:	desired.format = SDL_AUDIO_S16; break;
		case QSF_F32:	desired.format = SDL_AUDIO_F32; break;
		}

		//and then tailor that to what we're likely to actually get.
		if (SDL_GetAudioDeviceFormat(devid, &obtained, NULL))
		{
			//clamp the channels.
			if (desired.channels > obtained.channels)
				desired.channels = obtained.channels;
			//if its a format that we support, just give it what it prefers.
			switch(obtained.format)
			{
			case SDL_AUDIO_U8:		desired.format = obtained.format; sc->sn.samplebytes = 1;	sc->sn.sampleformat = QSF_U8;	break;
			case SDL_AUDIO_S8:		desired.format = obtained.format; sc->sn.samplebytes = 1;	sc->sn.sampleformat = QSF_S8;	break;
			case SDL_AUDIO_S16LE:
			case SDL_AUDIO_S16BE:	desired.format = obtained.format; sc->sn.samplebytes = 2;	sc->sn.sampleformat = QSF_S16;	break;
			/*case SDL_AUDIO_S32LE:
			case SDL_AUDIO_S32BE:	desired.format = obtained.format; sc->sn.samplebytes = 4;	sc->sn.sampleformat = QSF_S32;	break;*/
			case SDL_AUDIO_F32LE:
			case SDL_AUDIO_F32BE:	desired.format = obtained.format; sc->sn.samplebytes = 4;	sc->sn.sampleformat = QSF_F32;	break;
			default:	//just use what we originally asked for. let sdl figure out the conversions.
				break;
			}
		}


		sc->handle = SDL_OpenAudioDeviceStream(devid, &desired, SSDL3_Callback_Paint, sc);
		SDL_free(dev);
	}
	if (!sc->handle)
		return false;
	if (SDL_GetAudioStreamFormat(sc->handle, &desired, &obtained))
	{	//The mixer produces the stream input format; SDL converts to the device format.
		sc->sn.numchannels = desired.channels;
		sc->sn.speed = desired.freq;
		if (desired.format != obtained.format || desired.freq != obtained.freq || desired.channels != obtained.channels)
			Con_Printf("SDL3: %ibit%s %ihz audio input\n", SDL_AUDIO_BITSIZE(desired.format), SDL_AUDIO_ISFLOAT(desired.format)?" float":"", desired.freq);
		Con_Printf("SDL3: %ibit%s %ihz audio output\n", SDL_AUDIO_BITSIZE(obtained.format), SDL_AUDIO_ISFLOAT(obtained.format)?" float":"", obtained.freq);
		switch(desired.format)
		{
		case SDL_AUDIO_U8:		sc->sn.samplebytes = 1;	sc->sn.sampleformat = QSF_U8;	break;
		case SDL_AUDIO_S8:		sc->sn.samplebytes = 1;	sc->sn.sampleformat = QSF_S8;	break;
		case SDL_AUDIO_S16LE:
		case SDL_AUDIO_S16BE:	sc->sn.samplebytes = 2;	sc->sn.sampleformat = QSF_S16;	break;
		/*case SDL_AUDIO_S32LE:
		case SDL_AUDIO_S32BE:	sc->sn.samplebytes = 4;	sc->sn.sampleformat = QSF_S32;	break;*/
		case SDL_AUDIO_F32LE:
		case SDL_AUDIO_F32BE:	sc->sn.samplebytes = 4;	sc->sn.sampleformat = QSF_F32;	break;
		default:	//just use what we originally asked for. let sdl figure out the conversions.
			break;
		}
	}
	sc->Lock		= SSDL_Callback_LockBuffer;
	sc->Unlock		= SSDL_Callback_UnlockBuffer;
	sc->Submit		= SSDL_Callback_Submit;
	sc->Shutdown	= SSDL_Shutdown;
	sc->GetDMAPos	= SSDL_Callback_GetDMAPos;
	sc->selfpainting = true;

	SDL_ResumeAudioStreamDevice(sc->handle);

	return true;
}

static qboolean QDECL SDL_Enumerate(void (QDECL *cb) (const char *drivername, const char *devicecode, const char *readablename))
{
	SDL_AudioDeviceID *dev;
	int count, i;
	if(SSDL_InitAudio())
	{
		dev = SDL_GetAudioPlaybackDevices(&count);
		for (i = 0; i < count; i++)
		{
			const char *devname = SDL_GetAudioDeviceName(dev[i]);
			if (devname)
				cb(SDRVNAME, devname, va("SDL:%s", devname));
		}
		SDL_free(dev);
	}
	return true;
}

sounddriver_t SDL_Output =
{
	SDRVNAME,
	SDL_InitCard,
	SDL_Enumerate
};
#endif

#if 1 && defined(VOICECHAT)
// SDL3 recording uses an audio stream with explicit start and stop.
//Bugging out for me on windows, with really low audio levels. looks like there's been some float->int conversion without a multiplier. asking for float audio gives stupidly low values too.
typedef struct
{
	SDL_AudioStream *dev;
} sdlcapture_t;

static void QDECL SDL_Capture_Start(void *ctx)
{
	sdlcapture_t *d = ctx;
	SDL_ResumeAudioStreamDevice(d->dev);
}

static void QDECL SDL_Capture_Stop(void *ctx)
{
	sdlcapture_t *d = ctx;
	SDL_PauseAudioStreamDevice(d->dev);
}

static void QDECL SDL_Capture_Shutdown(void *ctx)
{
	sdlcapture_t *d = ctx;
	SDL_DestroyAudioStream(d->dev);
	Z_Free(d);
}

static qboolean QDECL SDL_Capture_Enumerate(void (QDECL *callback) (const char *drivername, const char *devicecode, const char *readablename))
{
	int i, count;
	if (SSDL_InitAudio())
	{
		SDL_AudioDeviceID *dev = SDL_GetAudioRecordingDevices(&count);
		for (i = 0; i < count; i++)
		{
			const char *name = SDL_GetAudioDeviceName(dev[i]);
			if (name)
				callback(SDRVNAME, name, va("SDL:%s", name));
		}
		SDL_free(dev);
	}
	return true;
}
static void *QDECL SDL_Capture_Init (int rate, const char *devname)
{
	sdlcapture_t c, *r;

	if (SSDL_InitAudio())
	{
		int i, count;
		SDL_AudioSpec have;
		SDL_AudioDeviceID dev = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;	//assume the default device.
		SDL_AudioDeviceID *devs = SDL_GetAudioRecordingDevices(&count);
		if (devname && *devname)
		for (i = 0; i < count; i++)
		{
			const char *name = SDL_GetAudioDeviceName(devs[i]);
			if (name && !strcmp(name, devname))
			{	//found it. use it instead of the default.
				dev = devs[i];
				break;
			}
		}
		SDL_free(devs);
		have.format = SDL_AUDIO_S16;
		have.channels = 1;
		have.freq = rate;
		c.dev = SDL_OpenAudioDeviceStream(dev, &have, NULL, NULL);
		if (c.dev)
		{
			r = Z_Malloc(sizeof(*r));
			*r = c;
			return r;
		}
	}
	return NULL;
}

/*minbytes is a hint to not bother wasting time*/
static unsigned int QDECL SDL_Capture_Update(void *ctx, unsigned char *buffer, unsigned int minbytes, unsigned int maxbytes)
{
	sdlcapture_t *c = ctx;
	int queuedsize = SDL_GetAudioStreamAvailable(c->dev);
    if (queuedsize <= 0) return 0;
	if (queuedsize < minbytes)
		return 0;
	if (queuedsize > maxbytes)
		queuedsize = maxbytes;

	queuedsize = SDL_GetAudioStreamData(c->dev, buffer, queuedsize);
	return queuedsize > 0 ? queuedsize : 0;
}
snd_capture_driver_t SDL_Capture =
{
	1,
	SDRVNAME,
	SDL_Capture_Enumerate,
	SDL_Capture_Init,
	SDL_Capture_Start,
	SDL_Capture_Update,
	SDL_Capture_Stop,
	SDL_Capture_Shutdown
};
#endif

