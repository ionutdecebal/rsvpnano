import {ESPLoader, Transport} from 'esptool-js';
import SparkMD5 from 'spark-md5';
import {CASES, validCase, fontDescription, parseReaderLog, prepareDrive, firmwareParts, reportMarkdown} from './core.mjs';
import {collectReader} from './serial.mjs';

const $ = id => document.getElementById(id);
const meta = JSON.parse($('release').textContent);
const supported = !!navigator.serial && window.isSecureContext;
let busy = '', prepared = false, raw = '', activity = '', source = 'USB device', aborter = null;
let startedAt = null, activeCase = null, elapsedTimer = null;
const sleep = ms => new Promise(resolve => setTimeout(resolve,ms));
const requestPort = () => navigator.serial.requestPort({filters:[{usbVendorId:0x303a}]});

function log(text) {
  activity = (activity + text).slice(-60000);
  $('log').textContent = activity;
  $('log').scrollTop = $('log').scrollHeight;
}
function status(label, title, message, kind = 'idle') {
  $('status-label').textContent = label; $('status-title').textContent = title;
  $('status-message').textContent = message; $('status').dataset.kind = kind;
}
function controls() {
  for (const button of document.querySelectorAll('[data-hardware]')) button.disabled = !supported || !!busy;
  $('prepare').disabled ||= !window.showDirectoryPicker;
  $('run').disabled ||= !prepared;
  $('import').disabled = !!busy;
  $('stop').hidden = busy !== 'run';
  $('report').disabled = !raw || !!busy;
  $('raw').disabled = !raw || !!busy;
}
function errorMessage(error) {
  if (error.name === 'NotFoundError' || error.name === 'AbortError') return 'Selection cancelled. Nothing new was installed or started.';
  return error.message || String(error);
}
function problem(error) {
  const message = errorMessage(error);
  status('ACTION NEEDED', 'Let’s check that.', message, 'error'); log('\n' + message + '\n');
}
function renderCases(text = raw, final = false) {
  const result = parseReaderLog(text);
  for (const name of CASES) {
    const card = $(name), row = result.cases[name];
    const put = (selector,value) => card.querySelector(selector).textContent = value;
    const finite = (value, divisor=1, decimals=0) => Number.isFinite(value) ? (value/divisor).toFixed(decimals) : '—';
    card.dataset.result = row ? (validCase(row,name) ? 'pass' : 'fail') : '';
    put('.case-verdict', row ? (result.fontIssues.includes(name) ? 'WRONG / UNKNOWN FONT' : validCase(row,name) ? 'PASS' : 'FAIL') : final ? 'INCOMPLETE' : activeCase === name ? 'RUNNING' : 'WAITING');
    put('.cycle-value', row ? finite(row.max_cycle_us,1000,2) : '—');
    put('.words', row ? finite(row.displayed) + ' / 1,000' : '—');
    put('.skipped', row ? finite(row.skipped) : '—');
    put('.late', row ? finite(row.deadline_misses) : '—');
    put('.timing', row ? `${fontDescription(row,name)} · playback ${finite(row.elapsed_us,1e6,3)} s · longest gap ${finite(row.max_gap_us,1000,2)} ms` : final ? 'No complete result received.' : 'Waiting for a complete device result.');
    card.querySelector('.budget-fill').style.width = row && Number.isFinite(row.max_cycle_us) ? Math.min(100,Math.max(0,row.max_cycle_us/750)) + '%' : '0%';
  }
  return result;
}
function finish(error = null) {
  clearInterval(elapsedTimer); activeCase = null;
  const result = renderCases(raw,true);
  if (result.passed && !error) {
    status('1,000 WPM / PASS', 'The Nano kept up.', 'Both sessions completed: all 2,000 words, no skipped words, and every measured reading cycle within 60 ms.', 'pass');
    $('test-progress').value = 120;
  } else {
    const reason = result.fontIssues.length ? 'This run used the wrong font size or lacks font metadata. The original diagnostic used Compact for its Large test. Reinstall the corrected diagnostic from this page and run it again.' : 'At least one requirement was not met, or the log is incomplete. Save the report and raw log for a closer look.';
    status('1,000 WPM / FAIL OR INCOMPLETE', 'Check the measurements.', error ? errorMessage(error) : reason, 'error');
  }
  $('session-label').textContent = result.passed && !error ? 'Both sessions complete' : 'Collection ended';
  $('connection').textContent = source.startsWith('Imported') ? source : 'USB released';
}

