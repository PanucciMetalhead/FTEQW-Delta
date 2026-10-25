# Warnings and sanitizer checks

FTEQW Delta keeps C17 as its language baseline. This page describes the
warning cleanup and expanded memory/undefined-behavior checks, including their
limits. See [C17.md](C17.md) for the language policy and original migration.

## What changed

The first cleanup removes unused bookkeeping, fixes misleading indentation,
uses explicit visibility-merge enum values, keeps SCTP parameter bytes explicit,
and corrects floating-point default comparisons in particle-definition export.
Plugin text helpers now pass their calculated character-set mask to the parser.
Reference-renderer locals are compiled only where their backends use them.
No movement, physics, collision or math source changes are required.

98 formatted-output calls now have bounds. Fixed arrays use their actual array
capacity; the completion log uses the allocated length of its trailing name.
Pointer-only and append builders were not automatically converted: they need
an explicit capacity and a review of any use of the formatting return value.
Within those capacities, the original text and number formats are retained.
The deprecated explicit Apple linker option for rejecting undefined plugin
symbols is removed; rejection remains the native linker's default. ELF plugin
builds retain their explicit undefined-symbol check.

The new tests exposed and now guard against:

- Rejection of a byte field ending exactly at the end of a packet.
- Incorrect padding between partial-bit fields and unchecked bit-buffer writes.
- Signed shifts while reconstructing high packet bits or extending signed fields,
  packing reliable headers and connection cookies, and swapping signed words.
- Oversized/negative buffer allocations, including allowed-overflow buffers.
- Zero-capacity string outputs and a trailing backslash reading past its NUL.
- Long Quake 3 script escapes overflowing their numeric accumulator.
- Normal cvar masks such as `0x80000000` overflowing the signed integer parser.
- Signed shifts while folding archive hash bytes into a purity checksum,
  decoding ZIP/GZIP/SHA-1 words, and packing palette/normal-map alpha.
- Signed shifts of negative PCM samples and surface texture origins, plus
  signed overflow when creating fallback gloss alpha. Sample scaling,
  lightmap grid coordinates and alpha byte values are preserved.
- Signed shifts in solid-box size fields; all 65,536 signed height encodings
  retain the same protocol bits and decoded bounds.
- Misaligned grouped payloads in debug builds and packed brush position/color
  arrays and original MDL animation poses; padding now respects the existing
  aligned vector types, including grouped poses with odd vertex counts.
- Misaligned pointer tables following word-sized VM memory in worker GC
  snapshots; padding preserves both copied memory and temporary string pointers.
- The top inventory bit using a signed shift; pickup flash timing is preserved.
- Remote pickup sounds indexing player views with seat `-1`; remote/spectator
  sounds retain their intended attribution without accessing a local view.
- MD4 hash callbacks cast to incompatible function-pointer types. Ubuntu/Clang's
  function sanitizer caught this during synthetic remaster PAK mounting;
  correctly typed adapters now call the original MD4 functions. The checksum
  algorithm and protocol values are unchanged.
- Long HMAC keys retaining their original length after hashing, overrunning
  the reduced key buffer; they now use the resulting digest length.
- Empty archive scans passing a null array to `qsort` during server startup.
  Linux sanitizer CI exposed this after macOS checks passed; scans now sort
  only when there are at least two files, preserving archive order.

MG3 rendering also exposed two client-side edge cases: particle effect/trail
and query entry points formed indexed pointers before rejecting invalid effect
indices; entity view angles narrowed floats directly to signed shorts, which
is undefined at ordinary half-turns. Index checks now precede pointer formation,
and received view angles quantize/wrap before narrowing, preserving 16-bit
packet values. Their extracted sanitizer fixtures cover fallback delegation,
absent particle tables, all angle bit patterns and fractional wrap boundaries.
The fourth sigil HUD mask also uses an unsigned shift, preserving its top
inventory bit without signed-shift overflow. Movement, physics and collision
sources remain unchanged. Dedicated BSP loading also leaves render-only
marksurface pointers null when the server omitted their storage, rather than
performing offset arithmetic on null; leaf contents and bounds are unchanged.

Cvar integer parsing now uses unsigned arithmetic with the same engine-width
bit patterns, including full-width masks and negative values. It preserves the
existing whitespace, character-literal and trailing-text conventions.
Malformed inputs are rejected or bounded; supported gameplay rules and wire
field definitions are unchanged.

## Run the sanitizer suite

GCC or Clang, Python 3.9+, and the normal native build dependencies are required.
Sanitizer builds are Debug builds on Linux/macOS; this option is not enabled for
MSVC/Windows. SDL3 is still a build dependency, but these CTest cases need no
Quake data, display, GPU or remote server.

