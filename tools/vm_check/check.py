"""Builds and tests the repo in a Parallels VM: Ubuntu, with GCC or Clang.

Unlike the Linux check's containers (tools/linux_check), the VM has a
desktop, so the tests that open a window run too, and a real MIDI system.
It starts or resumes the VM, shares the checkout with it read-only, copies
it inside (leaving out what git ignores), builds and tests it there, prints
Bazel's summary (tools/build_summary), and puts the VM back as it found it
(suspended, say). Exits with Bazel's exit code.

Usage:
  python3 tools/vm_check/check.py ubuntu --setup
  python3 tools/vm_check/check.py ubuntu gcc|clang [--keep-running] [BAZEL FLAG ...] [TARGET ...]
  python3 tools/vm_check/check.py ubuntu gcc|clang --log

--setup installs what the build needs in the VM, once (ubuntu_setup.sh).
The VM is the one Parallels VM whose OS is Ubuntu; with more than one, name
it with --vm NAME. It leaves out the 3DS and DS apps (tagged devkitpro),
which need Docker, and models from Blender (tagged blender), as there's no
Blender for Linux on ARM. With no targets it builds and tests everything
else. Bazel flags (--runs_per_test=10, say) are passed on. --log prints
the last run's full output.
"""

import json
import pathlib
import shlex
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
COMPILERS = ("gcc", "clang")
DEFAULT_TARGETS = ["//..."]
LEFT_OUT = "-devkitpro,-blender"
# The checkout's share: its name in Parallels, and where Ubuntu mounts it
SHARE_NAME = "SAKBox"
UBUNTU_SHARE = f"/media/psf/{SHARE_NAME}"
# How long the VM may take to come up with its desktop logged in
START_TIMEOUT = 300


def prlctl(*args, check=True):
    result = subprocess.run(["prlctl", *args], capture_output=True, text=True)
    if check and result.returncode != 0:
        raise SystemExit(f"prlctl {' '.join(args)} failed: {result.stderr.strip()}")
    return result


def guest(vm, command, tries=5):
    """Runs a shell command in the VM, as root, and returns its result.

    prlctl exec joins its arguments into one line for the guest's shell, so
    the command goes as one. Now and then it fails with "PrlJob_GetResult:
    Invalid argument" before the command runs; it's tried again.
    """
    for _ in range(tries):
        result = prlctl("exec", vm, command, check=False)
        if "PrlJob_GetResult" not in result.stderr:
            return result
        time.sleep(2)
    return result


def find_vm(name):
    vms = json.loads(prlctl("list", "--all", "--info", "--json").stdout)
    if name:
        matches = [vm for vm in vms if vm["Name"] == name]
    else:
        matches = [vm for vm in vms if vm.get("OS") == "ubuntu"]
    if len(matches) != 1:
        names = ", ".join(vm["Name"] for vm in matches) or "none"
        raise SystemExit(f"Can't tell which VM to use (Ubuntu VMs: {names}): "
                         "name it with --vm NAME.")
    return matches[0]


def bring_up(vm, state):
    if state == "suspended" or state == "paused":
        prlctl("resume", vm)
    elif state == "stopped":
        prlctl("start", vm)
    # Up once a command runs and the desktop's X server is there
    deadline = time.time() + START_TIMEOUT
    while time.time() < deadline:
        if guest(vm, "test -S /tmp/.X11-unix/X0", tries=1).returncode == 0:
            return
        time.sleep(3)
    raise SystemExit(f"{vm} didn't come up with its desktop in {START_TIMEOUT} s: is "
                     "it set up (--setup), and restarted since?")


def put_back(vm, state):
    if state in ("suspended", "paused"):
        prlctl("suspend", vm)
    elif state == "stopped":
        prlctl("stop", vm)


def share_checkout(vm, info):
    """Shares the checkout with the VM, read-only, unless it is already."""
    shares = info.get("Host Shared Folders") or {}
    share = shares.get(SHARE_NAME)
    wanted = {"enabled": True, "path": str(REPO), "mode": "ro"}
    if share != wanted:
        verb = "--shf-host-set" if share else "--shf-host-add"
        prlctl("set", vm, verb, SHARE_NAME, "--path", str(REPO), "--mode", "ro", "--enable")
    deadline = time.time() + 30
    while guest(vm, f"test -f {UBUNTU_SHARE}/MODULE.bazel", tries=1).returncode != 0:
        if time.time() > deadline:
            raise SystemExit(f"The checkout's share didn't appear at {UBUNTU_SHARE} in {vm}: "
                             "are Parallels Tools installed in it?")
        time.sleep(2)


