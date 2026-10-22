/* Copyright (C) 2026 FTEQW Delta contributors. GPL-2.0-or-later. */
#include "quakedef.h"
#ifdef AVAIL_MIDI
#include "fs.h"
#include "shader.h"

extern emenu_t *M_Options_Title(int *y, int infosize);
void M_Menu_MIDI_f(void);

typedef struct
{
    char *path, *value, *name;
    menubutton_t *button;
} midifont_t;
typedef struct
{
    midifont_t *fonts;
    size_t count, capacity;
    char folder[MAX_OSPATH];
    const char *origin;
    qboolean portable;
    int layoutoffset;
} midimenu_t;

static qboolean MIDI_SamePath(const char *a, const char *b)
{
#ifdef _WIN32
    return !Q_strcasecmp(a, b);
#else
    return !strcmp(a, b);
#endif
}

static int QDECL M_MIDI_Found(const char *name, qofs_t size, time_t mtime, void *ctx, searchpathfuncs_t *spath)
{
    midimenu_t *info = ctx;
    char path[MAX_OSPATH], label[MAX_OSPATH + 32], extension[16];
    midifont_t *font;
    (void)size;
    (void)mtime;
    (void)spath;
    /* Enumerating '*' also finds uppercase extensions on case-sensitive hosts. */
    COM_FileExtension(name, extension, sizeof(extension));
    if (Q_strcasecmp(extension, "sf2") && Q_strcasecmp(extension, "sf3"))
        return true;
    if (strchr(name, '/') || strchr(name, '\\'))
        return true; /* No subdirectories. */
    if (Q_snprintfz(path, sizeof(path), "%s%s", info->folder, name) >= sizeof(path))
        return true;
    for (size_t i = 0; i < info->count; ++i)
        if (MIDI_SamePath(info->fonts[i].path, path))
            return true;
    if (info->count == info->capacity)
        Z_ReallocElements((void **)&info->fonts, &info->capacity,
            info->capacity ? info->capacity * 2 : 16, sizeof(*info->fonts));
    font = &info->fonts[info->count++];
    memset(font, 0, sizeof(*font));
    font->path = Z_StrDup(path);
    font->value = info->portable ? Z_StrDup(va("soundfonts/%s", name)) : Z_StrDup(path);
    Q_snprintfz(label, sizeof(label), "%s (%s)", name, info->origin);
    font->name = Z_StrDup(label);
    return true;
}

static void M_MIDI_Scan(midimenu_t *info, const char *folder, const char *origin, qboolean portable)
{
    Q_strncpyz(info->folder, folder, sizeof(info->folder));
    for (char *s = info->folder; *s; ++s)
        if (*s == '\\')
            *s = '/';
    if (*info->folder && info->folder[strlen(info->folder)-1] != '/')
        Q_strncatz(info->folder, "/", sizeof(info->folder));
    info->origin = origin;
    info->portable = portable;
    Sys_EnumerateFiles(info->folder, "*", M_MIDI_Found, info, NULL);
}

static int M_MIDI_Sort(const void *a, const void *b)
{
    const midifont_t *x = a, *y = b;
    int order = Q_strcasecmp(COM_SkipPath(x->path), COM_SkipPath(y->path));
    return order ? order : strcmp(x->path, y->path);
}

static void M_MIDI_Free(emenu_t *menu)
{
    midimenu_t *info = menu->data;
    for (size_t i = 0; i < info->count; ++i)
    {
        Z_Free(info->fonts[i].path);
        Z_Free(info->fonts[i].value);
        Z_Free(info->fonts[i].name);
    }
    Z_Free(info->fonts);
}

static qboolean M_MIDI_ActivateKey(int key)
{
    return key == K_ENTER || key == K_KP_ENTER || key == K_GP_DIAMOND_CONFIRM ||
        key == K_GP_DIAMOND_ALTCONFIRM || key == K_MOUSE1 || key == K_TOUCHTAP;
}

static qboolean M_MIDI_Select(menuoption_t *option, emenu_t *menu, int key)
{
    midimenu_t *info = menu->data;
    if (!M_MIDI_ActivateKey(key))
        return false;
    for (size_t i = 0; i < info->count; ++i)
        if ((menuoption_t *)info->fonts[i].button == option)
        {
            /* Assign directly: filenames never become executable console text. */
            Cvar_Set(Cvar_FindVar("snd_midi_soundfont"), info->fonts[i].value);
            Con_Printf("MIDI SoundFont selected: %s\n", info->fonts[i].value);
            Cbuf_AddText("snd_restart\n", RESTRICT_LOCAL);
            M_RemoveMenu(menu);
            return true;
        }
    return false;
}

static qboolean M_MIDI_Refresh(menuoption_t *option, emenu_t *menu, int key)
{
    (void)option;
    if (!M_MIDI_ActivateKey(key))
        return false;
    M_RemoveMenu(menu);
    M_Menu_MIDI_f();
    return true;
}

static qboolean M_MIDI_Back(menuoption_t *option, emenu_t *menu, int key)
{
    (void)option;
    if (!M_MIDI_ActivateKey(key))
        return false;
    M_RemoveMenu(menu);
    return true;
}

