const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const C = require('../core.js');

test('zero HR/SpO2 mean no reading; zero battery remains a real value', () => {
  const s = { hr_1: 0, spo2_1: 0, batt_1: 0 };
  assert.equal(C.metric(s, 'hr', 1), null);
  assert.equal(C.metric(s, 'spo2', 1), null);
  assert.equal(C.metric(s, 'batt', 1), 0);
  assert.equal(C.metric(s, 'batt', 2), null);
});
test('reject nonnumeric, nonfinite and out-of-range readings', () => {
  for (const value of ['78', null, true, NaN, Infinity, -1, 256]) assert.equal(C.metric({ hr_1: value }, 'hr', 1), null);
  assert.equal(C.metric({ spo2_1: 101 }, 'spo2', 1), null);
});
test('legacy values work only for a legacy single node, without cross-node leakage', () => {
  assert.deepEqual(C.nodeIds({ hr: 78 }), [1]);
  assert.equal(C.metric({ hr: 78 }, 'hr', 1), 78);
  assert.equal(C.metric({ hr: 78 }, 'hr', 2), null);
  assert.equal(C.metric({ hr: 78, hr_2: 90 }, 'hr', 1), null);
});
test('discover suffixes and explicit nodes and retain numeric order', () => {
  assert.deepEqual(C.nodeIds({ nodes: [10, 2, '3', 0], hr_1: 78, node_online_2: true }), [1, 2, 10]);
});
test('partial merges preserve omitted fields; explicit null erases a reading', () => {
  assert.deepEqual(C.mergeShadow({ hr_1: 78, batt_1: 64 }, { data: { hr_1: null } }), { hr_1: null, batt_1: 64 });
  assert.deepEqual(C.shadowData({ data: { batt_1: 0 } }), { batt_1: 0 }); // Full snapshot replaces old data in the app.
});
test('ignore unexpected fields and prototype pollution keys', () => {
  const result = C.shadowData(JSON.parse('{"data":{"__proto__":{"polluted":true},"hr_1":80,"token":"secret"}}'));
  assert.deepEqual(result, { hr_1: 80 });
  assert.equal({}.polluted, undefined);
  assert.throws(() => C.shadowData({ data: [] }));
});
test('gateway disconnected does not present cached node_online as live', () => {
  const now = 1800000000000, s = { node_online_1: true, node_last_seen_1: now / 1000 };
  assert.equal(C.nodeState(s, 1, true, now), 'online');
  assert.equal(C.nodeState(s, 1, false, now), 'unknown');
  assert.equal(C.nodeState(s, 1, null, now), 'unknown');
  assert.equal(C.nodeState(s, 1, true, now + 91000), 'stale');
  assert.equal(C.nodeState(s, 1, true, now - 301000), 'stale');
  assert.equal(C.nodeState({ node_online_1: true }, 1, true, now), 'unverified');
  assert.equal(C.nodeState({ node_online_1: false }, 1, true, now), 'offline');
});
test('validate schedules before writing, retaining valid midnight and end-of-day', () => {
  assert.deepEqual(C.schedule('23:59, 00:00, 08:00'), ['00:00', '08:00', '23:59']);
  for (const value of ['', '24:00', '08:60', '8:00', '08:00,08:00', [], null, [1]]) assert.throws(() => C.schedule(value));
});
test('deduplicate retries and flag conflicting IDs', () => {
  const log = new C.EventLog();
  assert.equal(log.add({ ev: 'sos', id: 1, d: { hr: 90, spo2: 98 } }), true);
  assert.equal(log.add({ d: { spo2: 98, hr: 90 }, id: 1, ev: 'sos' }), false);
  assert.equal(log.conflicts, 0);
  assert.equal(log.add({ ev: 'sos', id: 1, d: { hr: 100 } }), false);
  assert.equal(log.conflicts, 1);
  assert.equal(log.events.length, 1);
});
test('accept incomplete hardware-test events without inventing id or event time', () => {
  const log = new C.EventLog();
  assert.equal(log.add({ ev: 'sos', node: 1, hr: 90 }, 1234), true);
  assert.equal(log.add({ ev: 'sos', node: 1, hr: 90 }), false);
  assert.equal(log.events[0].ts, undefined);
  assert.equal(log.events[0].id, undefined);
  assert.equal(log.events[0].received_at, 1234);
});
test('bound observation history and reject malformed events', () => {
  const log = new C.EventLog();
  for (let i = 0; i < 550; i++) log.add({ id: i, ev: 'pill_taken' });
  assert.equal(log.events.length, 500); assert.equal(log.seen.size, 500);
  for (const event of [null, {}, [], { ev: 42 }]) assert.equal(log.add(event), false);
});
test('existing project mock fixture is readable', () => {
  const mock = JSON.parse(fs.readFileSync(path.join(__dirname, '../../mock-data.json'), 'utf8'));
  const s = C.shadowData(mock.shadow_update), log = new C.EventLog();
  assert.equal(C.metric(s, 'hr', 1), 78);
  mock.events.forEach(event => log.add(event));
  assert.equal(log.events.length, 10);
});
