# console_log: a 3DS app's log, on the Mac

Reads what a 3DS app serves on port 5005 ([`debug_log`](../../projects/scratchpadd/debug_log/README.md)): a debug build's log (`bazel build -c dbg`; served by the app's [`NetworkPadd`](../../projects/scratchpadd/network/README.md)), or the 3DS benchmark's and the audio self-test's results once they finish. It prints what the app kept from before it connected, then each line as it comes.

```bash
python3 tools/console_log/console_log.py --3ds 192.168.4.25                 # the address the app shows
python3 tools/console_log/console_log.py --3ds 192.168.4.25 --save run.log  # and keep it
SAKBOX_3DS=192.168.4.25 python3 tools/console_log/console_log.py --once     # until the app quits
```

It can be started first: it waits for the app, and when the app quits, waits for it again (Ctrl-C to stop), or with `--once` stops. Status goes to stderr, the log to stdout. `bazel run -c dbg` of a 3DS app in Azahar serves it on this computer: `--3ds 127.0.0.1`.

On macOS, Local Network privacy keeps a Python that hasn't been allowed off the local network, so it connects through `/usr/bin/nc`, which macOS allows ([`tools/local_network`](../local_network/local_network.py), shared with [`fbi_install`](../fbi_install/README.md)). `console_log_test` checks it against a log server on localhost: reading the log, noticing the app quit, waiting for it and reading it again.
