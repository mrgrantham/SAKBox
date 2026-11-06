"""Builds and tests the repo in a Parallels VM: Ubuntu (GCC or Clang) or Windows (clang-cl).

Unlike the Linux check's containers (tools/linux_check), the VM has a
desktop, so the tests that open a window run too, and Ubuntu's a real MIDI
system. It starts or resumes the VM, shares the checkout with it read-only,
copies it inside (leaving out what git ignores), builds and tests it there
as the desktop's user, prints Bazel's summary (tools/build_summary), and
puts the VM back as it found it (suspended, say). Exits with Bazel's exit
code.

Usage:
  python3 tools/vm_check/check.py ubuntu|windows --setup
  python3 tools/vm_check/check.py ubuntu gcc|clang [OPTIONS] [BAZEL FLAG ...] [TARGET ...]
  python3 tools/vm_check/check.py windows [OPTIONS] [BAZEL FLAG ...] [TARGET ...]
  python3 tools/vm_check/check.py ubuntu gcc|clang --log
  python3 tools/vm_check/check.py windows --log

Options: --vm NAME (with more than one VM of that OS), --keep-running
(leave the VM running afterwards).

--setup installs what the build needs in the VM, once (ubuntu_setup.sh,
windows_setup.ps1). The VM is the one Parallels VM with that OS. It leaves
out the 3DS and DS apps (tagged devkitpro), which need Docker, and models
from Blender (tagged blender). With no targets it builds and tests
everything else. Bazel flags (--runs_per_test=10, say) are passed on. --log
prints the last run's full output.
"""

import base64
import json
import pathlib
import shlex
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
DEFAULT_TARGETS = ["//..."]
LEFT_OUT = "-devkitpro,-blender"
# The checkout's share's name in Parallels
SHARE_NAME = "SAKBox"
# How long the VM may take to come up with its desktop logged in
START_TIMEOUT = 300


# How long one prlctl command may take: now and then prlctl exec never
# returns, though the VM is fine
PRLCTL_TIMEOUT = 120


def prlctl(*args, check=True):
    try:
        result = subprocess.run(["prlctl", *args], capture_output=True, text=True,
                                timeout=PRLCTL_TIMEOUT)
    except subprocess.TimeoutExpired:
        result = subprocess.CompletedProcess(args, 255, "", "PrlJob_ timed out")
    if check and result.returncode != 0:
        raise SystemExit(f"prlctl {' '.join(args)} failed: {result.stderr.strip()}")
    return result


def exec_in(vm, *command, tries=5):
    """Runs a command in the VM (as root, or SYSTEM) and returns its result.

    Now and then prlctl exec fails with "PrlJob_GetResult: Invalid argument"
    or never returns; it's tried again.
    """
    for _ in range(tries):
        result = prlctl("exec", vm, *command, check=False)
        if "PrlJob_" not in result.stderr:
            return result
        time.sleep(2)
    return result


class Ubuntu:
    """Ubuntu: commands through bash, runs as systemd services."""

    os = "ubuntu"
    compilers = ("gcc", "clang")
    share = f"/media/psf/{SHARE_NAME}"

    def __init__(self, vm):
        self.vm = vm

    def run(self, command, tries=5):
        # (prlctl exec joins its arguments into one line for the guest's
        # shell, so the command goes as one)
        return exec_in(self.vm, command, tries=tries)

    def exists(self, path):
        return self.run(f"test -e {shlex.quote(path)}", tries=1).returncode == 0

    def read(self, path):
        return self.run(f"cat {shlex.quote(path)}").stdout

    def desktop_ready(self):
        return self.run("test -S /tmp/.X11-unix/X0", tries=1).returncode == 0

    def desktop_user(self):
        """The desktop's user: the first normal account (uid 1000)."""
        return self.run("id -nu 1000").stdout.strip()

    def work(self, compiler):
        return f"/home/{self.desktop_user()}/sakbox-{compiler}"

    def start(self, name, command, status_file, as_user):
        """Starts COMMAND as a systemd service NAME (as the desktop's user if
        AS_USER). sudo or setsid in prlctl exec's session didn't do: the
        command was now and then killed as prlctl returned. systemd won't
        start a second NAME, so a start tried again can't run it twice."""
        user = f"--uid={self.desktop_user()} " if as_user else ""
        self.run(f"rm -f {status_file}; systemd-run --quiet --collect --unit={name} {user}"
                 f"bash -c {shlex.quote(command)}")

    def running(self, name):
        result = self.run(f"systemctl is-active --quiet {name}")
        return result.returncode == 0, f"journalctl -u {name} in the VM says why"

    def setup(self):
        log, status = "/root/sakbox-setup.log", "/root/sakbox-setup.status"
        command = (f"bash {self.share}/tools/vm_check/ubuntu_setup.sh {self.desktop_user()} "
                   f"> {log} 2>&1; echo $? > {status}")
        return "sakbox-setup", command, status, log, False

    def build(self, compiler, flags, targets):
        if compiler == "clang":
            flags = ["--config=clang", *flags]
        work = self.work(compiler)
        command = shlex.join(["bash", f"{self.share}/tools/vm_check/ubuntu_run.sh", self.share,
                              compiler, *flags, "--", *targets])
        return f"sakbox-{compiler}", command, f"{work}.status", f"{work}.summary", True


