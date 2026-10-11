#!/usr/bin/env python3
"""Generate the documented QC API from this engine's pr_dumpplatform output.

The deliberately restricted conditional reader supports the exporter's default
(no -Fdefines/-Faccessors) format, not arbitrary QuakeC preprocessing.
Unknown declarations/explanation gaps fail rather than silently dropping APIs.
"""
import re, argparse, ast, hashlib
from pathlib import Path
def active_source(source):
    defines=set(); stack=[]; active=True; out=[]
    def cond(s):
        s=re.sub(r'defined\s*\(?\s*(\w+)\s*\)?',lambda m:str(m[1] in defines),s)
        s=s.replace('&&',' and ').replace('||',' or ')
        s=re.sub(r'!(?!=)',' not ',s)
        def evaluate(node):
            if isinstance(node, ast.Constant) and isinstance(node.value, bool): return node.value
            if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.Not): return not evaluate(node.operand)
            if isinstance(node, ast.BoolOp) and isinstance(node.op, (ast.And,ast.Or)):
                values=[evaluate(v) for v in node.values]
                return all(values) if isinstance(node.op,ast.And) else any(values)
            raise ValueError('Unsupported export condition: '+s)
        return evaluate(ast.parse(s.strip(),mode='eval').body)
    for line in source.splitlines(keepends=True):
        m=re.match(r'\s*#(\w+)\s*(.*)',line)
        if m:
            kind, arg=m.groups()
            if kind in ('if','ifdef','ifndef'):
                test=(arg.split()[0] in defines) if kind!='if' else cond(arg)
                if kind=='ifndef': test=not test
                stack.append((active,test)); active=active and test
            elif kind=='else':
                parent,test=stack[-1]; active=parent and not test
            elif kind=='endif': active=stack.pop()[0]
            elif kind in ('elif','include'): raise ValueError('Unsupported export directive: '+kind)
            elif active and kind=='define': defines.add(re.match(r'\w+',arg)[0]); out.append(line)
            elif active and kind=='undef': defines.discard(arg.strip())
            continue
        if active: out.append(line)
    if stack: raise ValueError('Unclosed conditional in exported header')
    return ''.join(out)
def parse(path):
    source=active_source(path.read_text())
    clean=re.sub(r'/\*.*?\*/|//[^\n]*',lambda m:re.sub(r'[^\n]',' ',m[0]),source,flags=re.S)
    clean=re.sub(r'^\s*#[^\n]*',lambda m:re.sub(r'[^\n]',' ',m[0]),clean,flags=re.M)
    entries=[]
    for m in re.finditer(r'(typedef struct\s*\{.*?\}\s*(\w+)\s*;|accessor\s+(\w+)\s*:\s*\w+\s*\{.*?\};)',clean,re.S):
        name=m[2] or m[3]
        entries.append(dict(name=name,decl=source[m.start():m.end()].strip(),description='Structured argument type used by the corresponding builtins; member comments describe its layout.' if m[2] else 'Compiler accessor wrapper for the corresponding handle; calls the get/set builtins shown in its declaration.',kind='type'))
        clean=clean[:m.start()]+re.sub(r'[^\n]',' ',clean[m.start():m.end()])+clean[m.end():]
    for m in re.finditer(r'^([^\n#][^;]*;)',clean,re.M):
        decl=' '.join(m[1].split())
        # Each exported declaration is one statement; comments belong to preceding declaration.
        if not decl or decl.startswith('accessor'): continue
        if decl.startswith('enum '):
            enumname=re.match(r'enum\s+(\w+)',decl)[1]
            entries.append(dict(name=enumname,decl=decl,description='Integer enumeration identifying JSON node types; used by json_get_value_type.',kind='type'))
            for value,name in enumerate(re.search(r'\{(.*?)\}',decl)[1].split(',')):
                name=name.strip()
                entries.append(dict(name=name,decl=f'const int {name} = {value};',description='JSON node kind: '+name.removeprefix('JSON_TYPE_').lower()+'. Returned by json_get_value_type.',kind='constant'))
            continue
        nm=re.search(r'\)\s*(\w+)\s*=\s*#',decl) if '= #' in decl else re.search(r'\b(\w+)\s*(?:\[\d+\])?\s*(?:=\s*[^;]*)?;$',decl)
        if not nm: raise ValueError(decl)
        name=nm[1]
        following=source[m.end():]
        comment=re.match(r'\s*/\*(.*?)\*/',following,re.S)
        desc=' '.join(comment[1].split()) if comment else ''
        base=re.sub(r'^(?:(?:FTEDEP|DEP_SSQC|DEP_CSQC|DEP|__deprecated)(?:\("[^"\\]*(?:\\.[^"\\]*)*"\))?\s+)+','',decl)
        category='builtin' if re.search(r'=\s*#',base) else 'type' if base.startswith('typedef ') else 'constant' if base.startswith('const ') or '=' in base else 'field' if base.startswith('.') else 'callback' if '(' in base else 'global'
        if category=='global' and ',' in base:
            t,names=base[:-1].split(' ',1)
            for n in names.split(','): entries.append(dict(name=n.strip(),decl=t+' '+n.strip()+';',description=desc,kind=category))
        else: entries.append(dict(name=name,decl=decl,description=desc,kind=category))
    # Empty extension defines are availability labels, not runtime constants.
    for line in source.splitlines():
        m=re.match(r'\s*#define\s+(\w+)(.*)',line)
        if not m or m[1] in {'FTEDEP','DEP','DEP_SSQC','DEP_CSQC','NETQUAKE','NQSSQC','QUAKEWORLD','QWSSQC','SSQC','CSQC','MENU'}: continue
        name,tail=m.groups(); comment=re.search(r'/\*(.*?)\*/',tail)
        value=re.sub(r'/\*.*?\*/|//.*','',tail).strip()
        kind='extension' if not value else 'type' if name in {'strbuf','searchhandle','hashtable','infostring','filestream'} else 'alias'
        entries.append(dict(name=name,decl='#define '+name+(' '+value if value else ''),description=' '.join(comment[1].split()) if comment else '',kind=kind))
    return entries