```sh
cmake -S . -B build/safety -DCMAKE_BUILD_TYPE=Debug \
  -DFTE_SANITIZERS=ON -DFTE_ENGINE=ON -DFTE_ENGINE_SERVER_ONLY=ON \
  -DFTE_ENGINE_CLIENT_ONLY=ON -DFTE_TOOL_QCC=ON \
  -DFTE_TOOL_QCVM=ON -DFTE_TOOL_QCCGUI=OFF -DBUILD_TESTING=ON
cmake --build build/safety --target fteqw fteqw-sv fteqcc qcvm c17-endian-check --parallel
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/safety --output-on-failure
```

The five registered cases are:

| Case | Coverage |
| --- | --- |
| `c17-endian` | Fixed-width endian helper patterns. |
| `parser-sanitizers` | Actual message/number/string/Q3 parsers, archive scans, WAD metadata and remaster detection; MD5 replacement preflight/fallback; checksum/hash and image/audio conversion; item stats and pickup sounds; entity view angles and particle indices; model/grouped allocation, skeletal lifetime and worker-GC snapshots. Bodies, relevant types and packing expressions come from current engine source. |
| `package-signatures` | Actual package authenticity/decoding and installation-gate bodies compiled in release and debug configurations; mirror independence, provider outcomes, catalog trust, malformed metadata, and unsigned data archives. |
| `engine-sanitizer-smoke` | Actual headless server cvar registration, filesystem initialization, command execution and shutdown; synthetic-PAK remaster autodetection and an explicit classic-profile override. |
| `qcc-regression` | Compiler help/diagnostics, warning controls, optimization fixtures and execution in the standalone VM. |

The standalone fixtures cover 12,600 packet round trips, 10,000 unaligned fields,
10,000 long/double byte fields, truncated inputs, output capacities 0–19, all
escape bytes, 300,000 number conversions and archive ordering/cleanup. Resource
checks include malformed remaster PAKs, 16,000 checksums, 100,000 ZIP/GZIP words,
MD4 callback-table/streaming/checksum vectors, SHA-1/HMAC references,
palette/normal-map alpha, full-range signed PCM,
300 resampling cases and 5,409 lightmap bounds. They check every signed solid-box
height, reliable header flags, connection cookies, native byte swaps, item flash
masks and pickup attribution. Storage checks exercise release/debug grouped
allocation/free, 16/32-bit brush indexes, client/server MDL pose groups and
worker-GC snapshot alignment/content.

MD5 checks exercise incompatible/malformed companion headers, fallback before
mesh parsing, unchanged compatible/explicit/static model paths and allocation
cleanup. WAD tests cover overlong build basenames, valid subsequent entries,
maximum-length root names, prefixed-path limits and Half-Life/heightmap downloads.

Undefined behavior is fatal in sanitizer builds and the standalone sanitizer
fixtures, even without an environment override. The parser fixtures compile
with `-Wall -Wextra -Werror`; their host services and input structures are small
stubs, while the functions under test come directly from the engine source.
They exercise byte/raw-bit packing, not the complete Huffman codec. For network
fault injection and real GnuTLS records, also run:

```sh
python3 tools/network_transport_tests.py --out build/network-tests
```

That separate check requires GnuTLS development files and `pkg-config`.
To run only the parser fixtures, or compare compilers:

```sh
python3 tools/parser_sanitizer_tests.py --cc clang --out build/parser-clang
python3 tools/parser_sanitizer_tests.py --cc gcc --out build/parser-gcc
```

The small parser fixtures free their allocations and explicitly enable leak
checking on Linux. Engine/compiler tests keep leak checking disabled because
process-lifetime caches and compiler shutdown ownership have not been fully
audited. Invalid-access and undefined-behavior checks remain active. This
exception does not establish that the entire engine is leak-free.

GitHub CI builds sanitized clients, dedicated servers and compiler/VM tools on
Linux and macOS and runs all five cases. Failure artifacts include parser,
server, compiler and network logs. A failed server smoke check also prints its
complete server log in the CTest output. CI has no owned game data and does not run
native graphical playtests.

## Warning inventory and remaining work

The macOS release all-target audit reduced ordinary Clang source-warning
locations from 38 to one. Eight explicit developer TODO diagnostics remain
after removal of the release-only package-signature exception, along with a gettext catalog warning and the inherited Apple linker alignment
warning. No global warning category was disabled to obtain this reduction.
The remaining unused capsule-height calculation is in `world.c`; that source
was kept unchanged to preserve the collision baseline.