async function flashMode() {
  if (busy) return;
  busy = 'boot'; controls();
  let port;
  try {
    port = await requestPort();
    status('USB FLASH MODE', 'Preparing the Nano…', 'The USB device may disappear briefly, then return as a bootloader.');
    await port.open({baudRate:1200});
    await port.setSignals({dataTerminalReady:true,requestToSend:false});
    await sleep(200);
    // Disconnection during the 1200-baud close is expected on native USB.
    await port.setSignals({dataTerminalReady:false,requestToSend:false}).catch(() => {});
    await port.close().catch(() => {}); port = null;
    status('USB FLASH MODE REQUESTED', 'Ready to install.', 'Wait a moment for USB to reappear, then select Install diagnostic or Restore Study13 and choose the Nano again.');
    log('\nRequested the native USB bootloader at 1200 baud.\n');
  } catch (error) { problem(error); }
  finally { if (port) await port.close().catch(() => {}); busy = ''; controls(); }
}

async function flash(kind) {
  if (busy) return;
  busy = 'flash'; controls();
  let transport;
  try {
    // Browser port permission must be requested directly from this button click.
    const port = await requestPort();
    const binary = atob($(kind + '-image').textContent.trim());
    const bytes = Uint8Array.from(binary,c => c.charCodeAt(0));
    status('CHECKING FIRMWARE', 'Getting ready…', 'Checking the included image before writing to the device.');
    const parts = await firmwareParts(bytes, meta[kind].sha256);
    transport = new Transport(port, false);
    const loader = new ESPLoader({transport,baudrate:115200,romBaudrate:115200,
      terminal:{clean(){},write:log,writeLine:text=>log(text+'\n')}});
    const chip = await loader.main();
    if (!/^ESP32-S3(?:\b|$)/.test(chip)) throw new Error('This firmware is only for the ESP32-S3 LCD 3.49 rev1. Nothing was flashed.');
    const title = kind === 'diagnostic' ? 'Installing the diagnostic…' : 'Restoring Study13…';
    status('USB INSTALLATION',title,'Keep the USB cable connected until installation and verification finish.');
    $('flash-progress').hidden = false; $('flash-progress').value = 0; $('flash-label').hidden = false;
    const totalBytes = parts.reduce((sum,p)=>sum+p.data.length,0);
    await loader.writeFlash({fileArray:parts,flashSize:'keep',flashMode:'keep',flashFreq:'keep',eraseAll:false,compress:true,
      calculateMD5Hash:image=>SparkMD5.ArrayBuffer.hash(image.buffer.slice(image.byteOffset,image.byteOffset+image.byteLength)),
      reportProgress:(index,written,total)=>{
        const before = parts.slice(0,index).reduce((sum,p)=>sum+p.data.length,0);
        const percentage = Math.min(100,(before+parts[index].data.length*written/total)/totalBytes*100);
        $('flash-progress').value = percentage;
        $('flash-label').textContent = title + ' ' + percentage.toFixed(0) + '%';
      }});
    $('flash-progress').value = 100; $('flash-label').textContent = 'Written and verified.';
    let resetFailed = false;
    try { await loader.after('hard_reset'); }
    catch (error) { resetFailed = true; log('\nFirmware verified. Automatic reset did not complete: '+error.message+'\n'); }
    prepared = false;
    if (kind === 'diagnostic') {
      $('step-install').classList.add('done');
      status('DIAGNOSTIC INSTALLED','Now prepare the book.',(resetFailed?'Press RESET on the Nano. ':'')+'Wait for its USB drive to appear, then choose it in step 2.');
    } else {
      status('STUDY13 RESTORED','Back to your book.',(resetFailed?'Press RESET on the Nano. ':'')+'The normal reader firmware was written and verified. You can close this page.','pass');
    }
  } catch (error) { problem(error); }
  finally { if (transport) await transport.disconnect().catch(error=>log('\nUSB release: '+error.message+'\n')); busy='';controls(); }
}

$('boot').onclick = flashMode;
$('restore-boot').onclick = flashMode;
$('install').onclick = () => flash('diagnostic');
$('restore').onclick = () => flash('restore');
$('prepare').onclick = async () => {
  if (busy) return;
  busy='prepare';controls();prepared=false;
  try {
    const root = await window.showDirectoryPicker({mode:'readwrite',id:'rsvp-benchmark'});
    await prepareDrive(root,meta.boardId);
    prepared=true;
    $('step-prepare').classList.add('done');
    $('prepare-hint').textContent = 'Copied to '+root.name+'. Safely eject this drive now, then connect in step 3.';
    status('TEST BOOK READY','Eject, then connect.', 'Safely eject the Nano drive in your file manager. After the device restarts, select Connect & run test within two minutes.');
    log('\nPrepared the 1,000-word test book and reader-only marker on '+root.name+'.\n');
  } catch(error) {problem(error);}
  finally {busy='';controls();}
};

