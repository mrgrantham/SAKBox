# Waffles

Mike Gashler's machine learning and data mining library: neural networks, decision trees, k-nearest neighbours, manifold learning, recommender systems, clustering and more, with command-line tools that use them. It is public domain.

It comes from [mikegashler/waffles](https://github.com/mikegashler/waffles). This copy was imported in February 2023 and matches upstream commit `f0cfa46` (2023-01-05). Upstream has had one commit since, `d6b734b` (2023-11-29); its two fixes were taken (see below) and the rest, which stubs out part of `GJsonAsADatabase`, was not. SAKBox maintains this copy from here.

## Building

| Target | What it is |
| --- | --- |
| `//projects/waffles:WClasses` | The library. Include `<WClasses/...>`; the namespace is still `GClasses`. |
| `:learn`, `:cluster`, `:dimred`, `:plot`, `:recommend`, `:sparse`, `:transform`, `:audio`, `:wizard` | The command-line tools |
| `//projects/waffles/demos/...` | Demos built with Bazel: `chess`, `caronhill`, `community` (a multi-user forum and recommender, served on port 8988 to the network, keeping its data in `GJsonAsADatabase` files), `eigen`, `hello_gui`, `hello_ml` (several learners predicting a pizza's taste and price) and `fourier_net` (the Fourier-initialised network from arXiv:1405.2262, trained on a NASDAQ series; `bazel run //projects/waffles/demos/fourier_net -- --epochs 5`, or no `--epochs` to run until stopped; it writes `fourier.svg` and `progress.svg` where it is run) |
| `:test` | Waffles' own test suite, including its command-line tool tests (the tools are its data deps). It fails if any test fails or, under `bazel test`, if the tools can't be run. |
| `:app_test`, `:cluster_test`, `:matrix_test`, ... | SAKBox's tests, one per area of the library (see Tests below) |

`waffles_wizard` and `waffles_plot graph` serve their pages to your browser at `http://localhost:8421`. Only this computer can connect, and they say so when they start. To let other devices on your network use them too, run `waffles_wizard -network` or `waffles_plot graph -network`; anyone on the network can then connect, as they warn. (In code, `GHttpServer`, `GDynamicPageServer`, `GTCPServer` and `GPackageServer` take a `networkAccessible` argument, false by default; the `community`, `diff`, `hello_web` and `security` demos, which are meant to be reached from other machines, pass true.)

ARFF `string` columns load as categories: each distinct text (compared exactly, ignoring surrounding quotes) is one value, so a column of names or labels is usable by any learner. Waffles stores values as numbers and has nowhere to keep text, and upstream loaded these columns as zeros, discarding the text. A column where most values differ (at least 20, in more than half the rows) is likely free text, which a learner would see as arbitrary numbers, so loading it prints a warning. Saving writes the column as a category (`{alice,bob,...}`), not `string`.

It builds with the rest of the repo as strict C++26 with `-Werror`. The other demos and the upstream Makefiles, CMake and Visual Studio files are kept but not used.

## What SAKBox has changed

- **Layout:** the library folder `src/GClasses` is `src/WClasses`, and includes use `<WClasses/...>` rather than relative paths. Platform checks use `__APPLE__` instead of a `DARWIN` define.
- **Modern C++:** `std::random_shuffle` (removed in C++17) became a Fisher-Yates shuffle with the learner's own seeded `GRand`, `sprintf` became `snprintf`, missing `override`s were added, an exception is caught by reference, and `GMasterThread`'s worker count is `std::atomic` rather than `volatile` (it is read without the lock). Classes that mustn't be copied (`Holder`, `ArrayHolder`, `GVec`) say so with `= delete`, instead of private copy functions that threw if reached (`GVecWrapper`, a friend of `GVec`, could reach them); `GVec` has a move constructor, so `a + b` returns by value without relying on the compiler skipping a copy; and `G3DVector` uses the compiler's own copy, so it is trivially copyable. A `GVecWrapper` copy wraps the same memory; assigning one is not allowed (`setData` or `copy` says which is meant). A filter's `prefilterFeatures` and `prefilterLabels` can return their input itself, so passing a temporary (`f.prefilterFeatures(a + b)`) is a compile error: `GFilter` declares both, beside deleted `&&` overloads, and does the pass-through itself; a filter implements the protected `transformedFeatures` and `transformedLabels` instead, returning its result or `nullptr` for "unchanged". So a new filter can't hide the deleted overloads or return its parameter.
- **Security** (upstream): Waffles' web server, used by `waffles_wizard` and `waffles_plot graph`, accepted connections from the whole network, and one bad header could overflow its memory: `Content-Length: -1` gave a zero-byte buffer that the request body was written into, and over-long URLs, parameters, cookies and `If-Modified-Since` dates were copied into fixed buffers unchecked. It now listens on this computer only unless asked (above), reads `Content-Length` strictly (up to 64 MB), and hangs up on, and logs, any request that doesn't fit. The HTTP client reads a reply's `Content-Length` the same way, and arrays it allocated with `new[]` are freed with `delete[]`. Tested in `http_test`, over real sockets.
- **Running other programs** (upstream): opening a URL or file (`GApp::openUrlInBrowser`, the GUI's `OpenFile`) built a shell command, `"open " + url + " &"`, so a `;` or `$(...)` in a URL or file name ran commands. The new `GApp::openFile` passes the path or URL to `open` (macOS) or `xdg-open`, then `gnome-open` (Linux), as one argument with no shell. `GApp::systemCall` no longer uses a shell either, like `systemExecute` and `systemCall` on Windows: quotes still group an argument (and are now removed, as a shell would; they used to be passed on), but pipes, redirection, `$VARIABLES`, wildcards and `;` are ordinary text. A program that can't be run exits with 127; the forked child used to throw instead and carry on as a second copy of the caller. Tested in `app_test`, with a fake `open` on `PATH` recording what it was given. The `depends/security` tools aren't built here; their `satellite` runs whatever commands its server sends, by design.
- **Reading numbers:** every `atoi` and `atof` is replaced by `parseNumber` (`GError.h`, beside `to_str`), built on `std::from_chars`: the whole text must be one number in range (finite, for decimals), and `.` is the decimal point whatever the locale. `atoi` and `atof` return 0 for `abc` and 12 for `12x`, and overflow silently. So now: JSON `1e5` is 100000, not 1; an ARFF value like `1.5abc` or `1.8m` is an error, not 1.5; a URL port must be 1-65535 (70000 used to become 4464); `pop_uint` rejects empty and too-big values; a category given by position must be in range; and in the `community` demo, a request for an item that doesn't exist (`?item=999999`) gets "No such item" instead of reading past the end of the list.
- **Bug fixes** (all upstream bugs; those with a visible effect have a test, in SAKBox's tests or Waffles' own suite):
  - `GBlockElPos::inverse` assigned instead of comparing (`y = UNKNOWN_REAL_VALUE ? ...`), so it always returned "unknown", and it inverted the wrong pieces of the function.
  - `GBitTable` and `GMatrix` assignment destroyed the data when assigned to themselves.
  - Integer division: sparse k-means' and `GRecommender`'s running means were wiped at every step, `GMath::wilcoxonPValue` truncated n(n+1)/4, and `GCluster` and `GRecommender` lost fractions.
  - `GArgReader::pop_double` turned non-numbers into 0; it now rejects them.
  - `GKMeansSparse` only ever recomputed the first row's cluster (a pointer into the cluster list stopped moving), and left an empty cluster empty; it's now restarted from the largest cluster's worst-fitting row. This also fixes `GSparseClusterRecommender`, whose test limit was loose enough to pass with the bug.
  - `GDenseClusterRecommender` clustered uninitialised memory for unrated items; they're now marked unknown.
  - `GSparseInstance` couldn't be reloaded once saved (missing from `GLearnerLoader`, and its loading constructor left a pointer uninitialised), mismatched rows and labels when pruning, and stepped an iterator before `begin()`.
  - `GTensor::parseArff` loaded only half of a file's values and divided by zero for a file with no attributes; its errors now name the column.
  - `GJsonAsADatabase`'s `=` crashed on a missing field with more path after it (`.a.b = 1`), replaced the whole database for a missing last field (`.a = 1`), named fields with the space before `=` (`"a "`), and mis-indexed one past a list's end; `GDomNode::set` crashed on a new, empty list.
  - `GAutoFilter` asked itself, not the learner, whether nominal values needed handling, so six of its rules never ran (one also checked labels where it meant features). No change for Waffles' own learners.
  - `GNeuralNet::clone` and `copyTopology` threw for every network: copying a layer copied its `GVecWrapper`s through `GVec`'s throwing copy constructor. The copy also left its gradient count uninitialised, so binding a clone failed with "Gradient size problem".
  - `GRayTraceTriMesh::makeCylinder` built its triangles facing inward. Upstream's dead mesh-orientation code is revived as `orientTriangles`.
  - `GImage::munge`'s "waves around the middle" effect (used by `GImage::captcha`) worked out each pixel's angle from the middle with unsigned numbers, which wrapped to about 4 billion above and left of it, so the waves came out wrong there.
  - HTTP `Date` and `Last-Modified` headers dropped the leading zero of days under 10 (`Thu, 5 Nov 2026`; HTTP wants `05`). They're made by the new `GTime::httpDate`. It and `GTime::asciiTime` no longer use `asctime`, `gmtime` or `localtime`, which share one buffer between all threads (two threads formatting a time at once could get each other's); the names are English whatever the locale, as HTTP requires.
  - `GStemmer` wrote the `'\0'` after a 64-letter word one byte past its buffer; words are now cut at 63 letters, as `GVocabulary` already does without a stemmer.
  - `GHttpServer::unescapeUrl` (URL parameters) took any letter after `%` as a hex digit and anything else as 2 or 0, so `%G0` decoded to `'\0'` and cut the value short, and a `%` near the end was dropped. Only `%XY` with two hex digits is decoded now; any other `%` is kept as it is.
  - `tolower` was given plain `char`s, which are negative for bytes above 127 (UTF-8 letters), and that is undefined behaviour; they are cast to `unsigned char` first.
  - Smaller fixes clang-tidy found: wrong `throw`s, `GHttp`'s port, `memcpy` of a non-trivial type, and how the `plot` and `wizard` tools seed their generators.
- **Tests:** the suite used to pass whatever failed (failures were printed, not reported) and skipped the command-line tool tests under Bazel; both are fixed. Three test functions that were never run (`GDenseClusterRecommender`, `GFunctionParser`, `GSparseInstance`) now are, with real limits instead of placeholders. `GWavelet::test` stays out: it reads a file on its author's machine.
- **Static analysis:** Waffles is checked by the repo's clang-tidy setup like the rest of the repo (see the top-level README). Fixes clang-tidy applied itself (`override`, `nullptr`, `const &` parameters) are in. Sorts of pointers say what they sort by, with `std::ranges` projections (`std::ranges::sort(m_rows, std::ranges::less{}, [dim](const GVec* pRow) { return (*pRow)[dim]; })`) instead of one-use comparator classes. `bugprone-nondeterministic-pointer-iteration-order` flagged every `std::sort` of pointers, even ones comparing the values pointed at, as these did; it doesn't look at `std::ranges` algorithms at all, so sorting pointers by address that way would go unflagged. Their results, including the order of equal values, are pinned by `matrix_test`, `raytrace_test` and `region_test`.
- **Warnings:** counters that are computed but never read (`epochs`, `hits`, `nDepth`, `nTries`, ...) are kept, as instrumentation, and marked `[[maybe_unused]]`.
- **Demos brought up to date:** upstream changed the neural network API in 2016-2019 (layers became blocks, several blocks per layer, weights in one vector) without updating every demo. `hello_ml` only needed a dead include removed. `fourier_net` was rewritten for blocks, using upstream's last version of the deleted `GNeuralNet::fourier` (2017) and the 2015 layer helpers it relied on as references; its data, deleted upstream in 2015, is restored from history.
- `GFile::loadFile` reports the missing path and the current directory when a file isn't found.

Changes inside the code are marked with `SAKBox:` comments where they fix behaviour.

## Tests

Two kinds:

- `:test` is upstream's own suite (`src/test/main.cpp`), including the command-line tool tests.
- SAKBox's tests are googletest files in `tests/`, one per area of the library and named after its header: `matrix_test.cpp` covers `GMatrix.h`, `raytrace_test.cpp` covers `GRayTrace.h`, and so on. Each is its own target (`:matrix_test`), listed in `BUILD`. Put a new test in the file for the area it tests, adding a file (and its name to the list in `BUILD`) if there isn't one yet, rather than grouping tests by why they were written. Say why a test exists in its comment instead: the bug it caught, or that it pins down behaviour before a rewrite (some values, like `image_test`'s pixel fingerprint, were recorded from the code before a rewrite rather than worked out).

## Roadmap

1. Make `GRand` a standard C++ random bit generator: add `result_type`, `min()`, `max()` and `operator()` (forwarding to `next()`), so `<random>` engine adaptors and algorithms that take a generator can use a Waffles `GRand` directly. Keep Waffles' own shuffle and distributions wherever exact results matter: the standard leaves `std::shuffle`'s and the `<random>` distributions' algorithms to each library, so the same seed gives different results on different platforms.
2. Only if something needs it: a `GRand` subclass backed by a `<random>` engine (for example `std::mt19937_64`, whose output the standard fixes, so it repeats on every platform). `GRand`'s methods are already virtual, so it only needs `next()` and `setSeed()`. Learners hold their `GRand m_rand` by value, so using it inside a learner would also mean holding the generator by pointer.
3. A strict mode for ARFF `string` columns: an option to refuse to load them, for when nothing should be loaded that Waffles can't represent as written, instead of converting them to categories.

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
