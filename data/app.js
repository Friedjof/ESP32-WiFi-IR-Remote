const $ = selector => document.querySelector(selector);
const iconIds = new Set(['power','plus','minus','mute','up','down','left','right','home','back','menu','exit','play','pause','stop','input','star','info','settings']);
const tones = new Set(['power','accent','red','green','yellow','blue']);
let profile = null;
let commands = {};
let repeatInterval = 250;
let holdTimeout = null;
let holdInterval = null;
let heldButton = null;
let learnTimer = null;
let noticeTimer = null;
let wasConnected = false;
let refreshing = null;
let finderCatalog = [];
let finderFeedback = {};
let lastFinderTest = null;
const safeFinderKeys = new Set(['volume_up','volume_down','mute','up','down','left','right','ok','home','back','input','channel_up','channel_down','menu','exit']);

async function api(path, options = {}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 6000);
  try {
    const response = await fetch(path, {cache:'no-store', signal:controller.signal, ...options});
    const data = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`);
    return data;
  } catch (error) {
    if (error.name === 'AbortError') throw new Error('ESP32 did not respond. Check WiFi and try again.');
    if (error instanceof TypeError) throw new Error('Connection lost. Check that the ESP32 is powered and connected to WiFi.');
    throw error;
  } finally { clearTimeout(timeout); }
}

function notice(message, success = false) {
  const element = $('#notice');
  element.textContent = message;
  element.classList.toggle('success', success);
  element.hidden = false;
  clearTimeout(noticeTimer);
  noticeTimer = setTimeout(() => { element.hidden = true; }, success ? 3000 : 6000);
}

async function sendCommand(name, held = false) {
  if (!commands[name]?.configured) {
    notice(`${prettyKey(name)} has no IR code in ${profile?.device || 'this remote'}. Choose a coded remote or open Finder.`);
    return;
  }
  try {
    await api(`/api/commands/${encodeURIComponent(name)}/send`, {method:'POST', headers:{'X-IR-Held':held ? '1' : '0'}});
  } catch (error) { notice(`${name.replaceAll('_',' ')}: ${error.message}`); }
}

function stopHold() {
  clearTimeout(holdTimeout);
  clearInterval(holdInterval);
  holdTimeout = holdInterval = null;
  if (heldButton) heldButton.classList.remove('pressed');
  heldButton = null;
}

function bindButton(button, spec) {
  button.addEventListener('pointerdown', event => {
    if (event.button !== 0) return;
    stopHold();
    if (!commands[spec.command]?.configured) { sendCommand(spec.command); return; }
    heldButton = button;
    button.classList.add('pressed');
    button.setPointerCapture(event.pointerId);
    sendCommand(spec.command);
    if (spec.repeat) {
      holdTimeout = setTimeout(() => {
        holdInterval = setInterval(() => sendCommand(spec.command, true), repeatInterval);
      }, 440);
    }
  });
  for (const eventName of ['pointerup','pointercancel','lostpointercapture']) button.addEventListener(eventName, stopHold);
  button.addEventListener('click', event => { if (event.detail === 0) sendCommand(spec.command); });
}
for (const eventName of ['pagehide','blur']) window.addEventListener(eventName, stopHold);
document.addEventListener('visibilitychange', () => { if (document.hidden) stopHold(); });

function makeButton(spec) {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'key';
  button.dataset.command = spec.command;
  button.setAttribute('aria-label', spec.label);
  if (tones.has(spec.tone)) button.classList.add(`tone-${spec.tone}`);
  if (!commands[spec.command]?.configured) button.classList.add('unconfigured');
  const confirmed = profile?.origin?.confirmed_commands?.includes(spec.command);
  if (confirmed) button.classList.add('confirmed');
  button.title = !commands[spec.command]?.configured ? `${spec.label}: IR code not configured` :
    confirmed ? `${spec.label}: marked working during Finder test` : `${spec.label}: not confirmed on this TV`;
  if (spec.span) button.style.gridColumn = `span ${spec.span}`;
  if (spec.column) {
    button.style.gridColumnStart = spec.column;
    button.style.gridColumnEnd = `span ${spec.span || 1}`;
  }
  if (spec.row) button.style.gridRow = spec.row;
  if (iconIds.has(spec.icon)) {
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('aria-hidden', 'true');
    const use = document.createElementNS('http://www.w3.org/2000/svg', 'use');
    use.setAttribute('href', `#${spec.icon}`);
    svg.append(use);
    button.append(svg);
  }
  const label = document.createElement('span');
  label.textContent = spec.label;
  button.append(label);
  bindButton(button, spec);
  return button;
}

