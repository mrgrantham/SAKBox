"""Tests for local_network's SocketConnection, against a server on
localhost: sending and waiting returning when the other end closes, and
when it resets the connection (as FBI does once it has installed)."""

import socket
import struct
import threading
import unittest

import local_network


def serve_once(finish):
    """A server on a free localhost port that reads what's sent, then
    `finish`es the connection ("close" or "reset"). Returns its port and a
    list that gets what it read."""
    server = socket.socket()
    server.bind(("127.0.0.1", 0))
    server.listen(1)
    received = []

    def run():
        connection, _ = server.accept()
        received.append(connection.recv(64))
        if finish == "reset":
            # (closing with a zero linger sends a reset, not a close)
            connection.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        connection.close()
        server.close()

    threading.Thread(target=run, daemon=True).start()
    return server.getsockname()[1], received


class SendAndWaitTest(unittest.TestCase):
    def test_returns_when_the_other_end_closes(self):
        port, received = serve_once("close")
        connection = local_network.SocketConnection("127.0.0.1", port)
        connection.send_and_wait(b"links")
        connection.close()
        self.assertEqual(received, [b"links"])

    def test_returns_when_the_other_end_resets(self):
        port, received = serve_once("reset")
        connection = local_network.SocketConnection("127.0.0.1", port)
        connection.send_and_wait(b"links")  # (no ConnectionResetError)
        connection.close()
        self.assertEqual(received, [b"links"])


if __name__ == "__main__":
    unittest.main()
