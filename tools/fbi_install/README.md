# Installing 3DS apps over Wi-Fi with FBI

Builds the repo's 3DS apps and installs them on a 3DS over Wi-Fi, with no SD card swapping: [`fbi_install.py`](fbi_install.py) builds their CIAs with Bazel, serves them from this computer over HTTP, and sends their links to [FBI](https://github.com/Steveice10/FBI)'s **Remote Install → Receive URLs over the network**, which downloads and installs each one.

```bash
python3 tools/fbi_install/fbi_install.py --3ds 192.168.4.21          # every 3DS app
python3 tools/fbi_install/fbi_install.py --3ds 192.168.4.21 //projects/scratchpadd/3ds/demo:scratchpadd-3ds-demo
```

On the 3DS, open FBI's **Remote Install → Receive URLs over the network** (its screen shows the 3DS's address, for `--3ds`; or set `SAKBOX_3DS`), and press A when it asks to install. The tool can be started first: it waits for FBI to listen (10 minutes; `--wait` to change), and exits when FBI has finished, saying which CIAs FBI downloaded. An app installed again replaces itself (each keeps its title ID; see [`tools/devkitpro`](../devkitpro/README.md)).

**What it needs:**
- A 3DS with custom firmware (Luma3DS) and FBI (2.6.1 is the latest), on the same network as this computer. The 3DS joins only 2.4 GHz WPA2 (or older) networks.
- Docker running, as for any 3DS build.

**Things it handles:**
- **ftpd uses FBI's port (5000).** A connection ftpd answers (it greets with `220`) is closed and tried again later, and the tool says to close ftpd. (A connection to FBI that sends nothing makes FBI show "Failed to read payload length", so the tool never connects only to look.)
- **macOS's Local Network privacy.** A Python that hasn't been allowed onto the local network can't connect to the 3DS ("No route to host"); the tool then connects with macOS's own `/usr/bin/nc`, which is allowed. Serving the files needs no permission. (A Terminal run asks once to allow Python, which also works.)

FBI reads the links as a 4-byte big-endian length, then the URLs, one a line, and closes the connection once it's done. `fbi_install_test` checks the tool against a fake FBI and ftpd on this computer.

**QR codes:** FBI's **Scan QR Code** reads one link reliably; a code holding several is too dense for the 3DS's camera. Receiving the links over the network has no such limit.