Debug builds with the current Apple SDK also expose a separate backlog of
legacy formatted-output deprecations. The bounded-call cleanup reduces that
backlog, but does not make all optional targets warning-free. Additional
conversion, initializer, extension and platform-specific warnings still need
focused review. `FTE_WERROR` therefore remains opt-in; do not add casts or
remove TODO diagnostics just to make a warning count smaller.

Save complete logs for comparable builds, then make a deduplicated report:

```sh
cmake --build build --clean-first --parallel > build/release-build.log 2>&1
python3 tools/warning_audit.py build/release-build.log --out build/release-warnings.json
```

The report groups GCC/Clang source locations by diagnostic, keeps developer
and tool warnings visible, and lists other warnings separately. It does not
suppress compiler output. Compare the same compiler, configuration and targets;
incremental logs omit unchanged files and cannot establish a full warning total.

## Validation record

On the owner's Mac/Metal system, release and full sanitizer builds passed,
as did the five sanitizer cases and real GnuTLS fault-injection checks.
Package verification also passed with genuine RSA/SHA-512 signatures in release
and debug configurations; see [PackageTrust.md](PackageTrust.md) for policy,
explicit trust overrides, reproduction commands, and coverage limits.
QuakeC optimization levels 0–3 produced byte-for-byte identical fixture output
against the saved pre-formatting compiler. The C17 gameplay comparison kept
movement, physics, collision and math instructions identical to the baseline.
Native graphics checks use isolated copies/links of the owner's game data.
The static-lighting fixture holds its E1M1 camera constant across menu/video
restart comparisons; physical mouse input cannot change that comparison pose.
See [SDL3-GPU.md](SDL3-GPU.md) for reproduction commands.

These checks cover the named functions and scenarios. They do not establish
whole-engine sanitizer cleanliness for every map, mod, codec, plugin or GPU
driver, and native Windows/Linux gameplay still needs testing on those systems.

Remaster-owned-data checks are documented in [QuakeRemaster.md](QuakeRemaster.md).
They remain separate from data-free CI and use isolated output directories.
Enhanced/classic model scenes, 59 skeletal files, 58 animation boundaries,
lighting changes and video restart also passed in the instrumented Mac/Metal
client. This does not extend that result to every campaign map or game path.

The MG3 follow-up passed 30 standalone sanitizer configurations and all five
CTest cases. Native Mac/Metal checks covered 63 compatible MD5 skeletons,
62 companion animations, animated rocket-ogre MDL fallback, lighting/video
restart, and base/MG3 client map transitions/save/load. Dedicated-server
start-to-first-map checks also passed with fatal ASan/UBSan. This validation
uses owned assets and is separate from data-free CI.

## Optional Jolt physics checks

With `FTE_PLUG_JOLT=ON`, CTest adds production adapter and C17 ABI tests. When
the compiler and dedicated server are enabled, it also compiles original SSQC
and a synthetic BSP to exercise opt-in, map collision, deferred touches and
entity removal, forces, sleeping, shape rebuilds, backend restart and plugin/map
reload. Absent/disabled/enabled modes compare ordinary Quake toss motion. ODE
comparisons are included when the optional library/plugin is available.

Build `jolt-check-deps` before running `ctest -R 'jolt-'`. The same tests run
with `FTE_SANITIZERS=ON` and instrument the Jolt dependency as well as the
adapter/plugin/server. Each engine run preserves separate logs and JSON results
under the build's `physics-tests` directory. See [JoltPhysics.md](JoltPhysics.md)
for dependency setup, validation results and coverage limits.

The joint milestone adds world/body anchors, hinge/slider limits and velocity
motors, connected-body collision exclusion, C17 joint calls and body-before-joint
teardown tests. The real engine fixture now includes an original three-bone
MD5 mesh and `.doll`: simulated bones, absolute-to-relative pose copying, joint
toggling, partial construction rollback, unsupported-joint rejection, and live
rig cleanup/recreation on backend restart and map/plugin reload. ODE supplies
the common fixed-joint baseline as well as free-fall/collision comparisons.

These checks exposed absolute MD5 poses being skipped by the QuakeC skeleton
helper, dedicated-server MD5 meshes losing their vertex count, incorrect
reference-pose selection/pivot indexing, and missing ragdoll cleanup on physics
shutdown. Repairs stay in skeletal/model and backend lifetime paths; classic
player movement and collision algorithms are unchanged.

