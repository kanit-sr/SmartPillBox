"""Run with python -B -m unittest discover -s .../web/tests -p 'test_*.py' -v.
All NETPIE calls are mocked; these are NOT device or cloud integration tests.
"""
import http.client
import importlib.util
import json
from pathlib import Path
import threading
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("dashboard_server", Path(__file__).resolve().parents[1] / "server.py")
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)


class ServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.httpd = server.LocalServer(("127.0.0.1", 0), server.Handler)
        cls.thread = threading.Thread(target=cls.httpd.serve_forever, daemon=True)
        cls.thread.start()
        cls.port = cls.httpd.server_port

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()
        cls.thread.join()

    def setUp(self):
        server.SESSIONS.clear()
        self.mock = patch.object(server, "netpie", side_effect=lambda creds, path, *args, **kwargs: {"data": {"hr_1": 80}} if path == "/shadow/data" else {"status": 1})
        self.netpie = self.mock.start()
        self.addCleanup(self.mock.stop)

    def request(self, method, path, body=None, cookie=None, origin=None, host=None):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        headers = {"Host": host or f"127.0.0.1:{self.port}"}
        if cookie:
            headers["Cookie"] = cookie
        if body is not None:
            headers["Content-Type"] = "application/json"
            headers["Origin"] = origin or f"http://127.0.0.1:{self.port}"
        conn.request(method, path, json.dumps(body) if body is not None else None, headers)
        response = conn.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        conn.close()
        return result

    def connect(self):
        status, headers, data = self.request("POST", "/api/connect", {"client_id": "test-client", "token": "test-token"})
        self.assertEqual(status, 200)
        self.assertNotIn(b"test-token", data)
        self.assertIn("HttpOnly", headers["Set-Cookie"])
        return headers["Set-Cookie"].split(";")[0]

    def test_static_assets_and_no_source_or_path_traversal(self):
        for path in ["/", "/styles.css", "/app.js", "/core.js"]:
            self.assertEqual(self.request("GET", path)[0], 200)
        for path in ["/server.py", "/../mock-data.json", "/api/credentials"]:
            self.assertEqual(self.request("GET", path)[0], 404)

    def test_connect_and_read_without_credentials_in_response(self):
        cookie = self.connect()
        status, _, raw = self.request("GET", "/api/snapshot", cookie=cookie)
        self.assertEqual(status, 200)
        self.assertTrue(json.loads(raw)["gateway_online"])
        self.assertEqual(json.loads(raw)["shadow"]["data"]["hr_1"], 80)

    def test_snapshot_requires_session(self):
        self.assertEqual(self.request("GET", "/api/snapshot")[0], 401)
        self.netpie.assert_not_called()

    def test_reject_foreign_origin_and_dns_rebinding_host(self):
        self.assertEqual(self.request("POST", "/api/connect", {}, origin="https://other.example")[0], 403)
        self.assertEqual(self.request("GET", "/", host=f"other.example:{self.port}")[0], 403)
        self.netpie.assert_not_called()

    def test_schedule_put_is_merge_and_only_changes_schedule(self):
        cookie = self.connect()
        self.assertEqual(self.request("POST", "/api/schedule", {"schedule": ["19:00", "08:00"]}, cookie)[0], 200)
        self.netpie.assert_called_with(("test-client", "test-token"), "/shadow/data", "PUT", {"data": {"schedule": ["08:00", "19:00"]}})

    def test_invalid_schedules_do_not_touch_cloud(self):
        cookie = self.connect()
        self.netpie.reset_mock()
        for times in [[], ["24:00"], ["08:00", "08:00"], [None], "08:00"]:
            self.assertEqual(self.request("POST", "/api/schedule", {"schedule": times}, cookie)[0], 400)
        self.netpie.assert_not_called()

    def test_command_uses_rest_topic_without_mqtt_prefix(self):
        cookie = self.connect()
        status, _, raw = self.request("POST", "/api/command", {"cmd": "clear_alarm"}, cookie)
        self.assertEqual(status, 200)
        self.assertFalse(json.loads(raw)["device_confirmed"])
        self.netpie.assert_called_with(("test-client", "test-token"), "/message/spb/cmd", "PUT", {"cmd": "clear_alarm"}, raw=True)
        self.assertEqual(self.request("POST", "/api/command", {"cmd": "anything-else"}, cookie)[0], 400)

    def test_disconnect_destroys_session(self):
        cookie = self.connect()
        self.assertEqual(self.request("POST", "/api/disconnect", {}, cookie)[0], 200)
        self.assertEqual(self.request("GET", "/api/snapshot", cookie=cookie)[0], 401)
        self.assertFalse(server.SESSIONS)

    def test_errors_do_not_fallback_to_fabricated_success(self):
        self.netpie.side_effect = server.APIError("NETPIE HTTP 401")
        status, _, raw = self.request("POST", "/api/connect", {"client_id": "test-client", "token": "test-token"})
        self.assertEqual(status, 502)
        self.assertIn("401", json.loads(raw)["error"])
        self.assertFalse(server.SESSIONS)

    def test_expired_credentials_are_removed(self):
        server.SESSIONS["expired"] = (("test-client", "test-token"), 0)
        self.httpd.service_actions()
        self.assertNotIn("expired", server.SESSIONS)


if __name__ == "__main__":
    unittest.main()
