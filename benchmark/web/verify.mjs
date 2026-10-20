// Browser UI checks use fake serial/storage only. No real device is requested.
import {chromium} from 'playwright';
import {build} from 'esbuild';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {dirname,resolve} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';

const root=dirname(fileURLToPath(import.meta.url)),out=resolve(root,'../../build/html-usb-benchmark');
await mkdir(out,{recursive:true});
const file=resolve(process.argv[2]||resolve(root,'dist/RSVP-Nano-USB-Speed-Test.html'));
const html=await readFile(file,'utf8');
const row=name=>`[bench] reader_result case=${name} font_size=${name==='normal'?1:0} font_px=${name==='normal'?43:52} ok=1 wpm=1000 expected=1000 displayed=1000 skipped=0 deadline_us=60000 deadline_misses=0 max_cycle_us=15300 max_gap_us=61300 elapsed_us=60000300 complete=1\n`;
const good=row('normal')+row('large')+'[bench] reader_complete ok=1\n[bench] done\n';
const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
const errors=[],network=[];let checks=0;
try{
  const page=await browser.newPage({viewport:{width:1440,height:1200}});
  page.setDefaultTimeout(8000);
  page.on('pageerror',error=>errors.push(error.message));
  await page.route('https://**/*',route=>{network.push(route.request().url());route.abort();});
  await page.goto(pathToFileURL(file).href);
  assert.deepEqual(await page.evaluate(()=>[!!navigator.serial,!!window.showDirectoryPicker,isSecureContext]),[true,true,true]);checks++;
  assert.equal(await page.locator('#run').isDisabled(),true);checks++;
  await page.screenshot({path:resolve(out,'desktop.png'),fullPage:true});
  await page.locator('#theme').click();
  assert.equal(await page.locator('html').getAttribute('class'),'light');checks++;
  await page.screenshot({path:resolve(out,'light.png'),fullPage:true});
  for(const width of [390,768,1024,1440]){
    await page.setViewportSize({width,height:1000});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false,'overflow at '+width);checks++;
  }
  await page.setViewportSize({width:390,height:1000});await page.screenshot({path:resolve(out,'mobile.png'),fullPage:true});
  await page.setViewportSize({width:1440,height:1200});await page.locator('#theme').click();
  const importLog=async text=>page.locator('#import').setInputFiles({name:'test.log',mimeType:'text/plain',buffer:Buffer.from(text)});
  await importLog(good);
  await page.waitForFunction(()=>document.querySelector('#status-label').textContent==='1,000 WPM / PASS');checks++;
  assert.equal(await page.locator('#normal .cycle-value').textContent(),'15.30');checks++;
  await page.screenshot({path:resolve(out,'imported-pass.png'),fullPage:true});
  const download=page.waitForEvent('download');await page.locator('#report').click();
  const report=await download;assert.ok(report.suggestedFilename().endsWith('.md'));checks++;
  for(const text of [row('normal'),good.replace('max_cycle_us=15300','max_cycle_us=60001'),good.replace('[bench] done',''),good.replace('case=large','case=normal')]){
    await importLog(text);await page.waitForFunction(()=>document.querySelector('#status-label').textContent==='1,000 WPM / FAIL OR INCOMPLETE');checks++;
  }
  await page.screenshot({path:resolve(out,'incomplete.png'),fullPage:true});
  // Exercise real bundled app code with fake browser devices and file handles.
  await page.reload();
  await page.evaluate(()=>{
    window.testWrites=[];window.testMode='pass';window.testCloses=0;
    const board=JSON.parse(document.querySelector('#release').textContent).boardId;
    window.showDirectoryPicker=async()=>({name:'RSVP_NANO',getDirectoryHandle:async()=>({getFileHandle:async(name,options)=>{
      if(!options)return {getFile:async()=>({text:async()=>'RSVP Nano benchmark\nboard='+board+'\n'})};
      return {createWritable:async()=>({write:async text=>testWrites.push([name,text]),close:async()=>{},abort:async()=>{}})};
    }})});
    Object.defineProperty(navigator,'serial',{value:{requestPort:async()=>{
      let controller;
      return {open:async()=>{},setSignals:async()=>{},close:async()=>{testCloses++;},
        readable:new ReadableStream({start(c){controller=c;}}),
        writable:new WritableStream({write(value){
          const text=new TextDecoder().decode(value);if(text!=='reader-start\n')throw Error('Unexpected command');
          if(window.testMode==='hold')return;
          if(window.testMode==='unplug'){controller.enqueue(new TextEncoder().encode('[bench] start board=Test id=test\n'));controller.close();return;}
          const row=name=>`[bench] reader_result case=${name} font_size=${name==='normal'?1:0} font_px=${name==='normal'?43:52} ok=1 wpm=1000 expected=1000 displayed=1000 skipped=0 deadline_us=60000 deadline_misses=0 max_cycle_us=15300 max_gap_us=61300 elapsed_us=60000300 complete=1\n`;
          for(const s of ['[be','nch] start board=Test id=test\n','[bench] reader_case name=normal wpm=1000\n',row('normal'),'[bench] reader_case name=large wpm=1000\n',row('large'),'[bench] reader_complete ok=1\n','[bench] do','ne\n'])controller.enqueue(new TextEncoder().encode(s));
        }})};
    }},configurable:true});
  });
  await page.locator('#prepare').click();await page.waitForFunction(()=>!document.querySelector('#run').disabled);
  assert.deepEqual(await page.evaluate(()=>testWrites.map(x=>x[0])),['reader-1000.rsvp','.run-ready']);checks++;
  await page.locator('#run').click();await page.waitForFunction(()=>document.querySelector('#status-label').textContent==='1,000 WPM / PASS');
  assert.equal(await page.evaluate(()=>testCloses),1);checks++;
  for(const mode of ['unplug','hold']){
    await page.evaluate(mode=>{window.testMode=mode;},mode);
    await page.locator('#prepare').click();await page.locator('#run').click();
    if(mode==='hold'){await page.locator('#stop').click();}
    await page.waitForFunction(()=>!document.querySelector('#prepare').disabled);
    assert.equal(await page.locator('#status').getAttribute('data-kind'),'error');checks++;
    assert.equal(await page.locator('#normal .cycle-value').textContent(),'—');checks++;
  }
  // Substitute only the flasher module in a disposable verification artifact.
  // The distributed file above always retains the actual esptool-js implementation.
  const fake=await build({absWorkingDir:root,entryPoints:['app.mjs'],bundle:true,write:false,format:'iife',platform:'browser',plugins:[{
    name:'fake-flasher',setup(build){build.onResolve({filter:/^esptool-js$/},()=>({path:'fake-flasher',namespace:'test'}));
      build.onLoad({filter:/.*/,namespace:'test'},()=>({contents:`
        export class Transport {async disconnect(){window.testDisconnected=true;}}
        export class ESPLoader {async main(){return window.testChip||'ESP32-S3';}
          async writeFlash(options){window.testFlash={eraseAll:options.eraseAll,parts:options.fileArray.map(p=>[p.address,p.data.length]),md5:options.calculateMD5Hash(options.fileArray[0].data)};options.reportProgress(3,100,100);}
          async after(){}}
      `,loader:'js'}));}}]});
  const mocked=html.replace(/<script>[^]*?<\/script>\s*<\/body>/,()=>'<script>'+fake.outputFiles[0].text.replace(/<\/script/gi,'<\\/script')+'</script></body>');
  const mockFile=resolve(out,'verification-only.html');await writeFile(mockFile,mocked);
  await page.goto(pathToFileURL(mockFile).href);
  await page.evaluate(()=>Object.defineProperty(navigator,'serial',{value:{requestPort:async()=>({})},configurable:true}));
  await page.locator('#install').click();await page.waitForFunction(()=>document.querySelector('#status-label').textContent==='DIAGNOSTIC INSTALLED');
  const flash=await page.evaluate(()=>window.testFlash);
  assert.equal(flash.eraseAll,false);assert.deepEqual(flash.parts.map(x=>x[0]),[0,32768,57344,65536]);assert.match(flash.md5,/^[a-f0-9]{32}$/);checks+=3;
  await page.locator('#restore').click();await page.waitForFunction(()=>document.querySelector('#status-label').textContent==='STUDY13 RESTORED');checks++;
  await page.evaluate(()=>{window.testChip='ESP32-C3';window.testFlash=null;});
  await page.locator('#install').click();await page.waitForFunction(()=>document.querySelector('#status').dataset.kind==='error');
  assert.equal(await page.evaluate(()=>window.testFlash),null);checks++;
  await page.goto(pathToFileURL(file).href);
  await page.evaluate(()=>{Object.defineProperty(navigator,'serial',{value:undefined,configurable:true});});
  // Unsupported browser tested at initialization, with a fresh document.
  await page.addInitScript(()=>{Object.defineProperty(navigator,'serial',{value:undefined,configurable:true});});
  await page.reload();assert.equal(await page.locator('#compatibility').isVisible(),true);assert.equal(await page.locator('#install').isDisabled(),true);checks++;
  assert.deepEqual(errors,[]);assert.deepEqual(network,[]);checks+=2;
  console.log(JSON.stringify({checks,errors,network,physicalDeviceTested:false},null,2));
}finally{await browser.close();}
