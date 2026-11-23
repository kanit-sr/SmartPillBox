/* No remote scripts, credentials in browser storage, or build step. */
(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const C = window.SPB;
  const dateFormat = new Intl.DateTimeFormat('th-TH', { timeZone: 'Asia/Bangkok', day: 'numeric', month: 'short', hour: '2-digit', minute: '2-digit' });
  const clockFormat = new Intl.DateTimeFormat('th-TH', { timeZone: 'Asia/Bangkok', hour: '2-digit', minute: '2-digit', second: '2-digit' });
  const stateNames = { idle: 'กล่องยาพร้อมทำงาน', pill_remind: 'ถึงเวลาเตือนรับยา', pill_escalate: 'ยังไม่ได้รับยาตามเวลาที่กำหนด', fall_alarm: 'แจ้งเตือนการล้ม — กรุณาตรวจสอบ', storage_warn: 'กล่องแจ้งเตือนสภาพการเก็บยา', sos: 'มีการขอความช่วยเหลือ SOS' };
  const eventNames = { fall_confirmed: 'ตรวจพบการล้ม', fall_acknowledged: 'มีผู้รับเหตุล้มแล้ว', fall_cancelled: 'ยกเลิกเหตุล้ม', sos: 'ขอความช่วยเหลือ SOS', pill_taken: 'รับยาแล้ว', pill_missed: 'ยังไม่ได้รับยา', low_battery: 'แบตเตอรี่ต่ำ', node_offline: 'กำไลขาดการติดต่อ', node_power_off: 'กำไลปิดเครื่อง', storage_warn: 'สภาพแวดล้อมกล่องมีการแจ้งเตือน' };
  const alerts = new Set(['fall_confirmed', 'sos', 'pill_missed', 'low_battery', 'node_offline', 'storage_warn']);
  let mode = 'demo', shadow = {}, selected = 1, gateway = null, lastRead = 0, pollMs = 15000;
  let log = new C.EventLog(), history = new Map(), fingerprints = new Map();
  let timer, generation = 0, pendingAlarm = null, busy = false, demoTick = 0;
  const text = (id, value) => { $(id).textContent = value; };
  const showNotice = message => { text('notice', message); $('notice').hidden = !message; };
  const valueText = value => value === null || value === undefined ? '—' : String(value);
  const when = seconds => C.numeric(seconds, 1, 8640000000000) === null ? 'ยังไม่มีเวลาเกิดเหตุ' : dateFormat.format(new Date(seconds * 1000));
  function resetData() {
    shadow = {}; gateway = null; lastRead = 0; selected = 1; pendingAlarm = null;
    log = new C.EventLog(); history = new Map(); fingerprints = new Map();
    text('schedule-feedback', '');
  }
  async function api(path, body) {
    const response = await fetch('/api/' + path, {
      method: body === undefined ? 'GET' : 'POST', credentials: 'same-origin',
      headers: body === undefined ? {} : { 'Content-Type': 'application/json' },
      body: body === undefined ? undefined : JSON.stringify(body), signal: AbortSignal.timeout(25000)
    });
    const result = await response.json();
    if (!response.ok) throw new Error(result.error || 'ตัวรันในเครื่องตอบกลับผิดพลาด');
    return result;
  }
  function addSamples(now) {
    for (const node of C.nodeIds(shadow)) {
      const seen = C.nodeValue(shadow, 'node_last_seen', node);
      const hr = C.metric(shadow, 'hr', node);
      const state = C.nodeState(shadow, node, gateway, now);
      // Without source timestamps, only a changed reading yields a new sample.
      const fingerprint = `${seen ?? 'no-time'}:${hr}:${state}`;
      if (fingerprints.get(node) === fingerprint) continue;
      fingerprints.set(node, fingerprint);
      const samples = history.get(node) || [];
      samples.push({ t: now, value: ['offline', 'stale', 'unknown'].includes(state) ? null : hr });
      history.set(node, samples.slice(-120));
    }
  }
  function accept(data) {
    shadow = C.shadowData(data.shadow); gateway = data.gateway_online;
    lastRead = Date.now();
    const newEvent = log.add(shadow.last_event);
    if (pendingAlarm && newEvent && shadow.last_event.ev === 'fall_acknowledged' && shadow.last_event.d?.by === 'dashboard' && shadow.last_event.node === pendingAlarm.node && shadow.last_event.ts * 1000 >= pendingAlarm.at - 1000) {
      showNotice('ได้รับ fall_acknowledged จากกล่องแล้ว ตรวจเสียงและไฟที่อุปกรณ์เพื่อยืนยันผลจริง');
      pendingAlarm = null;
    }
    addSamples(lastRead); render();
  }
  function render() {
    const demo = mode === 'demo';
    text('source-badge', demo ? 'DEMO · ข้อมูลจำลอง' : 'NETPIE · ข้อมูลจากอุปกรณ์');
    $('source-badge').className = 'badge' + (demo ? ' amber' : '');
    text('connection-label', demo ? 'ตัวเลขและเหตุการณ์ทั้งหมดเป็นตัวอย่าง' : gateway === null ? 'อ่านข้อมูลไม่สำเร็จ · ค่าบนหน้าจออาจเก่า' : `อ่านข้อมูลทุก ${pollMs / 1000} วินาที`);
    $('refresh').hidden = demo; $('demo-button').hidden = demo;
    text('connect-top-label', demo ? 'เชื่อมต่อ NETPIE' : 'เปลี่ยนอุปกรณ์');
    const ids = C.nodeIds(shadow);
    if (!ids.includes(selected)) selected = ids[0] || 1;
    $('node-select').replaceChildren(...(ids.length ? ids : [1]).map(id => new Option(`กำไล #${id}`, id)));
    $('node-select').value = String(selected);
    const state = C.nodeState(shadow, selected, gateway);
    const labels = { online: 'ออนไลน์', offline: 'ขาดการติดต่อ', stale: 'ข้อมูลเก่า', unverified: 'รายงานว่าออนไลน์', unknown: 'ยังยืนยันไม่ได้' };
    text('node-status', labels[state]);
    text('node-caption', `กำไล #${selected} · ${['online', 'unverified'].includes(state) ? 'ค่าล่าสุดที่อุปกรณ์รายงาน' : 'ค่าที่แสดงอาจเป็นข้อมูลเก่า โปรดตรวจการเชื่อมต่อ'}`);
    const seen = C.nodeValue(shadow, 'node_last_seen', selected);
    text('last-seen', C.numeric(seen, 1) === null ? 'ไม่มีเวลาติดต่อล่าสุดจากอุปกรณ์' : `ติดต่อล่าสุด ${when(seen)}`);
    for (const field of ['hr', 'spo2', 'batt']) text(field, valueText(C.metric(shadow, field, selected)));
    text('hr-note', C.metric(shadow, 'hr', selected) === null ? 'ไม่มีข้อมูลการวัด' : 'ค่าล่าสุดที่อ่านพบ');
    text('spo2-note', C.metric(shadow, 'spo2', selected) === null ? 'ไม่มีข้อมูลการวัด' : 'ค่าล่าสุดที่อ่านพบ');
    const battery = C.metric(shadow, 'batt', selected);
    $('battery-fill').style.width = `${battery ?? 0}%`;
    text('battery-note', battery === null ? 'ไม่มีข้อมูลแบตเตอรี่' : battery <= 20 ? 'แบตเตอรี่เหลือน้อย' : 'แบตเตอรี่คงเหลือ');
    text('gateway-status', `กล่องยา: ${gateway === true ? 'เชื่อม NETPIE' : gateway === false ? 'ออฟไลน์' : 'ยังยืนยันไม่ได้'}`);
    const boxState = shadow.box_state;
    text('state-title', stateNames[boxState] || 'ยังไม่มีสถานะกล่องยา');
    text('state-description', (demo ? 'สถานการณ์จำลอง · ' : gateway !== true ? 'สถานะที่บันทึกไว้ อาจไม่ใช่ปัจจุบัน · ' : '') + (shadow.alarm_node > 0 ? `เหตุจากกำไล #${shadow.alarm_node}` : 'สถานะที่กล่องรายงาน ไม่ใช่ผลประเมินสุขภาพ'));
    $('state-banner').className = 'status-banner' + (['fall_alarm', 'sos', 'storage_warn', 'pill_escalate'].includes(boxState) ? ' alert' : '');
    $('clear-alarm').disabled = !!pendingAlarm || gateway !== true || !['fall_alarm', 'sos'].includes(boxState);
    text('clear-alarm', pendingAlarm ? 'ส่งแล้ว · รอยืนยันจากกล่อง' : 'รับเหตุ / เคลียร์ alarm');
    text('temp', valueText(C.numeric(shadow.temp, -40, 125)));
    text('hum', valueText(C.numeric(shadow.hum, 0, 100)));
    text('lid', shadow.lid_open === true ? 'เปิดอยู่' : shadow.lid_open === false ? 'ปิดอยู่' : 'ไม่มีข้อมูล');
    text('person', shadow.person === true ? 'พบคน' : shadow.person === false ? 'ไม่พบคน' : 'ไม่มีข้อมูล');
    text('queue', C.numeric(shadow.queue, 0) === null ? 'ไม่มีข้อมูล' : `${shadow.queue} รายการ`);
    text('box-freshness', 'เป็นค่าล่าสุดใน Shadow; ไม่มีเวลาวัดแยกสำหรับเซนเซอร์กล่อง');
    const times = Array.isArray(shadow.schedule) ? shadow.schedule.filter(t => typeof t === 'string') : [];
    text('next-pill', typeof shadow.next_pill === 'string' ? shadow.next_pill : '—');
    text('schedule-count', `${times.length} มื้อ`);
    $('schedule-list').replaceChildren(...times.map(time => {
      const el = document.createElement('span'); el.className = 'schedule-slot' + (time === shadow.next_pill ? ' next' : ''); el.textContent = time; return el;
    }));
    if (!times.length) text('schedule-list', 'ยังไม่มีตารางยาใน Shadow');
    text('last-pill', C.numeric(shadow.last_pill_taken, 1) === null ? 'ยังไม่มีข้อมูล' : when(shadow.last_pill_taken));
    $('edit-schedule').disabled = !demo && !lastRead;
    text('updated-at', lastRead ? `${demo ? 'ข้อมูลจำลอง' : 'อ่าน Shadow'} ล่าสุด ${clockFormat.format(lastRead)}` : 'ยังไม่ได้อ่านข้อมูล');
    drawChart(); renderEvents();
  }
  function drawChart() {
    const samples = history.get(selected) || [];
    const values = samples.map(p => p.value).filter(v => v !== null);
    $('chart-empty').hidden = values.length > 0;
    const min = Math.max(0, Math.floor((Math.min(...values, 70) - 10) / 10) * 10);
    const max = Math.ceil((Math.max(...values, 90) + 10) / 10) * 10;
    const first = samples[0]?.t || Date.now(), last = Math.max(first + 1000, samples.at(-1)?.t || first);
    const x = t => 37 + (t - first) / (last - first) * 608;
    const y = v => 173 - (v - min) / (max - min) * 153;
    let svg = '';
    for (let i = 0; i <= 3; i++) {
      const n = min + (max - min) * i / 3;
      svg += `<line x1="37" x2="645" y1="${y(n)}" y2="${y(n)}" stroke="#e9ede6" stroke-dasharray="3 5"/><text x="0" y="${y(n) + 4}" fill="#a1aca2" stroke="none" font-size="10">${Math.round(n)}</text>`;
    }
    let path = '', previous = null;
    for (const point of samples) {
      if (point.value === null) { previous = null; continue; }
      const gap = previous && point.t - previous.t > Math.max(90000, pollMs * 3);
      path += `${previous && !gap ? 'L' : 'M'}${x(point.t).toFixed(1)},${y(point.value).toFixed(1)} `;
      svg += `<circle cx="${x(point.t)}" cy="${y(point.value)}" r="2.5" fill="#d98b7e" stroke="white"/>`;
      previous = point;
    }
    svg += `<path d="${path}" stroke="#d98b7e" stroke-width="2" fill="none"/>`;
    $('chart').innerHTML = svg; // All interpolations above are computed numbers, never device strings.
    text('chart-range', samples.length ? `${clockFormat.format(first)} — ${clockFormat.format(last)} · ${values.length} จุด` : 'ยังไม่มีข้อมูล');
  }
  function renderEvents() {
    const filter = $('event-filter').value;
    const events = log.events.filter(e => filter === 'all' || (filter === 'alert' ? alerts.has(e.ev) : e.ev.startsWith('pill_')));
    text('event-count', `${log.events.length} รายการ${log.conflicts ? ' · พบ ID ขัดแย้ง' : ''}`);
    $('export-events').disabled = !log.events.length;
    $('event-list').replaceChildren();
    if (!events.length) { const li = document.createElement('li'); li.className = 'empty'; li.textContent = 'ยังไม่มีเหตุการณ์ในรายการนี้'; $('event-list').append(li); }
    for (const event of events.slice(0, 50)) {
      const li = document.createElement('li'), dot = document.createElement('span'), content = document.createElement('div'), title = document.createElement('strong'), detail = document.createElement('p'), time = document.createElement('div');
      dot.className = 'event-dot' + (alerts.has(event.ev) ? ' warning' : ''); content.className = 'event-content'; time.className = 'event-time';
      title.textContent = eventNames[event.ev] || event.ev;
      const node = Number.isInteger(event.node) ? (event.node > 0 ? `กำไล #${event.node}` : 'กล่องยา') : 'ไม่ระบุอุปกรณ์';
      detail.textContent = node + (event.d ? ' · ' + JSON.stringify(event.d) : '') + (!Number.isInteger(event.id) ? ' · ไม่มี ID; แยกเหตุซ้ำเหมือนกันไม่ได้' : ` · #${event.id}`);
      time.textContent = C.numeric(event.ts, 1, 8640000000000) === null ? `อ่านพบ ${clockFormat.format(event.received_at)} (ไม่มีเวลาเกิด)` : when(event.ts);
      content.append(title, detail); li.append(dot, content, time); $('event-list').append(li);
    }
  }
  function demoStart() {
    generation++; clearTimeout(timer); busy = false; mode = 'demo'; resetData(); showNotice('');
    const now = Date.now(), sec = Math.floor(now / 1000);
    shadow = { nodes: [1, 2], hr_1: 78, spo2_1: 97, batt_1: 64, node_online_1: true, node_last_seen_1: sec, hr_2: 82, spo2_2: 98, batt_2: 88, node_online_2: true, node_last_seen_2: sec, temp: 28.5, hum: 62, lid_open: false, person: false, box_state: 'idle', alarm_node: 0, next_pill: '12:00', last_pill_taken: sec - 3600, queue: 0, schedule: ['08:00', '12:00', '19:00'] };
    gateway = true; lastRead = now;
    for (const n of [1, 2]) history.set(n, Array.from({ length: 26 }, (_, i) => ({ t: now - (25 - i) * 15000, value: Math.round(77 + n * 2 + Math.sin(i * .7) * 5 + Math.sin(i * 1.8) * 2) })));
    log.add({ ev: 'pill_taken', id: 1, node: 0, ts: sec - 3600, d: { slot: '08:00', late_min: 0 } }, now);
    log.add({ ev: 'low_battery', id: 2, node: 1, ts: sec - 2400, d: { batt: 19 } }, now);
    log.add({ ev: 'fall_acknowledged', id: 3, node: 1, ts: sec - 1200, d: { by: 'button', after_s: 20 } }, now);
    render(); scheduleNext();
  }
  function scheduleNext() { clearTimeout(timer); timer = setTimeout(tick, mode === 'demo' ? 5000 : pollMs); }
  async function tick() {
    if (busy) return;
    if (mode === 'demo') {
      demoTick++; shadow.hr_1 = Math.round(78 + Math.sin(demoTick * .8) * 4); shadow.hr_2 = Math.round(82 + Math.cos(demoTick * .6) * 3);
      shadow.node_last_seen_1 = shadow.node_last_seen_2 = Math.floor(Date.now() / 1000);
      lastRead = Date.now(); addSamples(lastRead); render(); scheduleNext(); return;
    }
    const current = generation; busy = true; $('refresh').disabled = true;
    try {
      const data = await api('snapshot'); if (current !== generation) return;
      if (gateway === null) showNotice('');
      accept(data);
      if (pendingAlarm && Date.now() - pendingAlarm.at > 45000) { pendingAlarm = null; showNotice('ยังไม่ได้รับ event ยืนยันการรับเหตุจากกล่อง กรุณาตรวจอุปกรณ์; ไม่ได้ส่งคำสั่งซ้ำอัตโนมัติ'); render(); }
    } catch (error) {
      if (current === generation) { gateway = null; addSamples(Date.now()); showNotice(`อ่าน NETPIE ไม่สำเร็จ: ${error.message}`); render(); }
    } finally { if (current === generation) { busy = false; $('refresh').disabled = false; scheduleNext(); } }
  }
  function openSettings() {
    text('connect-error', location.protocol === 'file:' ? 'เชื่อมต่อจริงโดยรัน python 4-dashboard-testing/dashboard/web/server.py แล้วเปิด http://127.0.0.1:8080' : '');
    $('settings-dialog').showModal();
  }
  $('open-settings').addEventListener('click', openSettings); $('connect-top').addEventListener('click', openSettings);
  document.querySelectorAll('[data-close]').forEach(el => el.addEventListener('click', () => el.closest('dialog').close()));
  $('settings-dialog').addEventListener('close', () => { $('token').value = ''; });
  $('connect-form').addEventListener('submit', async event => {
    event.preventDefault();
    if (location.protocol === 'file:') { text('connect-error', 'ต้องเปิดผ่านตัวรัน Python ที่ http://127.0.0.1:8080 เพื่อเชื่อมต่อจริง'); return; }
    const current = ++generation; clearTimeout(timer); busy = true; $('connect-submit').disabled = true; text('connect-error', 'กำลังเชื่อมต่อ…');
    try {
      const data = await api('connect', { client_id: $('client-id').value.trim(), token: $('token').value.trim() });
      if (current !== generation) return;
      mode = 'live'; pollMs = Number($('poll-seconds').value) * 1000; resetData(); showNotice(''); accept(data);
      $('settings-dialog').close(); $('token').value = ''; text('connect-error', '');
    } catch (error) { if (current === generation) text('connect-error', `เชื่อมต่อไม่ได้: ${error.message}`); }
    finally { $('connect-submit').disabled = false; if (current === generation) { busy = false; scheduleNext(); } }
  });
  $('demo-button').addEventListener('click', async () => {
    try { await api('disconnect', {}); demoStart(); } catch (error) { showNotice(`ปิด session ไม่สำเร็จ: ${error.message}`); }
  });
  $('refresh').addEventListener('click', () => { clearTimeout(timer); tick(); });
  $('node-select').addEventListener('change', event => { selected = Number(event.target.value); render(); });
  $('event-filter').addEventListener('change', renderEvents);
  $('edit-schedule').addEventListener('click', () => { $('schedule-input').value = Array.isArray(shadow.schedule) ? shadow.schedule.join(', ') : ''; text('schedule-error', ''); $('schedule-dialog').showModal(); });
  $('schedule-form').addEventListener('submit', async event => {
    event.preventDefault(); const current = generation;
    try {
      const times = C.schedule($('schedule-input').value); $('schedule-submit').disabled = true;
      if (mode === 'demo') { shadow.schedule = times; text('schedule-feedback', 'แก้ไขข้อมูลจำลองแล้ว · ไม่ได้ส่งไปยังอุปกรณ์'); }
      else {
        await api('schedule', { schedule: times }); if (current !== generation) return;
        const result = await api('snapshot'); if (current !== generation) return; accept(result);
        if (JSON.stringify(C.schedule(shadow.schedule)) !== JSON.stringify(times)) throw new Error('อ่านกลับแล้วยังไม่ตรงกับตารางที่ส่ง กรุณารีเฟรชเพื่อตรวจสอบ');
        text('schedule-feedback', 'อ่าน Shadow กลับตรงแล้ว · ยังต้องตรวจว่ากล่องนำตารางไปใช้');
      }
      $('schedule-dialog').close(); render();
    } catch (error) { text('schedule-error', error.message); }
    finally { $('schedule-submit').disabled = false; }
  });
  $('clear-alarm').addEventListener('click', async () => {
    if (!confirm('รับเหตุและส่งคำสั่งหยุด alarm ที่กล่องยา?')) return;
    const current = generation;
    pendingAlarm = { at: Date.now(), node: shadow.alarm_node }; render();
    try {
      if (mode === 'demo') { shadow.box_state = 'idle'; shadow.alarm_node = 0; pendingAlarm = null; showNotice('เคลียร์ alarm ในข้อมูลจำลองแล้ว'); }
      else { await api('command', { cmd: 'clear_alarm' }); if (current !== generation) return; showNotice('NETPIE รับคำสั่งแล้ว · รอ event ยืนยันจากกล่อง ไม่ได้หมายความว่าเสียงหรือไฟหยุดแล้ว'); }
    } catch (error) { if (current === generation) { pendingAlarm = null; showNotice(`ยังยืนยันการส่งไม่ได้: ${error.message} · ตรวจกล่องก่อนส่งอีกครั้ง`); } }
    if (current === generation) render();
  });
  $('export-events').addEventListener('click', () => {
    const blob = new Blob([JSON.stringify({ source: mode, exported_at: new Date().toISOString(), note: 'Session observations of last_event only; not complete device history.', events: log.events }, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob), a = document.createElement('a'); a.href = url; a.download = `smartpillbox-${mode}-events.json`; a.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
  });
  document.querySelectorAll('nav a').forEach(link => link.addEventListener('click', () => { document.querySelectorAll('nav a').forEach(a => a.classList.remove('active')); link.classList.add('active'); }));
  text('today', new Intl.DateTimeFormat('th-TH', { timeZone: 'Asia/Bangkok', day: 'numeric', month: 'short', year: 'numeric' }).format(new Date()));
  demoStart();
})();
