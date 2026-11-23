"""Regression checks for the offline checker, not for ESP32 firmware."""

from copy import deepcopy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import validate_payloads as checker

PART = Path(__file__).resolve().parents[2]
SCRIPT = PART / "test" / "tools" / "validate_payloads.py"
MOCK = PART / "dashboard" / "mock-data.json"


def record(payload, topic="@msg/spb/event", stamp="2026-11-21T08:00:00+07:00"):
    return json.dumps({"received_at": stamp, "topic": topic, "payload": payload})


class PayloadChecks(unittest.TestCase):
    def setUp(self):
        self.mock = checker.read_json(MOCK)
        self.event = deepcopy(self.mock["events"][0])

    def test_fixture_covers_all_agreed_events(self):
        checker.check_mock(self.mock)
        self.assertEqual(set(checker.EVENT_FIELDS), {event["ev"] for event in self.mock["events"]})

    def test_partial_shadow_and_zero_values_are_valid(self):
        result = checker.check_log([record({"data": {"hr": 0, "spo2": 0, "batt": 0}}, "@shadow/data/update")])
        self.assertEqual(result.shadows, 1)

    def test_full_fixture_requires_all_shadow_fields(self):
        del self.mock["shadow_update"]["data"]["queue"]
        with self.assertRaisesRegex(checker.ValidationError, "missing fields"):
            checker.check_mock(self.mock)

    def test_bool_is_not_numeric_sensor_data(self):
        with self.assertRaises(checker.ValidationError):
            checker.validate_shadow({"data": {"batt": True}})

    def test_percent_bounds(self):
        for value in (-1, 101, "20", None):
            with self.subTest(value=value), self.assertRaises(checker.ValidationError):
                checker.validate_shadow({"data": {"batt": value}})

    def test_missing_and_unknown_event_fields(self):
        for mutation in (lambda event: event["d"].pop("conf"),
                         lambda event: event["d"].update({"confidence": 91})):
            event = deepcopy(self.event)
            mutation(event)
            with self.assertRaises(checker.ValidationError):
                checker.validate_event(event)

    def test_bad_schedule_format(self):
        for value in (["24:00"], ["08:60"], ["8:00"], "08:00", [True]):
            with self.subTest(value=value), self.assertRaises(checker.ValidationError):
                checker.validate_shadow({"data": {"schedule": value}})

    def test_typo_in_shadow_is_rejected(self):
        with self.assertRaisesRegex(checker.ValidationError, "unknown fields"):
            checker.validate_shadow({"data": {"sp02": 97}})

    def test_identical_retry_is_counted_once(self):
        result = checker.check_log([record(self.event), record(self.event)])
        self.assertEqual((result.event_messages, len(result.events), result.duplicates), (2, 1, 1))

    def test_same_id_changed_payload_is_rejected(self):
        for field in ("ts", "d"):
            changed = deepcopy(self.event)
            if field == "ts":
                changed[field] += 1
            else:
                changed[field]["conf"] -= 1
            with self.subTest(field=field), self.assertRaisesRegex(checker.ValidationError, "conflicting"):
                checker.check_log([record(self.event), record(changed)])

    def test_backlog_does_not_have_to_arrive_in_timestamp_order(self):
        events = sorted(self.mock["events"], key=lambda event: event["ts"], reverse=True)
        result = checker.check_log([record(event) for event in events])
        result.check_expected(events)

    def test_missing_expected_event_fails(self):
        result = checker.check_log([record(self.event)])
        with self.assertRaisesRegex(checker.ValidationError, "missing ids"):
            result.check_expected(self.mock["events"])

    def test_changed_queued_event_fails(self):
        result = checker.check_log([record(self.event)])
        expected = deepcopy(self.event)
        expected["ts"] -= 1
        with self.assertRaisesRegex(checker.ValidationError, "changed from the queue"):
            result.check_expected([expected])

    def test_empty_logs_fail(self):
        for lines in ([], ["", "  "]):
            with self.subTest(lines=lines), self.assertRaisesRegex(checker.ValidationError, "no records"):
                checker.check_log(lines)

    def test_invalid_json_and_nonfinite_numbers_fail(self):
        for text in ('{"batt": NaN}', '{"x": Infinity}', '{"x": 1e999}', '{"x": 1, "x": 2}', '{'):
            with self.subTest(text=text):
                if text == '{"x": 1e999}':
                    with self.assertRaises(checker.ValidationError):
                        checker.validate_shadow({"data": {"temp": checker.parse_json(text)["x"]}})
                else:
                    with self.assertRaises(checker.ValidationError):
                        checker.parse_json(text)

    def test_wrong_topic_and_missing_timezone_fail_with_line_number(self):
        for line in (record(self.event, topic="unrelated"), record(self.event, stamp="2026-11-21T08:00:00")):
            with self.subTest(line=line), self.assertRaisesRegex(checker.ValidationError, "line 1"):
                checker.check_log([line])

    def test_string_payloads_are_supported(self):
        result = checker.check_log([record(json.dumps(self.event))])
        self.assertEqual(result.event_messages, 1)

    def test_cli_exit_codes(self):
        good = subprocess.run([sys.executable, "-B", str(SCRIPT), "--mock", str(MOCK)], capture_output=True, text=True)
        self.assertEqual(good.returncode, 0, good.stderr)
        with tempfile.TemporaryDirectory() as directory:
            invalid = Path(directory) / "invalid.jsonl"
            invalid.write_text(record(self.event, topic="wrong-topic"), encoding="utf-8")
            bad = subprocess.run([sys.executable, "-B", str(SCRIPT), "--log", str(invalid)], capture_output=True, text=True)
            self.assertEqual(bad.returncode, 1)
            missing = subprocess.run([sys.executable, "-B", str(SCRIPT), "--log", str(Path(directory) / "missing.jsonl")], capture_output=True, text=True)
            self.assertEqual(missing.returncode, 2)


if __name__ == "__main__":
    unittest.main()
