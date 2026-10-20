import {build} from 'esbuild';
import {readFile,writeFile,mkdir,readdir} from 'node:fs/promises';
import {dirname,resolve,basename} from 'node:path';
import {fileURLToPath} from 'node:url';
import {firmwareParts,sha256} from './core.mjs';

const root=dirname(fileURLToPath(import.meta.url)), repo=resolve(root,'../..');
const args=process.argv.slice(2);
function arg(name,fallback){const i=args.indexOf(name);return i<0?fallback:args[i+1];}
const restorePath=arg('--restore');
if(!restorePath)throw new Error('Usage: npm run build -- --restore /path/to/Study13/merged.bin [--diagnostic path/to/firmware.factory.bin] [--output file.html]');
const diagnosticPath=arg('--diagnostic',resolve(repo,'.pio/build/benchmark_waveshare_esp32s3_touch_lcd_349_rev1/firmware.factory.bin'));
const output=resolve(arg('--output',resolve(root,'dist/RSVP-Nano-USB-Speed-Test.html')));
const diagnostic=new Uint8Array(await readFile(diagnosticPath)), restore=new Uint8Array(await readFile(restorePath));
const diagnosticSha=await sha256(diagnostic),restoreSha=await sha256(restore);
if(!Buffer.from(diagnostic).includes(Buffer.from('font_size=%u font_id=%s font_px=%u')))
  throw new Error('Rebuild the diagnostic with font metadata before packaging this corrected browser test.');
const study13=JSON.parse(await readFile(resolve(dirname(restorePath),'build-info.json'),'utf8'));
if(study13.name!=='RSVP Nano Study 13' || study13.environment!=='waveshare_esp32s3_touch_lcd_349_rev1' || study13.installer_sha256!==restoreSha)
  throw new Error('The restore image must match the verified Study13 package and build-info.json.');
await firmwareParts(diagnostic,diagnosticSha);await firmwareParts(restore,restoreSha);
if(!Buffer.from(diagnostic.slice(0x8000,0x9000)).equals(Buffer.from(restore.slice(0x8000,0x9000))))
  throw new Error('Diagnostic and restore partition tables differ.');
const revision=await readFile(resolve(repo,'src/platforms/waveshare_lcd_349/rev1/WaveshareLcd349Revision.h'),'utf8');
const config=await readFile(resolve(repo,'src/platforms/waveshare_lcd_349/BoardConfig.h'),'utf8');
const boardId=(revision+'\n'+config).match(/(?:kBoardId|BOARD_ID)\s*=\s*"([^"]+)"/)?.[1];
if(!boardId)throw new Error('Cannot find the device marker board ID.');
const meta={revision:'test-02-font-fix',date:new Date().toISOString().slice(0,10),board:study13.board,boardId,
  diagnostic:{name:basename(diagnosticPath),sha256:diagnosticSha,bytes:diagnostic.length},
  restore:{name:basename(restorePath),sha256:restoreSha,bytes:restore.length}};
const bundled=await build({absWorkingDir:root,entryPoints:['app.mjs'],bundle:true,write:false,format:'iife',platform:'browser',target:'chrome120',minify:true,legalComments:'inline'});
const notices=[];
for(const [name,path] of [['RSVP Nano — MIT',resolve(repo,'LICENSE')],['esptool-js 0.7.0 — Apache-2.0','node_modules/esptool-js/LICENSE'],['pako — zlib','node_modules/pako/LICENSE'],['atob-lite — MIT','node_modules/atob-lite/LICENSE.md'],['spark-md5 — WTFPL','node_modules/spark-md5/LICENSE']])
  notices.push(name+'\n\n'+await readFile(resolve(root,path),'utf8'));
const firmwareLicenses=resolve(dirname(restorePath),'licenses');
for(const name of await readdir(firmwareLicenses))notices.push('Firmware / '+name+'\n\n'+await readFile(resolve(firmwareLicenses,name),'utf8'));
const escapeHtml=text=>text.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;');
const replacements={
  '@@STYLE@@':await readFile(resolve(root,'style.css'),'utf8'),
  '@@LOGO@@':'data:image/svg+xml;base64,'+(await readFile(resolve(repo,'assets/RSVP-Nano-logo-color.svg'))).toString('base64'),
  '@@LICENSES@@':escapeHtml(notices.join('\n\n────────────────────\n\n')),
  '@@RELEASE@@':JSON.stringify(meta).replaceAll('<','\\u003c'),
  '@@DIAGNOSTIC@@':Buffer.from(diagnostic).toString('base64'),
  '@@RESTORE@@':Buffer.from(restore).toString('base64'),
  '@@SCRIPT@@':bundled.outputFiles[0].text.replace(/<\/script/gi,'<\\/script')};
let html=await readFile(resolve(root,'template.html'),'utf8');
for(const [marker,value] of Object.entries(replacements))html=html.replace(marker,()=>value);
if(/@@[A-Z]+@@/.test(html))throw new Error('Unresolved HTML placeholder.');
await mkdir(dirname(output),{recursive:true});await writeFile(output,html);
await writeFile(resolve(dirname(output),'build-info.json'),JSON.stringify(meta,null,2)+'\n');
console.log('Built '+output+' ('+(Buffer.byteLength(html)/1048576).toFixed(2)+' MiB)\nDiagnostic: '+diagnosticSha+'\nRestore: '+restoreSha);
