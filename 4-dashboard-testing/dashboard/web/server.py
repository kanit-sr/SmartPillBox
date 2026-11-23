"""Local-only static server and fixed-destination NETPIE REST bridge (Python 3.10+).

Run from anywhere: python path/to/web/server.py
Credentials remain in server memory for at most eight hours, or until disconnect/exit.
"""
import argparse
import json
import re
import secrets
import threading
import time
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, build_opener, HTTPRedirectHandler

ROOT = Path(__file__).resolve().parent
NETPIE = "https://api.netpie.io/v2/device"
ASSETS = {"/": ("index.html", "text/html"), "/index.html": ("index.html", "text/html"),
          "/styles.css": ("styles.css", "text/css"), "/core.js": ("core.js", "text/javascript"),
          "/app.js": ("app.js", "text/javascript")}
SESSIONS = {}
LOCK = threading.Lock()
MAX_SESSION_AGE = 8 * 3600


class APIError(Exception):
    def __init__(self, message, status=502):
        super().__init__(message)
        self.status = status


class NoRedirect(HTTPRedirectHandler):
    # Never forward an Authorization header to a redirected destination.
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def netpie(credentials, path, method="GET", body=None, raw=False):
    encoded = None if body is None else json.dumps(body).encode("utf-8")
    request = Request(NETPIE + path, data=encoded, method=method, headers={
        "Authorization": "Device " + credentials[0] + ":" + credentials[1],
        "Accept": "application/json", "Content-Type": "text/plain" if raw else "application/json",
    })
    try:
        with build_opener(NoRedirect()).open(request, timeout=8) as response:
            data = response.read(2_000_001)
            if len(data) > 2_000_000:
                raise APIError("NETPIE response is too large")
            result = json.loads(data)
            if not isinstance(result, dict):
                raise APIError("Invalid NETPIE response")
            # Device APIs may include an HTTP-like status in a JSON envelope.
            status = result.get("status")
            if isinstance(status, int) and status >= 400:
                raise APIError(f"NETPIE returned status {status}")
            return result
    except HTTPError as error:
        explanations = {401: "ตรวจ Client ID และ Token", 403: "ตรวจสิทธิ์ของ Device", 429: "โควตาหรืออัตราเรียก API เต็ม ลองเพิ่มช่วงเวลาอ่าน"}
        raise APIError(f"NETPIE HTTP {error.code}: {explanations.get(error.code, 'คำขอไม่สำเร็จ')}") from None
    except (URLError, TimeoutError, OSError):
        raise APIError("ติดต่อ NETPIE ไม่สำเร็จ ตรวจอินเทอร์เน็ตแล้วลองใหม่") from None
    except (ValueError, UnicodeError):
        raise APIError("NETPIE ไม่ได้ส่งข้อมูล JSON ที่อ่านได้") from None


def snapshot(credentials):
    shadow = netpie(credentials, "/shadow/data")
    status = netpie(credentials, "/status")
    if not isinstance(shadow.get("data"), dict):
        raise APIError("Shadow response ไม่มี data object")
    online = status.get("status")
    return {"shadow": {"data": shadow["data"]}, "gateway_online": True if online == 1 else False if online == 0 else None}


def validate_schedule(value):
    if not isinstance(value, list) or not value or any(not isinstance(t, str) or not re.fullmatch(r"([01]\d|2[0-3]):[0-5]\d", t) for t in value):
        raise APIError("ตารางยาต้องมีเวลา HH:MM ระหว่าง 00:00–23:59", 400)
    if len(set(value)) != len(value):
        raise APIError("เวลาตารางยาซ้ำกัน", 400)
    return sorted(value)


class LocalServer(ThreadingHTTPServer):
    def service_actions(self):
        with LOCK:
            for key, (_, expires) in list(SESSIONS.items()):
                if expires <= time.time():
                    del SESSIONS[key]


