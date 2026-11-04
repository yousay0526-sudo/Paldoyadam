// Real Chromium persistence: refresh, separate tab, full browser restart.
// Build with --test-hooks. Requires the same Playwright dependency as browser_smoke.cjs.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const root = path.resolve(process.argv[2] || 'dist/web');
const output = path.resolve(process.argv[3] || 'build-save/browser-qa');
const key = 'paldoyadam.journey.autosave';
const errors = [];
const fields = ['quest', 'map', 'x', 'y', 'hp', 'coins', 'herbs', 'keyAttack'];
async function state(page) {
  return page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())));
}
async function phase(page, expected) {
  await page.waitForFunction(p => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).phase === p, expected);
}
async function click(page, action, value = 0) {
  await page.waitForFunction(({ action, value }) =>
    JSON.parse(Module.UTF8ToString(Module._paldo_test_buttons())).some(b => b.action === action && b.value === value),
  { action, value });
  const buttons = await page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_buttons())));
  const b = buttons.find(b => b.action === action && b.value === value);
  assert(b, 'Rendered control: ' + action);
  await page.mouse.click(b.x + b.w / 2, b.y + b.h / 2);
}
async function ready(page, url) {
  page.on('pageerror', e => errors.push(e.message));
  await page.goto(url);
  await page.waitForSelector('body[data-game-ready="true"]', { timeout: 90000 });
  await phase(page, 0);
}
async function saved(page, quest) {
  await page.waitForFunction(({ key, quest }) => {
    const hex = localStorage.getItem(key);
    return hex && parseInt(hex.slice(32, 34), 16) === quest;
  }, { key, quest });
}
function same(a, b) { for (const f of fields) assert.equal(a[f], b[f], f); }
(async () => {
  fs.mkdirSync(output, { recursive: true });
  const profile = fs.mkdtempSync(path.join(output, 'profile-'));
  const server = http.createServer((req, res) => {
    const url = new URL(req.url, 'http://localhost');
    if (url.pathname === '/favicon.ico') { res.writeHead(204); res.end(); return; }
    const file = path.resolve(root, '.' + decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname));
    if (!file.startsWith(root + path.sep)) { res.writeHead(403); res.end(); return; }
    fs.stat(file, (error, stat) => {
      if (error || !stat.isFile()) { res.writeHead(404); res.end(); return; }
      const mime = { '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream' };
      res.writeHead(200, { 'Content-Type': mime[path.extname(file)] || 'application/octet-stream', 'Content-Length': stat.size });
      fs.createReadStream(file).pipe(res);
    });
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const url = 'http://127.0.0.1:' + server.address().port;
  const options = { executablePath: process.env.PALDO_BROWSER || undefined, headless: true,
    chromiumSandbox: true, viewport: { width: 1100, height: 650 } };
  let context;
  try {
    context = await chromium.launchPersistentContext(profile, options);
    let page = await context.newPage(); await ready(page, url);
    assert.equal((await state(page)).saveAvailable, false);
    await page.keyboard.press('Space'); await phase(page, 1);
    await click(page, 'skip'); await phase(page, 2);
    const start = await state(page);
    await page.keyboard.down('ArrowUp');
    await page.waitForFunction(y => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).y < y - 80, start.y);
    await page.keyboard.up('ArrowUp');
    await page.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).heroFrame === -1);
    await page.keyboard.press('Space'); await phase(page, 3);
    for (let i = 0; i < 3; i++) await page.keyboard.press('Space');
    await phase(page, 2); assert.equal((await state(page)).quest, 1);
    // Configure the real key menu, then ensure journey operations keep it.
    await page.keyboard.press('Escape'); await phase(page, 10);
    await click(page, 'keys'); await click(page, 'key', 0); await page.keyboard.press('b');
    await page.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).keyAttack === 98);
    assert.equal((await state(page)).keyAttack, 98);
    await click(page, 'close'); await page.keyboard.press('Escape'); await phase(page, 2);
    await saved(page, 1);
    await page.keyboard.press('Escape'); await phase(page, 10);
    const expected = await state(page);
    await page.reload(); await page.waitForSelector('body[data-game-ready="true"]'); await phase(page, 0);
    assert.equal((await state(page)).saveAvailable, true);
    await click(page, 'continue'); await phase(page, 2); same(await state(page), expected);
    console.log('PASS real browser refresh, quest and key settings restored');
    // Training attacks must not alter the persisted journey record.
    await page.keyboard.press('F8');
    await page.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).training);
    const before = await page.evaluate(k => localStorage.getItem(k), key);
    await page.keyboard.press('b');
    await page.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).hits > 0);
    assert.equal(await page.evaluate(k => localStorage.getItem(k), key), before);
    await page.keyboard.press('F8'); await phase(page, 2); same(await state(page), expected);
    // Cancel new-journey confirmation through the real title controls.
    await page.keyboard.press('Escape'); await phase(page, 10); await click(page, 'title'); await phase(page, 0);
    await click(page, 'start');
    await page.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).newConfirm === 1);
    await click(page, 'cancel');
    await page.waitForFunction(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).newConfirm === 0);
    await page.close(); page = await context.newPage(); await ready(page, url);
    await click(page, 'continue'); await phase(page, 2); same(await state(page), expected);
    console.log('PASS tab reconnection, confirmation cancel, training isolation');
    const browser = context.browser(); await context.close(); context = null;
    assert.equal(browser.isConnected(), false);
    context = await chromium.launchPersistentContext(profile, options);
    page = await context.newPage(); await ready(page, url);
    assert.equal((await state(page)).saveAvailable, true);
    await page.screenshot({ path: path.join(output, 'continue-after-restart.png') });
    await click(page, 'continue'); await phase(page, 2); same(await state(page), expected);
    await page.screenshot({ path: path.join(output, 'journey-restored.png') });
    console.log('PASS full browser process restart, same origin/profile and saved progress');
    await page.keyboard.press('Escape'); await phase(page, 10); await click(page, 'title'); await phase(page, 0);
    await click(page, 'start'); await click(page, 'confirm'); await phase(page, 1);
    await saved(page, 0); assert.equal((await state(page)).keyAttack, 98);
    const record = await page.evaluate(k => localStorage.getItem(k), key);
    assert.equal(record.length, 928 * 2); assert.equal(record.slice(8, 10), '04');
    await page.reload(); await page.waitForSelector('body[data-game-ready="true"]'); await phase(page, 0);
    await click(page, 'continue'); await phase(page, 1); assert.equal((await state(page)).keyAttack, 98);
    console.log('PASS confirmed new journey writes v4, restarts prologue and preserves separate keys');
    assert.deepEqual(errors, []);
    console.log('PASS browser_save: no page errors; profile and screenshots: ' + output);
  } finally {
    if (context) await context.close();
    server.closeAllConnections(); await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
