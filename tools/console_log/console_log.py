"""Reads a 3DS app's debug log over Wi-Fi, as it's logged.

A debug build (bazel build -c dbg) of a 3DS app with a DebugLog::Session
and a NetworkPadd serves its log on port 5005 (//projects/scratchpadd/debug_log,
//projects/scratchpadd/network); this
connects, prints what the app kept from before, then each line as it comes,
and saves it all with --save. It waits for the app to start, and for it
again when it quits (Ctrl-C to stop), or stops with --once.

Usage:
  python3 tools/console_log/console_log.py --3ds ADDRESS [--save FILE] [--once]

ADDRESS is the 3DS's IP address (the app shows it, "Log: ADDRESS:5005"),
or set SAKBOX_3DS, as tools/fbi_install takes it.
"""

import argparse
import os
import pathlib
import sys
import time

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
# (run with python3 from the repo, not by Bazel: the shared module by path)
sys.path.insert(0, str(REPO / "tools" / "local_network"))
from local_network import connect  # noqa: E402

LOG_PORT = 5005  # DebugLog::Session::kLogPort (and DebugLog::LogServer::kPort)


def read_log(host, port, write, say=print, once=False, give_up_at=None, retry=2.0):
    """Connects to host:port (again whenever the app goes, unless `once`),
    passing what arrives to `write`. Returns once `once` is done, or at
    `give_up_at` (time.monotonic) if that's given."""
    waiting_said = False
    while give_up_at is None or time.monotonic() < give_up_at:
        try:
            connection = connect(host, port)
        except OSError:
            if not waiting_said:
                say(f"Waiting for the app's log on {host}:{port}")
                waiting_said = True
            time.sleep(retry)
            continue
        say(f"Connected to {host}:{port}")
        waiting_said = False
        try:
            while give_up_at is None or time.monotonic() < give_up_at:
                data = connection.read(0.5)
                if data is None:
                    break
                if data:
                    write(data)
        finally:
            connection.close()
        say("The app's log ended (the app quit?)")
        if once:
            return


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--3ds", dest="address", default=os.environ.get("SAKBOX_3DS"),
                        help="the 3DS's IP address (or set SAKBOX_3DS)")
    parser.add_argument("--port", type=int, default=LOG_PORT)
    parser.add_argument("--save", help="also write the log to this file")
    parser.add_argument("--once", action="store_true",
                        help="stop when the app quits, rather than waiting for it again")
    args = parser.parse_args(argv)
    if not args.address:
        parser.error("the 3DS's address: --3ds ADDRESS, or set SAKBOX_3DS")
    saved = open(args.save, "ab") if args.save else None

    def write(data):
        sys.stdout.buffer.write(data)
        sys.stdout.flush()
        if saved:
            saved.write(data)
            saved.flush()

    def say(text):
        print(f"[console_log] {text}", file=sys.stderr, flush=True)

    try:
        read_log(args.address, args.port, write, say, args.once)
    except KeyboardInterrupt:
        pass
    finally:
        if saved:
            saved.close()


if __name__ == "__main__":
    main(sys.argv[1:])
