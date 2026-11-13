"""Builds 3DS apps and installs them on a 3DS over Wi-Fi, with FBI.

Builds the apps' CIAs with Bazel, serves them from this computer over HTTP,
and sends their links to FBI's "Receive URLs over the network" (on the
3DS, FBI: Remote Install > Receive URLs over the network), which downloads
and installs each. It waits for FBI to listen, so it can be started first;
ftpd, which listens on the same port (5000), is told apart by its greeting
and waited out. Exits once FBI has installed them (it closes the connection
then), saying which it downloaded, or after --wait.

Usage:
  python3 tools/fbi_install/fbi_install.py --3ds ADDRESS [--debug] [TARGET ...]

ADDRESS is the 3DS's IP address (shown on FBI's screen), or set
SAKBOX_3DS. With no targets it installs every 3DS app in the repo (every
devkitpro_3ds_app). --debug installs debug builds (bazel -c dbg), which
send their log over Wi-Fi (tools/console_log). Needs Docker running, as any
3DS build does.
"""

import argparse
import functools
import http.server
import os
import pathlib
import struct
import subprocess
import sys
import threading
import time

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
# (run with python3 from the repo, not by Bazel: the shared module by path)
sys.path.insert(0, str(REPO / "tools" / "local_network"))
from local_network import connect, local_address  # noqa: E402
FBI_PORT = 5000  # FBI's "Receive URLs over the network" (and ftpd's)
GREETING_WAIT = 2.0  # seconds: ftpd greets at once; FBI says nothing


def payload(urls):
    """What FBI reads: the length of the URLs, big-endian, then the URLs, a
    line each."""
    text = "\n".join(urls).encode()
    return struct.pack(">I", len(text)) + text


def send_to_fbi(host, data, port=FBI_PORT, give_up_at=None, say=print):
    """Sends `data` to FBI on host:port once it's listening, waiting out
    ftpd, and returns once FBI has closed the connection (it has installed
    everything). False if `give_up_at` (a time.monotonic()) passes first."""
    told = set()
    while give_up_at is None or time.monotonic() < give_up_at:
        try:
            connection = connect(host, port)
        except OSError:
            if "waiting" not in told:
                say(f"Waiting for FBI on {host}: Remote Install > Receive URLs over the network")
                told.add("waiting")
            time.sleep(2)
            continue
        if connection.greeting(GREETING_WAIT).startswith(b"220"):
            connection.close()
            if "ftpd" not in told:
                say("ftpd is open on the 3DS (it uses FBI's port): close it, then open FBI's "
                    "Receive URLs over the network")
                told.add("ftpd")
            time.sleep(3)
            continue
        say("FBI is listening: sending the links (press A on the 3DS to install)")
        try:
            connection.send_and_wait(data)
        finally:
            connection.close()
        return True
    return False


class Downloads(http.server.SimpleHTTPRequestHandler):
    """Serves only the files it's given, by name, and counts each one sent."""

    def __init__(self, *args, files, sent, **kwargs):
        self.files = files
        self.sent = sent
        super().__init__(*args, **kwargs)

    def translate_path(self, path):
        name = path.lstrip("/").split("?")[0]
        return str(self.files.get(name, "/nonexistent"))

    def do_GET(self):
        super().do_GET()
        name = self.path.lstrip("/")
        if name in self.files:
            self.sent.append(name)

    def log_message(self, format, *args):
        pass  # (main reports what FBI fetched)


class DownloadServer(http.server.ThreadingHTTPServer):
    """Waits, on server_close, for downloads still being sent: a file is
    counted once it's all written, which can be after FBI has read it all
    and moved on."""

    daemon_threads = False


def serve(files, address):
    """Serves `files` ({name: path}) on `address`; returns the server (its
    port is server.server_address[1]) and the list of names sent so far,
    complete once server.server_close() returns."""
    sent = []
    handler = functools.partial(Downloads, files=files, sent=sent)
    server = DownloadServer((address, 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, sent


def bazel(*args):
    return subprocess.run(["bazel", *args], cwd=REPO, check=True, capture_output=True,
                          text=True).stdout


def build_cias(targets, flags=()):
    """Builds `targets` (every 3DS app if none) with Bazel `flags` and
    returns their CIAs' paths."""
    if not targets:
        targets = bazel("query", 'kind("devkitpro_3ds_app", //...)').split()
    print(f"Building {' '.join(targets)}", flush=True)
    subprocess.run(["bazel", "build", *flags, *targets], cwd=REPO, check=True)
    files = bazel("cquery", *flags, "--output=files", " + ".join(targets)).split()
    return [REPO / f for f in files if f.endswith(".cia")]


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--3ds", dest="address", default=os.environ.get("SAKBOX_3DS"),
                        help="the 3DS's IP address (or set SAKBOX_3DS)")
    parser.add_argument("--wait", type=float, default=600,
                        help="how long to wait for FBI, in seconds (default 600)")
    parser.add_argument("--debug", action="store_true",
                        help="debug builds (bazel -c dbg): their log comes over Wi-Fi")
    parser.add_argument("targets", nargs="*", help="3DS apps (default: all of them)")
    args = parser.parse_args(argv)
    if not args.address:
        parser.error("say where the 3DS is: --3ds ADDRESS (FBI's screen shows it)")

    cias = build_cias(args.targets, ["-c", "dbg"] if args.debug else [])
    if not cias:
        print("No CIAs among those targets", file=sys.stderr)
        return 1
    files = {cia.name: cia for cia in cias}
    address = local_address(args.address, FBI_PORT)
    server, sent = serve(files, address)
    port = server.server_address[1]
    urls = [f"http://{address}:{port}/{name}" for name in files]
    print(f"Serving {len(files)} CIA(s) from {address}:{port}", flush=True)
    try:
        if not send_to_fbi(args.address, payload(urls),
                           give_up_at=time.monotonic() + args.wait):
            print(f"FBI didn't listen within {args.wait:.0f} s", file=sys.stderr)
            return 1
    finally:
        server.shutdown()
        server.server_close()
    missing = sorted(set(files) - set(sent))
    for name in files:
        print(f"  {'downloaded by FBI' if name in sent else 'not downloaded'}: {name}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
