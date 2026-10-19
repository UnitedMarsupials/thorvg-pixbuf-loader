# Notes for AI agents (and humans) working on this tree

## Code style: BSD style(9) -- please, really

All C and C++ code here follows FreeBSD's
[style(9)](https://man.freebsd.org/cgi/man.cgi?query=style&sektion=9).
The plugin is C++ because ThorVG's primary API is C++ (its C binding is
optional, and off by default); that is no reason to switch to Google, LLVM
or GNOME conventions.  The tests are C, in the same style.  In particular:

* Indent with TABs (width 8); continuation lines are indented by 4 spaces.
* At most 80 columns, counting a TAB as 8.
* A function's return type is on its own line, the name starts in column 0
  and the opening brace has a line of its own.  Other braces are K&R style.
* `return (value);` -- parenthesized.
* One declaration block at the top of a function, then a blank line.
* Pointer asterisks bind to the name: `char *p`.
* A space after `if`, `for`, `while`, `switch`; none after function names.
* `/* ... */` in C, `//` is fine in C++.  No trailing-underscore or
  camelCase noise in C code; keep names consistent with their neighbours.
* No braces around single statements.

FreeBSD's own
[`.clang-format`](https://cgit.freebsd.org/src/tree/.clang-format) is the
authority for mechanical formatting.  There is deliberately no copy of it in
this repository; use the one from a FreeBSD source tree:

    clang-format --style=file:/usr/src/.clang-format -i src/*.cc src/*.h tests/*.c

Run it on every file you touch and then still read the result: it is not a
substitute for knowing the rules (it cannot place blank lines after
declarations, for instance), and where it and style(9) disagree, style(9)
wins.

Match the surrounding code: comment density, naming, idiom.

## Build and test

    make
    make test

`Makefile` holds the rules, in POSIX make plus the `!=` assignment, which
both BSD make and GNU make 4.0+ understand; `BSDmakefile` and `GNUmakefile`
include it and are the place for anything particular to one make or one
system.  Keep `Makefile` itself portable.  Dependencies are found through
pkg-config: `gdk-pixbuf-2.0`, `gmodule-2.0` and `thorvg-1` (ThorVG 1.x).

`tests/gen-data.sh` regenerates the sample files; they are committed, do not
regenerate them needlessly.  The TGS sample needs a ThorVG that inflates
gzip itself; the plugin does not.

## Things to keep in mind

* The loader renders untrusted files.  Keep the size and memory limits,
  never trust sizes or counts from the file, and keep `rpath` pointing at
  `/dev/null`: ThorVG opens a Lottie file's non-embedded image assets by
  whatever path the file names, and that must fail.  Always give ThorVG the
  `"lottie+json"` type, so that it never tries its other loaders (SVG,
  PNG, JPEG, ...) on what it is given.
* After touching the loader, run the tests under valgrind (the
  AddressSanitizer on FreeBSD has no leak detection):

      G_SLICE=always-malloc G_DEBUG=gc-friendly \
      GDK_PIXBUF_MODULE_FILE=tests/loaders.cache \
      valgrind --leak-check=full --show-leak-kinds=definite,indirect \
          --errors-for-leak-kinds=definite,indirect --error-exitcode=9 \
          tests/test-loader tests/data

* ThorVG is initialized once, without worker threads, and never shut down:
  the module stays resident.
* SVG is left to librsvg's loader; do not claim it without being asked.
* Ask before making build-system, license, dependency or tooling decisions.
* Do not add a `.clang-format` or a `COPYING` file to the repository.

## Commits

* Plain subject lines: no `[PATCH]`-style tags.
* Trailers (`Co-Authored-By:`, `Signed-off-by:`, `PR:`, ...) have exactly
  one TAB after the colon, never a space.  Check with
  `git log -1 --format=%B | cat -A`: the separator must show as `^I`.
* Name the model and the reasoning effort in `Co-Authored-By:`, and do not
  put an e-mail address there.
* Do not rewrite published history without being told to.
