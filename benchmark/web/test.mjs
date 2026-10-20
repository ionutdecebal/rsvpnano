import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {parseReaderLog,readerFixture,prepareDrive,firmwareParts,sha256,reportMarkdown} from './core.mjs';
import {collectReader} from './serial.mjs';

export const row=(name,extra='')=>`[bench] reader_result case=${name} font_size=${name==='normal'?1:0} font_px=${name==='normal'?43:52} ok=1 wpm=1000 expected=1000 displayed=1000 skipped=0 deadline_us=60000 deadline_misses=0 max_cycle_us=15300 max_gap_us=61300 elapsed_us=60000300 complete=1${extra}\n`;
export const goodLog=()=>row('normal')+row('large')+'[bench] reader_complete ok=1\n[bench] done\n';

test('complete sessions pass; failed, missing, malformed, repeated and reordered results fail closed',()=>{
  assert.equal(parseReaderLog(goodLog()).passed,true);
  for(const text of [goodLog().replace('displayed=1000','displayed=999'),goodLog().replace('skipped=0','skipped=1'),
    goodLog().replace('max_cycle_us=15300','max_cycle_us=60001'),goodLog().replace('max_cycle_us=15300','max_cycle_us=0'),
    goodLog().replace('elapsed_us=60000300','elapsed_us=70000000'),goodLog().replace('elapsed_us=60000300','elapsed_us=58000000'),
    goodLog().replace('deadline_misses=0','deadline_misses=1'),goodLog().replace('complete=1','complete=0'),
    goodLog().replace('ok=1','ok=0'),goodLog().replace('case=large','case=normal'),goodLog().replace('case=large','case=unexpected'),
    goodLog().replace('max_cycle_us=15300','max_cycle_us=15junk'),goodLog().replace('max_cycle_us=15300',''),
    goodLog().replace('ok=1','ok=1 ok=0'),row('normal'),goodLog().replace('[bench] done',''),
    goodLog().replace('reader_complete ok=1','reader_complete ok=10'),goodLog()+row('normal'),
    '[bench] done\n'+goodLog(), '[bench] reader_complete ok=1\n'+goodLog()])
    assert.equal(parseReaderLog(text).passed,false,text);
  assert.equal(parseReaderLog(goodLog().replaceAll('\n','\r\n')).passed,true);
  assert.equal(parseReaderLog(goodLog().replaceAll('[bench]','\x1b[32m[bench]').replaceAll('\n','\x1b[0m\n')).passed,true);
});

test('fixture contains exactly 1000 distinct numbered words matching the CLI',()=>{
  const content=readerFixture(),words=content.split('\n').filter(line=>!line.startsWith('@')).join(' ').trim().split(/\s+/);
  assert.equal(words.length,1000);assert.equal(new Set(words).size,1000);
  assert.equal(words[0],'a0001');assert.equal(words[999],'quietly1000');
});

test('a mislabeled Compact run cannot certify Large; historical headers retain actual sizes',()=>{
  const wrong=goodLog().replace('font_size=0','font_size=3');
  assert.equal(parseReaderLog(wrong).passed,false);
  assert.deepEqual(parseReaderLog(wrong).fontIssues,['large']);
  const old=goodLog().replace(/ font_size=\d+ font_px=\d+/g,'');
  assert.equal(parseReaderLog(old).passed,false);
  const historical='[bench] reader_case name=normal font_size=1\n[bench] reader_case name=large font_size=3\n'+old;
  assert.deepEqual(parseReaderLog(historical).fontIssues,['large']);
  assert.match(reportMarkdown(historical,{diagnostic:{sha256:'test'}}),/Compact · expected Large/);
  assert.equal(parseReaderLog('[bench] reader_case name=large font_size=3\n'+goodLog()).passed,false);
});

test('SD preparation requires board marker, writes only the fixture and ready marker, in that order',async()=>{
  const writes=[];
  const directory={getFileHandle:async(name,options)=>{
    if(!options)return {getFile:async()=>({text:async()=>'RSVP Nano benchmark\nboard=testboard\n'})};
    return {createWritable:async()=>({write:async content=>writes.push([name,content]),close:async()=>writes.push('closed'),abort:async()=>{}})};
  }};
  const root={getDirectoryHandle:async name=>{assert.equal(name,'benchmark');return directory;}};
  await assert.rejects(prepareDrive(root,'wrong-board'),/not the LCD/);assert.equal(writes.length,0);
  await prepareDrive(root,'testboard');assert.deepEqual(writes.map(x=>Array.isArray(x)?x[0]:x),['reader-1000.rsvp','closed','.run-ready','closed']);
  assert.equal(writes[2][1],'reader-only\n');
  await assert.rejects(prepareDrive({getDirectoryHandle:async()=>{throw Error();}},'testboard'),/No benchmark marker/);
});