static void M_MIDI_Background(emenu_t *menu)
{
    midimenu_t *info = menu->data;
    int offset = max(0, (vid.height - 200) / 2);
    /* MenuQC draws beneath native file pickers; cover it so two option pages
     * do not overlap. Keep classic 320x200 placement on larger virtual screens. */
    if (R2D_Flush)
        R2D_Flush();
    BE_Scissor(NULL);
    R2D_ImageColours(0.025, 0.02, 0.015, 1);
    R2D_FillBlock(0, 0, vid.width, vid.height);
    R2D_ImageColours(1, 1, 1, 1);
    /* The native scroll frame uses absolute option coordinates, so translate
     * the options rather than the menu origin when vertically centering. */
    for (menuoption_t *option = menu->options; option; option = option->common.next)
    {
        option->common.posy += offset - info->layoutoffset;
        if (option->common.type == mt_button && font_default && vid.rotpixelwidth)
        {
            conchar_t buffer[2048], *end;
            int px, py;
            end = COM_ParseFunString(CON_WHITEMASK, option->button.text, buffer, sizeof(buffer), false);
            Font_BeginString(font_default, 0, 0, &px, &py);
            px = Font_LineWidth(buffer, end);
            Font_EndString(font_default);
            /* Glyph advances round to whole physical pixels. strlen*8 can
             * underestimate their width at fractional scales and clip letters.
             * Re-measure on draw so changing UI scale/fonts also stays correct. */
            option->common.width = ceil(px * (double)vid.width / vid.rotpixelwidth);
            option->button.rightalign = false;
        }
    }
    info->layoutoffset = offset;
}

void M_Menu_MIDI_f(void)
{
    char root[MAX_OSPATH], folder[MAX_OSPATH], current[MAX_OSPATH];
    void *iterator = NULL;
    int y, top;
    emenu_t *menu = M_Options_Title(&y, sizeof(midimenu_t));
    midimenu_t *info = menu->data;
    menu->remove = M_MIDI_Free;
    menu->predraw = M_MIDI_Background;
    if (FS_SystemPath("soundfonts/", FS_BINARYPATH, folder, sizeof(folder)))
        M_MIDI_Scan(info, folder, "engine", true);
    while (COM_IteratePaths(&iterator, root, sizeof(root), NULL, 0))
        if (Q_snprintfz(folder, sizeof(folder), "%ssoundfonts/", root) < sizeof(folder))
            M_MIDI_Scan(info, folder, "game", false);
    if (FS_SystemPath("soundfonts/", FS_ROOT, folder, sizeof(folder)))
        M_MIDI_Scan(info, folder, "home/base", false);
#ifdef FTE_MIDI_INSTALLED_FONT
    Q_strncpyz(folder, FTE_MIDI_INSTALLED_FONT, sizeof(folder));
    *strrchr(folder, '/') = 0;
    M_MIDI_Scan(info, folder, "installed", false);
#endif
    if (info->count > 1)
        qsort(info->fonts, info->count, sizeof(*info->fonts), M_MIDI_Sort);
    if (!S_MIDI_SoundFontPath(current, sizeof(current)))
        *current = 0;
    MC_AddBufferedText(menu, 56, 56, y, "MIDI SoundFont", false, true); y += 12;
    MC_AddBufferedText(menu, 56, 56, y, "Select to apply and restart audio", false, false); y += 8;
    MC_AddBufferedText(menu, 56, 56, y, va("Current: %.22s", COM_SkipPath(Cvar_FindVar("snd_midi_soundfont")->string)), false, false); y += 8;
    MC_AddBufferedText(menu, 56, 56, y, "Add SF2/SF3 files to soundfonts/", false, false); y += 12;
    top = y;
    MC_AddFrameStart(menu, y);
    menu->selecteditem = (menuoption_t *)MC_AddCommand(menu, 56, 0, y, "Rescan Folder", M_MIDI_Refresh); y += 12;
    if (!info->count)
    {
        MC_AddBufferedText(menu, 56, 56, y, "No SoundFonts found.", false, false); y += 12;
    }
    for (size_t i = 0; i < info->count; ++i)
    {
        char label[128];
        size_t used = 0;
        qboolean active = MIDI_SamePath(info->fonts[i].path, current);
        /* Literal caret escapes and a short label keep arbitrary file names
         * from changing menu colors or drawing beyond the normal menu width. */
        label[used++] = active ? '*' : ' ';
        label[used++] = ' ';
        for (const char *s = info->fonts[i].name; *s && used < 30; ++s)
        {
            if (*s == '^')
                label[used++] = '^';
            label[used++] = (*s == '\r' || *s == '\n' || *s == '\t') ? ' ' : *s;
        }
        label[used] = 0;
        info->fonts[i].button = MC_AddCommand(menu, 56, 0, y, label, M_MIDI_Select);
        info->fonts[i].button->common.tooltip = info->fonts[i].path;
        if (active)
            menu->selecteditem = (menuoption_t *)info->fonts[i].button;
        y += 8;
    }
    y += 4;
    MC_AddCommand(menu, 56, 0, y, "Back", M_MIDI_Back);
    menu->cursoritem = MC_AddCursorSmall(menu, NULL, 48);
    MC_AddFrameEnd(menu, top);
    Con_DPrintf("MIDI SoundFont picker: %"PRIuSIZE" files\n", info->count);
    for (size_t i = 0; i < info->count; ++i)
        Con_DPrintf("MIDI SoundFont option: %s\n", info->fonts[i].value);
}
#endif
