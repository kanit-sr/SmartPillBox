"""Offline checks for the agreed cloud payloads; this does not execute firmware.

Use one gateway and one boot/session per log. Only Python's standard library is
required. See ../code-testing.md for input formats and coverage limitations.
"""

import argparse
from datetime import datetime
import json
import math
from pathlib import Path
import re
import sys


class ValidationError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ValidationError(message)


def integer(value, maximum=None):
    return type(value) is int and value >= 0 and (maximum is None or value <= maximum)


def number(value):
    return type(value) is int or (type(value) is float and math.isfinite(value))


def clock_time(value):
    return isinstance(value, str) and re.fullmatch(r"(?:[01][0-9]|2[0-3]):[0-5][0-9]", value) is not None


def schedule(value):
    # Empty/duplicate slot policy is not yet defined in the system specification.
    return isinstance(value, list) and all(clock_time(slot) for slot in value)


def choice(*values):
    return lambda value: isinstance(value, str) and value in values


PERCENT = lambda value: integer(value, 100)
HEART_RATE = lambda value: integer(value, 255)
HUMIDITY = lambda value: number(value) and 0 <= value <= 100
BOOLEAN = lambda value: type(value) is bool
SHADOW_FIELDS = {
    "hr": HEART_RATE,
    "spo2": PERCENT,
    "batt": PERCENT,
    "node_online": BOOLEAN,
    "node_last_seen": integer,
    "temp": number,
    "hum": HUMIDITY,
    "lid_open": BOOLEAN,
    "person": BOOLEAN,
    "box_state": choice("idle", "pill_remind", "pill_escalate", "fall_alarm", "storage_warn"),
    "next_pill": lambda value: isinstance(value, str),
    "last_pill_taken": integer,
    "queue": integer,
    "schedule": schedule,
}
EVENT_FIELDS = {
    "fall_confirmed": {"hr": HEART_RATE, "spo2": PERCENT, "conf": PERCENT},
    "fall_cancelled": {"conf": PERCENT},
    "sos": {"hr": HEART_RATE, "spo2": PERCENT},
    "pill_taken": {"slot": clock_time, "late_min": lambda value: number(value) and value >= 0},
    "pill_missed": {"slot": clock_time},
    "fall_acknowledged": {"by": choice("button", "dashboard"), "after_s": integer},
    "low_battery": {"batt": PERCENT},
    "node_offline": {"last_seen": integer},
    "node_power_off": {"batt": PERCENT},
    "storage_warn": {"temp": number, "hum": HUMIDITY},
}


def fields(data, rules, location, complete=True):
    require(isinstance(data, dict), f"{location}: expected an object")
    unknown = data.keys() - rules.keys()
    require(not unknown, f"{location}: unknown fields {sorted(unknown)}")
    if complete:
        missing = rules.keys() - data.keys()
        require(not missing, f"{location}: missing fields {sorted(missing)}")
    for key, value in data.items():
        require(rules[key](value), f"{location}.{key}: invalid type, format or range")


def validate_shadow(update, complete=False):
    require(isinstance(update, dict) and "data" in update, "shadow: expected {data: {...}}")
    fields(update["data"], SHADOW_FIELDS, "shadow.data", complete=complete)
    require(bool(update["data"]), "shadow.data: no fields to check")


def validate_event(event):
    fields(event, {"ev": choice(*EVENT_FIELDS), "ts": integer, "id": integer,
                   "d": lambda value: isinstance(value, dict)}, "event")
    fields(event["d"], EVENT_FIELDS[event["ev"]], f"event.{event['ev']}.d")


def reject_constant(value):
    raise ValidationError(f"non-standard JSON number: {value}")