Native Metal validation also exposed a network skeleton allocation through a
missing CSQC VM. Native poses now own heap storage when no VM is present, release
it on client shutdown/map changes, and transition safely to VM storage on reuse.
The data-free `skeleton` sanitizer fixture exercises the production allocation,
lookup, deferred deletion and reset functions: table exhaustion, world isolation,
native/VM ownership, cache reuse and both paths from native to VM storage. The
interactive demo also checks a rendered server rig and map reconnection under
ASan/UBSan, separately from headless CI.

The Jolt desktop CI follow-up covers GCC's strict adapter diagnostics, a shared
MSVC runtime across Jolt and its consumers, and Clang sanitizer module linkage.
Physics fixtures use accumulated simulation steps and completion-driven map/plugin
cycling, with a bounded ODE settling deadline, so initialization and host timewarps
cannot produce premature collision checks. Release builds are checked locally
as well as Debug and ASan/UBSan; platform CI remains the native Windows/Linux check.

Jolt and the adapter enable matching C++ RTTI for UBSan virtual-call checks,
retaining the vptr sanitizer on Linux. Windows integration logs explicitly use
plain stdio because the legacy colored console path bypasses redirected stdout.
The server exit code is included with failed integration diagnostics.

Plugin discovery on Linux exposed a zero-length `memcpy` with a null source in
`Z_StrCat` (and the same pattern in `Z_StrCatLen`). Both skip empty copies. The
`concat` fixture extracts the actual helpers and checks initial/empty, bounded
and self/substring appends, plus allocation cleanup. Its checked-copy host also
enforces glibc's nonnull contract on platforms whose libc headers lack it.

Model loading now uses standard `offsetof` instead of forming member addresses
through a null base, including the QTest MDL header-size calculation. The `modelreset`
fixture executes the production worker-reset statements against a synthetic model
layout, checking preserved header/name/cache/load state and cleared stale bounds
and payload. This covers the Linux Clang null-member diagnostic without changing
model layout or loader behavior.

QuakeC server/client field registration and the shared link-container macro now
use standard `offsetof` too. The `qcfields` and `qcfields_split` fixtures compile
the production entity declarations and registration macros, checking every
registered offset/type/name against real allocated structures in combined and
split VM layouts. They also exercise the production ABI 7 compatibility helper:
null interfaces, old/future versions and mismatched entity/joint/backend sizes are
rejected, and the old ABI 1 interface has a different request size. Runtime
physics assertions are unchanged; CI server watchdogs
allow 60 seconds per mode (300 for the whole case) so a slow shared runner can
finish its simulation steps. Timeouts retain the mode name and partial log.

Sanitizer-enabled Jolt hosts link through the C++ driver to include Clang's
C++ UBSan runtime, which provides the vptr cache/handlers needed when loading
the instrumented plugin into an otherwise C engine. Engine translation units
remain C17; the real `jolt-engine` test verifies dynamic loading, not just the
adapter linked directly into a test executable.


The humanoid milestone adds a data-free `jolt-humanoid` CTest case (logs under
`physics-humanoid/`). It builds an original 24-bone mesh/animation and 16-body
rig, verifies translation/orientation round trips for every bone, and checks
floor/stairs/slope/wall/overlap scenes, animation transitions, repeated cleanup
and live backend restart. Adapter checks cover universal/hinge2 independent
motors, hard/soft angular stops, perpendicular axes, world anchors and suspension
units. The older engine fixture also tests rotated bodies, seven-argument body
offsets, orientation targets, pivot offsets and malformed rig rejection.

The `skelnetwork` fixture extracts the actual `Bones_To_PosQuat4`, pose matrix
builder, quaternion interpolator and client network skeleton lifetime functions.
It checks 24-bone encode/decode errors against the existing quantization,
quaternion-sign-safe midpoint interpolation, bone-count replacement, stale
indices, allocation exhaustion, and native/VM cleanup under ASan/UBSan. This
brings the standalone source sanitizer suite to 37 fixtures. The optional
`physics_humanoid.py --network-test` connects two native clients to a private
loopback dedicated server and checks receipt/rendering of animated and simulated
poses without loading a client physics plugin. It requires owned UI assets and
GPU access, so it is a local validation rather than an asset-free CI test.


The instrumented two-client run also exercised console input while connecting.
It exposed a before-array pointer in the full-input comparison. The comparison
now uses an in-array end pointer. The `consoleinput` source fixture checks every
input length from empty to the 2046-character payload limit, including the full
marker and both terminators, under ASan/UBSan.


