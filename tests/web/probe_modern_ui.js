/* Probe: Options -> Modern UI in the WEB build (port addition, 2026-10-31).
 * Boots web/dist with a fixture tower, flips Options -> Modern UI through
 * the real menu, checks the shell remembered it (localStorage 'ct-ui' and
 * body.ui-modern), opens the Eval view and hovers a build-dock item, then
 * reloads and checks the next launch comes up modern (--ui=modern).
 *
 * Usage: CHROME=<chrome> SIMTOWER_EXE=<exe> node tests/web/probe_modern_ui.js
 * Output: tests/web/out-modern/ (screenshots + chrome.log)
 */
'use strict';
const fs = require('fs');
const path = require('path');
const { spawn, execSync } = require('child_process');
const { Cdp } = require('./cdp');

const REPO = path.resolve(__dirname, '../..');
const DIST = path.join(REPO, 'web/dist');
const OUT = path.join(__dirname, 'out-modern');
const PORT = 8616;
const URL_BASE = `http://127.0.0.1:${PORT}/index.html`;
const SAV = process.env.PROBE_SAV ||
            path.join(REPO, 'tests/fixtures/BARKLE4D.TDT');

/* Game-pixel spots at the 960x720 view (menu layout from main.c). */
const OPTIONS_MENU = [507, 9];
const MODERN_ITEM = [520, 178];     /* last Options item */
/* The Modern UI's build dock takes the left 96px, so the map window sits
 * just right of it (main.c DOCK_W, layout_windows). */
const DOCK_W = 96;
const MAP_TAB = [DOCK_W + 28, 289];  /* map window tab strip */
const EVAL_TAB = [DOCK_W + 78, 289];

function findExe() {
  const cands = [
    process.env.SIMTOWER_EXE,
    path.join(REPO, '../OpenSkyscraper/data/SIMTOWER.EXE'),
  ];
  for (const c of cands) if (c && fs.existsSync(c)) return c;
  throw new Error('SIMTOWER.EXE not found (set SIMTOWER_EXE)');
}

let fails = 0;
function check(cond, msg) {
  console.log((cond ? '  ok   ' : '  FAIL ') + msg);
  if (!cond) fails++;
}