$('run').onclick = async () => {
  if (busy || !prepared) return;
  busy='run';controls();
  let wakeLock;
  try {
    const port = await requestPort();
    raw='';source='USB device';startedAt=null;activeCase=null;
    renderCases();
    $('session-progress').hidden=false;$('test-progress').value=0;$('elapsed').textContent='0:00 elapsed';
    $('session-label').textContent='Waiting for the Nano…';
    $('connection').textContent='USB connected';
    status('WAITING FOR THE DEVICE','Starting the test…','The Nano will read 1,000 words twice. Measurements arrive after each one-minute session.');
    aborter=new AbortController();
    try {wakeLock=await navigator.wakeLock?.request('screen');}catch{}
    elapsedTimer=setInterval(()=>{
      if (startedAt === null) return;
      const seconds=Math.max(0,Math.floor((performance.now()-startedAt)/1000));
      $('elapsed').textContent=Math.floor(seconds/60)+':'+String(seconds%60).padStart(2,'0')+' elapsed';
      $('test-progress').value=Math.min(119,seconds);
    },500);
    await collectReader(port,{
      signal:aborter.signal,
      onStarted:()=>{
        startedAt=performance.now();
        status('TEST RUNNING','The Nano is reading.','Keep the cable connected. The first result should arrive in about one minute.');
      },
      onText:text=>{
        raw+=text;log(text);
        if(raw.length>2*1024*1024) throw new Error('Unexpectedly large serial output. Collection stopped.');
        // Do not parse a result until its complete line has arrived.
        const complete=raw.slice(0,raw.lastIndexOf('\n')+1);
        const matches=[...complete.matchAll(/\[bench\] reader_case name=(normal|large)\b/g)];
        if(matches.length) activeCase=matches.at(-1)[1];
        $('session-label').textContent=activeCase==='large'?'Session 2 of 2 · Largest font':activeCase==='normal'?'Session 1 of 2 · Normal font':'Waiting for the Nano…';
        renderCases(complete);
      }});
    prepared=false;$('step-run').classList.add('done');finish();
  } catch(error) {if(raw) {prepared=false;log('\n'+errorMessage(error)+'\n');finish(error);} else problem(error);}
  finally {
    clearInterval(elapsedTimer);await wakeLock?.release().catch(()=>{});
    aborter=null;busy='';$('connection').textContent='USB released';controls();
  }
};
$('stop').onclick=()=>aborter?.abort();

function download(content,extension,type) {
  const url=URL.createObjectURL(new Blob([content],{type}));
  const link=document.createElement('a');link.href=url;
  link.download='RSVP-Nano-1000-WPM-'+new Date().toISOString().replace(/[:.]/g,'-')+'.'+extension;
  link.click();setTimeout(()=>URL.revokeObjectURL(url),10000);
}
$('report').onclick=()=>download(reportMarkdown(raw,meta,source),'md','text/markdown');
$('raw').onclick=()=>download(raw,'log','text/plain');
$('import').onchange=async event=>{
  const file=event.target.files[0];if(!file || busy)return;
  try{
    if(file.size>2*1024*1024)throw new Error('Choose a serial log smaller than 2 MB.');
    raw=await file.text();source='Imported log';activeCase=null;
    $('session-progress').hidden=true;
    finish();
    $('status-message').textContent='Imported '+file.name+'. '+$('status-message').textContent;
  }catch(error){problem(error);}finally{controls();event.target.value='';}
};
$('theme').onclick=()=>{
  const light=document.documentElement.classList.toggle('light');
  $('theme').textContent=light?'Dark theme':'Light theme';
  $('theme').setAttribute('aria-label',light?'Switch to dark theme':'Switch to light theme');
};
window.addEventListener('beforeunload',event=>{if(busy){event.preventDefault();event.returnValue='';}});
$('build-info').textContent='Board: '+meta.board+'\nDiagnostic SHA-256: '+meta.diagnostic.sha256+'\nStudy13 SHA-256: '+meta.restore.sha256+'\nPackaged: '+meta.date;
if(!supported || !window.showDirectoryPicker){
  $('compatibility').hidden=false;
  $('compatibility').textContent='Open this HTML file in a current desktop Chrome or Edge browser to install and test over USB. This browser is missing a required USB or file-access API. You can still open an existing log below.';
}
controls();