Hinge2 suspension is exercised through the C adapter, compiled QuakeC entities
and `.doll` rigs. Checks cover load/deflection units, damping energy loss,
arbitrary-axis forces, live stiffness/damping edits without anchor drift,
invalid-edit retention, disabled constraints and restoring a rigid pivot. A
translated/rotated world-anchored rig verifies reference-space conversion and
backend restart; malformed spring definitions must roll back the entire rig.


The kinematic milestone adds `jolt-kinematic`: an original inline BSP brush and
compiled SSQC exercise lifts, rotating geometry, blocked doors, sleeping props,
skeletal bodies, support removal and map/plugin reload at 60/120 Hz. A classic
lift rider is compared with Jolt absent and enabled. Adapter checks cover
fast translation and rotation that cross a prop between non-overlapping endpoint
poses, bounded adaptive collision work, mesh kinematics, partial fixed ticks,
60/144 Hz target arrival, explicit teleport cancellation and stale handles.
The new engine callback runs Jolt after accepted entity movement; classic pusher
code skips only `MOVETYPE_PHYSICS` when that callback is present. Player movement
and prediction routines are unchanged. This intentionally adds Jolt-only branches
to `sv_phys.c`; the earlier source-identical movement assembly comparison is not
a claim about this new implementation. CI still compares GNU11/C17 output of the
same reviewed source revision.


The explicit collision-shape milestone adds `jolt-shapes`, with original MD5
models and authored `.jolt` recipes. The real SSQC fixture checks stacks, compound
openings versus single-hull filling, scaling, live recipe-path changes, rejected
geometry recovery, bounded file reads and map/plugin reload. Adapter checks cover
analytic cuboid/compound inertia, offset center-of-mass transforms, force units,
CCD through openings and against solid pieces, momentum transfer and lift contact.
Malformed/degenerate/nonfinite/oversized shapes leave no registered bodies. A
separate local run uses the client executable in dedicated mode to exercise both
model-extraction variants. This exposed and fixed a dedicated skeletal path that
read renderer tangent arrays while building collision vertices. The standalone
source-based sanitizer fixture count remains 37.


The mixed-animation milestone adds `jolt-mixed`. An original three-bone rig runs
through real SSQC with a kinematic root and passive connected bodies. It checks
physical prop contacts, explicit and default-mask release/reacquisition, bounded
joint distances, relative render-pose copying, invalid strengths, idempotent
clearing, live backend restart, entity reuse and map/plugin reload. Adapter tests
verify pose/handle/joint retention, authored mass and inertia after an initially
kinematic body becomes dynamic, inherited linear/angular velocity, stopped
releases, queued-torque cancellation and invalid/stale/cross-world transitions.
These run in Debug, Release and ASan/UBSan configurations. Shared skeleton code
uses optional backend callbacks; ODE/Bullet retain their existing animation path.
The rigid-body ABI is now 5, requiring physics plugins to be rebuilt with the host.


The independent-client milestone adds `jolt-csqc`, using real headless client
executables and original QuakeC/BSP/MD5 assets. A full client holds server and
client Jolt worlds concurrently; a client-only executable reconnects to a private
loopback server. Both reject cross-world skeleton access and exercise fixed-tick
`frametime`, contact callbacks that remove entities, pending-force cleanup,
collision dimensions, brush lifts, mixed rigs, isolated backend restart and
shutdown with live native objects. Map and plugin reloads repeat these checks.
One module selects a strictly checked split or combined native entity layout;
unknown layouts remain rejected. Desktop failure artifacts now retain these
client logs and the earlier mixed-rig logs.

A direct adapter regression reproduces small convex bodies falling through an
offset thin mesh. Centering mesh triangles and preserving the caller's origin
with a translated shape fixes that case. The regression varies floor extents,
origins, body sizes and contact locations, checks stationary origins and verifies
an offset kinematic mesh lift still carries a rider. Native Metal captures add
rendered mixed/released/restored poses to the numeric headless assertions.

The client-only fixture uses a separate install with no local client gamecode,
forcing checksum-named CSQC download/completion and cache reuse on reconnect.
ASan exposed a download-completion read of `dl->fsroot` after `DL_Abort` had freed
the record; the handler now captures that flag before releasing the record.
The test runner waits for sanitizer reports to finish before terminating a
failed process, preserving useful allocation/free stacks in the retained logs.


Linux client startup coverage exposed a zero-length `memcpy` with a null source
in `Cmd_AddTimer`. Empty timer payloads now skip the copy. The `timers` source
fixture enforces the same nonnull contract on macOS and checks empty/absent
payloads, payload ownership, terminators, deadlines and repeated cleanup. This
brings the standalone sanitizer suite to 38 fixtures at that milestone. Windows builds copy the
shared SDL runtime beside SDL-linked executables; the CSQC runner reports early
process exit codes in decimal and hexadecimal to distinguish loader failures
from timeouts.


