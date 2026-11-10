#!/usr/bin/env python3
"""IMPAKT v87, RADIO: local stand-ins for what the plugin's receivers talk to (tools/plugin_test radio_test.cpp).

  http://127.0.0.1:PORT/stream.mp3   an internet station: an MP3 looped in real time (a burst first), ICY titles
  /redir                             a redirection to it; /playlist.pls a playlist holding it; /aac an AAC station
  /json/stations/search, /json/url/  the stations' directory (three stations, one of them HLS, one AAC); a click
  /public/                           the receivers' public list (chunked; one receiver offline)
  /stats                             what was asked (clicks, receivers' connections, frequencies)
  ws://127.0.0.1:PORT+1/<ts>/SND      a shortwave receiver: its WebSocket, IMA ADPCM at 12001.135 Hz, its tone 500 Hz
                                      under 10 MHz, 900 Hz above (the frequency comes with SET mod)
  ws://127.0.0.1:PORT+2/...           a receiver with every channel taken (too_busy)
Usage: radio_mock.py PORT MP3FILE
"""
import base64, hashlib, json, math, socket, struct, sys, threading, time

PORT = int(sys.argv[1])
MP3 = open(sys.argv[2], 'rb').read()
STATS = {'clicks': [], 'ws': 0, 'busy': 0, 'mods': [], 'streams': 0}
LOCK = threading.Lock()

def stat(k, v=None):
    with LOCK:
        if isinstance(STATS[k], list): STATS[k].append(v)
        else: STATS[k] += 1

def http_headers(conn):
    data = b''
    while b'\r\n\r\n' not in data:
        c = conn.recv(4096)
        if not c: return None, None
        data += c
    head = data.split(b'\r\n\r\n', 1)[0].decode('latin-1')
    lines = head.split('\r\n')
    method, path, _ = lines[0].split(' ', 2)
    h = {}
    for ln in lines[1:]:
        if ':' in ln:
            k, v = ln.split(':', 1); h[k.strip().lower()] = v.strip()
    return path, h

def send(conn, status, ctype, body, extra=''):
    conn.sendall(('HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n%s\r\n' % (status, ctype, len(body), extra)).encode() + body)

STATIONS = [
    {'stationuuid': 'u-1', 'name': 'Mock Jazz', 'url': 'http://x/', 'url_resolved': 'http://127.0.0.1:%d/stream.mp3' % PORT, 'codec': 'MP3', 'bitrate': 128,
     'countrycode': 'FR', 'country': 'France', 'tags': 'jazz,smooth', 'votes': 10, 'hls': 0, 'lastcheckok': 1},
    {'stationuuid': 'u-2', 'name': 'Mock HLS', 'url_resolved': 'http://127.0.0.1/x.m3u8', 'codec': 'MP3', 'bitrate': 64, 'hls': 1},
    {'stationuuid': 'u-3', 'name': 'Mock AAC', 'url_resolved': 'http://127.0.0.1/aac', 'codec': 'AAC+', 'bitrate': 64, 'hls': 0},
]

def kiwi_entry(name, url, users, mx, offline='no'):
    return ("<div class='cl-entry'>\n<div class='cl-info'>\n<!-- name=%s -->\n<!-- sdr_hw=KiwiSDR v1.700 -->\n<!-- gps=(48.85, 2.35) -->\n"
            "<!-- loc=Paris -->\n<!-- antenna=Long wire -->\n<!-- bands=0-30000000 -->\n<!-- users=%d -->\n<!-- users_max=%d -->\n"
            "<!-- offline=%s -->\n<!-- status=active -->\n<a href='%s' target='_blank'>%s</a>\n</div></div>\n") % (name, users, mx, offline, url, url)

def public_page():
    return ('<html><body><div class=\'id-text-container\'>\nUpdated now\n</div>\n'
            + kiwi_entry('Mock receiver &amp; antenna', 'http://127.0.0.1:%d' % (PORT + 1), 1, 4)
            + kiwi_entry('Mock full', 'http://127.0.0.1:%d' % (PORT + 2), 4, 4)
            + kiwi_entry('Mock offline', 'http://10.0.0.1:8073', 0, 4, 'yes')
            + kiwi_entry('Mock emptiest', 'http://127.0.0.2:8073', 0, 8)
            + '</body></html>').encode()

