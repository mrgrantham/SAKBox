"""Connections to a device on the local network (a 3DS), made by Python,
or by the system's nc where macOS keeps Python off the local network: Local
Network privacy fails a Python that hasn't been allowed with "No route to
host", where nc, part of macOS, is allowed. Used by tools/fbi_install and
tools/console_log.
"""

import os
import select
import socket
import subprocess
import sys


def local_address(remote, port):
    """This computer's address on the network that reaches `remote` (no
    packet is sent)."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.connect((remote, port))
        return probe.getsockname()[0]


class SocketConnection:
    """A TCP connection to the 3DS, made by Python."""

    def __init__(self, host, port):
        self.socket = socket.create_connection((host, port), timeout=3)

    def greeting(self, wait):
        """What the other end says first, within `wait` seconds (b"" if nothing)."""
        ready, _, _ = select.select([self.socket], [], [], wait)
        return self.socket.recv(64) if ready else b""

    def read(self, wait):
        """What arrives within `wait` seconds (b"" if nothing); None once the
        other end has closed, or reset the connection (an app quitting can)."""
        ready, _, _ = select.select([self.socket], [], [], wait)
        if not ready:
            return b""
        try:
            data = self.socket.recv(4096)
        except ConnectionResetError:
            return None
        return data or None

    def send_and_wait(self, data):
        """Sends `data`, then waits until the other end closes, or resets the
        connection: FBI resets it when it has finished installing."""
        self.socket.sendall(data)
        self.socket.settimeout(None)
        try:
            while self.socket.recv(64):
                pass
        except ConnectionResetError:
            pass

    def close(self):
        self.socket.close()


class NcConnection:
    """The same, made by the system's nc: on macOS, Local Network privacy
    keeps a Python that hasn't been allowed off the local network (it fails
    with "No route to host"), where nc, part of macOS, is allowed."""

    def __init__(self, host, port):
        self.process = subprocess.Popen(["/usr/bin/nc", "-G", "3", host, str(port)],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL)
        # nc gives up after 3 s if nothing answers; connected, it stays
        try:
            self.process.wait(timeout=3.5)
            raise ConnectionRefusedError(f"nothing answered on {host}:{port}")
        except subprocess.TimeoutExpired:
            pass

    def greeting(self, wait):
        ready, _, _ = select.select([self.process.stdout], [], [], wait)
        return os.read(self.process.stdout.fileno(), 64) if ready else b""

    def read(self, wait):
        ready, _, _ = select.select([self.process.stdout], [], [], wait)
        if not ready:
            return b""
        data = os.read(self.process.stdout.fileno(), 4096)
        return data or None

    def send_and_wait(self, data):
        self.process.stdin.write(data)
        self.process.stdin.flush()
        # (stdin stays open: macOS's nc would otherwise close the connection)
        while os.read(self.process.stdout.fileno(), 64):
            pass

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()


def connect(host, port):
    """A connection to host:port, by Python, or by nc where macOS keeps
    Python off the local network"""
    try:
        return SocketConnection(host, port)
    except OSError as error:
        blocked = getattr(error, "errno", None) == 65  # EHOSTUNREACH
        if not (blocked and sys.platform == "darwin" and os.path.exists("/usr/bin/nc")):
            raise
        return NcConnection(host, port)