Further native CI coverage found empty ruleset file-hash lists reaching `qsort`;
rule and file lists now sort only when they contain multiple entries. The
`rulesets` fixture executes the actual sorting block and comparators with empty,
single-entry and multiple-entry lists, enforcing libc's nonnull contract on all
hosts. The `consoleoutput` fixture executes the Windows server formatter against
a redirected descriptor and verifies that short messages arrive before process
shutdown. Windows servers now flush stdout after each printed message. The CSQC
fixture uses its own post-map readiness marker because the engine's usual map
startup message is conditional. The source sanitizer suite now has 40 fixtures.


macOS C sanitizer builds now redeclare the `memcpy`, `memmove`, `memset`, `memcmp`
and `qsort` nonnull contracts that glibc already exposes. This adds checks to the
actual engine and tools without replacing libc or affecting ordinary builds.
The real `jolt-csqc` test then reproduces Linux's empty initial CSQC entity-table
clear locally. That clear now skips an absent table; allocated tables retain the
same reset behavior. The stronger instrumentation supplements the source fixtures
and keeps future zero-length null-pointer regressions visible on macOS too.


Linux loopback coverage also exercises the UDP epoll callback. Its embedded
context recovery, and the matching TCP path, now use `STRUCT_FROM_LINK` with
standard `offsetof` instead of taking a member address through a null pointer.
The `epoll` source fixture runs both production callbacks on all hosts under
ASan/UBSan, checking context recovery, packet draining/routing, stream flushing
and read-error dispatch. This brings the standalone suite to 41 fixtures. The
CSQC runner monitors the dedicated server while waiting for its client, reports
server sanitizer errors or early exits immediately, and includes both logs on a
connection timeout.


Fractional ragdoll animation uses internal six-axis position/orientation motors
on dynamic bodies, with finite mass/strength-scaled force limits. Adapter checks
cover gravity compliance, strength and mass response, blocked-target recovery,
quaternion sign equivalence, nonzero shape COM, small strengths, teleport rebasing,
fixed-step convergence, retained velocity, sleeping releases, authored joints and
motor cleanup. The C17 boundary calls the new control without exposing C++ types.
Real SSQC checks add fractional `.doll` defaults, runtime scaling, wall contacts,
release/reacquisition and restart. Full and client-only CSQC checks retain the
numeric render-pose comparisons, test fractional drive/release and leave a live
motor for native teardown across map/plugin/reconnect cycles. Optional native GPU
runs now retain 30 images, including fractional and released poses.

Local Apple Silicon validation on 2026-10-21 passed all 10 Debug tests, all 10
Release tests and all 13 ASan/UBSan tests. The adapter and C17 boundary also passed
against CI-pinned Jolt `1b2e77e3c6c09709e5edc79dc208459b8ff32051`. Native Metal
validation passed both client hosts, retaining 30 captures; representative driven
and released poses were visually inspected. The C17 guard checked 505 C
translation units and identical gameplay assembly for movement, server physics,
world collision and math. This milestone has not yet run Windows/Linux CI.

Legacy point-anchor coverage compares caller-ordered geometry with Homebrew ODE
for two rotated bodies and both world/body orderings. Adapter tests cover shared
defaults, independent world anchors, shape COM offsets, finite/bounded coordinates,
invalid modes, convergence, stale/cross-world handles and dependent-joint cleanup.
A C17 caller submits and queries explicit anchors. `jolt-pointanchors` compiles
original SSQC, runs both modes across three map loads each, and checks grouped
labels, individual deletion, edict reuse, rejected-edit preservation and recovery.
Only the deliberately injected invalid edit is permitted in its logs; unchanged
requests must report it once per map. Full and client-only CSQC worlds exercise
the same geometry and grouped cleanup across reload/reconnect cycles.

Local Apple Silicon validation on 2026-10-21 passed all 11 Debug tests, all 11
Release tests and all 14 ASan/UBSan tests. Adapter/C17 boundary checks also passed
against CI-pinned Jolt `1b2e77e3c6c09709e5edc79dc208459b8ff32051`. The C17 guard
checked 505 C translation units and identical movement, server-physics, collision
and math assembly. The generated QuakeC reference was refreshed and checked
against fresh engine exports. Windows/Linux CI remains to be run after publication.
ODE/Bullet remain while save/load, real-mod acceptance and many-body
performance/bandwidth validation are completed.


## Jolt entity-scene persistence

