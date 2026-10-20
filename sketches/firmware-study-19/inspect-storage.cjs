const {chromium}=require('../../benchmark/web/node_modules/playwright');
const fs=require('node:fs/promises'),path=require('node:path'),assert=require('node:assert/strict');
(async()=>{
 const out=path.resolve(__dirname,'../../build/study19');
 const browser=await chromium.launch({headless:true});
 try{
  const page=await browser.newPage({viewport:{width:1440,height:1100}}),errors=[],requests=[],checks=[];
  page.on('pageerror',e=>errors.push(e.message));
  await page.route('http**/*',r=>{requests.push(r.request().url());r.abort()});
  await page.clock.install();
  await page.goto('file://'+out+'/RSVP-Nano-UI-Preview-19.html');await page.evaluate(()=>document.fonts.ready);
  await page.selectOption('#scale','native');
  const click=s=>page.locator('#screen '+s).click();
  const snap=name=>page.locator('#screen').screenshot({path:out+'/'+name+'.png',animations:'disabled'});
  const check=async(name,fn)=>{await fn();checks.push(name);console.log('PASS: '+name);};
  await check('Card check precedes practice, waits for results and cannot be advanced while busy',async()=>{
   const original=await page.evaluate(()=>({word:state.word,wpm:state.wpm}));
   await click('[data-action="intro-next"]');
   assert.equal(await page.evaluate(()=>tour.storageVisible),true);
   assert.equal(await page.evaluate(()=>sdCheck.status),'checking');
   assert.equal(await page.locator('#screen [data-action="sd-retry"]').isDisabled(),true);
   await page.evaluate(()=>{action('intro-next');action('finish-intro');toggleReading()});
   assert.equal(await page.evaluate(()=>tour.step),0);
   assert.equal(await page.evaluate(()=>introSeen),null);
   await snap('sd-checking');
   await page.clock.runFor(1800);
   assert.equal(await page.evaluate(()=>sdCheck.status),'ready');
   assert.equal(await page.locator('#screen .passed').count(),3);
   assert.equal(await page.evaluate(()=>tour.storageVisible),true);
   await snap('sd-ready');await click('[data-action="sd-continue"]');
   assert.equal(await page.evaluate(()=>tour.step),1);
   await click('[data-action="intro-back"]');assert.equal(await page.evaluate(()=>sdCheck.status),'ready');
   await page.locator('#boot').click();assert.equal(await page.evaluate(()=>tour.playing),false);
   await click('[data-action="sd-continue"]');
   assert.deepEqual(await page.evaluate(()=>({word:state.word,wpm:state.wpm})),original);
  });
  await check('Failure stops at the failed check; pending stages are not reported as passed',async()=>{
   for(const [scenario,passed] of [['unavailable',0],['folders',1],['write',2],['timeout',2]]){
    await page.selectOption('#sd-scenario',scenario);await page.clock.runFor(3800);
    assert.equal(await page.evaluate(()=>sdCheck.status),scenario);
    assert.equal(await page.locator('#screen .passed').count(),passed);
    assert.equal(await page.locator('#screen .failed').count(),1);
    assert.equal(await page.locator('#screen .pending').count(),2-passed);
    assert.equal(await page.locator('#screen [data-action="sd-continue"]').count(),0);
    assert.equal(await page.locator('#screen [data-action="sd-demo"]').count(),1);
    assert.equal(await page.evaluate(()=>sdTimer),null);
   }
  });
  await check('Demo and exit after a failure never record first-time setup completion',async()=>{
   await page.selectOption('#sd-scenario','write');await page.clock.runFor(1800);
   await click('[data-action="sd-demo"]');assert.equal(await page.evaluate(()=>tour.step),1);
   await page.evaluate(()=>exitIntroduction('completed'));
   assert.equal(await page.evaluate(()=>introSeen),null);
   assert.equal(await page.evaluate(()=>localStorage.getItem(INTRO_KEY)),null);
   await page.locator('[data-action="simulate-restart"]').click();
   assert.equal(await page.evaluate(()=>state.view),'onboarding');assert.equal(await page.evaluate(()=>tour.step),0);
   await click('[data-action="intro-next"]');await page.clock.runFor(1800);
   assert.equal(await page.evaluate(()=>sdCheck.status),'write');
  });
  await check('Retry, cancellation and scenario replacement discard old callbacks',async()=>{
   await click('[data-action="sd-retry"]');assert.equal(await page.evaluate(()=>sdCheck.status),'checking');
   await click('[data-action="sd-back"]');await page.clock.runFor(5000);
   assert.equal(await page.evaluate(()=>tour.storageVisible),false);assert.equal(await page.evaluate(()=>tour.step),0);
   await page.selectOption('#sd-scenario','timeout');await page.clock.runFor(1150);
   await page.selectOption('#sd-scenario','ready');await page.clock.runFor(5000);
   assert.equal(await page.evaluate(()=>sdCheck.status),'ready');assert.equal(await page.locator('#screen .passed').count(),3);
   await click('[data-action="sd-continue"]');await page.evaluate(()=>exitIntroduction('completed'));
   assert.equal(await page.evaluate(()=>JSON.parse(localStorage.getItem(INTRO_KEY)).status),'completed');
   await page.reload();assert.equal(await page.evaluate(()=>state.view),'menu');
  });
  const clipped=[];
  const layout=async(locale,theme,scenario)=>{
   await page.selectOption('#scale','native');
   await page.selectOption('#preview-language',locale);await page.selectOption('#theme',theme);
   await page.selectOption('#sd-scenario',scenario);await page.clock.runFor(3800);
   const issues=await page.locator('#screen').evaluate(screen=>{
    const bounds=screen.getBoundingClientRect();return [...screen.querySelectorAll('.sd-copy19,.sd-copy19 h2,.sd-copy19 p,.sd-copy19 small,.sd-checklist19 strong,.sd-row-status19,button')].filter(el=>{
     const r=el.getBoundingClientRect();return el.scrollWidth>el.clientWidth+1||el.scrollHeight>el.clientHeight+1||r.bottom>bounds.bottom+1||r.right>bounds.right+1;
    }).map(el=>el.textContent);
   });clipped.push(...issues.map(text=>({locale,theme,scenario,text})));
   await snap('sd-'+locale+'-'+theme+'-'+scenario);
  };
  await check('Every result fits all three languages, with success and error reviewed in five themes',async()=>{
   for(const locale of ['en','de','ro'])for(const scenario of ['ready','unavailable','folders','write','timeout'])await layout(locale,'ink',scenario);
   for(const theme of ['paper','nord','dracula','gruvbox-dark'])for(const scenario of ['ready','write'])await layout('en',theme,scenario);
   assert.deepEqual(clipped,[]);
  });
  await check('Leaving or restarting during a check cannot produce a late result',async()=>{
   await page.selectOption('#sd-scenario','timeout');await page.clock.runFor(1150);
   await page.locator('[data-action="simulate-restart"]').click();await page.clock.runFor(5000);
   assert.equal(await page.evaluate(()=>state.view),'menu');
   await page.locator('[data-action="try-sd-check"]').click();await page.evaluate(()=>navigate('help'));await page.clock.runFor(5000);
   assert.equal(await page.evaluate(()=>state.view),'help');assert.equal(await page.evaluate(()=>sdTimer),null);
  });
  assert.deepEqual(errors,[]);assert.deepEqual(requests,[]);
  await fs.writeFile(out+'/storage-verification.json',JSON.stringify({checks,errors,requests,clipped},null,2));
  console.log(JSON.stringify({checks:checks.length,errors,requests,clipped},null,2));
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
