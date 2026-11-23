/* Data helpers shared by the browser and Node's built-in test runner. */
(function (root) {
  'use strict';
  const own = (o, k) => Object.prototype.hasOwnProperty.call(o, k);
  const object = v => v !== null && typeof v === 'object' && !Array.isArray(v);
  const fieldPattern = /^(hr|spo2|batt|node_online|node_last_seen)(_[1-9]\d*)?$/;
  const boxFields = new Set(['nodes', 'temp', 'hum', 'lid_open', 'person', 'box_state', 'alarm_node', 'next_pill', 'last_pill_taken', 'queue', 'schedule', 'last_event']);
  function shadowData(payload) {
    const data = object(payload) && own(payload, 'data') ? payload.data : payload;
    if (!object(data)) throw new Error('รูปแบบ Shadow ไม่ถูกต้อง');
    return Object.fromEntries(Object.entries(data).filter(([k]) => fieldPattern.test(k) || boxFields.has(k)));
  }
  function mergeShadow(previous, payload) { return { ...previous, ...shadowData(payload) }; }
  function nodeIds(data) {
    const ids = new Set(Array.isArray(data.nodes) ? data.nodes.filter(n => Number.isInteger(n) && n > 0) : []);
    for (const key of Object.keys(data)) {
      if (fieldPattern.test(key) && /_\d+$/.test(key)) ids.add(Number(key.split('_').at(-1)));
    }
    if (!ids.size && ['hr', 'spo2', 'batt', 'node_online'].some(k => own(data, k))) ids.add(1);
    return [...ids].sort((a, b) => a - b);
  }
  function nodeValue(data, field, node) {
    if (own(data, `${field}_${node}`)) return data[`${field}_${node}`];
    // Legacy single-node values must never leak into another registered node.
    if (node === 1 && !Object.keys(data).some(k => fieldPattern.test(k) && /_\d+$/.test(k))) return data[field];
    return undefined;
  }
  function numeric(value, min = -Infinity, max = Infinity) {
    return typeof value === 'number' && Number.isFinite(value) && value >= min && value <= max ? value : null;
  }
  function metric(data, field, node) {
    const value = nodeValue(data, field, node);
    return numeric(value, field === 'batt' ? 0 : 1, field === 'hr' ? 255 : 100);
  }
  function nodeState(data, node, gateway, now = Date.now()) {
    if (gateway !== true) return 'unknown';
    const online = nodeValue(data, 'node_online', node);
    if (online === false) return 'offline';
    const seen = numeric(nodeValue(data, 'node_last_seen', node), 1);
    if (seen !== null && (now - seen * 1000 > 90000 || seen * 1000 - now > 300000)) return 'stale';
    return online === true ? (seen === null ? 'unverified' : 'online') : 'unknown';
  }
  function schedule(value) {
    const times = Array.isArray(value) ? value : typeof value === 'string' ? value.split(',').map(t => t.trim()) : [];
    if (!times.length || times.some(t => typeof t !== 'string' || !/^([01]\d|2[0-3]):[0-5]\d$/.test(t))) throw new Error('ใส่เวลา 00:00–23:59 เช่น 08:00, 12:00, 19:00');
    if (new Set(times).size !== times.length) throw new Error('มีเวลาซ้ำ กรุณาใช้แต่ละเวลาเพียงครั้งเดียว');
    return [...times].sort();
  }
  function canonical(value) {
    if (Array.isArray(value)) return '[' + value.map(canonical).join(',') + ']';
    if (object(value)) return '{' + Object.keys(value).sort().map(k => JSON.stringify(k) + ':' + canonical(value[k])).join(',') + '}';
    return JSON.stringify(value);
  }
  class EventLog {
    constructor() { this.events = []; this.seen = new Map(); this.conflicts = 0; }
    add(event, receivedAt = Date.now()) {
      if (!object(event) || typeof event.ev !== 'string' || !event.ev.length) return false;
      const signature = canonical(event);
      const key = Number.isInteger(event.id) ? `id:${event.id}` : `raw:${signature}`;
      if (this.seen.has(key)) {
        if (this.seen.get(key) !== signature) this.conflicts++;
        return false;
      }
      this.seen.set(key, signature);
      this.events.unshift({ ...event, received_at: receivedAt });
      if (this.events.length > 500) { this.events.pop(); this.seen.delete(this.seen.keys().next().value); }
      return true;
    }
  }
  const api = { shadowData, mergeShadow, nodeIds, nodeValue, numeric, metric, nodeState, schedule, EventLog };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.SPB = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