class Windows:
    """Windows: commands through PowerShell, runs as detached processes."""

    os = "win-11"
    compilers = ("clang-cl",)
    share = rf"\\Mac\{SHARE_NAME}"

    def __init__(self, vm):
        self.vm = vm

    def run(self, script, tries=5, as_user=False):
        # Encoded, so prlctl exec's joining of its arguments can't touch the
        # script's quotes
        encoded = base64.b64encode(script.encode("utf-16-le")).decode()
        user = ["--current-user"] if as_user else []
        return exec_in(self.vm, *user, "powershell", "-NoProfile", "-NonInteractive",
                       "-EncodedCommand", encoded, tries=tries)

    @staticmethod
    def quote(text):
        return "'" + text.replace("'", "''") + "'"

    def exists(self, path):
        return self.run(f"if (Test-Path {self.quote(path)}) {{ exit 0 }} else {{ exit 1 }}",
                        tries=1).returncode == 0

    def read(self, path):
        return self.run(f"Get-Content -Raw {self.quote(path)}").stdout.replace("\r\n", "\n")

    def desktop_user(self):
        """The user logged in at the VM's screen, DOMAIN\\name."""
        return self.run("(Get-CimInstance Win32_ComputerSystem).UserName").stdout.strip()

    def desktop_ready(self):
        return bool(self.run("(Get-CimInstance Win32_ComputerSystem).UserName", tries=1)
                    .stdout.strip())

    def start(self, name, command, status_file, as_user):
        """Starts COMMAND (a program and its arguments), detached: as the user
        at the screen if AS_USER (prlctl exec --current-user, in their
        desktop's session, so windows can open), else as SYSTEM. It outlives
        prlctl exec. (A scheduled task with an interactive logon was tried
        first: in this VM it never started.)"""
        self.run(f"Remove-Item -ErrorAction SilentlyContinue {self.quote(status_file)}")
        arguments = subprocess.list2cmdline(command[1:])
        self.run(f"Start-Process -WindowStyle Hidden -FilePath {self.quote(command[0])} "
                 f"-ArgumentList {self.quote(arguments)}", as_user=as_user)

    def running(self, name):
        """Whether the program started for NAME (its script's name is in its
        command line) is running."""
        result = self.run("Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -match "
                          f"{self.quote(name)} -and $_.ProcessId -ne $PID }} | Measure-Object | "
                          "Select-Object -ExpandProperty Count")
        count = result.stdout.strip()
        # (an answer that didn't come is taken as still running)
        return not count.isdigit() or int(count) > 0, f"{name} ended without its exit code"

    def setup(self):
        folder = r"C:\sakbox-setup"
        log, status = rf"{folder}\setup.log", rf"{folder}\setup.status"
        setup_script = self.share + r"\tools\vm_check\windows_setup.ps1"
        script = (f"New-Item -ItemType Directory -Force {self.quote(folder)} | Out-Null; "
                  f"try {{ & {self.quote(setup_script)} *> "
                  f"{self.quote(log)}; $code = 0 }} catch {{ $_ | Out-File -Append {self.quote(log)}; "
                  f"$code = 1 }}; Set-Content {self.quote(status)} $code")
        command = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script]
        return "windows_setup.ps1", command, status, log, False

    def build(self, compiler, flags, targets):
        work = rf"C:\sakbox\{compiler}"
        # pythonw: no console window on the desktop for the run
        command = ["pythonw", self.share + r"\tools\vm_check\windows_run.py", self.share,
                   compiler, *flags, "--", *targets]
        return "windows_run.py", command, f"{work}.status", f"{work}.summary", True

    def work(self, compiler):
        return rf"C:\sakbox\{compiler}"


GUESTS = {"ubuntu": Ubuntu, "windows": Windows}


def find_vm(guest_class, name):
    vms = json.loads(prlctl("list", "--all", "--info", "--json").stdout)
    if name:
        matches = [vm for vm in vms if vm["Name"] == name]
    else:
        matches = [vm for vm in vms if vm.get("OS") == guest_class.os]
    if len(matches) != 1:
        names = ", ".join(vm["Name"] for vm in matches) or "none"
        raise SystemExit(f"Can't tell which VM to use (VMs with OS {guest_class.os}: {names}): "
                         "name it with --vm NAME.")
    return matches[0]