def unique_keys(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_json(text):
    try:
        return json.loads(text, parse_constant=reject_constant, object_pairs_hook=unique_keys)
    except json.JSONDecodeError as exc:
        raise ValidationError(f"invalid JSON: {exc.msg} at column {exc.colno}") from exc
    except ValueError as exc:
        raise ValidationError(f"invalid JSON: {exc}") from exc


def read_json(path):
    return parse_json(Path(path).read_text(encoding="utf-8-sig"))


class DataCheck:
    def __init__(self):
        self.shadows = 0
        self.event_messages = 0
        self.duplicates = 0
        self.events = {}

    def add_event(self, event):
        validate_event(event)
        event_id = event["id"]
        if event_id in self.events:
            require(self.events[event_id] == event,
                    f"event id {event_id}: conflicting payload; check ts/ev/d or split boot sessions")
            self.duplicates += 1
        else:
            self.events[event_id] = event
        self.event_messages += 1

    def check_expected(self, expected):
        require(isinstance(expected, list) and expected, "expected-events: expected a nonempty array")
        reference = DataCheck()
        for event in expected:
            reference.add_event(event)
        missing = reference.events.keys() - self.events.keys()
        require(not missing, f"expected-events: missing ids {sorted(missing)}")
        for event_id, event in reference.events.items():
            require(self.events[event_id] == event,
                    f"expected-events: id {event_id} changed from the queue record")


def check_mock(document):
    require(isinstance(document, dict), "mock: expected an object")
    require("shadow_update" in document and "events" in document, "mock: missing shadow_update/events")
    validate_shadow(document["shadow_update"], complete=True)
    require(isinstance(document["events"], list) and document["events"], "mock.events: expected a nonempty array")
    result = DataCheck()
    result.shadows = 1
    for event in document["events"]:
        result.add_event(event)
    return result


def check_log(lines):
    result = DataCheck()
    for line_number, line in enumerate(lines, 1):
        if not line.strip():
            continue
        try:
            record = parse_json(line)
            require(isinstance(record, dict), "log record: expected an object")
            require(all(key in record for key in ("received_at", "topic", "payload")),
                    "log record: missing received_at/topic/payload")
            stamp = record["received_at"]
            require(isinstance(stamp, str), "received_at: expected ISO 8601 with timezone")
            try:
                received = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
            except ValueError as exc:
                raise ValidationError("received_at: invalid ISO 8601 timestamp") from exc
            require(received.tzinfo is not None, "received_at: timezone is required")
            payload = record["payload"]
            if isinstance(payload, str):
                payload = parse_json(payload)
            if record["topic"] == "@msg/spb/event":
                result.add_event(payload)
            elif record["topic"] == "@shadow/data/update":
                validate_shadow(payload)
                result.shadows += 1
            else:
                raise ValidationError("unsupported topic; select event and shadow-update records only")
        except ValidationError as exc:
            raise ValidationError(f"line {line_number}: {exc}") from exc
    require(result.shadows + result.event_messages > 0, "log: no records to check")
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--mock", type=Path, help="Full shadow + event fixture JSON")
    source.add_argument("--log", type=Path, help="Normalized MQTT JSONL, one gateway/boot per file")
    parser.add_argument("--expected-events", type=Path, help="JSON array captured independently from the queue")
    args = parser.parse_args(argv)
    if args.expected_events and not args.log:
        parser.error("--expected-events requires --log")
    try:
        if args.mock:
            result = check_mock(read_json(args.mock))
        else:
            with args.log.open(encoding="utf-8-sig") as stream:
                result = check_log(stream)
        if args.expected_events:
            result.check_expected(read_json(args.expected_events))
    except (OSError, UnicodeError) as exc:
        print(f"INPUT ERROR: {exc}", file=sys.stderr)
        return 2
    except ValidationError as exc:
        print(f"DATA CHECK FAILED: {exc}", file=sys.stderr)
        return 1
    print(f"DATA CHECK OK: shadows={result.shadows}, event_messages={result.event_messages}, "
          f"unique_events={len(result.events)}, duplicates={result.duplicates}")
    if not args.expected_events:
        print("Missing events were not checked: no independent expected-event list supplied.")
    print("This checks data only; it does not establish a firmware or hardware PASS.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
