# chips-test

Tests and sample emulators for https://git.tiepy.dev/stever/chips

Live demos of the example emulators: https://git.tiepy.dev/stever/tiny8bit

NOTE: on Linux, additional dev packages need to be present for X11, GL and ALSA development.

Create a 'workspace' directory (which will be populated with additional dependencies),
clone and cd into ```chips-test```:

```bash
> mkdir workspace
> cd workspace
> git clone https://github.com/floooh/chips-test
> cd chips-test
```

Finally, build and run one of the emulators (for instance the Amstrad CPC):

```bash
> ./fips setup emscripten
```

```bash
> ./fips build
> ./fips list targets
> ./fips run zx
> ./fips run zx-ui
```

To get optimized builds for performance testing:

```bash
> ./fips set config linux-make-release
> ./fips build
> ./fips run [target]
```

To open project in IDE:
```bash
> ./fips set config [linux|osx|win64]-vscode-debug
> ./fips gen
> ./fips open
```

To build the WebAssembly demos.

```bash
# first get ninja (on Windows a ninja.exe comes with the fips build system)
> ninja --version
1.8.2
# now install the emscripten toolchain, this needs a lot of time and memory
> ./fips setup emscripten
...
# from here on as usual...
> ./fips set config wasm-ninja-release
> ./fips build
...
> ./fips list targets
...
> ./fips run zx
...
```

When the above emscripten build steps work, you can also build and test the
entire samples webpage like this:

```bash
> ./fips webpage build
...
> ./fips webpage serve
...
```

## Many Thanks To:

- utest.h: https://github.com/sheredom/utest.h
