# FTEQCC - QuakeC compiler

## Synopsis

    fteqcc [options] [source.src | source.qc ...]
    fteqcc --help[=TOPIC]
    fteqcc --manual

Compile QuakeC into progs.dat, csprogs.dat, or another specified output.
Without a source argument, look for preprogs.src, then progs.src, in the
source directory. Existing bytecode changes only when its source is rebuilt.

## Quick start

    fteqcc -srcfile progs.src
    fteqcc -srcfile csprogs.src -o csprogs.dat -Tfte
    fteqcc -srcfile progs.src -Wall -Werror
    fteqcc -srcfile progs.src -O2 -Fwasm
    fteqcc --help=optimizations
    fteqcc --manual | less

The target must match the engines that will run the mod. Do not select the
FTE target for a mod that must also run on a vanilla Quake interpreter.

## Input and output

    -src DIRECTORY       Directory containing sources and progs.src.
    -srcfile PATH        Explicit starting source file.
    -o PATH              Override the output filename.
    -I PATH              Add an include search directory; -IPATH also works.
    -DNAME[=VALUE]        Define a preprocessor macro.
    -TNAME               Select the bytecode target; see --help=targets.
    -std=NAME            Select a source compatibility dialect.
    -v                   Increase verbosity; may be repeated.
    --opt-report         Show bytecode totals and optimization counters.
    --version            Print the compiler version.
    -log / -nolog        Enable or disable the CLI log file.
    --color=auto          Color when output is a terminal (default).
    --color=always        Force color; --color=never disables it.

Common dialects include qcc, hcc, C, qccx, reacc, and gmqcc. Dialects change
keywords and language semantics, not just warning preferences. They are
compatibility aids, not guarantees that every source extension is supported.

## Optimization

The default enables a selected set of optimizations, not every -O1 pass.
    -O0                  Disable all optional optimization passes.
    -O1                  Enable passes at level 1, including symbol stripping.
    -O2                  Also enable level 2 passes, including local overlap.
    -O3                  Also enable level 3 passes; unsuitable for addons.
    -ONAME               Enable a named pass (or its short alias).
    -Ono-NAME            Disable a named pass.
    -Fwasm               Write qc.asm to inspect generated instructions.
    -flno                Write line-number information.

Higher levels can remove debugger information and symbols used by addons or
mutators. Local overlap depends on initialization analysis. Do not assume
that smaller bytecode is faster, or that all levels preserve compatibility
with reflection, savegames, and multiple-program hooks. Compare execution
results and VM timings before changing a project's optimization settings.
Use --opt-report for readable bytecode totals, enabled passes, and selected
optimization counters. Use -v for the existing detailed per-pass counters.
Named command-line passes override corresponding optimization pragmas.
The optimization catalog describes each pass, its level, and its default.

## Warnings and diagnostics

    -Wall                Enable most warnings, subject to dialect exceptions.
    -Wextra              Enable additional, potentially noisy warnings.
    -Werror              Treat enabled warnings as errors; suppress output.
    -WNAME               Enable a named warning or diagnostic code.
    -Wno-NAME            Disable one warning.
    -Werror-NAME         Make one warning an error.
    -Wnone               Disable warnings.
    -Wno-mundane         Disable a group of stylistic/unused-code warnings.
    --diagnostics=rich   Source excerpts and carets (CLI default).
    --diagnostics=plain  Original one-line diagnostics for scripts and IDEs.
    -Fmsvcstyle          Original MSVC filename(line) diagnostic format.

Aliases include uninitialized, missing-return, unused, unreachable, shadow,
and format. Numeric codes such as F302 remain accepted. Source warnings can
also be controlled with #pragma warning enable|disable|error CODE.
The warning catalog lists accepted names and codes. Warning defaults vary
with dialect and pragmas. Uninitialized-local and precache analysis have
limitations; a clean compile is not proof that the program is correct.

Rich output shows cached source text where available. Carets identify the
current token only when its physical source location is known; deferred
warnings can show a line without a caret. Lines with non-ASCII text before
the token omit the caret because terminal character widths vary. Includes
and macro expansions keep their existing context notes. Virtual #line filenames may lack an excerpt.
MSVC style suppresses excerpts. Neither format changes diagnostic severity.

## Source projects

An old-style progs.src starts with the output filename, followed by source
filenames. A new-style source project uses directives, for example:

    #output "progs.dat"
    #include "defs.qc"
    #include "main.qc"

    #pragma target FTE
    #pragma sourcefile "csprogs.src"
    #pragma optimise no-locals_overlapping
    #pragma warning enable F302

Use #pragma sourcefile to compile additional modules from one project.
Use #pragma optimise addon when building addons that depend on symbols.
Language flags (-F) and keywords (-K) are listed by --help=flags.
Disabled keywords can still be written using a double-underscore prefix.

## Package and decompiler commands

    -l PACKAGE                 List files in a pak or pk3.
    -x PACKAGE [FILENAMES...]   Extract files into the working directory.
    -p PACKAGE FILENAME        Write an archived file to standard output.
    -0 PACKAGE DIRECTORY       Create a hybrid uncompressed pak/zip.
    -9 PACKAGE DIRECTORY       Create a conventional compressed pk3.
    -z PACKAGE DIRECTORY       Create a spanned pk3 and external data parts.
    -d BYTECODE                Decompile into the working directory.

Extraction and decompilation write files. Use an empty working directory to
avoid overwriting sources. Decompilation cannot recover original source
faithfully, especially after symbol stripping and optimization. Package
commands are CLI features and consume their remaining arguments as paths.

## Exit status

The CLI returns 0 for a successful compile or help request, and nonzero for
compilation failure, an invalid help topic, or invalid diagnostics format.
Warnings alone normally do not fail compilation unless promoted to errors.
Unknown legacy options can still be warnings; use -Werror in automated builds.

## Help topics

    --help                    Common commands and options.
    --help=optimizations      All optimization passes and compatibility notes.
    --help=warnings           Warning codes and readable aliases.
    --help=flags              Language flags and keywords.
    --help=targets            Output formats and compatibility.
    --manual                  This manual followed by all option catalogs.
    man fteqcc                Installed Unix manual page.

The installed man page covers usage and refers to the live CLI catalogs.
Full upstream language reference:
https://fte.triptohell.info/moodles/fteqcc/README.html

## Development validation

    cmake -S . -B build/all -DFTE_TOOL_QCVM=ON
    cmake --build build/all --target fteqcc qcvm
    python3 tools/qcc_regression.py
    python3 tools/build_qcc_manual.py --check

The regression tool checks help, warning controls, plain/MSVC diagnostics,
source excerpts, includes, macros, long lines, CRLF, and missing arguments.
It executes a fixture at -O0 through -O3 in the actual QuakeC VM, checking
arithmetic, float precision, vectors, side effects, recursion, branches,
loops, and entity-field writes. --baseline PATH also checks byte-for-byte
output against a previous compiler with fixed fixture build metadata.
Generated files and logs go into the ignored build/qcc-tests directory.

The macOS comparison passed at all four levels with identical bytecode.
The fixture uses 126, 115, 112, and 112 statements at -O0 through -O3;
these reductions come from existing passes. This update adds diagnostics,
documentation, and reporting; it does not add optimization passes or change
their defaults. Native Windows/Linux runtime validation remains pending.
