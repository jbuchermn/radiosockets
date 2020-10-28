import os
import time
import json
import cgi

from threading import Thread
from http.server import HTTPServer, SimpleHTTPRequestHandler
from urllib.parse import urlparse
from mimetypes import guess_type

PORT = 8080
STATIC = os.path.join(os.path.dirname(
    os.path.realpath(__file__)), 'build')
print(STATIC)


class Webserver(Thread):
    def __init__(self, daemon):
        super().__init__()

        class RequestHandler(SimpleHTTPRequestHandler):
            def do_POST(self):

                ctype, _ = cgi.parse_header(self.headers['content-type'])
                if ctype != 'application/json':
                    self.send_response(400)
                    self.end_headers()
                    return
                    
                length = int(self.headers['content-length'])
                message = json.loads(self.rfile.read(length))

                result = daemon.cmd_json(message)

                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(result).encode("utf-8"))

            def do_GET(self):
                param = urlparse(self.path)

                if os.path.isfile(STATIC + '/' + param.path):
                    self.send_response(200)
                    t, _ = guess_type(STATIC + '/' + param.path)
                    if t is not None:
                        self.send_header('Content-Type', t)
                    self.end_headers()

                    with open(STATIC + '/' + param.path, 'rb') as fin:
                        self.copyfile(fin, self.wfile)

                else:
                    self.send_response(200)
                    self.send_header('Content-Type', 'text/html')
                    self.end_headers()

                    with open(os.path.join(STATIC, 'index.html'), 'rb') as fin:
                        self.copyfile(fin, self.wfile)

        self._httpd = HTTPServer(("0.0.0.0", PORT), RequestHandler)
        self._running = True

    def run(self):
        print("HTTP Server running on 0.0.0.0:%d" % PORT)
        while self._running:
            self._httpd.handle_request()

    def stop(self):
        self._running = False
        os.system("wget -O - localhost:%d > /dev/null" % PORT)


if __name__ == '__main__':
    try:
        server = Webserver(lambda m: m)
        server.run()
    except Exception as e:
        print(e)


