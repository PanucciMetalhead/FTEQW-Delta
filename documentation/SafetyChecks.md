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
- Signed shifts while reconstructing high packet bits or extending signed fields.
- Oversized/negative buffer allocations, including allowed-overflow buffers.
- Zero-capacity string outputs and a trailing backslash reading past its NUL.
- Long Quake 3 script escapes overflowing their numeric accumulator.
- Normal cvar masks such as `0x80000000` overflowing the signed integer parser.

Cvar integer parsing now uses unsigned arithmetic with the same engine-width
bit patterns, including full-width masks and negative values. It preserves the
existing whitespace, character-literal and trailing-text conventions.
Malformed inputs are rejected or bounded; supported gameplay rules and wire
field definitions are unchanged.

## Run the sanitizer suite

GCC or Clang, Python 3, and the normal native build dependencies are required.
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
| `parser-sanitizers` | Actual message, string, number and Q3 escape parser bodies extracted from the current source; 12,600 signed/unsigned packet round trips, 10,000 unaligned fields, short input, capacities 0–19, all escape bytes, long input, and 300,000 number conversions. |
| `package-signatures` | Actual package authenticity/decoding and installation-gate bodies compiled in release and debug configurations; mirror independence, provider outcomes, catalog trust, malformed metadata, and unsigned data archives. |
| `engine-sanitizer-smoke` | Actual headless server cvar registration, filesystem initialization, command execution and shutdown in an isolated directory. |
| `qcc-regression` | Compiler help/diagnostics, warning controls, optimization fixtures and execution in the standalone VM. |

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
server, compiler and network logs. CI has no owned game data and does not run
native graphical playtests.

## Warning inventory and remaining work

The macOS release all-target audit reduced ordinary Clang source-warning
locations from 38 to one. Eight explicit developer TODO diagnostics remain after removal of the
release-only package-signature exception,
along with a gettext catalog warning and the inherited Apple linker alignment
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
