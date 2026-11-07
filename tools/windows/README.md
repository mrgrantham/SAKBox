# Windows

What the Windows build (clang-cl) uses; see the top-level README's [Windows](../../README.md#windows) notes for what it needs.

| File | What it is |
| --- | --- |
| `BUILD` | `:clang_cl`, the platform `.bazelrc`'s `clang_cl` config runs on, which picks rules_cc's clang-cl toolchains; `:clang_builtins`, Clang's runtime helpers (128-bit division and the like), which lld-link doesn't link itself; `:arm64` and `:x86_64`, Windows on each processor |
