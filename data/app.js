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

async function sendCommand(name) {
  try {
    await api(`/api/commands/${encodeURIComponent(name)}/send`, {method:'POST'});
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
    heldButton = button;
    button.classList.add('pressed');
    button.setPointerCapture(event.pointerId);
    sendCommand(spec.command);
    if (spec.repeat) {
      holdTimeout = setTimeout(() => {
        holdInterval = setInterval(() => sendCommand(spec.command), repeatInterval);
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
  button.title = commands[spec.command]?.configured ? spec.label : `${spec.label}: IR code not configured`;
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
    for (const group of page.groups) {
      const wrapper = document.createElement('div');
      wrapper.className = 'button-group';
      const heading = document.createElement('h2');
      heading.className = 'group-label';
      heading.textContent = group.label;
      const grid = document.createElement('div');
      grid.className = 'button-grid';
      grid.style.setProperty('--columns', group.columns);
      group.buttons.forEach(spec => grid.append(makeButton(spec)));
      wrapper.append(heading, grid);
      shell.append(wrapper);
    }
    section.append(shell);
    host.append(section);
    tabs.append(makeTab(section.id, page.label));
  }
  tabs.append(makeTab('codes', 'Codes'), makeTab('diagnostics', 'Status'));
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
    $('#config-editor').value = JSON.stringify(profile, null, 2);
    const select = $('#profile-select');
    select.replaceChildren(...remoteList.profiles.map(item => {
      const option = document.createElement('option');
      option.value = item.id;
      option.textContent = item.device;
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
    wasConnected = false;
    $('#connection').classList.remove('online');
    $('#connection-text').textContent = 'Offline';
    $('#status-json').textContent = error.message;
    notice(error.message);
  }
}

$('#profile-select').addEventListener('change', async event => {
  try {
    await api('/api/remotes/active', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({id:event.target.value})});
    await refreshProfile();
  } catch (error) { notice(error.message); await refreshProfile().catch(() => {}); }
});

$('#create-profile').addEventListener('click', async () => {
  const name = $('#new-name').value.trim();
  if (!name || !profile) { notice('Enter a name for the new remote.'); return; }
  const id = name.toLowerCase().normalize('NFKD').replace(/[\u0300-\u036f]/g, '').replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '').slice(0, 32);
  if (!id) { notice('Use letters or numbers in the remote name.'); return; }
  const copy = JSON.parse(JSON.stringify(profile));
  copy.id = id;
  copy.device = name;
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
setInterval(loadStatus, 10000);
