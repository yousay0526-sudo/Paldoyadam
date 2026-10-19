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
async function hiddenTaps(context, page) {
  const entry=await context.newCDPSession(page), taps=[];
  for(let tap=0;tap<7;tap++) {
    taps.push(entry.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:60,y:18}]}));
    taps.push(entry.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]}));
  }
  await Promise.all(taps);
  await entry.detach();
  await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).training);
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
  let browser, currentPage;
  try {
    browser=await chromium.launch({executablePath:process.env.PALDO_BROWSER||undefined,
      args:['--no-sandbox','--disable-dev-shm-usage']});
    const url='http://127.0.0.1:'+server.address().port;
    for (const mobile of [false,true]) {
      const width=mobile?844:1100, height=mobile?390:650;
      const context=await browser.newContext({viewport:{width,height},deviceScaleFactor:mobile?2:1,
        isMobile:mobile,hasTouch:mobile});
      const page=await context.newPage();
      currentPage=page;
      page.on('pageerror',e=>errors.push(e.message));
      page.on('console',m=>{if(m.type()==='error')errors.push(m.text());});
      await page.goto(url);
      await page.waitForSelector('body[data-game-ready="true"]',{timeout:90000});
      let s=await state(page);
      assert.equal(s.phase,0); assert.equal(Math.round(s.width),width); assert.equal(Math.round(s.height),height);
      assert.equal(s.density,mobile?2:1);
      // The backing canvas and the CSS viewport must agree on both axes.
      // An old back buffer stretched to Safari's new viewport distorted art/text.
      async function canvasAspectMatches() {
        return page.evaluate(() => {
          const canvas = document.querySelector('#canvas');
          const box = canvas.getBoundingClientRect();
          if (!canvas.width || !canvas.height || !box.width || !box.height) return false;
          return Math.abs(canvas.width / canvas.height - box.width / box.height) < 0.015;
        });
      }
      await page.waitForFunction(() => {
        const canvas = document.querySelector('#canvas'), box = canvas.getBoundingClientRect();
        return canvas.width > 0 && canvas.height > 0 &&
          Math.abs(canvas.width / canvas.height - box.width / box.height) < 0.015;
      });
      if (mobile) {
        // Mimic iPhone browser chrome reducing available height while in landscape.
        await page.setViewportSize({width:844,height:320});
        await page.waitForFunction(() => {
          const canvas = document.querySelector('#canvas'), box = canvas.getBoundingClientRect();
          return Math.abs(canvas.width / canvas.height - box.width / box.height) < 0.015;
        });
        assert(await canvasAspectMatches(),'Mobile canvas must preserve aspect after viewport height changes');
        await page.setViewportSize({width:844,height:390});
        await page.waitForFunction(() => {
          const canvas = document.querySelector('#canvas'), box = canvas.getBoundingClientRect();
          return Math.abs(canvas.width / canvas.height - box.width / box.height) < 0.015;
        });
      }
      await shot(page,mobile?'mobile-title':'desktop-title');
      if (mobile) await page.touchscreen.tap(width*.75,height*.35+25);
      else await page.keyboard.press('Space');
      await phase(page,1);
      await page.waitForTimeout(550);
      await shot(page,mobile?'mobile-prologue':'desktop-prologue');
      // Advance all five narrative panels using real keyboard/touch input.
      for(let i=0;i<5;i++) {
        if(mobile) await page.touchscreen.tap(width-98,height-100);
        else await page.keyboard.press('Space');
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
        await page.waitForTimeout(120);
        assert((await state(page)).heroFrame >= 0,'Touch movement must animate the C hero');
        await page.waitForTimeout(380);
        await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
        await page.waitForTimeout(80);
        const after=await state(page);
        assert(after.x>before.x+20,'Touch drag must move the C player to the right');
        assert.equal(after.clear,true);
        assert.equal(after.heroFrame,-1,'Releasing touch must restore the standing pose');
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
        await page.keyboard.down('w'); await page.waitForTimeout(120);
        assert((await state(page)).heroFrame >= 0,'Keyboard movement must animate the C hero');
        await page.waitForTimeout(330); await page.keyboard.up('w');
        await page.waitForTimeout(80);
        assert((await state(page)).y<before.y-20,'Keyboard movement must reach the C game');
        assert.equal((await state(page)).heroFrame,-1,'Releasing a movement key must restore the standing pose');
        await page.keyboard.press('i'); await phase(page,5);
        await shot(page,'bag');
        await page.keyboard.press('Escape'); await phase(page,2);
        await page.keyboard.press('j'); await phase(page,6);
        await page.keyboard.press('Escape'); await phase(page,2);
        await page.keyboard.press('Space'); await phase(page,3);
        assert.equal((await state(page)).dialogue,0,'Opening dialogue must not also skip a line');
        assert.equal((await state(page)).cooldown,0,'Space must not trigger the skill');
        await page.keyboard.down('Space');
        await page.keyboard.down('Space');
        await page.waitForTimeout(150);
        assert.equal((await state(page)).dialogue,1,'Repeated keydown must not skip another line');
        await page.keyboard.up('Space');
        await shot(page,'dialogue');
        for(let i=0;i<2;i++){await page.keyboard.press('Space');await page.waitForTimeout(100);}
        await phase(page,2);
        assert.equal((await state(page)).quest,1,'Elder dialogue must accept the quest');
        await page.keyboard.press('f');
        assert.equal((await state(page)).phase,2,'Pickup key must not reopen nearby NPC dialogue');
        assert.equal((await state(page)).herbs,0,'No loot in reach must not grant an item');
        await page.keyboard.press('q');
        assert((await state(page)).cooldown>0,'Q must trigger the skill');
        await page.setViewportSize({width:960,height:540});await page.waitForTimeout(150);
        assert.equal(Math.round((await state(page)).width),960);
        if(process.env.PALDO_SKIP_FULLSCREEN === '1') console.log('SKIP browser fullscreen (explicit PALDO_SKIP_FULLSCREEN=1)');
        else {
          await page.mouse.click(960-83,32);
          await page.waitForFunction(()=>!!document.fullscreenElement,{},{timeout:5000});
          await page.evaluate(()=>document.exitFullscreen());
          await page.waitForTimeout(150);
        }
      }
      assert.equal((await state(page)).clear,true);
      await shot(page,mobile?'mobile-village':'desktop-village');
      const original=await state(page);
      if(mobile) await hiddenTaps(context,page);
      else await page.keyboard.press('F8');
      await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).training);
      assert.equal((await state(page)).dummyHp,240);
      if(mobile) {
        const cdp=await context.newCDPSession(page);
        const point={x:728,y:274};
        await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[point]});
        point.y-=40;
        await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[point]});
        await page.waitForTimeout(100);
        point.y+=40;
        await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[point]});
        await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).combo===2);
        point.x-=40;
        await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[point]});
        await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).hits===3);
        point.x+=40;
        await cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[point]});
        assert.equal((await state(page)).queued,1);
        await page.setViewportSize({width:390,height:844});
        await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).height>800);
        const rotated=await state(page);
        assert.equal(rotated.queued,-2); assert.equal(rotated.attackDown,false);
        await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
        await page.setViewportSize({width:844,height:390});
        await page.waitForTimeout(700);
        assert.equal((await state(page)).hits,3,'Rotation must cancel the queued fourth attack');
      } else {
        await page.keyboard.press('z');
        await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).hits===1);
        await page.keyboard.press('r');
        await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).combo===2);
        await page.keyboard.press('q');
        await page.waitForFunction(()=>JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).hits===3);
      }
      const practice=await state(page);
      assert.equal(practice.combo,3); assert.equal(practice.misses,0); assert.equal(practice.dummyHp,240);
      assert.equal(practice.hp,100); assert.equal(practice.quest,0);
      await shot(page,mobile?'mobile-training':'desktop-training');
      if(mobile) {
        await page.touchscreen.tap(694,26);
        await page.waitForFunction(()=>!JSON.parse(Module.UTF8ToString(Module._paldo_test_state())).training);
        let returned=await state(page);
        for(const field of ['quest','x','y','hp','coins','herbs']) assert.equal(returned[field],original[field],field+' restored by exit button');
        await hiddenTaps(context,page);
        await page.touchscreen.tap(818,26); await phase(page,10);
        await page.touchscreen.tap(422,300); await phase(page,2);
      } else {
        await page.keyboard.press('Escape'); await phase(page,10);
        await page.keyboard.press('F8'); await phase(page,2);
      }
      const restored=await state(page);
      assert.equal(restored.training,false); assert.equal(restored.queued,-2); assert.equal(restored.attackDown,false);
      for(const field of ['quest','x','y','hp','coins','herbs']) assert.equal(restored[field],original[field],field+' restored');
      console.log('PASS browser',mobile?'mobile touch/DPR2/rotation/training'
        :'desktop keyboard/dialogue/panels/training/'+(process.env.PALDO_SKIP_FULLSCREEN === '1'?'fullscreen skipped':'fullscreen'),await state(page));
      await context.close();
    }
    assert.deepEqual(errors,[],'No browser or console errors');
  } catch(error) {
    if(currentPage && !currentPage.isClosed()) {
      console.error('Last browser state:',await state(currentPage));
      await shot(currentPage,'failure');
    }
    throw error;
  } finally {
    if(browser) await browser.close();
    await new Promise(resolve=>server.close(resolve));
  }
})().catch(e=>{console.error(e);process.exitCode=1;});