class Handler(BaseHTTPRequestHandler):
    server_version = "SmartPillBox/1.0"

    def send(self, status, value, content_type="application/json", cookie=None):
        data = value if isinstance(value, bytes) else json.dumps(value, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type + "; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; connect-src 'self'; img-src 'self' data:; object-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'")
        if cookie:
            self.send_header("Set-Cookie", cookie)
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def host_allowed(self):
        return self.headers.get("Host") in {f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"}

    def session_id(self):
        cookie = SimpleCookie()
        try:
            cookie.load(self.headers.get("Cookie", ""))
            return cookie["spb_session"].value if "spb_session" in cookie else None
        except Exception:
            return None

    def credentials(self):
        sid = self.session_id()
        with LOCK:
            # Expire all sessions, including abandoned browser tabs.
            for key, (_, expires) in list(SESSIONS.items()):
                if expires <= time.time():
                    del SESSIONS[key]
            session = SESSIONS.get(sid)
        if session is None:
            raise APIError("Session หมดอายุ กรุณาเชื่อมต่อ NETPIE อีกครั้ง", 401)
        return session[0]

    def do_GET(self):
        if not self.host_allowed():
            self.send(403, {"error": "Local host only"})
            return
        if self.path in ASSETS:
            name, mime = ASSETS[self.path]
            self.send(200, (ROOT / name).read_bytes(), mime)
        elif self.path == "/api/snapshot":
            try:
                self.send(200, snapshot(self.credentials()))
            except APIError as error:
                self.send(error.status, {"error": str(error)})
        else:
            self.send(404, {"error": "Not found"})

    def do_POST(self):
        origin = self.headers.get("Origin")
        if not self.host_allowed() or origin != "http://" + self.headers.get("Host", ""):
            self.send(403, {"error": "Same-origin local requests only"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length < 1 or length > 4096 or self.headers.get_content_type() != "application/json":
                raise APIError("Invalid request body", 400)
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, dict):
                raise APIError("Expected a JSON object", 400)
            if self.path == "/api/connect":
                values = (body.get("client_id"), body.get("token"))
                if any(not isinstance(v, str) or not re.fullmatch(r"[A-Za-z0-9_.-]{1,256}", v) for v in values):
                    raise APIError("Client ID หรือ Token ไม่ถูกต้อง", 400)
                data = snapshot(values)  # Do not replace an existing working session on failure.
                sid = secrets.token_urlsafe(32)
                with LOCK:
                    SESSIONS.pop(self.session_id(), None)
                    for key, (_, expires) in list(SESSIONS.items()):
                        if expires <= time.time():
                            del SESSIONS[key]
                    SESSIONS[sid] = (values, time.time() + MAX_SESSION_AGE)
                self.send(200, data, cookie=f"spb_session={sid}; HttpOnly; SameSite=Strict; Path=/api; Max-Age={MAX_SESSION_AGE}")
            elif self.path == "/api/disconnect":
                with LOCK:
                    SESSIONS.pop(self.session_id(), None)
                self.send(200, {"ok": True}, cookie="spb_session=; HttpOnly; SameSite=Strict; Path=/api; Max-Age=0")
            elif self.path == "/api/schedule":
                times = validate_schedule(body.get("schedule"))
                netpie(self.credentials(), "/shadow/data", "PUT", {"data": {"schedule": times}})
                self.send(200, {"accepted": True})
            elif self.path == "/api/command":
                if body != {"cmd": "clear_alarm"}:
                    raise APIError("Unsupported command", 400)
                netpie(self.credentials(), "/message/spb/cmd", "PUT", body, raw=True)
                self.send(200, {"accepted": True, "device_confirmed": False})
            else:
                self.send(404, {"error": "Not found"})
        except APIError as error:
            self.send(error.status, {"error": str(error)})
        except (ValueError, UnicodeError):
            self.send(400, {"error": "Invalid JSON body"})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    server = LocalServer(("127.0.0.1", args.port), Handler)
    print(f"SmartPillBox dashboard: http://127.0.0.1:{server.server_port}", flush=True)
    print("Local access only. Press Ctrl+C to stop and discard credentials.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        SESSIONS.clear()


if __name__ == "__main__":
    main()