This section records the entity-only milestone. The skeletal follow-up below
supersedes its live-rig save refusal.

Native FTE saves and legacy extended `.sav` files now contain a versioned,
bounded numeric physics block. The decoder stages records, validates entity
bindings and finite values, and reconstructs colliders from mod-owned assets.
It does not invoke Jolt's opaque replay deserializer or restore native pointers.
The input cap is 16 MiB, with at most 8,192 bodies and 16,384 joints. Missing or
incompatible backends, clock/mode/version mismatches and malformed records fail
loading. Active skeletal rigs and queued commands/forces refuse ordinary saves
before writing; pending collision-shape rebuilds must first reach a physics tick.

`jolt-save` exercises native and legacy paths in the dedicated server and a
headless full client, including a fresh engine process. It verifies moving and
spinning props, sleeping bodies, model-derived hulls, original slider limits,
compressed suspension and legacy world point anchors after wake-up. Fractional
joint coordinates catch precision loss in the VM's ordinary entity text. Each
format includes four restores, two refused overwrites, 18 malformed snapshots
and a missing-backend case. The malformed cases cover version/mode/clock,
counts, truncation/trailing data, bad entity IDs, quaternions, numeric bounds,
flags, nonfinite/oversized tokens, oversized blocks, nested/quoted data, tiny
mass and zero primitive dimensions. The adapter additionally checks pending
kinematic targets, fractional ticks, disabled joints and remapped native handles.

