"""fbi_install against a fake FBI on this computer: what it sends, the
CIAs served and fetched, and ftpd (on FBI's port) waited out."""

import pathlib
import socket
import struct
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request

import fbi_install


class FakeThreeDs:
    """Listens where FBI would. The first `ftpd_first` connections are
    answered as ftpd answers (a greeting, then nothing); the next as FBI:
    it reads the length and the URLs, downloads each, and closes."""

    def __init__(self, ftpd_first):
        self.listener = socket.create_server(("127.0.0.1", 0))
        self.port = self.listener.getsockname()[1]
        self.ftpd_first = ftpd_first
        self.urls, self.downloaded = [], []
        self.ftpd_connections = []
        threading.Thread(target=self.run, daemon=True).start()

    def close(self):
        for connection in self.ftpd_connections:
            connection.close()
        self.listener.close()

    def run(self):
        while True:
            connection, _ = self.listener.accept()
            if self.ftpd_first:
                self.ftpd_first -= 1
                connection.sendall(b"220 Hello!\r\n")
                self.ftpd_connections.append(connection)  # (left open, as ftpd leaves it)
                continue
            length = struct.unpack(">I", self.read(connection, 4))[0]
            self.urls = self.read(connection, length).decode().split("\n")
            for url in self.urls:
                self.downloaded.append(urllib.request.urlopen(url).read())
            connection.sendall(b"\x01")
            connection.close()
            return

    @staticmethod
    def read(connection, count):
        data = b""
        while len(data) < count:
            data += connection.recv(count - len(data))
        return data


class FbiInstallTest(unittest.TestCase):
    def test_payload_is_the_length_then_a_url_a_line(self):
        data = fbi_install.payload(["http://a/x.cia", "http://a/y.cia"])
        self.assertEqual(struct.unpack(">I", data[:4])[0], len(data) - 4)
        self.assertEqual(data[4:], b"http://a/x.cia\nhttp://a/y.cia")

    def test_serves_the_cias_and_waits_out_ftpd(self):
        directory = self.enterContext(tempfile.TemporaryDirectory())
        files = {}
        for name, contents in (("one.cia", b"first"), ("two.cia", b"second")):
            path = pathlib.Path(directory, name)
            path.write_bytes(contents)
            files[name] = path
        server, sent = fbi_install.serve(files, "127.0.0.1")
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        port = server.server_address[1]
        urls = [f"http://127.0.0.1:{port}/{name}" for name in files]

        three_ds = FakeThreeDs(ftpd_first=1)
        self.addCleanup(three_ds.close)
        said = []
        self.assertTrue(fbi_install.send_to_fbi("127.0.0.1", fbi_install.payload(urls),
                                                port=three_ds.port,
                                                give_up_at=time.monotonic() + 30,
                                                say=said.append))
        self.assertEqual(three_ds.urls, urls)
        self.assertEqual(three_ds.downloaded, [b"first", b"second"])
        self.assertTrue(any("ftpd" in line for line in said))
        with self.assertRaises(urllib.error.HTTPError) as refused:  # only the CIAs given
            urllib.request.urlopen(f"http://127.0.0.1:{port}/fbi_install.py")
        refused.exception.close()
        server.shutdown()
        server.server_close()  # (waits for the counting, as main does)
        self.assertEqual(sorted(sent), ["one.cia", "two.cia"])

    def test_gives_up_when_nothing_listens(self):
        with socket.create_server(("127.0.0.1", 0)) as taken:
            port = taken.getsockname()[1]
        said = []
        self.assertFalse(fbi_install.send_to_fbi("127.0.0.1", b"", port=port,
                                                 give_up_at=time.monotonic() + 1,
                                                 say=said.append))
        self.assertIn("Waiting for FBI", said[0])


if __name__ == "__main__":
    unittest.main()
