"""Builds a Cyclone program or test with a given `real` type.

Cyclone's real type is picked for the whole build by the setting
//projects/cyclone:real (see BUILD). These rules build one target with it set,
so a target such as flightsim_flex is always the flight sim on FlexReal,
whatever the command line says:

    real_binary(name = "flightsim_flex", target = ":flightsim", real = "flex_real")
    real_test(name = "ragdoll_test_flex_point16", target = ":ragdoll_test", real = "flex_point16")
"""

def _set_real(_settings, attr):
    return {"//projects/cyclone:real": attr.real}

_real_transition = transition(
    implementation = _set_real,
    inputs = [],
    outputs = ["//projects/cyclone:real"],
)

def _with_real_impl(ctx):
    # The target, built with the real type, run as this one
    target = ctx.attr.target[0][DefaultInfo]
    executable = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(
        output = executable,
        target_file = target.files_to_run.executable,
        is_executable = True,
    )
    runfiles = ctx.runfiles(files = [executable]).merge(target.default_runfiles)
    return [DefaultInfo(executable = executable, runfiles = runfiles)]

_ATTRS = {
    "real": attr.string(
        mandatory = True,
        values = ["double", "float", "flex_real", "flex_point", "flex_point16"],
        doc = "The real type, as //projects/cyclone:real takes it",
    ),
    "target": attr.label(
        mandatory = True,
        executable = True,
        cfg = _real_transition,
        doc = "The cc_binary or cc_test to build with it",
    ),
}

real_binary = rule(
    implementation = _with_real_impl,
    attrs = _ATTRS,
    executable = True,
    doc = "A Cyclone program built with a given real type",
)

real_test = rule(
    implementation = _with_real_impl,
    attrs = _ATTRS,
    test = True,
    doc = "A Cyclone test built with a given real type",
)