def bring_up(guest, state):
    if state in ("suspended", "paused"):
        prlctl("resume", guest.vm)
    elif state == "stopped":
        prlctl("start", guest.vm)
    # Up once a command runs and someone's logged in at the desktop
    deadline = time.time() + START_TIMEOUT
    while time.time() < deadline:
        if guest.desktop_ready():
            return
        time.sleep(3)
    raise SystemExit(f"{guest.vm} didn't come up with its desktop logged in in {START_TIMEOUT} s: "
                     "is it set up (--setup), and restarted since?")


def put_back(vm, state):
    if state in ("suspended", "paused"):
        prlctl("suspend", vm)
    elif state == "stopped":
        prlctl("stop", vm)


def share_checkout(guest, info):
    """Shares the checkout with the VM, read-only, unless it is already."""
    shares = info.get("Host Shared Folders") or {}
    share = shares.get(SHARE_NAME)
    wanted = {"enabled": True, "path": str(REPO), "mode": "ro"}
    if share != wanted:
        verb = "--shf-host-set" if share else "--shf-host-add"
        prlctl("set", guest.vm, verb, SHARE_NAME, "--path", str(REPO), "--mode", "ro", "--enable")
    marker = guest.share + ("\\" if isinstance(guest, Windows) else "/") + "MODULE.bazel"
    deadline = time.time() + 30
    while not guest.exists(marker):
        if time.time() > deadline:
            raise SystemExit(f"The checkout's share didn't appear at {guest.share} in {guest.vm}: "
                             "are Parallels Tools installed in it?")
        time.sleep(2)


def run_detached(guest, name, command, status_file, as_user):
    """Runs a command in the VM, detached, and returns the code it wrote.

    A long prlctl exec is the one most likely to fail midway, so the command
    runs on its own (NAME: a systemd service on Ubuntu, the script's name on
    Windows), and prlctl only checks on it every 10 seconds. The command
    writes its exit code to STATUS_FILE last.
    """
    guest.start(name, command, status_file, as_user)
    while True:
        time.sleep(10)
        if guest.exists(status_file):
            return int(guest.read(status_file).strip() or 1)
        running, why = guest.running(name)
        if not running and not guest.exists(status_file):
            raise SystemExit(f"The run in {guest.vm} ({name}) stopped without finishing: {why}.")


def main(argv):
    vm_name = None
    if "--vm" in argv:
        at = argv.index("--vm")
        vm_name = argv[at + 1]
        argv = argv[:at] + argv[at + 2:]
    keep_running = "--keep-running" in argv
    argv = [arg for arg in argv if arg != "--keep-running"]
    if not argv or argv[0] not in GUESTS:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    guest_class, rest = GUESTS[argv[0]], argv[1:]
    setup = rest == ["--setup"]
    if not setup:
        # Windows has one compiler, so it isn't named
        if len(guest_class.compilers) == 1:
            compiler = guest_class.compilers[0]
        elif rest and rest[0] in guest_class.compilers:
            compiler, rest = rest[0], rest[1:]
        else:
            print(__doc__.strip(), file=sys.stderr)
            return 2

    info = find_vm(guest_class, vm_name)
    state = info["State"]
    guest = guest_class(info["Name"])
    print(f"Using {guest.vm} ({state}).", flush=True)
    if setup:
        # Parallels pauses an idle VM, which stops a long build's checks
        prlctl("set", guest.vm, "--pause-idle", "off")
    bring_up(guest, state)
    try:
        share_checkout(guest, info)
        if setup:
            name, command, status_file, log, as_user = guest.setup()
            status = run_detached(guest, name, command, status_file, as_user)
            print("\n".join(guest.read(log).splitlines()[-5:]))
            return status
        if rest == ["--log"]:
            print(guest.read(guest.work(compiler) + ".log"), end="")
            return 0
        flags = [f"--build_tag_filters={LEFT_OUT}", f"--test_tag_filters={LEFT_OUT}"]
        # Bazel's own flags (--runs_per_test=10, say) go before the targets
        flags += [arg for arg in rest if arg.startswith("--")]
        targets = [arg for arg in rest if not arg.startswith("--")] or DEFAULT_TARGETS
        print(f"Building and testing with {compiler}: {' '.join(targets)}", flush=True)
        name, command, status_file, summary, as_user = guest.build(compiler, flags, targets)
        status = run_detached(guest, name, command, status_file, as_user)
        print(guest.read(summary), end="")
        return status
    finally:
        if not keep_running:
            put_back(guest.vm, state)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
