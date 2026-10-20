// Preserve the supplied SVG path; pre-render small alpha masks for the MCU.
const fs=require('node:fs');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || '../benchmark/web/node_modules/playwright');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 const page=await browser.newPage();
 const svg=fs.readFileSync('assets/pocket-lines/logo-transparent.svg','utf8');
 const masks=await page.evaluate(async svg=>{
  document.body.innerHTML=svg;
  const bounds=document.querySelector('path').getBBox();
  const source=svg.replace(/viewBox="[^"]*"/,`viewBox="${bounds.x} ${bounds.y} ${bounds.width} ${bounds.height}"`);
  const image=new Image();image.src='data:image/svg+xml;base64,'+btoa(source);await image.decode();
  const masks=[];
  for(let size=6;size<=32;size++){
   const canvas=document.createElement('canvas');canvas.width=canvas.height=size;
   const ctx=canvas.getContext('2d');ctx.drawImage(image,0,0,size,size);
   const pixels=ctx.getImageData(0,0,size,size).data,runs=[];
   let last=-1,count=0;
   for(let i=3;i<pixels.length;i+=4){const a=Math.round(pixels[i]/17);
    if(a===last&&count<4095)count++;else{if(count)runs.push((count<<4)|last);last=a;count=1;}}
   if(count)runs.push((count<<4)|last);masks.push({size,runs});
  }
  return masks;
 },svg);
 let out='// Generated from assets/pocket-lines/logo-transparent.svg by tools/generate_pocket_lines.cjs.\n#pragma once\n#include <cstdint>\n#include <span>\nnamespace ui::pocketlines::asset {\n';
 for(const {size,runs} of masks){out+=`inline constexpr uint16_t size${size}[]{\n`;
  for(let i=0;i<runs.length;i+=18)out+=' '+runs.slice(i,i+18).join(',')+',\n';out+='};\n';}
 out+='inline std::span<const uint16_t> mask(int size) { switch(size) {\n';
 for(const {size} of masks)out+=`case ${size}: return size${size};\n`;
 out+='default: return {};\n}}\n}\n';
 fs.writeFileSync('src/ui/PocketLines.generated.h',out);
 await page.setContent(svg);await page.screenshot({path:'build/study20-firmware/supplied-logo.png'});
 await browser.close();console.log(`Generated ${masks.length} SVG alpha masks (${masks.reduce((n,m)=>n+m.runs.length*2,0)} bytes).`);
})().catch(e=>{console.error(e);process.exitCode=1;});
