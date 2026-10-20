const {chromium}=require('../../benchmark/web/node_modules/playwright');
const fs=require('node:fs/promises');
const path=require('node:path');
const assert=require('node:assert/strict');
(async()=>{
 const out=path.resolve(__dirname,'../../build/study19');
 const browser=await chromium.launch({headless:true});
 const page=await browser.newPage({viewport:{width:1440,height:1100},deviceScaleFactor:1});
 const errors=[],requests=[],checks=[];
 page.on('pageerror',e=>errors.push(e.message));
 await page.route('http**/*',r=>{requests.push(r.request().url());r.abort()});
 await page.goto('file://'+out+'/RSVP-Nano-UI-Preview-19.html');
 await page.evaluate(()=>document.fonts.ready);
 const click=async selector=>{
  await page.locator('#screen '+selector).click();
  if(selector==='[data-action="intro-next"]'&&await page.evaluate(()=>!!tour?.storageVisible)){
   await page.locator('#screen [data-action="sd-continue"]').click();
  }
 };
 const get=fn=>page.evaluate(fn);
 const check=async(name,fn)=>{await fn();checks.push(name);console.log("PASS: "+name)};
 const snap=async name=>{await page.mouse.move(0,0);await page.locator('#screen').screenshot({path:out+'/'+name+'.png'})};
 const swipe=async(selector='.intro-practice')=>{
  const r=await page.locator('#screen '+selector).boundingBox();
  await page.mouse.move(r.x+r.width*.5,r.y+r.height*.85);await page.mouse.down();
  await page.mouse.move(r.x+r.width*.5,r.y+r.height*.1,{steps:8});await page.mouse.up();
 };
 await check('Fresh first launch, practice, swipe and completion',async()=>{
  assert.equal(await get(()=>state.view),'onboarding');assert.equal(await get(()=>tour.step),0);
  await page.screenshot({path:out+'/workbench.png',fullPage:true});await snap('welcome');
  const place=await get(()=>state.word),speed=await get(()=>state.wpm),brightness=await get(()=>state.brightness);
  await click('[data-action="intro-next"]');await snap('practice');
  assert.equal(await page.locator('#screen [data-action="intro-next"]').isDisabled(),true);
  await click('.intro-practice');assert.equal(await get(()=>tour.playing),true);
  await page.waitForTimeout(500);assert.ok(await get(()=>tour.index)>0);
  await click('.intro-practice');assert.equal(await get(()=>tour.playing),false);
  assert.equal(await page.locator('#screen [data-action="intro-next"]').isDisabled(),false);
  await snap('practice-paused');await click('[data-action="intro-next"]');await snap('swipe');
  await swipe();assert.equal(await get(()=>tour.controls),true);await snap('controls-found');
  await click('[data-action="intro-faster"]');assert.equal(await get(()=>tour.wpm),310);
  await page.locator('#screen .tour-brightness').focus();await page.keyboard.press('End');assert.equal(await get(()=>tour.brightness),100);
  const slider=page.locator('#screen .tour-brightness'),sr=await slider.boundingBox();
  await page.mouse.move(sr.x+sr.width/2,sr.y+sr.height*.7);await page.mouse.down();
  await page.mouse.move(sr.x+sr.width/2,sr.y+sr.height*.25,{steps:6});await page.mouse.up();
  assert.ok(Math.abs(await get(()=>tour.brightness)-76)<=1);
  const savedBrightness=await get(()=>tour.brightness);
  await page.mouse.move(sr.x+sr.width/2,sr.y+sr.height*.9);await page.mouse.down();
  await page.dispatchEvent('#screen','pointercancel',{pointerId:1});await page.mouse.up();
  assert.equal(await get(()=>tour.brightness),savedBrightness);
  for(const step of [3,4,5,6,7,8,9,10]){await click('[data-action="intro-next"]');assert.equal(await get(()=>tour.step),step);await snap('lesson-'+step);}
  await click('[data-action="finish-intro"]');assert.equal(await get(()=>state.view),'reader');
  assert.equal(await get(()=>state.word),place);assert.equal(await get(()=>state.wpm),speed);assert.equal(await get(()=>state.brightness),brightness);
  assert.equal(await get(()=>state.playing),false);
  assert.equal(await page.locator('#screen .reading-cue,#screen .play-button').count(),0);
  assert.equal(await get(()=>JSON.parse(localStorage.getItem(INTRO_KEY)).status),'completed');
  await snap('reader');
 });
 await check('Completion survives actual reload and replay leaves it intact',async()=>{
  await page.reload();await page.evaluate(()=>document.fonts.ready);
  assert.equal(await get(()=>state.view),'menu');
  assert.equal(await page.locator('#screen [data-slide="brightness"]').count(),0);
  await click('[data-go="settings"]');assert.equal(await page.locator('#screen [data-go="controls"]').count(),0);
  await click('[data-go="help"]');await snap('help');await click('[data-go="button-map"]');await snap('button-map');await click('[data-physical="1"]');assert.equal(await get(()=>highlightedButton),1);await click('[data-go="help"]');await click('[data-action="replay-intro"]');
  assert.equal(await get(()=>tour.step),0);
  await page.reload();assert.equal(await get(()=>state.view),'menu');
 });
 await check('Menu and settings topics, page selection and Focus explanations',async()=>{
  await page.evaluate(()=>{beginIntroduction();tour.step=5;render()});
  await click('[data-topic="chapters"]');assert.equal(await get(()=>tour.menuTopic),'chapters');
  await click('[data-action="intro-next"]');
  const word=page.locator('#screen [data-tour-word="3"]');const chosen=await word.textContent();await word.click();
  await click('[data-tool="return"]');assert.equal(await page.locator('#screen .tour-chosen').textContent(),chosen);
  await click('[data-tool="browse"]');await click('[data-tool="find"]');await click('[data-tool="search"]');
  assert.equal(await get(()=>tour.pageTool),'search');
  await click('[data-action="intro-next"]');await click('[data-topic="appearance"]');assert.equal(await get(()=>tour.settingTopic),'appearance');
  await click('[data-action="intro-next"]');await click('[data-kind="timer"]');assert.equal(await get(()=>tour.focusKind),'timer');
  assert.equal(await get(()=>state.focus),null);
  await click('[data-action="intro-next"]');await click('[data-physical="2"]');assert.equal(await get(()=>highlightedButton),2);
  await click('[data-action="intro-next"]');
  await click('[data-action="finish-intro"]');
 });
 await check('Skipping survives reload; interrupted first introduction returns',async()=>{
  await page.evaluate(()=>action('fresh-install'));await click('[data-action="skip-intro"]');
  await page.reload();assert.equal(await get(()=>state.view),'menu');
  assert.equal(await get(()=>introSeen.status),'skipped');
  await page.evaluate(()=>action('fresh-install'));await click('[data-action="intro-next"]');
  await page.reload();assert.equal(await get(()=>state.view),'onboarding');assert.equal(await get(()=>tour.step),0);
 });
 await check('Hold lesson starts on hold, pauses on release and survives cancellation',async()=>{
  await page.evaluate(()=>{state.textAction='hold';beginIntroduction()});await click('[data-action="intro-next"]');
  await snap('hold-practice');await click('.intro-practice');assert.equal(await get(()=>tour.playing),false);
  let r=await page.locator('#screen .intro-practice').boundingBox();
  await page.mouse.move(r.x+r.width*.5,r.y+r.height*.6);await page.mouse.down();
  await page.waitForTimeout(450);assert.equal(await get(()=>tour.playing),true);
  await page.mouse.up();assert.equal(await get(()=>tour.playing),false);assert.equal(await get(()=>tour.paused),true);
  await page.mouse.down();await page.waitForTimeout(450);
  await page.dispatchEvent('#screen','pointercancel',{pointerId:1});await page.mouse.up();
  assert.equal(await get(()=>tour.playing),false);
  await click('[data-action="intro-next"]');await swipe();for(let i=0;i<8;i++)await click('[data-action="intro-next"]');await click('[data-action="finish-intro"]');
  await page.reload();assert.equal(await get(()=>state.textAction),'hold');assert.equal(await get(()=>state.view),'menu');
 });
 await check('Everyday reader keeps gestures, hold release and clean controls',async()=>{
  await click('[data-go="reader"]');
  const r=await page.locator('#screen .word-zone').boundingBox();
  await page.mouse.move(r.x+r.width*.5,r.y+r.height*.5);await page.mouse.down();await page.waitForTimeout(450);
  assert.equal(await get(()=>state.playing),true);await page.mouse.up();assert.equal(await get(()=>state.playing),false);
  await swipe('.word-zone');assert.equal(await get(()=>state.view),'gesture-tray');
  assert.equal(await page.locator('#screen [data-go="controls"],#screen [data-action="play"]').count(),0);
  await snap('quick-controls');await click('[data-action="faster"]');assert.equal(await get(()=>state.wpm),310);
  const bright=page.locator('#screen [data-slide="brightness"]');await bright.focus();await page.keyboard.press('End');assert.equal(await get(()=>state.brightness),100);
  await click('[data-go="reader"]');await click('.reveal-controls');assert.equal(await get(()=>state.view),'gesture-tray');
  await click('[data-action="page-mode"]');assert.equal(await get(()=>state.readerSurface),'page');
  assert.equal(await page.locator('#screen .page-toolbar button').count(),3);
  await click('[data-action="word-mode"]');assert.equal(await get(()=>state.playing),false);
 });
 await check('Keyboard and hardware alternatives',async()=>{
  await page.evaluate(()=>{state.textAction='tap';beginIntroduction()});await click('[data-action="intro-next"]');
  await page.locator('#screen .intro-practice').focus();await page.keyboard.press('Enter');
  assert.equal(await get(()=>tour.playing),true);await page.keyboard.press('Space');assert.equal(await get(()=>tour.playing),false);
  await click('[data-action="intro-next"]');await page.locator('#screen .intro-practice').focus();await page.keyboard.press('ArrowUp');
  assert.equal(await get(()=>tour.controls),true);
  await page.evaluate(()=>exitIntroduction('completed'));await page.locator('#boot').click();assert.equal(await get(()=>state.playing),true);
  await page.locator('#boot').click();assert.equal(await get(()=>state.playing),false);
  await page.locator('#power').click();assert.equal(await get(()=>state.view),'menu');
 });
 const overflow=[];
 await check('All introduction steps and help fit the three languages',async()=>{
  for(const locale of ['en','de','ro']){
   await page.selectOption('#preview-language',locale);
   for(const touch of ['tap','hold']){
    await page.evaluate(touch=>{state.textAction=touch;beginIntroduction()},touch);
    for(const step of [0,1,2,3,4,5,6,7,8,9,10]){
     await page.evaluate(step=>{tour.step=step;tour.paused=false;render()},step);await snap(locale+'-'+touch+'-intro-'+step);
     const clipped=await page.locator('#screen button,#screen .intro-copy15,#screen .tuning-adjust,#screen .tuning-hint,#screen .tuning-label,#screen .tuning-caption,#screen .slider-content').evaluateAll(els=>els.filter(el=>el.scrollWidth>el.clientWidth+2||el.scrollHeight>el.clientHeight+2).map(el=>el.textContent));
     overflow.push(...clipped.map(text=>({locale,touch,step,text})));
    }
    await page.evaluate(()=>{tour.step=2;tour.controls=true;render()});await snap(locale+'-intro-tray');
   }
   for(const [step,key,topics] of [[5,'menuTopic',['library','chapters','focus','settings','backBook']],[7,'settingTopic',['reading','appearance','display','connections','device','help']],[6,'pageTool',['browse','find','search','chapters','position','return']],[8,'focusKind',['reading','timer']]]){
    for(const topic of topics){
     await page.evaluate(({step,key,topic})=>{tour.step=step;tour[key]=topic;render()},{step,key,topic});
     const clipped=await page.locator('#screen button,#screen .intro-copy15,#screen .tuning-adjust,#screen .tuning-hint,#screen .tuning-label,#screen .tuning-caption,#screen .slider-content').evaluateAll(els=>els.filter(el=>el.scrollWidth>el.clientWidth+2||el.scrollHeight>el.clientHeight+2).map(el=>el.textContent));
     overflow.push(...clipped.map(text=>({locale,step,topic,text})));
    }
   }
   for(const key of ['longDelay','complexDelay','punctuationDelay']){
    await page.evaluate(key=>{tour.step=4;tour.tuningTouched=key;render()},key);
    const clipped=await page.locator('#screen .tuning-hint').evaluateAll(els=>els.filter(el=>el.scrollHeight>el.clientHeight+2).map(el=>el.textContent));
    overflow.push(...clipped.map(text=>({locale,key,text})));
   }
   await page.evaluate(()=>{tour.step=3;tour.wpm=1000;render()});
   assert.equal(await page.locator('#screen [data-action="tuning-faster"]').isDisabled(),true);
   // Verify the visible word bounds too, including each language's longest word.
   for(const mode of ['word','chunk']){
    await page.evaluate(mode=>{state.mode=mode;tour.tuningIndex=tuningWords().reduce((best,w,i,all)=>w.length>all[best].length?i:best,0);render()},mode);
    const outside=await page.locator('#screen .tuning-word').evaluate(el=>{
     const bounds=el.getBoundingClientRect();
     return [...el.querySelectorAll('.word-before,.word-after,.chunk-line')].filter(word=>{const r=word.getBoundingClientRect();return r.left<bounds.left-1||r.right>bounds.right+1}).map(word=>word.textContent);
    });
    overflow.push(...outside.map(text=>({locale,mode,text})));
   }
   await page.evaluate(()=>{state.mode='word'});
   await page.evaluate(()=>navigate('help'));await snap(locale+'-help');
  }
  assert.deepEqual(overflow,[]);
 });
 await check('Themes, Focus transitions and responsive workbench',async()=>{
  const colours=[];
  for(const theme of ['ink','paper','nord','dracula','gruvbox-dark']){
   await page.selectOption('#theme',theme);await page.evaluate(()=>beginIntroduction());await snap('welcome-'+theme);
   colours.push(await page.locator('#screen').evaluate(el=>getComputedStyle(el).backgroundColor));
  }
  assert.equal(new Set(colours).size,5);
  await page.selectOption('#preview-language','en');await page.selectOption('#theme','ink');
  await page.evaluate(()=>exitIntroduction('completed'));
  await page.locator('#demo-start').click();await page.locator('#demo-expire').click();
  assert.equal(await get(()=>state.view),'focus-full');assert.equal(await get(()=>state.playing),false);
  await page.locator('#demo-break').click();assert.equal(await get(()=>state.playing),false);
  await click('[data-action="resume-round"]');assert.equal(await get(()=>state.playing),true);
  await page.evaluate(()=>{action('end-focus');navigate('menu')});
  for(const width of [390,768,1440]){
   await page.setViewportSize({width,height:1100});await page.waitForTimeout(100);
   assert.ok(await get(()=>document.documentElement.scrollWidth<=innerWidth),width+' page overflow');
  }
 });
 await check('Storage denial still permits completion during the open session',async()=>{
  const blocked=await browser.newPage();
  blocked.on('pageerror',e=>errors.push(e.message));
  await blocked.addInitScript(()=>Object.defineProperty(window,'localStorage',{get(){throw new Error('Storage blocked for test')}}));
  await blocked.goto('file://'+out+'/RSVP-Nano-UI-Preview-19.html');
  assert.equal(await blocked.evaluate(()=>state.view),'onboarding');
  await blocked.locator('#screen [data-action="skip-intro"]').click();
  await blocked.locator('[data-action="simulate-restart"]').click();
  assert.equal(await blocked.evaluate(()=>state.view),'menu');
  assert.match(await blocked.locator('#onboarding-status').textContent(),/storage is unavailable/);
  await blocked.close();
 });
 await check('Speed and pacing drafts, comparison playback and explicit application',async()=>{
  await page.setViewportSize({width:1440,height:1100});
  await page.evaluate(()=>{action('fresh-install');state.word=37});
  await page.evaluate(()=>action('try-tuning'));
  assert.equal(await get(()=>tour.step),3);
  await click('[data-action="tuning-faster"]');assert.equal(await get(()=>tour.wpm),310);
  assert.equal(await get(()=>state.wpm),300);
  await click('[data-action="tuning-play-even"]');await page.waitForTimeout(450);
  assert.ok(await get(()=>tour.tuningIndex)>=2);
  await click('[data-action="tuning-play-even"]');assert.equal(await get(()=>tour.tuningPlaying),false);
  await click('[data-action="intro-next"]');assert.equal(await get(()=>tour.step),4);
  const long=page.locator('#screen [data-tuning="longDelay"]');
  await long.focus();await page.keyboard.press('Home');await page.keyboard.press('ArrowRight');assert.equal(await get(()=>tour.longDelay),50);
  assert.equal(await get(()=>state.longDelay),200);
  const complex=page.locator('#screen [data-tuning="complexDelay"]'),r=await complex.boundingBox();
  await page.mouse.move(r.x+r.width*.25,r.y+r.height/2);await page.mouse.down();
  await page.mouse.move(r.x+r.width*.45,r.y+r.height/2,{steps:6});await page.mouse.up();
  assert.equal(await get(()=>tour.complexDelay),450);
  await page.mouse.move(r.x+r.width*.8,r.y+r.height/2);await page.mouse.down();
  await page.dispatchEvent('#screen','pointercancel',{pointerId:1});await page.mouse.up();
  assert.equal(await get(()=>tour.complexDelay),450);
  await page.locator('#screen [data-tuning="punctuationDelay"]').focus();await page.keyboard.press('End');
  assert.equal(await get(()=>tour.punctuationDelay),1000);
  await click('[data-action="tuning-play-paced"]');await page.waitForTimeout(450);
  await click('[data-action="tuning-play-even"]');assert.equal(await get(()=>tour.tuningMode),'even');
  assert.equal(await get(()=>tour.tuningIndex),0);
  await page.evaluate(()=>window.dispatchEvent(new Event('blur')));assert.equal(await get(()=>tour.tuningPlaying),false);
  await snap('pacing-tuned');
  await click('[data-action="apply-tuning"]');assert.equal(await get(()=>tour.step),5);
  assert.deepEqual(await get(()=>[state.wpm,state.longDelay,state.complexDelay,state.punctuationDelay,state.word]),[310,50,450,1000,37]);
  await page.evaluate(()=>exitIntroduction('completed'));
  await page.reload();await page.evaluate(()=>document.fonts.ready);
  assert.deepEqual(await get(()=>[state.wpm,state.longDelay,state.complexDelay,state.punctuationDelay]),[310,50,450,1000]);
  assert.equal(await get(()=>state.view),'menu');
  await page.evaluate(()=>navigate('button-map'));
  assert.deepEqual(await page.locator('#screen .button-legend small').allTextContents(),['Left button','Middle button','Right button']);
 });
 await check('Keeping existing settings, skipping and leaving a sample discard drafts',async()=>{
  await page.evaluate(()=>action('try-tuning'));
  await click('[data-action="tuning-faster"]');await click('[data-action="intro-next"]');
  await page.locator('#screen [data-tuning="punctuationDelay"]').focus();await page.keyboard.press('Home');
  await click('[data-action="intro-next"]');assert.equal(await get(()=>tour.step),5);
  assert.deepEqual(await get(()=>[state.wpm,state.punctuationDelay]),[310,1000]);
  await page.evaluate(()=>action('try-tuning')); await click('[data-action="tuning-faster"]');
  await click('[data-action="tuning-play-even"]');await click('[data-action="skip-intro"]');
  assert.equal(await get(()=>tuningTimer),null);assert.equal(await get(()=>tour),null);
  await page.reload();assert.deepEqual(await get(()=>[state.wpm,state.punctuationDelay]),[310,1000]);
  await page.evaluate(()=>action('try-tuning')); await click('[data-action="tuning-play-even"]');
  await page.selectOption('#preview-language','de');assert.equal(await get(()=>tour.tuningPlaying),false);
  await click('[data-action="tuning-play-even"]');await page.evaluate(()=>navigate('menu'));
  assert.equal(await get(()=>tuningTimer),null);
  await page.selectOption('#preview-language','en');
  await page.evaluate(()=>navigate('pace'));
  await page.locator('#screen [data-slide="punctuationDelay"]').focus();await page.keyboard.press('Home');
  await page.reload();assert.equal(await get(()=>state.punctuationDelay),0);
 });
 await check('Sample playback honours base WPM, added delays, replay and completion',async()=>{
  // A controlled browser clock checks actual word transitions, not just a formula.
  const timed=await browser.newPage();timed.on('pageerror',e=>errors.push(e.message));
  await timed.clock.install({time:new Date('2026-10-09T12:00:00Z')});
  await timed.clock.pauseAt(new Date('2026-10-09T12:00:01Z'));
  await timed.goto('file://'+out+'/RSVP-Nano-UI-Preview-19.html');
  await timed.evaluate(()=>{action('try-tuning');tour.wpm=600;render();startTuningSample('even')});
  await timed.clock.runFor(99);assert.equal(await timed.evaluate(()=>tour.tuningIndex),0);
  await timed.clock.runFor(1);assert.equal(await timed.evaluate(()=>tour.tuningIndex),1);
  await timed.evaluate(()=>{stopTuningSample();tour.wpm=300;startTuningSample('even')});
  await timed.clock.runFor(199);assert.equal(await timed.evaluate(()=>tour.tuningIndex),0);
  await timed.clock.runFor(1);assert.equal(await timed.evaluate(()=>tour.tuningIndex),1);
  await timed.evaluate(()=>{stopTuningSample();tour.step=4;tour.wpm=600;tour.longDelay=150;tour.complexDelay=250;tour.punctuationDelay=400;render();startTuningSample('paced')});
  // quiet. is the fourth word: its 100 ms base now has a 400 ms punctuation pause.
  await timed.clock.runFor(300);assert.equal(await timed.evaluate(()=>tuningWord()),'quiet.');
  await timed.clock.runFor(499);assert.equal(await timed.evaluate(()=>tuningWord()),'quiet.');
  await timed.clock.runFor(1);assert.equal(await timed.evaluate(()=>tuningWord()),'I');
  await timed.evaluate(()=>{startTuningSample('paced');tour.tuningIndex=tuningWords().indexOf('unpredictable,');updateTuningSample();queueTuningWord()});
  const longIndex=await timed.evaluate(()=>tour.tuningIndex);
  await timed.clock.runFor(899);assert.equal(await timed.evaluate(()=>tour.tuningIndex),longIndex);
  await timed.clock.runFor(1);assert.equal(await timed.evaluate(()=>tour.tuningIndex),longIndex+1);
  await timed.evaluate(()=>startTuningSample('even'));assert.equal(await timed.evaluate(()=>tour.tuningIndex),0);
  const count=await timed.evaluate(()=>tuningWords().length);
  await timed.clock.runFor(count*100);assert.equal(await timed.evaluate(()=>tour.tuningDone),true);
  assert.equal(await timed.evaluate(()=>tour.tuningPlaying),false);assert.equal(await timed.evaluate(()=>tuningTimer),null);
  assert.equal(await timed.evaluate(()=>tour.tuningIndex),count-1);
  await timed.evaluate(()=>toggleTuningSample('even'));assert.equal(await timed.evaluate(()=>tour.tuningIndex),0);
  await timed.evaluate(()=>navigate('menu'));await timed.clock.runFor(10000);
  assert.equal(await timed.evaluate(()=>state.playing),false);assert.equal(await timed.evaluate(()=>tuningTimer),null);
  await timed.close();
 });
 await check('Book upload guide: native layout, translations, themes, links and Help return',async()=>{
  await page.setViewportSize({width:1440,height:1100});await page.selectOption('#scale','native');
  const before=await get(()=>({word:state.word,wpm:state.wpm}));
  await page.evaluate(()=>action('try-book-upload'));assert.equal(await get(()=>tour.step),10);
  assert.ok((await page.locator('#screen .intro-step').textContent()).includes('10 / 10'));
  assert.equal(await page.locator('#screen [data-action="finish-intro"]').count(),1);
  for(const locale of ['en','de','ro'])for(const theme of ['ink','paper','nord','dracula','gruvbox-dark'])for(const method of ['wifi','usb']){
   await page.selectOption('#preview-language',locale);await page.selectOption('#theme',theme);
   await click('[data-method="'+method+'"]');
   assert.equal(await get(()=>bookUploadMethod),method);
   assert.equal(await page.locator('#screen .companion-qr').getAttribute('href'),'https://ionutdecebal.github.io/rsvpnano/');
   assert.equal(await page.locator('#screen .companion-qr rect').getAttribute('fill'),'#fff');
   assert.equal(await page.locator('#screen .companion-qr path').getAttribute('fill'),'#000');
   const bounds=await page.locator('#screen .companion-qr').boundingBox();assert.equal(bounds.width,111);assert.equal(bounds.height,111);
   const clipped=await page.locator('#screen').evaluate(screen=>{
    const bounds=screen.getBoundingClientRect(),issues=[];
    for(const el of screen.querySelectorAll('.upload-steps18 p,.upload-methods18 a,.upload-methods18 button,.companion-qr')){
     const box=el.getBoundingClientRect();
     if(el.scrollWidth>el.clientWidth+1||el.scrollHeight>el.clientHeight+1||box.right>bounds.right+1||box.bottom>bounds.bottom+1)issues.push(el.textContent);
    }
    const lines=[...screen.querySelectorAll('.upload-steps18 li p')].map(el=>el.getBoundingClientRect());
    for(let i=1;i<lines.length;i++)if(lines[i-1].bottom>lines[i].top+1)issues.push('overlapping instruction rows');
    return issues;
   });assert.deepEqual(clipped,[],locale+'/'+theme+'/'+method);
   await snap('upload-'+locale+'-'+theme+'-'+method);
  }
  await click('[data-action="finish-intro"]');assert.equal(await get(()=>state.view),'reader');
  assert.deepEqual(await get(()=>({word:state.word,wpm:state.wpm})),before);
  for(const locale of ['en','de','ro']){
   await page.selectOption('#preview-language',locale);await page.evaluate(()=>navigate('help'));await snap('help-upload-'+locale);
   await click('[data-go="add-books"]');assert.equal(await get(()=>state.view),'add-books');
   await snap('help-upload-guide-'+locale);await click('[data-go="help"]');assert.equal(await get(()=>state.view),'help');
  }
 });
 assert.deepEqual(errors,[]);assert.deepEqual(requests,[]);
 await fs.writeFile(out+'/verification.json',JSON.stringify({checks,errors,requests,overflow},null,2));
 console.log(JSON.stringify({checks:checks.length,errors,requests,overflow},null,2));
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