function activateTab(id) {
  stopHold();
  document.querySelectorAll('.panel,.tab').forEach(element => element.classList.remove('active'));
  document.getElementById(id)?.classList.add('active');
  document.querySelector(`[data-tab="${id}"]`)?.classList.add('active');
  document.body.classList.toggle('page-scroll', ['finder','codes','diagnostics'].includes(id));
  window.scrollTo(0, 0);
}

function makeTab(id, label) {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'tab';
  button.dataset.tab = id;
  button.textContent = label;
  button.addEventListener('click', () => activateTab(id));
  return button;
}

function renderRemote() {
  stopHold();
  const host = $('#dynamic-pages');
  host.replaceChildren();
  const tabs = $('#tabs');
  tabs.replaceChildren();
  for (const page of profile.pages) {
    const section = document.createElement('section');
    section.id = `page-${page.id}`;
    section.className = 'panel remote-panel';
    section.setAttribute('aria-label', page.label);
    const shell = document.createElement('div');
    shell.className = 'remote-shell';
    if (!Object.values(commands).some(command => command.configured)) {
      const setup = document.createElement('div');
      setup.className = 'remote-setup';
      const message = document.createElement('span');
      message.textContent = 'This remote has no IR codes yet.';
      const finder = document.createElement('button');
      finder.type = 'button';
      finder.textContent = 'Find codes';
      finder.addEventListener('click', () => activateTab('finder'));
      setup.append(message, finder);
      section.append(setup);
    }
    for (const group of page.groups) {
      const wrapper = document.createElement('div');
      wrapper.className = 'button-group';
      if (group.buttons.length && group.buttons.every(spec => /^digit_[0-9]$/.test(spec.command))) {
        wrapper.classList.add('number-pad-group');
      }
      const heading = document.createElement('h2');
      heading.className = 'group-label';
      heading.textContent = group.label;
      const grid = document.createElement('div');
      grid.className = 'button-grid';
      grid.style.setProperty('--columns', group.columns);
      if (group.buttons.some(spec => spec.column || spec.row || spec.span)) {
        group.buttons.forEach(spec => grid.append(makeButton(spec)));
      } else {
        grid.classList.add('flow');
        for (let index = 0; index < group.buttons.length; index += group.columns) {
          const row = document.createElement('div');
          row.className = 'button-row';
          const items = group.buttons.slice(index, index + group.columns);
          row.style.setProperty('--items', items.length);
          if (items.length === 1) {
            row.classList.add('single');
            row.style.setProperty('--single-width', `calc((100% - ${(group.columns - 1) * 7}px) / ${group.columns})`);
          }
          items.forEach(spec => row.append(makeButton(spec)));
          grid.append(row);
        }
      }
      wrapper.append(heading, grid);
      shell.append(wrapper);
    }
    section.append(shell);
    host.append(section);
    tabs.append(makeTab(section.id, page.label));
  }
  tabs.append(makeTab('finder', 'Finder'), makeTab('codes', 'Settings'), makeTab('diagnostics', 'Status'));
  activateTab(`page-${profile.pages[0].id}`);
}