def stream(conn, h):
    stat('streams')
    meta = h.get('icy-metadata') == '1'
    metaint = 8192
    conn.sendall(('HTTP/1.0 200 OK\r\nContent-Type: audio/mpeg\r\nicy-name: Mock\r\n%s\r\n' % ('icy-metaint: %d\r\n' % metaint if meta else '')).encode())
    rate = 128000 / 8.0
    pos, sent, n, t0, song = 0, 0, 0, time.time(), 0
    burst = 64 * 1024
    toMeta = metaint
    while True:
        due = burst + (time.time() - t0) * rate
        if sent >= due: time.sleep(0.02); continue
        chunk = MP3[pos:pos + 1024]
        if len(chunk) < 1024: pos = 0; chunk = MP3[:1024]
        pos += 1024
        out = b''
        for b in chunk:
            out += bytes([b]); toMeta -= 1
            if meta and toMeta == 0:
                song += 1
                t = ("StreamTitle='Mock Song %d';" % (song // 3)).encode() if song % 3 == 1 else b''
                L = (len(t) + 15) // 16
                out += bytes([L]) + t.ljust(L * 16, b'\0')
                toMeta = metaint
        try: conn.sendall(out)
        except OSError: return
        sent += len(chunk)

def http_client(conn):
    try:
        path, h = http_headers(conn)
        if path is None: return
        if path.startswith('/stream.mp3'): stream(conn, h); return
        if path.startswith('/redir'): conn.sendall(b'HTTP/1.1 302 Found\r\nLocation: /stream.mp3\r\nContent-Length: 0\r\n\r\n'); return
        if path.startswith('/playlist.pls'): send(conn, '200 OK', 'audio/x-scpls', ('[playlist]\nNumberOfEntries=1\nFile1=http://127.0.0.1:%d/stream.mp3\n' % PORT).encode()); return
        if path.startswith('/aac'): conn.sendall(b'HTTP/1.0 200 OK\r\nContent-Type: audio/aacp\r\n\r\n' + b'\xff\xf1' * 1000); return
        if path.startswith('/json/stations/search'): send(conn, '200 OK', 'application/json', json.dumps(STATIONS).encode()); return
        if path.startswith('/json/url/'): stat('clicks', path[10:]); send(conn, '200 OK', 'application/json', b'{"ok":true}'); return
        if path.startswith('/stats'):
            with LOCK: send(conn, '200 OK', 'application/json', json.dumps(STATS).encode())
            return
        if path.startswith('/public'):
            body = public_page()
            conn.sendall(b'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nTransfer-Encoding: chunked\r\n\r\n')
            for i in range(0, len(body), 700):
                part = body[i:i + 700]
                conn.sendall(('%x\r\n' % len(part)).encode() + part + b'\r\n')
            conn.sendall(b'0\r\n\r\n'); return
        send(conn, '404 Not Found', 'text/plain', b'no')
    except OSError:
        pass
    finally:
        try: conn.close()
        except OSError: pass

# ---- the WebSocket side
def ws_accept(conn):
    path, h = http_headers(conn)
    if path is None: return None
    key = h.get('sec-websocket-key', '')
    acc = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
    conn.sendall(('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n' % acc).encode())
    return path

def ws_send(conn, payload, op=2):
    n = len(payload)
    hdr = bytes([0x80 | op]) + (bytes([n]) if n < 126 else bytes([126]) + struct.pack('>H', n))
    conn.sendall(hdr + payload)

def ws_recv(conn, buf):
    """one message from the client, or None (what is not complete stays in buf)"""
    while True:
        if len(buf) >= 2:
            n = buf[1] & 127; off = 2
            if n == 126:
                if len(buf) < 4: n = -1
                else: n = struct.unpack('>H', bytes(buf[2:4]))[0]; off = 4
            if n >= 0 and len(buf) >= off + 4 + n:
                mk = buf[off:off + 4]; p = bytes(b ^ mk[i & 3] for i, b in enumerate(buf[off + 4:off + 4 + n]))
                op = buf[0] & 15
                del buf[:off + 4 + n]
                return op, p
        try:
            c = conn.recv(4096)
        except socket.timeout:
            return None
        if not c: raise OSError('closed')
        buf += c

STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]
class Enc:
    def __init__(s): s.i = 0; s.p = 0
    def code(s, x):
        d = x - s.p; c = 0
        if d < 0: c = 8; d = -d
        st = STEP[s.i]; df = st >> 3
        if d >= st: c |= 4; d -= st; df += st
        if d >= st >> 1: c |= 2; d -= st >> 1; df += st >> 1
        if d >= st >> 2: c |= 1; df += st >> 2
        s.p = max(-32768, min(32767, s.p - df if c & 8 else s.p + df))
        s.i = max(0, min(88, s.i + IDX[c]))
        return c

def kiwi(conn):
    path = ws_accept(conn)
    if path is None: return
    stat('ws')
    conn.settimeout(0.01)
    buf = bytearray(); rate = 12001.135
    for m in ['client_public_ip=127.0.0.1', 'rx_chans=4', 'badp=0', 'version_maj=1', 'version_min=700', 'bandwidth=30000000', 'audio_init=0', 'audio_rate=12000']:
        ws_send(conn, ('MSG ' + m).encode())
    started, freq, ph, seq, enc, t0, sent = False, 0.0, 0.0, 0, Enc(), None, 0
    while True:
        r = ws_recv(conn, buf)
        if r is not None:
            op, p = r
            if op == 8: return
            t = p.decode('latin-1')
            if t.startswith('SET AR OK'): ws_send(conn, ('MSG sample_rate=%.3f' % rate).encode())
            if t.startswith('SET mod='):
                f = float(t.split('freq=')[1].split()[0]); stat('mods', f); freq = f
            if t.startswith('SET agc='): started = True; t0 = time.time()
            continue
        if not started: continue
        due = (time.time() - t0) * rate
        while sent + 512 <= due:
            hz = 500.0 if freq < 10000 else 900.0
            codes = []
            for i in range(512):
                codes.append(enc.code(int(10000 * math.sin(ph)))); ph += 2 * math.pi * hz / rate
            data = bytes(codes[i] | (codes[i + 1] << 4) for i in range(0, 512, 2))
            smeter = int((-73 + 127) * 10)
            ws_send(conn, b'SND' + struct.pack('<BI', 0x10, seq) + struct.pack('>H', smeter) + data)
            seq += 1; sent += 512

def busy(conn):
    if ws_accept(conn) is None: return
    stat('busy')
    ws_send(conn, b'MSG too_busy=4')
    time.sleep(0.2)
    conn.close()

def serve(port, fn):
    s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', port)); s.listen(16)
    while True:
        c, _ = s.accept()
        threading.Thread(target=lambda: _run(fn, c), daemon=True).start()

def _run(fn, c):
    try: fn(c)
    except OSError: pass
    finally:
        try: c.close()
        except OSError: pass

for p, f in ((PORT, http_client), (PORT + 1, kiwi), (PORT + 2, busy)):
    threading.Thread(target=serve, args=(p, f), daemon=True).start()
print('ready', flush=True)
while True: time.sleep(1)
