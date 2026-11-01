/* Probe: mobile play — real touch gestures on an emulated phone.
 *
 * Emulates a 412x860 @2x touch phone (pointer: coarse), boots a fresh
 * campaign, then drives Input.dispatchTouchEvent and asserts outcomes via
 * the engine's ct_view_state() export (zoom, camera, funds, the parked
 * placement) rather than pixels:
 *   - the canvas fits the phone at a whole/fractional scale, touch opens 2x
 *   - a tap with a build tool parks the ghost (no build, no charge)
 *   - a drag builds straight away
 *   - a long-press never builds
 *   - a two-finger spread zooms in a whole step; the finger left after a
 *     pinch pans and never builds
 *   - rotating re-lays the view out
 * Screenshots (portrait + landscape): tests/web/out-touch/
 *
 * Run: node tests/web/probe_touch.js
 *   env CHROME=<path> on hosts without `chromium` (macOS: Google Chrome)
 *   env CT_EXE=<path to SIMTOWER.EXE> (default web/dev-assets/SIMTOWER.EXE)
 */
'use strict';
const fs = require('fs');
const path = require('path');
const { spawn, execSync } = require('child_process');
const { Cdp } = require('./cdp');

const REPO = path.resolve(__dirname, '../..');
const DIST = path.join(REPO, 'web/dist');
const OUT = path.join(__dirname, 'out-touch');
const PORT = 8621;
const URL_BASE = `http://127.0.0.1:${PORT}/index.html`;
const EXE = process.env.CT_EXE || path.join(REPO, 'web/dev-assets/SIMTOWER.EXE');
const CELL_W = 8, CELL_H = 36;

