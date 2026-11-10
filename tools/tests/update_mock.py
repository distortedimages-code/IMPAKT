#!/usr/bin/env python3
"""A stand-in for GitHub's releases API, for tools/plugin_test/update_test.cpp (v87).

  /latest      a newer release (v0.88.0) with an installer for each system
  /same        the release the test runs (v0.87.0)
  /none        no release yet (404)
  /noasset     a newer release without the installers
  /dl/<name>   a redirect to /blob/<name> (as GitHub's assets do)
  /blob/<name> 1 MB of bytes (byte i = i % 251)
Usage: update_mock.py PORT
"""
import http.server, json, sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8771
BLOB = bytes(i % 251 for i in range(1 << 20))
NAMES = ['IMPAKT-macOS.dmg', 'IMPAKT-Windows-Setup.exe', 'IMPAKT-Linux.tar.gz']


def release(tag, assets=True):
    return {'tag_name': tag, 'name': 'IMPAKT ' + tag[1:], 'html_url': 'http://127.0.0.1:%d/page/%s' % (PORT, tag),
            'body': 'What is new in ' + tag + '\r\n- RADIO: more stations\r\n- an updater',
            'assets': [{'name': n, 'browser_download_url': 'http://127.0.0.1:%d/dl/%s' % (PORT, n)} for n in NAMES] if assets else []}


class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def send(self, code, body, ctype='application/json', extra=None):
        self.send_response(code)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        p = self.path.split('?')[0]
        ua = self.headers.get('User-Agent', '')
        if not ua.startswith('IMPAKT/'):
            return self.send(403, b'{"message":"no user agent"}')
        if p == '/latest':
            return self.send(200, json.dumps(release('v0.88.0')).encode())
        if p == '/same':
            return self.send(200, json.dumps(release('v0.87.0')).encode())
        if p == '/noasset':
            return self.send(200, json.dumps(release('v0.88.0', False)).encode())
        if p == '/none':
            return self.send(404, b'{"message":"Not Found"}')
        if p.startswith('/dl/'):
            return self.send(302, b'', 'text/plain', {'Location': 'http://127.0.0.1:%d/blob/%s' % (PORT, p[4:])})
        if p.startswith('/blob/'):
            return self.send(200, BLOB, 'application/octet-stream')
        self.send(404, b'{}')


http.server.ThreadingHTTPServer(('127.0.0.1', PORT), H).serve_forever()
