# Waffles

Mike Gashler's machine learning and data mining library: neural networks, decision trees, k-nearest neighbours, manifold learning, recommender systems, clustering and more, with command-line tools that use them. It is public domain.

It comes from [mikegashler/waffles](https://github.com/mikegashler/waffles). This copy was imported in February 2023 and matches upstream commit `f0cfa46` (2023-01-05). Upstream has had one commit since, `d6b734b` (2023-11-29); its two fixes were taken (see below) and the rest, which stubs out part of `GJsonAsADatabase`, was not. SAKBox maintains this copy from here.

## Building

| Target | What it is |
| --- | --- |
| `//projects/waffles:WClasses` | The library. Include `<WClasses/...>`; the namespace is still `GClasses`. |
| `:learn`, `:cluster`, `:dimred`, `:plot`, `:recommend`, `:sparse`, `:transform`, `:audio`, `:wizard` | The command-line tools |
| `//projects/waffles/demos/...` | Demos built with Bazel: `chess`, `caronhill`, `eigen`, `hello_gui`, `hello_ml` (several learners predicting a pizza's taste and price) and `fourier_net` (the Fourier-initialised network from arXiv:1405.2262, trained on a NASDAQ series; `bazel run //projects/waffles/demos/fourier_net -- --epochs 5`, or no `--epochs` to run until stopped; it writes `fourier.svg` and `progress.svg` where it is run) |
| `:test` | Waffles' own test suite |
| `:activation_test` | Neural-network activations: each `inverse()` undoes `eval()` |

It builds with the rest of the repo as strict C++26 with `-Werror`. The other demos and the upstream Makefiles, CMake and Visual Studio files are kept but not used.

## What SAKBox has changed

- **Layout:** the library folder `src/GClasses` is `src/WClasses`, and includes use `<WClasses/...>` rather than relative paths. Platform checks use `__APPLE__` instead of a `DARWIN` define.
- **Modern C++:** `std::random_shuffle` became `std::shuffle`, `sprintf` became `snprintf`, missing `override`s were added, an exception is caught by reference, and `GMasterThread`'s worker count is `std::atomic` rather than `volatile` (it is read without the lock).
- **Bug fix:** `GBlockElPos::inverse` assigned instead of comparing (`y = UNKNOWN_REAL_VALUE ? ...`), so it always returned "unknown", and it inverted the wrong pieces of the function.
- **Warnings:** counters that are computed but never read (`epochs`, `hits`, `nDepth`, `nTries`, ...) are kept, as instrumentation, and marked `[[maybe_unused]]`.
- **Demos brought up to date:** upstream changed the neural network API in 2016-2019 (layers became blocks, several blocks per layer, weights in one vector) without updating every demo. `hello_ml` only needed a dead include removed. `fourier_net` was rewritten for blocks, using upstream's last version of the deleted `GNeuralNet::fourier` (2017) and the 2015 layer helpers it relied on as references; its data, deleted upstream in 2015, is restored from history.
- `GFile::loadFile` reports the missing path and the current directory when a file isn't found.

Changes inside the code are marked with `SAKBox:` comments where they fix behaviour.

## Upstream's README

Waffles is a comprehensive machine learning and data mining library. It includes many tools and algorithms from neural networks to recommender systems.

# Build Instructions
You may be interested in a more stable but older build of Waffles, check out the release for Waffles v1.0.

## Linux and OS X

### To install

1. Install g++ and make (if they are not already installed)
    1. On Debian, Ubuntu, and derivatives:

            sudo apt-get install g++ make

    2. On Red Hat, Fedora, and derivatives:

            sudo yum install g++ make

    3. On OS X:

            xcode-select --install

2. cd src
3. sudo make install

### To uninstall

	sudo make uninstall

### To build optimized binaries without installing them

	make opt

### To build unoptimized binaries with debug symbols

	make dbg

To build demo apps and additional tools, see web/docs/linux.html.

## Windows

1.	Install Microsoft Visual C++ 2013 Express Edition.
2.	File->Open->Project/Solution
3.	Open waffles\src\waffles.sln
4.	Change to Release mode.
5.	Build (F7).
6.	Set the startup app to the app you want to try.
7.	Set any relevant arguments in Project->Properties->Debugging->Command Arguments
8.	Run it (F5).

If you also want to try the demo apps, see web/docs/windows.html.

***

For more detailed instructions, troubleshooting help, an overview of this toolkit, and instructions for using it, see web/index.html.
