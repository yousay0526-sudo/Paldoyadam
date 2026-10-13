// Run after scripts/build_web.py --test-hooks --output dist/test.
// Requires Playwright. PALDO_BROWSER can select an installed Chromium executable.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');

const root = path.resolve(process.argv[2] || 'dist/test');
const shots = process.env.PALDO_SCREENSHOTS;
const errors = [];
async function state(page) {
  return page.evaluate(() => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())));
}
async function phase(page, value) {
  await page.waitForFunction(v => JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).phase === v, value);
}
async function shot(page, name) {
  if (shots) { fs.mkdirSync(shots, {recursive:true}); await page.screenshot({path:path.join(shots,name+'.png')}); }
}

(async () => {
  const server = http.createServer((req,res) => {
    const url = new URL(req.url, 'http://localhost');
    const file = path.resolve(root, '.' + decodeURIComponent(url.pathname === '/' ? '/index.html' : url.pathname));
    if (!file.startsWith(root + path.sep)) {res.writeHead(403);res.end();return;}
    fs.stat(file,(error,stat)=>{
      if(error || !stat.isFile()){res.writeHead(404);res.end();return;}
      const mime = {'.html':'text/html','.js':'text/javascript','.wasm':'application/wasm','.data':'application/octet-stream'};
      res.writeHead(200,{'Content-Type':mime[path.extname(file)]||'application/octet-stream','Content-Length':stat.size});
      fs.createReadStream(file).pipe(res);
    });
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  let browser;
  try {
    browser=await chromium.launch({executablePath:process.env.PALDO_BROWSER||undefined,
      args:['--no-sandbox','--disable-dev-shm-usage']});
    const url='http://127.0.0.1:'+server.address().port;
    for (const mobile of [false,true]) {
      const width=mobile?844:1100, height=mobile?390:650;
      const context=await browser.newContext({viewport:{width,height},deviceScaleFactor:mobile?2:1,
        isMobile:mobile,hasTouch:mobile});
      const page=await context.newPage();
      page.on('pageerror',e=>errors.push(e.message));
      page.on('console',m=>{if(m.type()==='error')errors.push(m.text());});
      await page.goto(url);
      await page.waitForSelector('body[data-game-ready="true"]',{timeout:90000});
      let s=await state(page);
      assert.equal(s.phase,0); assert.equal(Math.round(s.width),width); assert.equal(Math.round(s.height),height);
      assert.equal(s.density,mobile?2:1);
      await shot(page,mobile?'mobile-title':'desktop-title');
      if (mobile) await page.touchscreen.tap(width*.75,height*.35+25);
      else await page.keyboard.press('Enter');
      await phase(page,1);
      await page.waitForTimeout(550);
      await shot(page,mobile?'mobile-prologue':'desktop-prologue');
      // Advance all five narrative panels using real keyboard/touch input.
      for(let i=0;i<5;i++) {
        if(mobile) await page.touchscreen.tap(width-98,height-100);
        else await page.keyboard.press('Enter');
        await page.waitForTimeout(120);
      }
      await phase(page,2);
      s=await state(page);
      assert.equal(s.clear,true);
      if(mobile) {
        const cdp=await context.newCDPSession(page);
        const before=await state(page);
        await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:120,y:240}]});
        await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[{x:190,y:240}]});
        await page.waitForTimeout(500);
        await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
        const after=await state(page);
        assert(after.x>before.x+20,'Touch drag must move the C player to the right');
        assert.equal(after.clear,true);
        await page.setViewportSize({width:390,height:844});
        await page.waitForTimeout(150);
        const portrait=await state(page);
        assert(portrait.height>portrait.width);
        await page.keyboard.down('w');await page.waitForTimeout(250);await page.keyboard.up('w');
        assert.equal((await state(page)).y,portrait.y,'Portrait mode must pause movement');
        await shot(page,'portrait');
        await page.setViewportSize({width:844,height:390});
        await page.waitForTimeout(150);
      } else {
        const before=await state(page);
        await page.keyboard.down('w'); await page.waitForTimeout(450); await page.keyboard.up('w');
        assert((await state(page)).y<before.y-20,'Keyboard movement must reach the C game');
        await page.keyboard.press('i'); await phase(page,5);
        await shot(page,'bag');
        await page.keyboard.press('Escape'); await phase(page,2);
        await page.keyboard.press('j'); await phase(page,6);
        await page.keyboard.press('Escape'); await phase(page,2);
        await page.keyboard.press('e'); await phase(page,3);
        await shot(page,'dialogue');
        for(let i=0;i<3;i++){await page.keyboard.press('Enter');await page.waitForTimeout(100);}
        await phase(page,2);
        assert.equal((await state(page)).quest,1,'Elder dialogue must accept the quest');
        await page.setViewportSize({width:960,height:540});await page.waitForTimeout(150);
        assert.equal(Math.round((await state(page)).width),960);
        await page.mouse.click(960-83,32);
        await page.waitForFunction(()=>!!document.fullscreenElement,{},{timeout:5000});
        await page.evaluate(()=>document.exitFullscreen());
        await page.waitForTimeout(150);
      }
      assert.equal((await state(page)).clear,true);
      await shot(page,mobile?'mobile-village':'desktop-village');
      console.log('PASS browser',mobile?'mobile touch/DPR2/rotation':'desktop keyboard/dialogue/panels/fullscreen',await state(page));
      await context.close();
    }
    assert.deepEqual(errors,[],'No browser or console errors');
  } finally {
    if(browser) await browser.close();
    await new Promise(resolve=>server.close(resolve));
  }
})().catch(e=>{console.error(e);process.exitCode=1;});