"""Explanations for exported symbols without useful source descriptions."""
NOTES={}
def notes(block):
    for line in block.strip().splitlines():
        names,meaning=line.split(': ',1)
        for name in names.split(): NOTES[name]=meaning
notes('''
random: Pseudorandom float between zero and one. The endpoint can be one; clamp computed array indices.
normalize: Returns a vector with the same direction and unit length; handle zero-length input deliberately.
vlen: Returns a vector's Euclidean length in game units.
vectoyaw: Returns the horizontal yaw angle of a direction vector, in degrees.
spawn: Allocates a new entity with initially cleared fields. Configure, link and eventually remove it.
remove: Removes the given entity. Do not continue using it; its slot may be reused.
find: Searches entities after start for a matching string field; returns world when there is no match.
precache_sound: Registers a game-relative sound path before use; normally called during map spawning.
stof: Parses a numeric string into a float; validate input rather than interpreting a zero result as proof of validity.
search_end: Releases a filesystem search handle when finished enumerating its results.
bprint: Prints to all players. NQ and QW use different signatures; QW adds a priority argument. Menu version is deprecated.
dprint: Developer/debug console output, controlled by developer settings.
ftos: Converts a float to a temporary text string.
vtos: Converts a vector to a temporary text string.
eprint: Prints an entity's fields for debugging.
rint: Rounds to the nearest integer value, returned as a float.
floor: Rounds downward toward negative infinity.
ceil: Rounds upward toward positive infinity.
fabs: Returns the absolute value.
cvar: Reads a console variable as a float; a missing/non-numeric value normally reads as zero.
localcmd: Queues commands for this VM's host console. Append a newline; never concatenate untrusted text.
nextent: Returns the next live entity after the argument, or world at the end.
vectoangles: Converts a direction vector to Euler angles in degrees; optional roll/up conventions depend on the signature.
cvar_set: Sets a console variable from a string; the engine may protect or restrict specific variables.
centerprint: Sends centered text to the specified player's view.
anglemod: Wraps an angle into the engine's 0-to-360-degree representation.
cvar_setf: Sets a console variable from a float, converting it to text.
rotatevectorsbyvectors: Transforms the existing v_forward/v_right/v_up basis by the supplied basis vectors.
skinforname: Finds a model's skin index by its exported skin name; check the returned index before use.
te_muzzleflash: Requests a muzzle-flash effect at the supplied entity; does not implement weapon damage.
mod: Returns the floating-point remainder of val divided by m. Avoid a zero divisor.
sqlreadblob: Copies a query cell's binary data into the caller's buffer, bounded by maxsize; requires a SQL backend.
sqlescapeblob: Encodes/escapes binary data for the selected SQL backend. Validate buffer size and backend support.
pushmove: Attempts to move/rotate a pusher and affected entities using the engine's pusher collision handling.
frametoname: Returns the exported animation/frame name for a model frame index.
skintoname: Returns the exported skin/material name for a model skin index.
num_for_edict: Converts an entity reference to this VM's entity-slot number, not a cross-VM entity reference.
buf_cvarlist: Fills a string buffer with console-variable names matching pattern and excluding antipattern.
gettime gettimed: Returns the selected clock: 0=host time cached at frame start (default), 1=milliseconds clock converted to seconds, 5=client simulation time when available. gettimed uses double precision; use time for scheduled server thinks.
getsurfacenumtriangles: Returns the triangle count of the selected surface of an entity model.
getsurfacetriangle: Returns the three vertex indices of the selected surface triangle, encoded in a vector.
digest_hex: Computes the requested digest (for example SHA256) of the supplied text and returns hexadecimal text; check algorithm availability.
rotatevectorsbytag: Transforms the current v_forward/v_right/v_up basis by an entity's model attachment/tag transform.
drawline: Draws a 2D line of the specified width between two positions using the active 2D drawing context.
iscachedpic: Tests whether an image is available in the picture cache.
precache_pic: Loads/caches an image for 2D drawing; flags control loading behavior. It does not create a world sprite model.
drawgetimagesize: Returns a cached image's width and height in the vector's X and Y components.
drawcharacter: Draws one character code at a 2D position with scale, color, alpha and blend flags.
drawrawstring: Draws text without the colored-text interpretation used by ordinary string drawing.
drawpic: Draws a whole cached image in a 2D rectangle with tint, alpha and blend flags.
drawfill: Draws a colored 2D rectangle, with alpha and optional blend flags.
drawsetcliparea: Restricts subsequent 2D drawing to the supplied rectangle.
drawresetcliparea: Removes the current 2D drawing clip rectangle.
drawstring: Draws a string using the active font and supplied position, scale, color, alpha and flags.
stringwidth: Measures text width at the supplied font size; usecolours controls colored-text parsing.
drawsubpic: Draws a subrectangle of an image into a 2D destination rectangle; source coordinates are normalized image coordinates.
cin_open: Opens a cinematic/video resource identified by id; actual decoding requires a supported decoder.
cin_close: Closes the cinematic resource identified by id.
cin_setstate: Changes a cinematic's playback state to the supplied state value.
cin_getstate: Returns a cinematic's playback state.
cin_restart: Restarts playback of the named cinematic resource.
boxparticles: Emits the selected particle effect over the supplied origin/direction box; flags and countmultiplier control the effect.
keynumtostring_omgwtf keynumtostring_menu: Legacy key-number-to-name conversion. Deprecated; use the non-deprecated key naming API for new code.
findkeysforcommand: Returns key bindings associated with a console command, optionally within a bind map.
setmousetarget getmousetarget: Sets/gets the legacy mouse event destination; prefer the documented input routing API for new code.
setkeybind: Assigns a console command binding to a key, optionally for a bind map/modifier; respect player settings.
getbindmaps: Returns the active bind maps in a vector.
setbindmaps: Selects the active bind maps using the supplied vector.
msprint: Deprecated MenuQC client-slot print interface; it is not an SSQC player entity print call.
etof: Converts a MenuQC entity reference to its numeric entity index.
ftoe: Converts a MenuQC numeric entity index to an entity reference; validate the index.
setkeydest: Sets the keyboard input destination (game, console or menu according to the engine's routing identifiers).
getkeydest: Returns the current keyboard input destination.
sin cos: Computes sine/cosine with an angle measured in radians, not degrees.
sqrt: Returns the square root; require a nonnegative input for a real result.
pow: Raises the first argument to the second argument's power.
asin acos atan: Computes an inverse trigonometric function, returning radians; respect its input domain.
atan2: Computes the quadrant-aware arctangent of a pair of components, returning radians; follow the exported argument order.
min max: Returns the smallest/largest supplied value.
bound: Clamps value between min and max.
changepitch: Moves an entity's pitch toward idealpitch at its pitch_speed limit.
etos: Converts an entity reference to a diagnostic text string.
logfrag: Records a frag event for the supplied killer and victim; does not change health or scores itself.
findfloat: Finds an entity after start whose specified numeric/variant field matches; returns world if absent.
hidepic: Hides a previously named legacy screen-picture slot; prefer CSQC for new HUDs.
movepic: Moves a legacy screen-picture slot to a new position/zone.
changepic: Changes the image used by a legacy screen-picture slot.
fopen: Opens a game-relative file in the selected FILE_* mode, returning a handle; check failure and close it.
fclose: Closes an open file handle.
fgets: Reads a line from a file handle; check the documented EOF behavior and string lifetime.
fputs: Writes a string to an open file handle; include your own newline if required.
RegisterTempEnt: Registers a custom temporary-effect type/format for supporting clients; returns its type identifier.
CustomTempEnt: Emits an effect using a registered custom temporary-effect format. Producer and client must agree on its parameters.
particle2 particle3 particle4: Emits a Hexen II-style particle effect using the supplied direction range, box or radius and effect parameters.
bitshift: Shifts bits left for positive quantity and right for negative quantity; uses the extension's integer conversion rules.
checkpvs: Tests whether an entity is potentially visible from the supplied view position; PVS is not a line-of-sight collision trace.
matchclientname: Finds a client matching a name, optionally selecting a match; handle no/ambiguous matches.
sqlconnect: Opens a SQL connection using the selected driver; requires a usable backend/plugin and returns a connection identifier.
sqldisconnect: Closes the selected SQL connection.
sqlopenquery: Submits a SQL query and invokes its callback with result metadata; lifetime depends on query type/backend.
sqlclosequery: Releases an open SQL query/result identifier.
sqlreadfield: Reads a result cell as a string.
sqlerror: Gets the selected SQL connection/query's error description.
sqlescape: Escapes a string using the SQL backend's rules; do not treat untrusted text as raw SQL syntax.
sqlversion: Returns the selected SQL backend's version text.
sqlreadfloat: Reads a SQL result cell as a float.
findchain: Finds entities matching a string field and links them through .chain; world terminates the chain.
findchainfloat: Finds entities matching a numeric/variant field and links them through .chain.
vectorvectors: Builds v_forward/v_right/v_up from a direction vector rather than Euler angles.
getsurfacenumpoints: Returns the number of points on the selected model surface.
getsurfacepoint: Returns the selected surface point in the model's coordinate conventions.
getsurfacenormal: Returns the selected surface's normal vector.
getsurfacetexture: Returns the selected surface's texture/material name.
getsurfacenearpoint: Finds the surface nearest a supplied point; interpret coordinate space according to the model API.
getsurfaceclippedpoint: Returns a point projected/clipped onto the selected surface.
clientcommand: Passes a player's command to the engine's ordinary command handler; useful for forwarding unhandled SV_ParseClientCommand commands.
setattachment: Attaches an entity to another entity's named model tag; both model/tag and client protocol support matter.
findflags: Finds the next entity with matching bits in a float field, returning world if absent.
findchainflags: Builds a .chain list of entities with matching field flag bits.
gettagindex: Looks up a named model attachment/tag index on an entity.
dropclient: Disconnects the selected player from the server.
spawnclient: Creates a server-side client/bot entity; game code must implement its game behavior.
clienttype: Reports a client's connection/bot classification; interpret the engine's documented values rather than guessing from its name.
WriteUnterminatedString: Writes string bytes without a terminating NUL to a selected network message buffer; receiver must know the length.
edict_num: Gets an entity from this VM's entity number; validate range and lifetime.
buf_create: Allocates a string-buffer handle.
buf_del: Frees a string buffer and its contents.
buf_getsize: Returns the string buffer's index extent/size; sparse indices may include empty entries.
buf_copy: Copies one string buffer's contents to another.
buf_sort: Sorts buffer strings, with prefix length and reverse-order controls.
buf_implode: Joins the buffer's strings using glue and returns the resulting string.
bufstr_get: Retrieves a string at a buffer index.
bufstr_set: Stores a string at a buffer index.
bufstr_add: Adds a string to a buffer, with ordered placement control; returns its index.
bufstr_free: Releases the string at the specified buffer index.
strftime: Formats a clock/calendar timestamp using the supplied format; uselocaltime selects local time or UTC.
strtolower strtoupper: Returns the supplied string converted to lower/upper case according to the builtin's character handling.
cvar_defstring: Reads the console variable's default value as text.
pointsound: Starts a spatial sound at a fixed world position instead of following an entity.
strreplace: Replaces matching substrings in subject, with case-sensitive matching.
strireplace: Replaces matching substrings in subject, ignoring case.
getsurfacepointattribute: Gets the requested SPA_* attribute of a selected surface point.
crc16: Computes a legacy 16-bit checksum; deprecated in favor of digest_hex and not suitable for authentication.
cvar_type: Returns console-variable existence/type/flag information; inspect the extension's CVAR_TYPE_* bits.
getentityfieldstring: Reads a selected entity field as text using its reflected field index.
putentityfieldstring: Parses text into a selected entity field; validate field type, index and input.
precache_vwep_model: Registers a third-person visible weapon model using the ZQ_VWEP interface.
CSQC_Ent_Remove: Client callback when a custom network entity is removed; release its client-side resources/state.
CSQC_Event_Sound: Client sound-event callback. Follow the exact signature and return convention for event handling.
GameCommand: Entry point for commands explicitly sent to the QC module; validate arguments before acting.
CSQC_Parse_SetAngles: Client callback receiving a server view-angle update.
CSQC_PlayerInfoChanged: Client callback when information for a player changes; refresh cached UI information.
CSQC_ServerInfoChanged: Client callback when server information changes.
CSQC_ServerSound: Client callback for a server sound event; use the passed origin/entity/sample information.
CSQC_MapEntityEdited: Client notification of a map-entity edit; refresh any dependent editor/view state.
m_init: Initializes the custom menu module.
m_shutdown: Shuts down the custom menu module; free resources it owns.
m_keydown m_keyup: Menu keyboard callbacks for key press/release; use the exact key/character parameters.
m_toggle: Callback for toggling the custom menu's visibility/state.
m_consolecommand: Menu console-command handler; return the declared result when deciding whether a command was handled.
m_gethostcachecategory: Menu callback classifying a server browser entry for category display.
serverflags: Game-managed persistent server/map progression bits, traditionally carried across map changes.
total_secrets total_monsters: Game-maintained totals advertised to the client; the engine does not discover your custom secrets/enemies.
found_secrets killed_monsters: Game-maintained progress counters; increment according to your rules.
v_forward v_up v_right: Direction-basis result globals written by makevectors/vectorvectors and related operations; overwritten by the next call.
trace_allsolid: Nonzero when the trace remains entirely in solid geometry.
trace_startsolid: Nonzero when the trace begins inside a solid.
trace_fraction: Completed fraction of the requested movement, from zero to one; one means no blocking impact.
trace_endpos: End position of the latest trace.
trace_plane_normal: Normal of the surface hit by the latest trace; meaningful for a valid impact.
trace_plane_dist: Plane distance for the latest trace's impact plane.
trace_ent: Entity hit by the latest trace; world can indicate world geometry or the null result depending on other trace outputs.
trace_inopen trace_inwater: Legacy trace flags recording passage through open/water contents.
msg_entity: Player entity selecting the recipient for MSG_ONE messages and related unicast operations.
end_sys_globals end_sys_fields: Layout/CRC boundary marker for system globals/fields. Keep generated order; it is not a gameplay variable to mutate.
input_timelength: Duration of the current input command, in seconds; use it for input-driven simulation.
input_movevalues: Current command's intended forward, sideways and upward movement components.
input_buttons: Bitmask of button states in the current input command.
input_impulse: Current command's impulse number; game code assigns meaning to impulses.
trace_endcontents trace_endcontentsf: Contents at the trace endpoint; the f variant is a legacy floating-point representation.
trace_surfaceflags trace_surfaceflagsf: Surface flags for the trace hit; the f variant is legacy. Do not confuse flags with contents.
trace_brush_id trace_brush_faceid: Identifiers for the brush/brush face hit by a trace, usable with the matching map-editing API.
trace_dpstartcontents trace_dphitcontents trace_dphitq3surfaceflags: Legacy DarkPlaces-compatible trace contents/surface information; prefer the current typed trace fields.
intermission: Client intermission state; use it to change HUD/view presentation.
clframetime: Client frame delta in seconds; different from server simulation time.
servertime serverprevtime: Current/previous server update time exposed to CSQC for interpolation.
serverdeltatime: Difference between server update times, for interpolation/simulation bookkeeping.
intermission_time: Client time associated with the current intermission.
pmove_mins pmove_maxs: Player prediction collision-box extents; keep them consistent with server movement rules.
pmove_jump_held: Prediction jump-button latch for preventing repeated jumping while held.
pmove_waterjumptime: Prediction timer for water-jump movement behavior.
input_clienttime: Timestamp of the current client input command in the relevant client clock domain.
autocvar_vid_conwidth autocvar_vid_conheight: Compiler autocvar-backed console/UI width and height values; use for virtual 2D layout.
punchangle: Player view kick angles; game code can set them for impacts/recoil.
touch: Entity callback when collision/touch processing contacts another entity; self is this entity and other is the contact.
use: Game-defined entity activation callback; your target/interaction code invokes it.
think: Entity callback scheduled by nextthink; reschedule explicitly for repeated work.
blocked: Pusher callback when an obstruction prevents movement; other identifies the blocker.
groundentity: Entity supporting this entity when grounded; inspect FL_ONGROUND as well.
health max_health: Game-managed current/maximum health. Implement your own damage, healing and death policy.
frags: Player score exposed to client scoreboard/stat handling; game code updates it.
weapon: Game-managed weapon selection/stat identifier; the engine does not implement firing from this value alone.
weaponmodel: Player first-person viewmodel path; precache before assigning.
weaponframe: Animation frame of the player's first-person viewmodel.
currentammo: Game-managed ammunition displayed for the current weapon.
ammo_shells ammo_nails ammo_rockets ammo_cells: Conventional Quake ammunition fields/stats; your conversion may choose how to use them.
items: Conventional inventory bitmask sent to clients; set/clear bits according to your game's inventory rules.
takedamage: DAMAGE_* eligibility setting used by game damage code; it does not supply a complete damage implementation.
chain: Linked-list field used by findchain APIs or your game code; calls can overwrite it.
deadflag: Conventional game-managed dead/alive state; implement death transitions yourself.
view_ofs: Player eye offset from origin, also used when constructing aiming traces.
button0: Player attack button state from input.
button1: Legacy player button field; do not assume a universal gameplay action for it.
button2: Player jump button state; game code implements its jump policy.
impulse: Player input impulse command; consume/reset it according to your game's command policy.
idealpitch: Desired pitch, used by changepitch and game view/AI logic.
netname: Player's displayed network name; handle it as untrusted text.
enemy goalentity: Game-managed target/chase references used by AI/movement logic; validate their lifetime.
flags: Entity FL_* bitmask, combining engine-maintained movement state and game policy bits.
colormap: Legacy player-color translation/index information; model and protocol conventions apply.
team: Conventional game-managed team number; does not implement friendly-fire/team rules itself.
armortype armorvalue: Conventional game-managed armor absorption fraction and amount; implement damage absorption explicitly.
waterlevel: Engine movement state for liquid immersion, conventionally 0=dry, 1=feet, 2=waist, 3=submerged.
watertype: Liquid contents identifier where the entity is immersed; use CONTENT_* values.
ideal_yaw yaw_speed: Desired yaw and maximum yaw adjustment used by changeyaw/AI.
aiment: Attachment/follow target entity for the corresponding movement mode.
spawnflags: Bitmask parsed from the map; engine spawn-filter bits and class-specific game flags have different roles.
target targetname: Map strings for game-defined activation links; your code performs lookup and activation.
dmg_take dmg_save: Player damage-feedback amounts accumulated for client feedback; actual health/armor rules are game code.
dmg_inflictor: Entity identifying the source position of damage feedback.
owner: Owning entity, commonly the projectile's shooter; used by relevant collision/game rules.
movedir: Direction convention used by game movers/triggers; initialize it from your map conventions.
sounds: On worldspawn selects map music track; on other entities it is a conventional game-defined sound selector.
noise noise1 noise2 noise3: Conventional per-entity sound-path fields; precache and play them explicitly.
pmove_flags: Movement/prediction flags for the corresponding physics path; keep client/server interpretation consistent.
geomtype: Rigid-body geometry selector from GEOMTYPE_*; requires an available physics backend.
friction erp: Rigid-body surface friction/error reduction parameters; meaning and availability depend on the physics backend.
jointtype jointgroup: Rigid-body joint type/group selection; requires a compatible physics backend and related entity setup.
mass: Rigid-body mass; not a replacement for ordinary MOVETYPE_WALK player movement rules.
bouncefactor bouncestop: Rigid-body restitution and stopping threshold parameters; backend dependent.
damp_linear damp_angular max_angular: Rigid-body linear/angular damping and angular-speed limit; backend dependent.
pitch_speed: Maximum pitch adjustment used by changepitch.
maxspeed: Per-entity/player movement speed limit in the supported movement path.
viewzoom: Player view/FOV scale communicated by supporting protocols.
playerclass hasted: Hexen II-compatible player-class/haste state; not generic Quake inventory semantics.
renderflags: Client rendering RF_* bitmask; use flags valid for this VM/render API.
frame4 lerpfrac4: Additional animation frame and blend fraction used by extended animation blending.
clientcolors: Legacy combined player-color value, deprecated because it cannot represent RGB player colors.
TRUE: Boolean true, represented as one; use FALSE/zero for false.
FALSE: Boolean false, represented as zero. Not a file handle or error-string value by itself.
MOVETYPE_NONE: No automatic entity movement; game code may reposition it explicitly.
MOVETYPE_WALK: Walking/player movement using the standard movement path and collision handling.
MOVETYPE_STEP: Step-based ground monster movement; game code supplies AI/movement intents.
MOVETYPE_FLY: Flying movement without ordinary gravity; valid player behavior requires the relevant extension/path.
MOVETYPE_TOSS: Gravity-driven thrown entity; normally settles on collision.
MOVETYPE_PUSH: Brush pusher movement such as a door or platform, with obstruction handling and its ltime clock.
MOVETYPE_NOCLIP: Movement without blocking collision; game code should deliberately control who may use it.
MOVETYPE_FLYMISSILE: Flying projectile movement with missile collision conventions.
MOVETYPE_BOUNCE: Gravity-driven motion that bounces on collision.
MOVETYPE_BOUNCEMISSILE: Bouncing projectile variant, requiring the advertised extended movement support.
MOVETYPE_FOLLOW: Follows an attachment target using the extension's attachment conventions.
SOLID_NOT: Non-solid entity; no blocking collision.
SOLID_TRIGGER: Overlap/touch-only trigger volume; does not block movement.
SOLID_BBOX: Blocking axis-aligned entity collision box.
SOLID_SLIDEBOX: Player/monster sliding collision box, with the movement path's step/slide rules.
GEOMTYPE_NONE: No rigid-body collision geometry.
GEOMTYPE_SOLID: Derive rigid-body collision geometry from the entity's solid/model configuration.
DAMAGE_NO: Game damage convention: not damageable.
DAMAGE_YES: Game damage convention: damageable.
DAMAGE_AIM: Game damage convention: damageable and eligible for traditional aim assistance.
CONTENT_EMPTY: Legacy contents value for empty space.
CONTENT_SOLID: Legacy contents value for solid world geometry.
CONTENT_WATER CONTENT_SLIME CONTENT_LAVA: Legacy liquid contents values identifying water, slime or lava; damage/drowning behavior is game code.
CONTENT_SKY: Legacy contents value for sky geometry.
CONTENTBIT_NONE: No flags in an extended contents bitmask.
CONTENTBITS_FLUID: Combined extended liquid-contents mask; use bit tests, not legacy negative contents comparisons.
SPA_S_AXIS SPA_T_AXIS: Surface texture-coordinate basis vectors for getsurfacepointattribute.
SPA_TEXCOORDS0: Surface point's primary texture coordinates for getsurfacepointattribute.
SPA_LIGHTMAP0_TEXCOORDS SPA_LIGHTMAP0_COLOR: Surface point's first lightmap coordinates or lighting color attribute.
CHAN_WEAPON CHAN_VOICE CHAN_ITEM CHAN_BODY: Conventional nonzero entity sound channels for weapon, voice, item or body events; replace the previous sound on that entity/channel.
INFOKEY_P_SVPING: Player infokey for server-measured ping.
INFOKEY_P_CHALLENGE: Player connection challenge infokey; not a substitute for an authentication scheme.
INFOKEY_P_USERID: Player server-assigned user identifier infokey; not an entity number.
INFOKEY_P_TRUSTLEVEL: Player trust/authentication-level infokey; interpret the server's authentication policy.
FL_FLY FL_SWIM: Entity capability flags for flying/swimming movement paths.
FL_CLIENT: Marks a client/player entity; do not arbitrarily turn a non-client entity into a network client with this bit.
FL_INWATER: Engine movement flag for immersion in liquid.
FL_MONSTER: Marks a monster for applicable collision/trace/game conventions; it does not implement AI.
FL_GODMODE FL_NOTARGET: Conventional game policy flags for invulnerability or exclusion from AI targeting; your rules must respect them.
FL_ITEM: Item/pickup collision convention flag.
FL_ONGROUND: Engine movement flag indicating ground support; check before game actions such as jumping.
FL_PARTIALGROUND: Movement flag for partial ground support in step/monster movement.
FL_WATERJUMP: Movement flag for an active water-jump state.
FL_JUMPRELEASED: Legacy game jump-button latch convention.
MOVE_NORMAL: Ordinary entity/world collision mode for trace operations.
EF_BRIGHTFIELD: Classic particle field effect on an entity.
EF_MUZZLEFLASH: Classic muzzle-flash effect bit; visual effect only.
EF_BRIGHTLIGHT EF_DIMLIGHT: Classic bright/dim dynamic light effect bits; renderer settings may disable their presentation.
EF_FLAG1 EF_FLAG2: QuakeWorld player flag-carrying presentation bits.
MF_ROCKET MF_GRENADE: Model flag selecting the conventional rocket/grenade particle trail.
MF_ROTATE: Model flag enabling the conventional automatic item rotation presentation.
PRINT_LOW PRINT_MEDIUM PRINT_HIGH PRINT_CHAT: QuakeWorld text message priority levels, filtered/displayed according to client print settings.
FONT_DEFAULT: Default font identifier for client/menu text drawing.
IE_ACCELEROMETER IE_GYROSCOPE: Input-event types carrying accelerometer/gyroscope data on supported devices.
LFLAG_NORMALMODE LFLAG_REALTIMEMODE: Light flags selecting visibility in ordinary or real-time lighting modes.
LFLAG_LIGHTMAP: Light flag for contributing through the supported lightmap lighting path.
LFLAG_FLASHBLEND: Light flag requesting the supported flashblend presentation path.
LFLAG_NOSHADOWS: Light flag suppressing shadow casting.
LFLAG_SHADOWMAP: Light flag selecting the supported shadow-map path.
LFLAG_CREPUSCULAR: Light flag requesting supported crepuscular/shaft effects; renderer support is required.
LFLAG_ORTHOSUN: Light flag selecting a directional/orthographic sunlight-style projection.
''')

