const element = id => document.getElementById(id);
let latest = null;
let clockOffset = 0;
let failed = false;
function status() {
  const node = element('status');
  if (failed) { node.textContent = 'Connection interrupted'; node.className = 'stale'; return; }
  if (!latest) { node.textContent = 'Waiting for sensor'; node.className = ''; return; }
  const age = Math.max(0, Date.now() / 1000 + clockOffset - latest.timestamp);
  node.textContent = age < 60 ? 'Live' : 'Sensor offline';
  node.className = age < 60 ? 'live' : 'stale';
  element('updated').textContent = `Last received ${Math.floor(age)} seconds ago`;
}
async function refresh() {
  try {
    const response = await fetch('api/readings', {cache: 'no-store'});
    if (!response.ok) throw new Error('API unavailable');
    const data = await response.json();
    failed = false;
    clockOffset = data.server_time - Date.now() / 1000;
    latest = data.readings[0] || null;
    if (latest) {
      element('temperature').textContent = latest.temperature_c.toFixed(2);
      element('fahrenheit').textContent = `${(latest.temperature_c * 9 / 5 + 32).toFixed(2)} °F`;
      element('humidity').textContent = latest.humidity_pct.toFixed(2);
      element('uptime').textContent = `Board uptime: ${latest.uptime_s}s`;
      element('samples').replaceChildren(...data.readings.slice(0, 8).map(reading => {
        const row = document.createElement('tr');
        for (const value of [new Date(reading.timestamp * 1000).toLocaleTimeString(), `${reading.temperature_c.toFixed(2)} °C`, `${reading.humidity_pct.toFixed(2)} %`]) {
          const cell = document.createElement('td'); cell.textContent = value; row.append(cell);
        }
        return row;
      }));
    }
  } catch { failed = true; }
  status();
  setTimeout(refresh, 2000);
}
setInterval(status, 1000);
refresh();
