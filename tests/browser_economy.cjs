// Real Chromium input/persistence. The only write adapter installs an isolated
// initial save before the first journey. During play hooks are read-only.
// Build: python scripts/build_web.py --test-hooks --output dist/economy-test
// Fixture: build-economy/paldo-economy-tests build-economy/economy-start.sav
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const root = path.resolve(process.argv[2] || 'dist/economy-test');
const output = path.resolve(process.argv[3] || 'build-economy/browser-qa');
const fixture = fs.readFileSync(process.argv[4] || 'build-economy/economy-start.sav').toString('hex');
const key = 'paldoyadam.journey.autosave';
async function state(p) { return p.evaluate(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state()))); }
async function phase(p, n) { await p.waitForFunction(n => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).phase === n, n); }
async function click(p, action, value = 0) {
  await p.waitForFunction(({action,value}) => JSON.parse(Module.UTF8ToString(Module._paldo_test_buttons())).some(b => b.action === action && b.value === value), {action,value});
  const b = await p.evaluate(({action,value}) => JSON.parse(Module.UTF8ToString(Module._paldo_test_buttons())).find(b => b.action === action && b.value === value), {action,value});
  await p.mouse.click(b.x + b.w/2, b.y + b.h/2);
}
async function walk(p, goal) {
  const points = await p.evaluate(([x,y]) => JSON.parse(Module.UTF8ToString(Module._paldo_test_route(x,y))), goal);
  assert(points.length, 'Reachable route');
  for (const point of points) {
    let reached = false;
    for (let step=0;step<350;step++) {
      const s = await state(p), dx=point[0]-s.x, dy=point[1]-s.y;
      assert.equal(s.phase,2,'Walking stays in play'); assert(s.hp>0);
      const d=Math.hypot(dx,dy); if(d<10) {reached=true;break;}
      const keys=[];
      if(Math.abs(dx)>5) keys.push(dx>0?'ArrowRight':'ArrowLeft');
      if(Math.abs(dy)>5) keys.push(dy>0?'ArrowDown':'ArrowUp');
      for (const k of keys) await p.keyboard.down(k);
      await p.waitForTimeout(d>35?100:30);
      for (const k of keys) await p.keyboard.up(k);
    }
    assert(reached,'Route waypoint reached');
  }
}
(async()=>{
  fs.mkdirSync(output,{recursive:true});
  const profile=fs.mkdtempSync(path.join(output,'profile-'));
  const errors=[];
  const server=http.createServer((req,res)=>{
    const url=new URL(req.url,'http://localhost');
    if(url.pathname==='/favicon.ico'){res.writeHead(204);res.end();return;}
    const file=path.resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
    if(!file.startsWith(root+path.sep)){res.writeHead(403);res.end();return;}
    fs.stat(file,(error,stat)=>{
      if(error||!stat.isFile()){res.writeHead(404);res.end();return;}
      res.writeHead(200,{'Content-Type':{'.html':'text/html','.js':'text/javascript','.wasm':'application/wasm','.data':'application/octet-stream'}[path.extname(file)]||'application/octet-stream'});
      fs.createReadStream(file).pipe(res);
    });
  });
  let context;
  try {
    await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
    const url='http://127.0.0.1:'+server.address().port;
    const options={executablePath:process.env.PALDO_BROWSER||undefined,headless:true,viewport:{width:1100,height:650}};
    context=await chromium.launchPersistentContext(profile,options);
    await context.addInitScript(({key,fixture})=>{if(!localStorage.getItem(key))localStorage.setItem(key,fixture);},{key,fixture});
    let page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));
    await page.goto(url);await page.waitForSelector('body[data-game-ready="true"]',{timeout:90000});
    await click(page,'continue');await phase(page,2);
    assert.equal((await state(page)).coins,55);
    await page.keyboard.down('a');
    await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).huntHp0===0);
    await page.keyboard.up('a');
    await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).leather===1);
    const picked=await state(page);assert.equal(picked.coins,64);assert.equal(picked.herbs,0);assert.equal(picked.quest,2);
    await page.keyboard.press('i');await phase(page,5);await click(page,'bag-tab',2);
    await page.screenshot({path:path.join(output,'materials-bag.png')});await click(page,'close');await phase(page,2);
    await walk(page,[1152,1410]);await page.keyboard.press('Space');
    await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).map===0);
    await walk(page,[.21*2304,.285*1536]);await page.keyboard.press('Space');await phase(page,4);
    await click(page,'smith-tab',1);await page.screenshot({path:path.join(output,'materials-sale.png')});
    await click(page,'sell-one');await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).coins===67);
    assert.equal((await state(page)).leather,0);await click(page,'close');await phase(page,2);
    await page.keyboard.press('Escape');await phase(page,10);await click(page,'title');await phase(page,0);
    await page.reload();await page.waitForSelector('body[data-game-ready="true"]');await click(page,'continue');await phase(page,2);
    assert.equal((await state(page)).coins,67);assert.equal((await state(page)).leather,0);
    await page.keyboard.press('F8');await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).training);
    const before=await page.evaluate(k=>localStorage.getItem(k),key);
    await page.keyboard.press('a');await page.waitForTimeout(1500);
    assert.equal(await page.evaluate(k=>localStorage.getItem(k),key),before);
    await page.keyboard.press('F8');await page.keyboard.press('Escape');await phase(page,10);
    await context.close();context=null;
    context=await chromium.launchPersistentContext(profile,options);page=await context.newPage();
    await page.goto(url);await page.waitForSelector('body[data-game-ready="true"]');await click(page,'continue');await phase(page,2);
    const restored=await state(page);assert.equal(restored.coins,67);assert.equal(restored.leather,0);assert.equal(restored.herbs,0);
    await page.screenshot({path:path.join(output,'economy-after-restart.png')});assert.deepEqual(errors,[]);
    console.log('PASS real Chromium hunt, pickup, bag, village sale, refresh, process restart and training isolation');
  } finally { if(context)await context.close();await new Promise(resolve=>server.close(resolve)); }
})().catch(e=>{console.error(e);process.exitCode=1;});