def supplement(name,kind,decl):
    if name in NOTES: return NOTES[name]
    if re_match(r'parm\d+$',name): return 'Player map-transfer parameter '+name[4:]+': set in SetNewParms/SetChangeParms, restore via setspawnparms; valid only in the transfer workflow.'
    if re_match(r'button\d+$',name): return 'Additional player input button state; bind its action explicitly rather than assuming classic Quake semantics.'
    if name.startswith('K_'): return 'Key/event code for '+name[2:].lower().replace('_',' ')+', used in client/menu input callbacks and key-binding APIs. This value is an engine key code, not necessarily a Unicode character.'
    prefixes={
        'SOLID_PHYSICS_': ('Legacy rigid-body solid selector for ', '; backend support is required; prefer documented geomtype setup.'),
        'GEOMTYPE_': ('Rigid-body geometry selector for ', '; use with .geomtype and a supporting physics backend.'),
        'JOINTTYPE_': ('Rigid-body joint selector for ', '; use with .jointtype and backend-specific joint setup.'),
        'CONTENTBIT_': ('Extended contents-mask bit for ', '; test bitwise, independently of legacy CONTENT_* values.'),
        'TE_': ('Temporary network effect identifier: ', '. Use the corresponding te_* builtin or the specified wire format; the effect does not implement damage.'),
        'te_': ('Emits a temporary visual effect: ', '. Parameters select location/direction/color/count as declared; this does not implement damage.'),
        'EV_': ('Reflected QC value-type identifier: ', '; used by typed field/hash/reflection APIs.'),
        'STAT_': ('Client/server statistic index for ', '. Game code maintains custom/conventional game values; transmit/read with matching stat APIs.'),
        'VF_': ('View property selector: ', '; use with getproperty/setproperty and the documented scalar/vector component type.'),
        'LFIELD_': ('Dynamic-light property selector: ', '; use with getlight/setlight. The suffix identifies the property, not a value to assign to it.'),
        'TEREDIT_': ('Terrain editing operation: ', '; pass to terrain_edit with the matching operation arguments; terrain support is required.'),
        'SLIST_': ('Server-browser property/comparison selector: ', '; use with the matching gethostcache/sethostcachemask/sorting API.'),
        'SL_ORG_': ('Legacy picture origin/alignment code: ', '; T/B/M mean top/bottom/middle and L/R/M mean left/right/middle. Prefer CSQC layout for new HUDs.'),
    }
    for prefix,(before,after) in prefixes.items():
        if name.startswith(prefix): return before+name[len(prefix):].lower().replace('_',' ')+after
    if kind=='extension': return 'Compile-time label for the '+name.replace('_',' ')+' feature/interface. It does not enable that feature at runtime; query checkextension("'+name+'") and read the related declarations before using it.'
    if kind=='alias': return 'Source macro expanding to the expression/builtin shown in its declaration; not a separate engine entry point. Its target API has the same scope and capability requirements.'
    if kind=='type': return 'Compiler type/handle alias used by the signatures below. Respect the underlying type and matching allocation/cleanup APIs; it is not a mutable game global.'
    return None

