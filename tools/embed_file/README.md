# embed_file

Compiles a file's bytes into a C++ program, so the program carries the file inside it rather than finding it when it runs. ScratchPadd's ImGui theme uses it for its fonts.

```starlark
load("//tools/embed_file:embed_file.bzl", "embed_file")

embed_file(
    name = "questrial",                 # a cc_library: depend on it
    src = "@questrial_font//file",      # any file, here one Bazel downloads
    namespace = "Theme::Fonts",
    symbol = "kQuestrial",                # a constant: kName, as the repo names them
)
```

The library defines `Theme::Fonts::kQuestrial` (`const unsigned char[]`, the file's bytes, aligned to 4) and `Theme::Fonts::kQuestrialSize` (`const std::size_t`). Declare them where they're used:

```cpp
namespace Theme::Fonts {
extern const unsigned char kQuestrial[];
extern const std::size_t kQuestrialSize;
}
```

`embed_file.py` writes the source file when the library is built, run by Bazel's Python (`rules_python`); nothing needs installing. Other arguments, such as `data` (files that go with it into the runfiles, like a font's license) and `visibility`, are passed to the `cc_library`.
