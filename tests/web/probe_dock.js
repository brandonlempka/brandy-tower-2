/* Probe: the Modern UI build dock (BrandyTower's palette) on a phone and
 * on a desktop.
 *
 * Phone (412x860 @2x, touch): the dock is a strip along the bottom edge;
 *   - tapping a slot picks that item (and a greyed one does not)
 *   - a sideways swipe slides the strip and neither builds nor pans
 *   - a long-press on a slot pins its tooltip (screenshot)
 *   - a tap on the tower parks the ghost: the confirm box's nudge pads are
 *     thumb-sized (screenshot)
 * Desktop (1280x800, mouse): the dock is a column down the left edge;
 *   - hovering a slot shows its tooltip beside the column (screenshot)
 *   - the wheel over the dock scrolls it, not the tower
 * Screenshots: tests/web/out-dock/
 *
 * Run: node tests/web/probe_dock.js   (after `make web`)
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
const OUT = path.join(__dirname, 'out-dock');
const PORT = 8623;
const URL_BASE = `http://127.0.0.1:${PORT}/index.html`;
const EXE = process.env.CT_EXE || path.join(REPO, 'web/dev-assets/SIMTOWER.EXE');
const ITEM = { LOBBY: 1, FLOOR: 2, OFFICE: 3, METRO: 13 };

let fails = 0;
function check(cond, msg) {
  console.log(`  ${cond ? 'ok  ' : 'FAIL'} ${msg}`);
  if (!cond) fails++;
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  fs.rmSync(OUT, { recursive: true, force: true });
  fs.mkdirSync(OUT, { recursive: true });
  try { execSync(`lsof -ti tcp:${PORT} | xargs kill 2>/dev/null`); } catch (e) {}
  const server = spawn('python3', ['-m', 'http.server', String(PORT), '--bind', '127.0.0.1'],
    { cwd: DIST, stdio: 'ignore' });
  const cdp = new Cdp({
    profile: path.join(OUT, 'profile'),
    chromeLog: path.join(OUT, 'chrome.log'),
  });
  process.on('exit', () => {
    try { server.kill(); } catch (e) {}
    try { cdp.chrome.kill(); } catch (e) {}
  });
  await sleep(800);
  cdp.launch();
  const sid = await cdp.openPage('about:blank');

  const state = async () => JSON.parse(await cdp.eval(
    `Module.ccall('ct_view_state','string',[],[])`));
  const dock = async () => JSON.parse(await cdp.eval(
    `Module.ccall('ct_dock_state','string',[],[])`));
  const until = async (pred, get = state, ms = 5000) => {
    const end = Date.now() + ms;
    let st = await get();
    while (!pred(st) && Date.now() < end) { await sleep(100); st = await get(); }
    return st;
  };
  let cv;
  const measure = async () => {
    cv = await cdp.eval(`(() => { const c = Module.canvas, r = c.getBoundingClientRect();
      return { w: c.width, h: c.height, cw: r.width, ch: r.height, x: r.left, y: r.top,
               lw: JSON.parse(Module.ccall('ct_view_state','string',[],[])).sw,
               lh: JSON.parse(Module.ccall('ct_view_state','string',[],[])).sh }; })()`);
  };
  /* logical canvas px -> CSS px */
  const css = (lx, ly) => ({ x: cv.x + lx * cv.cw / cv.lw, y: cv.y + ly * cv.ch / cv.lh });
  const centre = (r) => css(r[0] + r[2] / 2, r[1] + r[3] / 2);
  const slotOf = (d, item) => d.slots.find((s) => s.item === item);

  async function boot(width, height, scale, mobile) {
    await cdp.send('Emulation.setDeviceMetricsOverride',
      { width, height, deviceScaleFactor: scale, mobile }, sid);
    await cdp.send('Emulation.setTouchEmulationEnabled',
      mobile ? { enabled: true, maxTouchPoints: 5 } : { enabled: false }, sid);
    await cdp.navigate(URL_BASE);
    await cdp.eval(`(localStorage.setItem('ct-ui', 'modern'), 0)`);
    await cdp.navigate(URL_BASE);
    await cdp.waitFor(`typeof Module!=='undefined' && Module.engineReady===true`, 90000);
    await cdp.setFileInput('#exefile', [EXE]);
    await cdp.eval(`(document.getElementById('exefile')
        .dispatchEvent(new Event('change')), 0)`);
    await cdp.waitFor(`!document.getElementById('startbtn').disabled`, 30000);
    await cdp.eval(`(document.getElementById('startbtn').click(), 0)`);
    await cdp.waitFor(`document.getElementById('canvaswrap').style.display==='block'`, 30000);
    await sleep(2500);
    await measure();
  }

  /* Touch events built in the page (as probe_touch.js does). */
  let down = [];
  const touch = async (type, pts) => {
    pts = pts.map((p, i) => ({ x: p.x, y: p.y, id: p.id ?? i }));
    let changed;
    if (type === 'touchStart') changed = pts.filter((p) => !down.some((d) => d.id === p.id));
    else if (type === 'touchMove') changed = pts;
    else changed = down.filter((d) => !pts.some((p) => p.id === d.id));
    down = pts;
    await cdp.eval(`(() => { const c = Module.canvas;
      const mk = (p) => new Touch({ identifier: p.id, target: c,
        clientX: p.x, clientY: p.y, pageX: p.x, pageY: p.y });
      c.dispatchEvent(new TouchEvent('${type.toLowerCase()}', {
        touches: ${JSON.stringify(pts)}.map(mk), targetTouches: ${JSON.stringify(pts)}.map(mk),
        changedTouches: ${JSON.stringify(changed)}.map(mk), bubbles: true, cancelable: true }));
      return 0; })()`);
  };
  const tap = async (p) => {
    await touch('touchStart', [p]); await sleep(60);
    await touch('touchEnd', []); await sleep(300);
  };
  const mouse = async (type, p, extra = {}) => {
    await cdp.send('Input.dispatchMouseEvent', Object.assign({
      type, x: p.x, y: p.y, button: type === 'mouseMoved' ? 'none' : 'left',
      buttons: type === 'mousePressed' ? 1 : 0, clickCount: 1 }, extra), sid);
    await sleep(80);
  };

  /* ------------------------------------------------------------ phone */
  console.log('dock probe — phone (412x860 @2x):');
  await boot(412, 860, 2, true);
  let d = await dock();
  let s = await state();
  check(s.ui === 1 && d.active === 1, 'Modern UI on, dock active');
  check(d.horizontal === 1, 'narrow view: the dock is a bottom strip');
  check(d.view[1] + d.view[3] === s.sh && d.view[2] === s.sw,
        `strip spans the bottom edge (view ${d.view})`);
  check(d.max > 0, `strip overflows and slides (max scroll ${d.max})`);
  await cdp.screenshot(path.join(OUT, '1_phone_dock.png'));

  const lobby = slotOf(d, ITEM.LOBBY);
  await tap(centre(lobby.r));
  s = await until((st) => st.build === ITEM.LOBBY);
  check(s.build === ITEM.LOBBY, 'tap on "Lobby" picks the lobby');

  /* swipe the strip left: it slides, the tower doesn't move */
  const cam0 = { cx: s.cx, cy: s.cy, zoom: s.zoom, money: s.money };
  const a = centre(lobby.r);
  await touch('touchStart', [a]);
  for (let k = 1; k <= 10; k++) {
    await touch('touchMove', [{ x: a.x - k * 25, y: a.y }]);
    await sleep(16);
  }
  await touch('touchEnd', []);
  d = await until((dd) => dd.scroll > 100, dock, 3000);
  s = await state();
  check(d.scroll > 100, `swipe slid the strip (scroll ${d.scroll})`);
  check(s.cx === cam0.cx && s.cy === cam0.cy && s.zoom === cam0.zoom,
        'swipe did not pan or zoom the tower');
  check(s.money === cam0.money && s.build === ITEM.LOBBY,
        'swipe neither built nor changed the tool');
  await sleep(800);                       /* let the fling settle */
  await cdp.screenshot(path.join(OUT, '2_phone_slid.png'));

  /* a locked item (Metro, 4 stars) stays greyed and unpicked */
  /* slide until Metro (near the far end) is wholly on screen */
  d = await dock();
  for (let tries = 0; tries < 8; tries++) {
    const r = slotOf(d, ITEM.METRO).r;
    if (r[0] >= 0 && r[0] + r[2] <= d.view[2]) break;
    const y = centre(r).y;
    await touch('touchStart', [{ x: cv.x + cv.cw * 0.9, y }]);
    for (let k = 1; k <= 12; k++) {
      await touch('touchMove', [{ x: cv.x + cv.cw * 0.9 - k * 25, y }]);
      await sleep(16);
    }
    await touch('touchEnd', []);
    await sleep(1200);
    d = await dock();
  }
  const metroNow = slotOf(d, ITEM.METRO);
  await tap(centre(metroNow.r));
  s = await state();
  check(metroNow.r[0] + metroNow.r[2] <= d.view[2], `Metro slid into view (x ${metroNow.r[0]})`);
  check(s.build === ITEM.LOBBY && s.pending === 0,
        'tap on locked Metro leaves the tool alone and builds nothing');

  /* long-press a slot: its tooltip pins above the strip */
  const lp = centre(metroNow.r);
  await touch('touchStart', [lp]); await sleep(800);
  await touch('touchEnd', []); await sleep(400);
  await cdp.screenshot(path.join(OUT, '3_phone_longpress_tip.png'));

  /* tap the tower with the lobby tool: the confirm box opens */
  await tap(css(s.sw / 2, s.sh * 0.55));
  s = await until((st) => st.pending === 1);
  check(s.pending === 1, 'tap on the tower parks the ghost (confirm box)');
  await sleep(300);
  await cdp.screenshot(path.join(OUT, '4_phone_confirm.png'));

  /* ---------------------------------------------------------- desktop */
  console.log('dock probe — desktop (1280x800):');
  await boot(1280, 800, 1, false);
  d = await dock();
  s = await state();
  check(d.active === 1 && d.horizontal === 0, 'wide view: the dock is a side column');
  check(d.view[0] === 0 && d.view[3] + d.view[1] === s.sh, `column spans the left edge (view ${d.view})`);
  const office = slotOf(d, ITEM.OFFICE);
  await mouse('mouseMoved', centre(office.r));
  await sleep(900);
  await cdp.screenshot(path.join(OUT, '5_desktop_hover_tip.png'));

  /* click picks; wheel scrolls the dock and not the tower */
  await mouse('mousePressed', centre(office.r));
  await mouse('mouseReleased', centre(office.r));
  s = await until((st) => st.build === ITEM.OFFICE);
  check(s.build === ITEM.OFFICE, 'click on "Office" picks the office');
  const cy0 = s.cy;
  await cdp.send('Input.dispatchMouseEvent', { type: 'mouseWheel',
    x: centre(office.r).x, y: centre(office.r).y, deltaX: 0, deltaY: 300 }, sid);
  d = await until((dd) => dd.scroll > 0, dock, 3000);
  s = await state();
  check(d.scroll > 0, `wheel scrolled the column (scroll ${d.scroll})`);
  check(s.cy === cy0, 'wheel over the column left the tower where it was');
  await sleep(700);
  await cdp.screenshot(path.join(OUT, '6_desktop_scrolled.png'));

  console.log(fails ? `${fails} FAILURE(S)` : 'all dock checks passed');
  process.exit(fails ? 1 : 0);
})().catch((e) => { console.error(e); process.exit(1); });