async function refreshProfile() {
  if (refreshing) return refreshing;
  refreshing = (async () => {
    const [remoteList, config, commandList] = await Promise.all([
      api('/api/remotes'), api('/api/config'), api('/api/commands')
    ]);
    profile = config;
    commands = commandList.commands || {};
    repeatInterval = commandList.repeat_interval_ms || 250;
    $('#target').textContent = profile.device;
    const confirmed = profile.origin?.confirmed_commands || [];
    const configuredCount = Object.values(commands).filter(command => command.configured).length;
    $('#profile-proof').textContent = confirmed.length ?
      `Tested working: ${confirmed.map(prettyKey).join(', ')}` :
      configuredCount ? `${configuredCount} IR codes loaded · not tested on this device` :
      'No IR codes in this remote. Open Finder to test candidates.';
    $('#config-editor').value = JSON.stringify(profile, null, 2);
    const select = $('#profile-select');
    select.replaceChildren(...remoteList.profiles.map(item => {
      const option = document.createElement('option');
      option.value = item.id;
      option.textContent = item.configured_commands ? item.device : `${item.device} · no IR codes`;
      return option;
    }));
    select.value = remoteList.active;
    const learnSelect = $('#learn-command');
    const previous = learnSelect.value;
    learnSelect.replaceChildren(...Object.keys(commands).sort().map(name => {
      const option = document.createElement('option');
      option.value = name;
      option.textContent = name.replaceAll('_', ' ');
      return option;
    }));
    if (commands[previous]) learnSelect.value = previous;
    const editSelect = $('#key-edit-command');
    const previousEdit = editSelect.value;
    editSelect.replaceChildren(...Object.keys(commands).sort().map(name => {
      const option = document.createElement('option');
      option.value = name; option.textContent = prettyKey(name);
      return option;
    }));
    if (commands[previousEdit]) editSelect.value = previousEdit;
    populateKeyEditor();
    renderRemote();
  })();
  try { await refreshing; } finally { refreshing = null; }
}

async function loadStatus() {
  try {
    const data = await api('/api/status');
    $('#status-json').textContent = JSON.stringify(data, null, 2);
    $('#connection').classList.toggle('online', data.wifi.connected);
    $('#connection-text').textContent = data.wifi.connected ? 'Connected' : 'WiFi offline';
    if (data.wifi.connected && (!wasConnected || data.active_remote !== profile?.id)) {
      await refreshProfile();
    }
    wasConnected = data.wifi.connected;
  } catch (error) {
    const lostConnection = wasConnected;
    wasConnected = false;
    $('#connection').classList.remove('online');
    $('#connection-text').textContent = 'Offline';
    $('#status-json').textContent = error.message;
    if (lostConnection) notice(error.message);
  }
}

function prettyKey(name) { return name.replaceAll('_', ' ').replace(/\b\w/g, letter => letter.toUpperCase()); }

function renderFinder() {
  const brand = $('#finder-brand').value;
  const picker = $('#finder-profile');
  const previous = picker.value;
  picker.replaceChildren(...finderCatalog.filter(item => item.manufacturer === brand).map(item => {
    const option = document.createElement('option');
    option.value = item.id;
    option.textContent = item.name;
    return option;
  }));
  if ([...picker.options].some(option => option.value === previous)) picker.value = previous;
  const selected = finderCatalog.find(item => item.id === picker.value);
  const meta = $('#finder-meta');
  meta.replaceChildren();
  const keyPicker = $('#finder-command');
  const previousKey = keyPicker.value;
  keyPicker.replaceChildren();
  if (!selected) { meta.textContent = 'No candidates available.'; return; }
  const summary = document.createElement('p');
  summary.textContent = `${selected.protocol} · ${Math.round(selected.frequency / 1000)} kHz · ${Object.keys(selected.commands).length} commands · ${selected.status.replaceAll('_',' ')}`;
  const models = document.createElement('p');
  models.textContent = `Models: ${selected.models.join(', ')}. ${selected.notes}`;
  const source = document.createElement('p');
  source.textContent = `Source: ${selected.source}`;
  meta.append(summary, models, source);
  if (selected.url?.startsWith('https://github.com/')) {
    const link = document.createElement('a');
    link.href = selected.url; link.target = '_blank'; link.rel = 'noopener noreferrer';
    link.textContent = 'View original data';
    meta.append(link);
  }
  for (const name of Object.keys(selected.commands).filter(key => safeFinderKeys.has(key))) {
    const option = document.createElement('option');
    option.value = name; option.textContent = prettyKey(name);
    keyPicker.append(option);
  }
  if ([...keyPicker.options].some(option => option.value === previousKey)) keyPicker.value = previousKey;
  else if ([...keyPicker.options].some(option => option.value === 'volume_up')) keyPicker.value = 'volume_up';
  const results = finderFeedback[selected.id] || {};
  const good = Object.entries(results).filter(([, worked]) => worked).map(([name]) => prettyKey(name));
  const bad = Object.entries(results).filter(([, worked]) => !worked).map(([name]) => prettyKey(name));
  $('#finder-progress').textContent = `Worked: ${good.join(', ') || 'none'} · Did not work: ${bad.join(', ') || 'none'}. Confirm two different keys to save.`;
  $('#finder-activate').disabled = good.length < 2;
  $('#finder-question').hidden = true;
  lastFinderTest = null;
}