def desktop_user(vm):
    """The VM's desktop user: the first normal account (uid 1000)."""
    return guest(vm, "id -nu 1000").stdout.strip()


def run_detached(vm, unit, command, status_file, user=None):
    """Runs a command in the VM, detached, and returns the code it wrote.

    A long prlctl exec is the one most likely to fail midway, so the command
    runs on its own, as a systemd service named UNIT (sudo or setsid in
    prlctl exec's session didn't do: the command was now and then killed
    when prlctl returned), and prlctl only checks on it. The command writes
    its exit code to STATUS_FILE last. systemd won't start a second UNIT, so
    a start tried again can't run it twice.
    """
    as_user = f"--uid={user} " if user else ""
    guest(vm, f"rm -f {status_file}; systemd-run --quiet --collect --unit={unit} {as_user}"
              f"bash -c {shlex.quote(command)}")
    while True:
        if guest(vm, f"test -f {status_file}", tries=1).returncode == 0:
            return int(guest(vm, f"cat {status_file}").stdout.strip() or 1)
        running = guest(vm, f"systemctl is-active --quiet {unit}")
        if running.returncode != 0 and "PrlJob" not in running.stderr:
            if guest(vm, f"test -f {status_file}").returncode == 0:
                continue
            raise SystemExit(f"The run in {vm} ({unit}) stopped without finishing: "
                             f"journalctl -u {unit} in the VM says why.")
        time.sleep(10)


def main(argv):
    vm_name = None
    if "--vm" in argv:
        at = argv.index("--vm")
        vm_name = argv[at + 1]
        argv = argv[:at] + argv[at + 2:]
    keep_running = "--keep-running" in argv
    argv = [arg for arg in argv if arg != "--keep-running"]
    if not argv or argv[0] != "ubuntu" or len(argv) < 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    rest = argv[1:]
    setup = rest == ["--setup"]
    if not setup and rest[0] not in COMPILERS:
        print(__doc__.strip(), file=sys.stderr)
        return 2

    info = find_vm(vm_name)
    vm, state = info["Name"], info["State"]
    print(f"Using {vm} ({state}).", flush=True)
    bring_up(vm, state)
    try:
        share_checkout(vm, info)
        user = desktop_user(vm)
        home = f"/home/{user}"
        if setup:
            status = run_detached(
                vm, "sakbox-setup", f"bash {UBUNTU_SHARE}/tools/vm_check/ubuntu_setup.sh {user} "
                f"> /root/sakbox-setup.log 2>&1; echo $? > /root/sakbox-setup.status",
                "/root/sakbox-setup.status")
            print(guest(vm, "tail -5 /root/sakbox-setup.log").stdout, end="")
            return status
        compiler, rest = rest[0], rest[1:]
        work = f"{home}/sakbox-{compiler}"
        if rest == ["--log"]:
            print(guest(vm, f"cat {work}.log").stdout, end="")
            return 0
        flags = ["--config=clang"] if compiler == "clang" else []
        flags += [f"--build_tag_filters={LEFT_OUT}", f"--test_tag_filters={LEFT_OUT}"]
        # Bazel's own flags (--runs_per_test=10, say) go before the targets
        flags += [arg for arg in rest if arg.startswith("--")]
        targets = [arg for arg in rest if not arg.startswith("--")] or DEFAULT_TARGETS
        print(f"Building and testing with {compiler}: {' '.join(targets)}", flush=True)
        run = shlex.join(["bash", f"{UBUNTU_SHARE}/tools/vm_check/ubuntu_run.sh", UBUNTU_SHARE,
                          compiler, *flags, "--", *targets])
        status = run_detached(vm, f"sakbox-{compiler}", run, f"{work}.status", user=user)
        print(guest(vm, f"cat {work}.summary").stdout, end="")
        return status
    finally:
        if not keep_running:
            put_back(vm, state)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