test('failed fixture copy cannot commit a run-ready marker',async()=>{
  const names=[];let aborted=false;
  const root={getDirectoryHandle:async()=>({getFileHandle:async(name,options)=>{
    if(!options)return {getFile:async()=>({text:async()=>'RSVP Nano benchmark\nboard=testboard\n'})};
    names.push(name);return {createWritable:async()=>({write:async()=>{throw Error('Disk full');},abort:async()=>{aborted=true;}})};
  }})};
  await assert.rejects(prepareDrive(root,'testboard'),/Disk full/);assert.deepEqual(names,['reader-1000.rsvp']);assert.equal(aborted,true);
});

test('embedded images pass integrity checks and skip the settings region',async()=>{
  const html=await readFile(new URL('./dist/RSVP-Nano-USB-Speed-Test.html',import.meta.url),'utf8');
  const meta=JSON.parse(/<script id="release"[^>]*>([^<]+)<\/script>/.exec(html)[1]);
  for(const name of ['diagnostic','restore']){
    const base64=new RegExp(`<script id="${name}-image"[^>]*>([^<]+)<\\/script>`).exec(html)[1];
    const bytes=new Uint8Array(Buffer.from(base64,'base64'));
    const parts=await firmwareParts(bytes,meta[name].sha256);
    assert.deepEqual(parts.map(p=>p.address),[0,0x8000,0xe000,0x10000]);
    assert.ok(parts.every(p=>p.address+p.data.length<=0x9000 || p.address>=0xe000));
    bytes[bytes.length-1]^=1;await assert.rejects(firmwareParts(bytes,meta[name].sha256),/integrity/);
    bytes[12]=2;await assert.rejects(firmwareParts(bytes,await sha256(bytes)),/ESP32-S3/);
  }
});

function serialMock(onWrite){
  let controller;const writes=[];let closed=false;
  return {writes,get closed(){return closed;},
    get controller(){return controller;},open:async()=>{},setSignals:async()=>{},close:async()=>{closed=true;},
    readable:new ReadableStream({start(c){controller=c;}}),
    writable:new WritableStream({write(value){const s=new TextDecoder().decode(value);writes.push(s);onWrite?.(s,controller);}})};
}
test('serial handshake tolerates split banners and keeps one connection for results',async()=>{
  const encoder=new TextEncoder();let raw='',started=0;
  const port=serialMock((command,c)=>{
    assert.equal(command,'reader-start\n');
    for(const text of ['[be','nch] st','art board=Test id=test\n',row('normal'),row('large'),'[bench] reader_complete ok=1\n','[bench] do','ne\x1b[','0m\n'])c.enqueue(encoder.encode(text));
  });
  await collectReader(port,{onText:text=>raw+=text,onStarted:()=>started++,timeoutMs:100});
  assert.equal(started,1);assert.equal(parseReaderLog(raw).passed,true);assert.equal(port.closed,true);assert.equal(port.writes.length,1);
});
test('USB unplug, timeout and cancellation release the port without a pass',async()=>{
  const disconnected=serialMock((s,c)=>c.close());
  await assert.rejects(collectReader(disconnected,{onText:()=>{},timeoutMs:100}),/disconnected/);
  assert.equal(disconnected.closed,true);
  const timeout=serialMock();await assert.rejects(collectReader(timeout,{onText:()=>{},timeoutMs:10}),/Timed out/);assert.equal(timeout.closed,true);
  const cancelled=serialMock(),controller=new AbortController();
  const job=collectReader(cancelled,{onText:()=>{},signal:controller.signal,timeoutMs:100});
  setTimeout(()=>controller.abort(),5);
  await assert.rejects(job,/Collection stopped/);assert.equal(cancelled.closed,true);
});
test('report preserves failed and incomplete status, with measurement limits',()=>{
  const meta={diagnostic:{sha256:'test'}};
  assert.match(reportMarkdown(goodLog(),meta),/\*\*PASS\*\*/);
  assert.match(reportMarkdown(row('normal'),meta),/FAIL \/ INCOMPLETE/);
  assert.match(reportMarkdown(goodLog(),meta),/physical panel scanout/);
});