async function loadCandidates() {
  const data = await api('/api/candidates');
  finderCatalog = data.profiles || [];
  finderFeedback = data.feedback || {};
  renderFinder();
}

$('#finder-brand').addEventListener('change', renderFinder);
$('#finder-profile').addEventListener('change', renderFinder);
$('#finder-test').addEventListener('click', async () => {
  const id = $('#finder-profile').value, command = $('#finder-command').value;
  if (!id || !command) return;
  try {
    await api(`/api/candidates/${encodeURIComponent(id)}/test`, {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({command})});
    lastFinderTest = {id, command};
    $('#finder-question').hidden = false;
    notice(`Sent ${prettyKey(command)} once. Check the TV.`, true);
  } catch (error) { notice(error.message); }
});

async function answerFinder(worked) {
  if (!lastFinderTest) return;
  const {id, command} = lastFinderTest;
  try {
    await api(`/api/candidates/${encodeURIComponent(id)}/feedback`, {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({command, worked})});
    finderFeedback[id] ||= {};
    finderFeedback[id][command] = worked;
    renderFinder();
    notice(worked ? 'Working key saved. Test another key.' : 'Result saved. Try another key or profile.', true);
  } catch (error) { notice(error.message); }
}
$('#finder-yes').addEventListener('click', () => answerFinder(true));
$('#finder-no').addEventListener('click', () => answerFinder(false));
$('#finder-activate').addEventListener('click', async () => {
  const id = $('#finder-profile').value;
  if (!id || !confirm('Save this candidate as a remote? Commands you have not tested may still be wrong for your TV.')) return;
  try {
    await api(`/api/candidates/${encodeURIComponent(id)}/activate`, {method:'POST'});
    await refreshProfile();
    notice('Candidate saved as the active remote. Untested keys remain unverified.', true);
  } catch (error) { notice(error.message); }
});
$('#finder-reset').addEventListener('click', async () => {
  const id = $('#finder-profile').value;
  if (!id || !confirm('Clear all saved test results for this candidate?')) return;
  try {
    await api(`/api/candidates/${encodeURIComponent(id)}/feedback`, {method:'DELETE'});
    await loadCandidates();
    notice('Finder results cleared.', true);
  } catch (error) { notice(error.message); }
});

$('#flipper-file').addEventListener('change', event => {
  const file = event.target.files[0];
  $('#flipper-selected').textContent = file ? `${file.name} · ${file.size} bytes` : 'No file selected.';
  if (file && !$('#flipper-name').value.trim()) $('#flipper-name').value = file.name.replace(/\.ir$/i, '').replaceAll('_', ' ');
});
$('#flipper-import').addEventListener('click', async () => {
  const file = $('#flipper-file').files[0];
  const device = $('#flipper-name').value.trim();
  if (!file || !device) { notice('Choose a .ir file and enter a remote name.'); return; }
  if (file.size > 24576) { notice('File exceeds the 24 KB import limit.'); return; }
  const id = device.toLowerCase().normalize('NFKD').replace(/[\u0300-\u036f]/g, '').replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '').slice(0,32);
  if (!id) { notice('Use letters or numbers in the remote name.'); return; }
  try {
    const content = await file.text();
    const result = await api('/api/import/flipper', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({id, device, content})});
    await refreshProfile();
    notice(`Imported ${result.commands} signals. Test before relying on them.`, true);
  } catch (error) { notice(`Flipper import failed: ${error.message}`); }
});