def re_match(pattern,text):
    import re
    return re.fullmatch(pattern,text)

MODES={'nq':'NQ SSQC','qw':'QW SSQC','cs':'CSQC','menu':'MenuQC'}
BEGIN='<!-- BEGIN GENERATED QUAKEC API -->'
END='<!-- END GENERATED QUAKEC API -->'

def reference(folder):
    entries={}; best={}; hashes=[]
    for mode,label in MODES.items():
        path=folder/(mode+'_api.qc')
        exported=path.read_text(encoding='utf-8')
        marker={'nq':'NQSSQC','qw':'QWSSQC','cs':'CSQC','menu':'MENU'}[mode]
        if not re.search(r'^\s*#define '+marker+r'\s*$',exported,re.M):
            raise ValueError(f'{path.name} is not the expected {label} export')
        hashes.append(f'{path.name}: {hashlib.sha256(path.read_bytes()).hexdigest()}')
        rows=parse(path)
        raw_builtins=set(re.findall(r'\)\s*(\w+)\s*=\s*#',active_source(exported)))
        parsed_builtins={e['name'] for e in rows if e['kind']=='builtin'}
        if raw_builtins!=parsed_builtins:
            raise ValueError(f'{path.name}: builtin coverage mismatch: {raw_builtins ^ parsed_builtins}')
        for e in rows:
            key=e['kind'],e['name'],e['decl']
            scopes=entries.setdefault(key,dict(e,modes=[]))['modes']
            if label not in scopes: scopes.append(label)
            bkey=e['kind'],e['name']
            if len(e['description'])>len(best.get(bkey,'')): best[bkey]=e['description']
    for e in entries.values():
        desc=e['description']
        if e['name']=='FALSE': desc=NOTES['FALSE']
        if e['name']=='memfree': desc=desc.replace('allocated with memfree','allocated with memalloc')
        short=not desc or bool(re.fullmatch(r'Part of _?[A-Z][A-Z0-9_]*(?:, _?[A-Z][A-Z0-9_]*)*',desc))
        if short:
            other=best.get((e['kind'],e['name']),'')
            explanation=supplement(e['name'],e['kind'],e['decl'])
            if not explanation and other and not re.fullmatch(r'Part of _?[A-Z][A-Z0-9_]*(?:, _?[A-Z][A-Z0-9_]*)*',other): explanation=other
            if not explanation: raise ValueError(f"Needs a reviewed explanation: {e['kind']} {e['name']}")
            desc=(desc+'. ' if desc else '')+explanation
        e['description']=desc
    # Different modules can bind the same name to different numbers/signatures.
    # Retain those as separate entries rather than flattening by name.
    grouped={kind:[] for kind in ('builtin','callback','global','field','constant','type','alias','extension')}
    for e in entries.values(): grouped[e['kind']].append(e)
    names={e['name'] for e in entries.values()}
    out=[BEGIN,'',f'This snapshot lists **{len(names)} distinct names** in **{len(entries)} declarations** across the four VM interfaces.','']
    out+=['<!-- Export SHA-256 hashes for reproducibility:\n'+'\n'.join(hashes)+'\n-->','']
    headings={'builtin':'Engine functions (builtins)','callback':'Callbacks you implement','global':'Engine globals and layout markers','field':'Entity fields','constant':'Constants','type':'Types and handle aliases','alias':'Convenience macros','extension':'Extension feature labels'}
    for kind,rows in grouped.items():
        rows.sort(key=lambda e:(e['name'].casefold(),e['decl']))
        out += ['### '+headings[kind],'',f'{len(rows)} declarations.','']
        for e in rows:
            n,desc,decl=e['name'],e['description'],e['decl']
            groups=re.search(r'Part of (_?[A-Z][A-Z0-9_]*(?:, _?[A-Z][A-Z0-9_]*)*)',desc)
            extension_groups=groups[1].strip().split(', ') if groups else []
            if n.startswith('FTE_') or any(g.startswith('FTE_') for g in extension_groups): compatibility='FTE extension'
            elif extension_groups or kind=='extension': compatibility='Shared extension'
            elif '#0:' in decl: compatibility='Unnumbered/FTE binding'
            else: compatibility='Engine interface'
            if kind in ('type','alias'): compatibility='Source convenience'
            out += [f'#### `{n}`','',f"**Scope:** {', '.join(e['modes'])}. **Compatibility:** {compatibility}.",'','```qc',decl,'```','',desc,'']
    out += [END,'']
    return '\n'.join(out)

def main():
    parser=argparse.ArgumentParser(description='Refresh the complete QC reference in writing_progs.md from four pr_dumpplatform exports.')
    parser.add_argument('--api-dir',required=True,type=Path,help='folder containing nq_api.qc, qw_api.qc, cs_api.qc, menu_api.qc')
    parser.add_argument('--document',type=Path,default=Path(__file__).resolve().parents[1]/'documentation/writing_progs.md')
    parser.add_argument('--check',action='store_true',help='fail if the checked-in reference differs, without writing')
    args=parser.parse_args()
    text=args.document.read_text(encoding='utf-8')
    if text.count(BEGIN)!=1 or text.count(END)!=1: parser.error('Document must contain exactly one pair of generated-section markers')
    before,rest=text.split(BEGIN); _,after=rest.split(END)
    updated=before+reference(args.api_dir).rstrip()+after
    if args.check:
        if updated!=text: raise SystemExit('QuakeC reference is stale; regenerate it with the same exports.')
        print('QuakeC reference is current.')
    else:
        args.document.write_text(updated,encoding='utf-8')
        print(f'Updated {args.document} ({len(updated.encode())} bytes).')

if __name__=='__main__': main()