let fails = 0;
function check(cond, msg) {
  console.log(`  ${cond ? 'ok  ' : 'FAIL'} ${msg}`);
  if (!cond) fails++;
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  fs.rmSync(OUT, { recursive: true, force: true });
  fs.mkdirSync(OUT, { recursive: true });
  try { execSync(`fuser -k ${PORT}/tcp 2>/dev/null`); } catch (e) {}
  const server = spawn('python3', ['-m', 'http.server', String(PORT), '--bind', '127.0.0.1'],
    { cwd: DIST, stdio: 'ignore' });
  const cdp = new Cdp({
    profile: path.join(OUT, 'profile'),
    chromeLog: path.join(OUT, 'chrome.log'),
  });
  const cleanup = () => {
    try { server.kill(); } catch (e) {}
    try { cdp.chrome.kill(); } catch (e) {}
  };
  process.on('exit', cleanup);
  await sleep(800);
  cdp.launch();

  const sid = await cdp.openPage('about:blank');
  const phone = async (w, h) => {
    await cdp.send('Emulation.setDeviceMetricsOverride',
      { width: w, height: h, deviceScaleFactor: 2, mobile: true }, sid);
    await cdp.send('Emulation.setTouchEmulationEnabled',
      { enabled: true, maxTouchPoints: 5 }, sid);
  };
  await phone(412, 860);
  await cdp.navigate(URL_BASE);
  await cdp.waitFor(`typeof Module!=='undefined' && Module.engineReady===true`, 90000);

  /* fresh campaign in its own slot, so a saved tower can't interfere */
  await cdp.setFileInput('#exefile', [EXE]);
  await cdp.eval(`(document.getElementById('exefile')
      .dispatchEvent(new Event('change')), 0)`);
  await cdp.waitFor(`!document.getElementById('startbtn').disabled`, 30000);
  await cdp.eval(`(document.getElementById('startbtn').click(), 0)`);
  await cdp.waitFor(`document.getElementById('canvaswrap').style.display==='block'`, 30000);
  await sleep(2500);

  const state = async () => JSON.parse(await cdp.eval(
    `Module.ccall('ct_view_state','string',[],[])`));
  /* Poll the engine until pred(state) holds: the game handles input once
   * per frame, and headless software GL can run it at a few fps. */
  const until = async (pred, ms = 5000) => {
    const end = Date.now() + ms;
    let st = await state();
    while (!pred(st) && Date.now() < end) { await sleep(100); st = await state(); }
    return st;
  };
  const canvas = async () => cdp.eval(`(() => { const c = Module.canvas,
      r = c.getBoundingClientRect();
      return { w: c.width, h: c.height, cw: r.width, ch: r.height,
               x: r.left, y: r.top }; })()`);

  /* logical canvas px -> CSS px for Input.dispatchTouchEvent */
  let cv = await canvas();
  const css = (lx, ly) => ({ x: cv.x + lx * cv.cw / cv.w, y: cv.y + ly * cv.ch / cv.h });
  /* screen (logical) position of a grid cell's centre, from the camera */
  const cellAt = (s, floor, cell) => {
    const vx = cell * CELL_W - Math.trunc(s.cx) + Math.trunc(s.vw / 2) + CELL_W / 2;
    const vy = -floor * CELL_H - Math.trunc(s.cy) + Math.trunc(s.vh / 2) + CELL_H / 2;
    return css(vx * s.z, vy * s.z);
  };
  /* Touch events built in the page (CDP's Input.dispatchTouchEvent hangs
   * in headless Chrome on this canvas). `pts` is every finger still down,
   * as in CDP; changedTouches is derived the way a browser would. */
  let down = [];
  const touch = async (type, pts) => {
    pts = pts.map((p, i) => ({ x: p.x, y: p.y, id: p.id ?? i }));
    let changed;
    if (type === 'touchStart') changed = pts.filter((p) => !down.some((d) => d.id === p.id));
    else if (type === 'touchMove') changed = pts;
    else changed = down.filter((d) => !pts.some((p) => p.id === d.id))
                       .map((d) => pts.find((p) => p.id === d.id) || d);
    down = type === 'touchEnd' ? pts : pts;
    const ev = type.replace('touch', 'touch').toLowerCase();
    await cdp.eval(`(() => { const c = Module.canvas;
      const mk = (p) => new Touch({ identifier: p.id, target: c,
        clientX: p.x, clientY: p.y, pageX: p.x, pageY: p.y });
      const all = ${JSON.stringify(pts)}.map(mk);
      c.dispatchEvent(new TouchEvent('${ev}', { touches: all,
        targetTouches: all, changedTouches: ${JSON.stringify(changed)}.map(mk),
        bubbles: true, cancelable: true }));
      return 0; })()`);
  };
  let lastTapMs = 0;
  const tap = async (p) => {
    const t0 = Date.now();
    await touch('touchStart', [p]); await sleep(60);
    await touch('touchEnd', []);
    lastTapMs = Date.now() - t0;
    await sleep(250);
  };

  console.log('touch probe (412x860 @2x):');
  let s = await state();
  check(cv.w >= 480 && cv.cw <= 412.5, `canvas ${cv.w}x${cv.h} logical fits the phone (css ${cv.cw}x${cv.ch})`);
  check(s.z === 2, 'a touch-first device opens at 2x');
  check(s.map === 0, 'narrow view starts without the minimap');
  await cdp.screenshot(path.join(OUT, '1-portrait.png'));

  /* Lobby tool by keyboard (layout-independent), then tap the ground. */
  await cdp.key('l', 'KeyL', 76);
  await sleep(200);
  s = await state();
  const money0 = s.money;
  const center = Math.round(s.cx / CELL_W);
  const tp = cellAt(s, 0, center);
  await tap(tp);
  s = await until((st) => st.pending === 1);
  if (process.env.DEBUG) console.log('tap at', tp, 'held', lastTapMs, 'ms', JSON.stringify(s));
  check(s.pending === 1 && s.money === money0, 'tap with a build tool parks the ghost, charges nothing');
  await cdp.screenshot(path.join(OUT, '2-confirm.png'));

  /* drag along the ground row: builds at once */
  const a = cellAt(s, 0, center - 6), b = cellAt(s, 0, center + 6);
  await touch('touchStart', [a]);
  for (let k = 1; k <= 10; k++) {
    await touch('touchMove', [{ x: a.x + (b.x - a.x) * k / 10, y: a.y }]);
    await sleep(30);
  }
  await touch('touchEnd', []);
  s = await until((st) => st.money < money0);
  check(s.money < money0 && s.pending === 0, `drag builds straight away ($${money0 - s.money})`);

  /* long-press: tooltip only */
  const money1 = s.money;
  const lp = cellAt(s, 0, center);
  await touch('touchStart', [lp]); await sleep(800);
  await touch('touchEnd', []); await sleep(300);
  s = await state();
  check(s.money === money1 && s.pending === 0, 'long-press never builds');
  await cdp.screenshot(path.join(OUT, '3-longpress.png'));

  /* pinch out about the middle: one whole step in */
  const z0 = s.zoom;
  const mid = css(cv.w / 2, cv.h / 2);
  await touch('touchStart', [{ x: mid.x - 20, y: mid.y, id: 1 }]);
  await touch('touchStart', [{ x: mid.x - 20, y: mid.y, id: 1 }, { x: mid.x + 20, y: mid.y, id: 2 }]);
  for (let k = 1; k <= 10; k++) {
    await touch('touchMove', [{ x: mid.x - 20 - k * 2, y: mid.y, id: 1 },
                              { x: mid.x + 20 + k * 2, y: mid.y, id: 2 }]);
    await sleep(30);
  }
  /* lift finger 2, keep moving finger 1: must pan, not build */
  await touch('touchEnd', [{ x: mid.x - 40, y: mid.y, id: 1 }]);
  s = await until((st) => st.zoom > z0);
  await sleep(500);
  s = await state();
  check(s.zoom === z0 + 1, `pinch out 40->80px (x2) zooms one step (${z0} -> ${s.zoom})`);
  const cam0 = s.cx, money2 = s.money;
  for (let k = 1; k <= 8; k++) {
    await touch('touchMove', [{ x: mid.x - 40 - k * 10, y: mid.y, id: 1 }]);
    await sleep(30);
  }
  await touch('touchEnd', []);
  s = await until((st) => st.cx !== cam0);
  await sleep(500);
  s = await state();
  check(s.cx !== cam0 && s.money === money2, 'finger left after a pinch pans, never builds');

  /* rotate to landscape */
  await phone(860, 412);
  await until((st) => st.sw > st.sh, 8000);

  if (process.env.DEBUG) console.log('after rotate', await cdp.eval(
    `JSON.stringify([innerWidth, innerHeight, visualViewport && visualViewport.width, getComputedStyle(document.documentElement).getPropertyValue('--ct-css-w')])`));
  cv = await canvas();
  s = await state();
  check(s.sw === cv.w && s.sh === cv.h && s.sw > s.sh,
        `rotation re-sizes the engine view (${s.sw}x${s.sh})`);
  await cdp.screenshot(path.join(OUT, '4-landscape.png'));

  console.log(fails ? `${fails} FAILURE(S)` : 'all touch checks passed');
  await cdp.close();
  cleanup();
  process.exit(fails ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(2); });