async function loadNetwork() {
  const [network, security] = await Promise.all([api('/api/network'), api('/api/security')]);
  $('#wifi-ssid').value = network.station.ssid || '';
  $('#wifi-state').textContent = network.station.connected ? `Connected: ${network.station.ip}` : network.station.configured ? 'Connecting or offline. Open AP stays available.' : 'Home WiFi is not configured.';
  $('#admin-state').textContent = security.admin_password_set ? 'Admin password enabled for changes.' : 'No admin password; anyone on the open AP can change settings.';
}
$('#wifi-save').addEventListener('click', async () => {
  try {
    await api('/api/network', {method:'PUT', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ssid:$('#wifi-ssid').value.trim(), password:$('#wifi-password').value})});
    $('#wifi-password').value = '';
    notice('Connecting to home WiFi. The open AP remains available.', true);
    setTimeout(() => loadNetwork().catch(error => notice(error.message)), 5000);
  } catch (error) { notice(error.message); }
});
$('#wifi-clear').addEventListener('click', async () => {
  try {
    await api('/api/network', {method:'PUT', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ssid:'',password:''})});
    await loadNetwork(); notice('Home WiFi forgotten. The AP remains on.', true);
  } catch (error) { notice(error.message); }
});
async function setAdmin(password) {
  try {
    await api('/api/security', {method:'PUT', headers:{'Content-Type':'application/json'}, body:JSON.stringify({password})});
    $('#admin-password').value = '';
    await loadNetwork(); notice('Admin access updated.', true);
  } catch (error) { notice(error.message); }
}
$('#admin-save').addEventListener('click', () => setAdmin($('#admin-password').value));
$('#admin-clear').addEventListener('click', () => setAdmin(''));

$('#profile-select').addEventListener('change', async event => {
  try {
    await api('/api/remotes/active', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({id:event.target.value})});
    await refreshProfile();
  } catch (error) { notice(error.message); await refreshProfile().catch(() => {}); }
});

function populateKeyEditor() {
  const command = profile?.commands?.[$('#key-edit-command').value];
  if (!command) return;
  $('#key-edit-protocol').value = command.protocol;
  $('#key-edit-frequency').value = command.frequency || profile.default_frequency;
  $('#key-edit-code').value = command.code || '';
  $('#key-edit-bits').value = command.bits || 32;
  $('#key-edit-timings').value = (command.timings || []).join(' ');
  $('#key-edit-encoded').hidden = command.protocol === 'RAW';
  $('#key-edit-raw').hidden = command.protocol !== 'RAW';
}
$('#key-edit-command').addEventListener('change', populateKeyEditor);
$('#key-edit-protocol').addEventListener('change', () => {
  const protocol = $('#key-edit-protocol').value;
  $('#key-edit-encoded').hidden = protocol === 'RAW';
  $('#key-edit-raw').hidden = protocol !== 'RAW';
  if (protocol === 'RC5' || protocol === 'RC6') $('#key-edit-frequency').value = 36000;
  else if (protocol === 'SONY') $('#key-edit-frequency').value = 40000;
  else $('#key-edit-frequency').value = 38000;
  if (protocol === 'RC5') $('#key-edit-bits').value = 12;
  if (protocol === 'RC6') $('#key-edit-bits').value = 20;
  if (protocol === 'SONY') $('#key-edit-bits').value = 12;
  if (protocol === 'NEC' || protocol === 'SAMSUNG') $('#key-edit-bits').value = 32;
});
$('#key-edit-save').addEventListener('click', async () => {
  if (!profile) return;
  const name = $('#key-edit-command').value;
  const protocol = $('#key-edit-protocol').value;
  const frequency = Number($('#key-edit-frequency').value);
  if (!Number.isInteger(frequency) || frequency < 30000 || frequency > 60000) { notice('Frequency must be 30000–60000 Hz.'); return; }
  let command = {protocol, frequency};
  if (protocol === 'RAW') {
    const raw = $('#key-edit-timings').value.trim();
    if (raw && !/^\d+(?:\s+\d+)*$/.test(raw)) { notice('RAW timings must be whole microsecond values separated by spaces.'); return; }
    command.timings = raw ? raw.split(/\s+/).map(Number) : [];
    const oldDuty = profile.commands[name].duty_cycle;
    if (oldDuty && profile.commands[name].protocol === 'RAW') command.duty_cycle = oldDuty;
  } else {
    const code = $('#key-edit-code').value.trim();
    const bits = Number($('#key-edit-bits').value);
    if (code && !/^(?:0x)?[0-9a-f]{1,16}$/i.test(code)) { notice('Enter a hexadecimal code, optionally prefixed with 0x.'); return; }
    if (!Number.isInteger(bits) || bits < 1 || bits > 64) { notice('Bits must be between 1 and 64.'); return; }
    command.code = code || null;
    command.bits = bits;
  }
  const updated = JSON.parse(JSON.stringify(profile));
  updated.commands[name] = command;
  try {
    await api('/api/config', {method:'PUT', headers:{'Content-Type':'application/json'}, body:JSON.stringify(updated)});
    await refreshProfile();
    notice(`${prettyKey(name)} saved.`, true);
  } catch (error) { notice(`Key not saved: ${error.message}`); }
});