(async () => {
  fs.rmSync(OUT, { recursive: true, force: true });
  fs.mkdirSync(OUT, { recursive: true });
  const exeStaged = path.join(DIST, 'SIMTOWER.EXE');
  const staged = !fs.existsSync(exeStaged);
  if (staged) fs.copyFileSync(findExe(), exeStaged);
  try { execSync(`lsof -ti tcp:${PORT} | xargs kill 2>/dev/null`); } catch (e) {}
  const server = spawn('python3', ['-m', 'http.server', String(PORT),
                                   '--bind', '127.0.0.1'],
                       { cwd: DIST, stdio: 'ignore' });
  const cdp = new Cdp({ profile: path.join(OUT, 'profile'),
                        chromeLog: path.join(OUT, 'chrome.log') });
  process.on('exit', () => {
    if (staged) try { fs.unlinkSync(exeStaged); } catch (e) {}
    try { server.kill(); } catch (e) {}
    try { cdp.chrome.kill(); } catch (e) {}
  });
  await new Promise(r => setTimeout(r, 800));
  cdp.launch(['--window-size=980,820']);

  async function boot(importSave) {
    await cdp.waitFor(`typeof Module!=='undefined' && Module.engineReady===true`, 90000);
    await cdp.setFileInput('#exefile', [exeStaged]);
    cdp.evalDetached(`document.getElementById('exefile')
        .dispatchEvent(new Event('change'))`);
    await cdp.waitFor(`!document.getElementById('startbtn').disabled`, 20000);
    if (importSave) {
      await cdp.setFileInput('#importfile', [SAV]);
      cdp.evalDetached(`document.getElementById('importfile')
          .dispatchEvent(new Event('change'))`);
      /* The shell asks for the tower's name in its in-page modal. */
      await cdp.waitFor(`document.getElementById('modal').style.display==='flex'`, 10000);
      await cdp.eval(`(function(){ document.getElementById('minput').value='Probe Tower';
        document.getElementById('mok').click(); return 1; })()`);
      await cdp.waitFor(`Array.from(document.querySelectorAll('#slotlist .slot'))
          .some(function(r){ return r.querySelector('.nm').textContent ===
            'Probe Tower' && r.className.indexOf('sel') >= 0; })`, 20000);
    }
    cdp.evalDetached(`document.getElementById('startbtn').click()`);
    await cdp.waitFor(`document.getElementById('canvaswrap').style.display==='block'`, 60000);
    await new Promise(r => setTimeout(r, 6000));
  }

  let map;
  const viewState = async () => JSON.parse(
    await cdp.eval(`Module.ccall('ct_view_state','string',[],[])`));
  const toView = (gx, gy) => ({ x: map.left + gx * (map.cssW / map.vw),
                                y: map.top + gy * (map.cssH / map.vh) });
  async function measure() {
    map = await cdp.eval(`(function(){
      var c = Module.canvas, r = c.getBoundingClientRect(),
          v = JSON.parse(Module.ccall('ct_view_state','string',[],[]));
      return { left: r.left, top: r.top, cssW: r.width, cssH: r.height,
               vw: v.sw, vh: v.sh,
               raw: v }; })()`);
  }
  async function mouse(type, gx, gy) {
    const v = toView(gx, gy);
    await cdp.send('Input.dispatchMouseEvent',
      { type, x: v.x, y: v.y, button: type === 'mouseMoved' ? 'none' : 'left',
        buttons: type === 'mouseReleased' ? 0 : 1, clickCount: 1 },
      cdp.sessionId);
    await new Promise(r => setTimeout(r, 150));
  }
  async function click(gx, gy) {
    await mouse('mouseMoved', gx, gy);
    await mouse('mousePressed', gx, gy);
    await mouse('mouseReleased', gx, gy);
    await new Promise(r => setTimeout(r, 600));
  }

  console.log('== first launch (classic)');
  await cdp.openPage(URL_BASE);
  await boot(true);
  await measure();
  console.log('view state:', JSON.stringify(map.raw));
  check(!(await cdp.eval(`document.body.classList.contains('ui-modern')`)) &&
        (await viewState()).ui === 0, 'starts classic');
  await cdp.screenshot(path.join(OUT, '1_classic.png'));

  console.log('== Options -> Modern UI');
  await click(...OPTIONS_MENU);
  await cdp.screenshot(path.join(OUT, '2_options_menu.png'));
  await click(...MODERN_ITEM);
  check((await viewState()).ui === 1, 'engine switched skins (ui_modern)');
  check(await cdp.eval(`localStorage.getItem('ct-ui')==='modern'`),
        'ctUiEvent stored ct-ui=modern');
  check(await cdp.eval(`document.body.classList.contains('ui-modern')`),
        'shell buttons switched to the glass style');
  await cdp.screenshot(path.join(OUT, '3_modern.png'));

  console.log('== Eval view + build dock');
  const mapMode = async () => (await viewState()).mode;
  await click(...EVAL_TAB);
  check(await mapMode() === 1, 'Eval tab hit-tests beside the dock (map_mode 1)');
  await click(...MAP_TAB);
  check(await mapMode() === 0, 'Map tab hit-tests beside the dock (map_mode 0)');
  await click(...EVAL_TAB);
  await cdp.screenshot(path.join(OUT, '4_modern_eval.png'));
  const dock = JSON.parse(await cdp.eval(`Module.ccall('ct_dock_state','string',[],[])`));
  check(dock.active === 1 && dock.horizontal === 0, 'build dock is a side column');
  const office = dock.slots.find((sl) => sl.item === 3);
  await mouse('mouseMoved', office.r[0] + office.r[2] / 2, office.r[1] + office.r[3] / 2);
  await new Promise(r => setTimeout(r, 900));
  await cdp.screenshot(path.join(OUT, '5_modern_dock_tooltip.png'));

  console.log('== reload: remembered?');
  await cdp.eval(`window.ctAutosave && 0`);
  await cdp.navigate(URL_BASE);
  await boot(false);
  await measure();
  check(await cdp.eval(`document.body.classList.contains('ui-modern')`) &&
        (await viewState()).ui === 1, 'second launch passes --ui=modern');
  await cdp.screenshot(path.join(OUT, '6_relaunch.png'));

  console.log(fails ? `\n${fails} FAILURE(S)` : '\nall checks passed');
  process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(1); });
