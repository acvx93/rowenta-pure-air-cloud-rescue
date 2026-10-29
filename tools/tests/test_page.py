"""Runs the firmware's web page in a headless browser (Edge or Chrome) with a fake API.

Skipped when no browser is found.
"""
import html
import os
import re
import shutil
import subprocess

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BROWSERS = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    shutil.which("chromium") or "", shutil.which("google-chrome") or "",
]
BROWSER = next((b for b in BROWSERS if b and os.path.exists(b)), None)

# Fake API: answers like the firmware and records every POST; then clicks the buttons.
HARNESS = """<script>
window.calls = []; window.errors = [];
window.onerror = (m) => { errors.push(m); };
window.fetch = async (url, opts) => {
  if (opts && opts.method === 'POST') calls.push(url + ' ' + opts.body);
  const data = {'/api/status': {valid: true, power: true, mode: 3, speed: 3, light: 0, lock: false, pm25: 5,
                                time: '12:00', rssi: -60, sinric: {configured: false, connected: false}},
                '/api/schedule': [], '/api/sinric': {appKey: '', deviceId: '', lightDeviceId: '',
                                                     airDeviceId: '', alertThreshold: 35, secretSaved: false}};
  return new Response(JSON.stringify(data[url] || {}), {status: url in data ? 200 : 204});
};
setTimeout(() => {
  for (const id of ['bPower']) document.getElementById(id).click();
  for (const b of document.querySelectorAll('#modes button, #lights button')) b.click();
  for (const b of document.querySelectorAll('button[onclick]')) if (!b.id) b.click();
  setTimeout(() => document.body.setAttribute('data-result', JSON.stringify({calls, errors})), 300);
}, 500);
</script>"""


def run_page(tmp_path):
    source = open(os.path.join(ROOT, "firmware", "src", "page.h"), encoding="utf-8").read()
    page = re.search(r'R"HTML\((.*)\)HTML"', source, re.S).group(1)
    page = page.replace("<script>", HARNESS + "<script>", 1)
    path = tmp_path / "page.html"
    path.write_text(page, encoding="utf-8")
    out = subprocess.run([BROWSER, "--headless=new", "--disable-gpu", "--virtual-time-budget=5000",
                          "--dump-dom", path.as_uri()], capture_output=True, text=True, timeout=60).stdout
    m = re.search(r'data-result="([^"]*)"', out)
    assert m, "the page did not finish"
    import json
    return json.loads(html.unescape(m.group(1)))


@pytest.mark.skipif(BROWSER is None, reason="no headless browser available")
def test_page_buttons_send_commands_without_errors(tmp_path):
    result = run_page(tmp_path)
    assert result["errors"] == []
    assert '/api/command {"power":false}' in result["calls"]  # power is on, so the button turns it off
    assert '/api/command {"mode":4}' in result["calls"]
    assert '/api/command {"light":2}' in result["calls"]
    assert any(c.startswith("/api/sinric ") for c in result["calls"])  # "Save and restart"
