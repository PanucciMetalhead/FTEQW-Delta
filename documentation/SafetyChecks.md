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
split VM layouts. They also exercise the production ABI 4 compatibility helper:
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
