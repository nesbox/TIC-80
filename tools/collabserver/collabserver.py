#!/usr/bin/python3
#
# MIT License
#
# Copyright (c) 2019 Wade Brainerd - wadetb@gmail.com
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
import argparse
import pathlib
import re
import socket
import struct
import time
import threading

import http.server
import socketserver

from urllib.parse import urlparse, parse_qs

DEFAULT_PORT = 8000
DEFAULT_DATA_DIRECTORY = '.'
DEFAULT_GREETING = 'welcome to collab :)'

PROTOCOL_VERSION = 1

TIC80_SIZE = 1 * 1024 * 1024

DATA_EXT = '.tic_collab'


class TIC:
    def __init__(self, data_path, name):
        self.path = (data_path / name).with_suffix(DATA_EXT)

        if not self.path.exists():
            with self.path.open('wb') as file:
                file.write(b'\0' * TIC80_SIZE)
            self.init_needed = True
        else:
            self.init_needed = False

        # No buffering, as multiple clients will access the same file simultaneously.
        self.file = self.path.open('r+b', buffering=0)

        self.condition = threading.Condition()
        self.watchers = {}

    def read_mem(self, offset, size):
        self.file.seek(offset)
        return self.file.read(size)

    def write_mem(self, offset, data):
        self.file.seek(offset)
        self.file.write(data)

    def signal_update(self, offset, size):
        with self.condition:
            for k in self.watchers.keys():
                self.watchers[k].append((offset, size))
            self.condition.notify_all()

    def watch(self, client_key):
        pending_updates = []
        self.watchers[client_key] = pending_updates
        while True:
            with self.condition:
                self.condition.wait()
                while len(pending_updates):
                    yield pending_updates.pop()


class CollabRequestHandler(http.server.BaseHTTPRequestHandler):
    def lookup_tic(self, path):
        name = path.replace('/', '').lower()

        if re.match(r'[^a-z_-]', name):
            raise ValueError

        if not name in self.server.tics:
            self.server.tics[name] = TIC(server.data_path, name)

        return self.server.tics[name]

    def send_response_with_content(self, content):
        self.send_response(200)
        self.send_header('Content-Length', '{}'.format(len(content)))
        self.end_headers()

        self.wfile.write(content)

    def do_request(self):
        url = urlparse(self.path)
        query = parse_qs(url.query)

        try:
            tic = self.lookup_tic(url.path)

            if self.command == 'GET':
                if not len(query):
                    header = struct.pack('<bb', PROTOCOL_VERSION,
                                         tic.init_needed)
                    greeting = self.server.greeting.encode()
                    self.send_response_with_content(header + greeting)
                    tic.init_needed = False

                elif 'watch' in query:
                    self.send_response(200)
                    self.send_header('Transfer-Encoding', 'chunked')
                    self.send_header('X-Accel-Buffering', 'no')
                    self.end_headers()

                    # Long-lived connection, ensure routers don't drop it.
                    self.connection.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)

                    prefix = '8\r\n'.encode()
                    suffix = '\r\n'.encode()
                    self.wfile.write(prefix + struct.pack('<ii', 0, TIC80_SIZE) + suffix)

                    for offset, size in tic.watch(self.client_address):
                        if offset is None:
                            break
                        self.wfile.write(prefix + struct.pack('<ii', offset, size) + suffix)

                    self.wfile.write('0\r\n\r\n'.encode())

                elif 'offset' in query:
                    offset = int(query['offset'][0])
                    size = int(query['size'][0])

                    self.send_response_with_content(tic.read_mem(offset, size))

            elif self.command == 'PUT':
                offset = int(query['offset'][0])
                size = int(query['size'][0])

                tic.write_mem(offset, self.rfile.read(size))
                tic.signal_update(offset, size)

                self.send_response(200)
                self.send_header('Content-Length', '0')
                self.end_headers()

        except ValueError:
            self.send_error(400)
        except BrokenPipeError:
            pass
        except OSError:
            self.send_error(500)

    def do_GET(self):
        self.do_request()

    def do_PUT(self):
        self.do_request()


class CollabServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    def __init__(self, address, port, data_dir, greeting):
        super().__init__((address, port), CollabRequestHandler)

        self.tics = {}

        self.data_path = pathlib.Path(data_dir)
        self.data_path.mkdir(exist_ok=True)

        self.greeting = greeting

    def shutdown(self):
        self.socket.close()
        for tic in self.tics.values():
            tic.signal_update(None, None)


try:
    parser = argparse.ArgumentParser(description='TIC-80 collaboration server')
    parser.add_argument('--address',
                        type=str,
                        default='',
                        help='Listen address [default: all interfaces]')
    parser.add_argument('--port',
                        type=int,
                        default=DEFAULT_PORT,
                        help='Listen port [default: 8000]')
    parser.add_argument('--data-dir',
                        type=str,
                        default=DEFAULT_DATA_DIRECTORY,
                        metavar='DIR',
                        help='Data directory [default: current directory]')
    parser.add_argument('--greeting',
                        type=str,
                        default=DEFAULT_GREETING,
                        help='Alternate server greeting')
    args = parser.parse_args()

    server = CollabServer(args.address, args.port, args.data_dir,
                          args.greeting)
    print('Started collab server on port {}:{} with data in "{}"'.format(
        args.address, args.port, args.data_dir))
    server.serve_forever()

except KeyboardInterrupt:
    print('Shutting down collab server')
    server.shutdown()