$('#create-profile').addEventListener('click', async () => {
  const name = $('#new-name').value.trim();
  if (!name || !profile) { notice('Enter a name for the new remote.'); return; }
  const id = name.toLowerCase().normalize('NFKD').replace(/[\u0300-\u036f]/g, '').replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '').slice(0, 32);
  if (!id) { notice('Use letters or numbers in the remote name.'); return; }
  const copy = JSON.parse(JSON.stringify(profile));
  copy.id = id;
  copy.device = name;
  delete copy.origin;
  for (const command of Object.values(copy.commands)) {
    if (command.protocol === 'RAW') command.timings = [];
    else command.code = null;
  }
  try {
    await api('/api/remotes', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(copy)});
    await refreshProfile();
    $('#new-name').value = '';
    notice('Remote created with empty IR codes.', true);
  } catch (error) { notice(`Remote not created: ${error.message}`); }
});

$('#delete-profile').addEventListener('click', async () => {
  if (!profile || !confirm(`Delete ${profile.device} and its saved IR codes? Download a backup first if needed.`)) return;
  try {
    await api(`/api/remotes/${encodeURIComponent(profile.id)}`, {method:'DELETE'});
    await refreshProfile();
    notice('Remote deleted.', true);
  } catch (error) { notice(`Remote not deleted: ${error.message}`); }
});

$('#save-config').addEventListener('click', async () => {
  try {
    const body = JSON.stringify(JSON.parse($('#config-editor').value));
    await api('/api/config', {method:'PUT', headers:{'Content-Type':'application/json'}, body});
    await refreshProfile();
    notice('Profile saved.', true);
  } catch (error) { notice(`Profile not saved: ${error.message}`); }
});
$('#reload-config').addEventListener('click', () => refreshProfile().catch(error => notice(error.message)));
$('#download').addEventListener('click', () => { location.href = '/api/config/export'; });
$('#import-file').addEventListener('change', async event => {
  const file = event.target.files[0];
  if (!file) return;
  try {
    const imported = JSON.parse(await file.text());
    const same = imported.id === profile?.id;
    await api(same ? '/api/config/import' : '/api/remotes', {
      method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(imported)
    });
    await refreshProfile();
    notice('Profile imported.', true);
  } catch (error) { notice(`Import failed: ${error.message}`); }
  event.target.value = '';
});

$('#start-learn').addEventListener('click', async () => {
  try {
    const command = $('#learn-command').value;
    await api('/api/learn/start', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({command})});
    $('#learn-result').textContent = 'Waiting for the original remote…';
    $('#save-learn').disabled = true;
    clearInterval(learnTimer);
    learnTimer = setInterval(pollLearn, 1000);
  } catch (error) { notice(error.message); }
});
async function pollLearn() {
  try {
    const data = await api('/api/learn/status');
    if (data.state === 'waiting') return;
    clearInterval(learnTimer);
    $('#learn-result').textContent = data.state === 'captured' ? JSON.stringify(data.candidate, null, 2) : `Learning: ${data.state}`;
    $('#save-learn').disabled = data.state !== 'captured';
  } catch (error) { clearInterval(learnTimer); notice(error.message); }
}
$('#save-learn').addEventListener('click', async () => {
  try {
    await api('/api/learn/commit', {method:'POST'});
    await refreshProfile();
    $('#save-learn').disabled = true;
    notice('Learned code saved.', true);
  } catch (error) { notice(error.message); }
});
$('#refresh-status').addEventListener('click', loadStatus);
$('#restart').addEventListener('click', async () => {
  try { await api('/api/restart', {method:'POST'}); notice('ESP32 is restarting. Reconnection can take a moment.', true); }
  catch (error) { notice(error.message); }
});

loadStatus();
loadCandidates().catch(error => notice(`Finder unavailable: ${error.message}`));
loadNetwork().catch(error => notice(`Network settings unavailable: ${error.message}`));
setInterval(loadStatus, 10000);
