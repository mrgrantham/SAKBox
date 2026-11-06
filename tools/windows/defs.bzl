"""What's left out of the Windows build (see the top-level README's Windows notes)."""

# For target_compatible_with: the target, and so everything that depends on
# it, is skipped on Windows (and an error if named on its own there).
NOT_ON_WINDOWS = select({
    "@platforms//os:windows": ["@platforms//:incompatible"],
    "//conditions:default": [],
})