Cold-process loading exposed a pre-existing null client-pointer increment in
the native save reader. The loop now indexes its saved-player records directly
and bounds names by the destination array. The same cold loads run under UBSan.
Skeletal resources, CSQC worlds and mod-owned `SV_PerformSave` formats are outside
this entity persistence scope. Contact caches are rebuilt; bit-exact replay is
not claimed. See [saving entity scenes](JoltPhysics.md#saving-entity-scenes).

Local Apple Silicon validation on 2026-10-21 passed 12/12 Debug, 12/12 Release
and 15/15 ASan/UBSan CTest checks, including 42 standalone sanitizer
fixtures. Adapter/C17 boundary checks passed against CI-pinned Jolt
`1b2e77e3c6c09709e5edc79dc208459b8ff32051`; the C17 guard checked 505 C units and
unchanged assembly for classic movement, server physics, world collision and
math. Windows/Linux CI remains to be run after publication. ODE/Bullet remain
until skeletal save/load, real-mod acceptance and many-body performance/bandwidth
gates are complete.

The new delayed-worker fixture also executes production worker teardown with
created handles but a zero live-worker count. Shutdown must wait for each stop
callback/join before destroying the conditions, including workers that have not
entered their thread function yet. This fixes a startup/quit race exposed by the
headless client's malformed-save rejection checks.


## Filesystem creation callback compatibility

Linux Clang UBSan caught an existing return-type mismatch when `jolt-save` first
created its save directory: `FSSTDIO_CreateLoc` returned `unsigned int`, while
the filesystem's `CreateFile` callback requires the `qboolean` enum. The callback
now uses that exact signature, preserving the existing zero/one results and
file-creation behavior. No sanitizer checks are disabled.

The `filecreate` source fixture extracts both the production function and the
`CreateFile` declaration from `fs.h` and uses the callback without casts. Some
compilers accept the enum/unsigned assignment without a warning; calling through
a volatile pointer lets Clang's indirect-call sanitizer detect the mismatch.
The fixture also checks nested locations, restored path
separators, zero offsets/lengths, readonly refusal without writes to the output,
overlong-path rejection and repeated calls. The standalone sanitizer suite now
contains 43 fixtures. Native and legacy Jolt save/load checks exercise the real
engine filesystem path too; the Clang Linux CI rerun checks its indirect-call
sanitizer in the full engine.

Local follow-up validation passed Debug and Release `jolt-save`, all 43 source
sanitizer fixtures, the instrumented engine startup test and instrumented
native/legacy save loading. A separate Clang 23 `-fsanitize=function` run on macOS
reproduced the exact CI diagnostic with the original callback and passed with
the corrected signature. The full Linux check remains subject to the new CI run.


## Jolt skeletal persistence

Physics save format 2 adds world-owned skeleton poses and IDs, shared animation
sources, authored file/inline rig definitions, per-body animation strengths and
next matrices, current physical runtime, original constraint frames and enabled
flags. Fractional motors retain their accepted target pose and target velocities.
The engine recreates VM-owned storage rather than restoring native pointers;
exact numeric handles keep references in QuakeC globals valid. Required IDs
owned by another world's active or cached resources reject loading.

The overall 16 MiB input/15 MiB output caps still apply. Additional bounds are
1,024 skeletons, 32,768 bones, 1 MiB per definition and 2 MiB of definitions total.
Counts are checked before definition parsing/allocation. Models, bone counts,
entity/source relationships, poses, animation masks and servo values are checked;
failed partial restoration cleans up its new world's resources. The engine's
rigid-body plugin interface is ABI 7; older modules must be rebuilt. Existing
version-1 entity checkpoints remain readable in both native and legacy formats.

`jolt-rig-save` exercises seven original skeletons: passive, exact/fractional,
detached and suspended rigs, a shared animation source and a render-copy object.
Both native FTE and legacy `.sav` paths include repeated and fresh-process loads.
Bodies must resume falling, soft motors remain blocked then recover after wall
removal, disabled joints stay disabled, suspension retains its original rest
frame, names remain editable and release/backend restart remain safe. Each
format rejects 41 malformed physics blocks and a missing backend. The additional
cases cover skeleton counts/duplicates, models/bone counts, owner/source IDs,
nonfinite poses, definition size/hex/NUL/counts/mass/shape/reference poses, masks,
activation flags, quaternions, servo velocities, joint types and enabled flags.

The new `skelcheckpoint` source fixture executes production model validation and
VM allocation with exact IDs. It checks copied poses, numeric/type/count bounds,
live and cached foreign-world protection, own-world reset and repeated cleanup.
The standalone suite now contains 45 fixtures. Adapter tests compare fresh-world
continuation for passive, exact and fractional modes, pending targets and partial
ticks, saved momentum, sleeping bodies and rejected runtime edits. The ordinary
entity fixture also loads a version-1 snapshot in each format and confirms
invalid ownership still refuses overwriting an existing checkpoint.

Local Apple Silicon validation on 2026-10-22 passed 13/13 Debug, 13/13 Release
and 16/16 ASan/UBSan CTest checks. Adapter and C17 boundary tests also passed with
CI-pinned Jolt `1b2e77e3c6c09709e5edc79dc208459b8ff32051`. The C17 guard checked
505 C units and identical classic movement, server physics, world collision and
math assembly. This milestone leaves classic movement and Q3 gamecode physics
unchanged. New Windows/Linux CI checks remain to run after publication.

Independent CSQC simulation and custom `SV_PerformSave` formats remain outside
SSQC save ownership. Transient `skel_mmap` pointers need to be reacquired by mod
code after loading. Solver/contact warm-start data is rebuilt, so checkpoints
continue simulation rather than providing bit-exact replay. ODE/Bullet remain
until real-mod acceptance and many-body performance/bandwidth gates are complete.


The final ownership review also added explicit main-thread cache registration
for restored and inline definitions. Cleanup unlinks cached definitions safely;
worker-loaded model defaults keep their separate model ownership. The
`skeldefinitions` fixture executes production text copying, registration,
freeing and flushing with counted native/string allocations, checking active
references, model-owned definitions, duplicate registration, invalid input and
repeated cleanup. A connected-body regression proves that a kinematic handoff
can wake an earlier restored neighbor; activation flags are now applied after
all motion types and motors, preserving saved sleep state across the whole rig.

Checkpoint capture applies the same velocity bounds as restoration. An adapter
regression submits a valid but extreme one-tick kinematic target, confirms its
release velocity exceeds save limits and checks that saving refuses that state
before serialization. After an idle tick clears the release velocity, capture
and restore succeed again. This does not clamp or change simulation behavior.


## Rig definition initialization on desktop CI

The live-rig checkpoint validator exposed uninitialized `refanimtime` in newly
allocated `.doll` definitions. `BZ_Malloc` deliberately does not clear memory;
definitions using the default skin reference pose did not assign that field.
Depending on heap contents, valid live scenes could refuse saving or restoring.
Linux CI rejected its first save; Windows completed native loading but rejected
the legacy restore and waited until the test watchdog expired.

Rig creation now clears the definition and parser context before setting
authored defaults. This also initializes the current body/joint selectors.
The `skeldefaults` fixture extracts the production constructor, types and
checkpoint validation functions, supplies allocations filled with `0x7f`, and
checks deterministic reference time, empty selectors, default properties and
repeated cleanup. It reproduces checkpoint rejection before the fix and passes
after it. The source sanitizer suite now contains 46 fixtures.

The cancelled macOS job in that run had no assigned runner or executed steps;
it did not report an engine failure. Cross-platform confirmation of this fix
comes from the follow-up desktop CI run.
