"""console_log against a log server on localhost standing in for a 3DS
app's: it waits for the app, reads what's sent, notices the app quit, and
reads the next one."""

import socket
import threading
import time
import unittest

import console_log


class FakeApp:
    """Serves `texts` to one reader, one connection each, then stops"""

    def __init__(self, texts, delay=0.0):
        self.listener = socket.socket()
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.port = self.listener.getsockname()[1]
        self.texts, self.delay = texts, delay
        self.thread = threading.Thread(target=self.serve, daemon=True)

    def start(self):
        time.sleep(self.delay)  # (the app not yet started)
        self.listener.listen(1)
        self.thread.start()

    def serve(self):
        for text in self.texts:
            connection, _ = self.listener.accept()
            connection.sendall(text)
            connection.close()  # (the app quitting)
        self.listener.close()


class ConsoleLogTest(unittest.TestCase):
    def test_reads_the_log_and_notices_the_app_quit(self):
        app = FakeApp([b"one\ntwo\n"])
        app.start()
        got, said = [], []
        console_log.read_log("127.0.0.1", app.port, got.append, said.append, once=True,
                             give_up_at=time.monotonic() + 5)
        self.assertEqual(b"".join(got), b"one\ntwo\n")
        self.assertTrue(any("ended" in s for s in said))

    def test_waits_for_the_app_and_reads_it_again(self):
        app = FakeApp([b"first run\n", b"second run\n"], delay=0)
        port = app.port
        got, said = [], []
        starter = threading.Thread(target=lambda: (time.sleep(0.3), app.start()))
        starter.start()
        console_log.read_log("127.0.0.1", port, got.append, said.append,
                             give_up_at=time.monotonic() + 3, retry=0.1)
        starter.join()
        self.assertEqual(b"".join(got), b"first run\nsecond run\n")
        self.assertTrue(any("Waiting" in s for s in said))


if __name__ == "__main__":
    unittest.main()
