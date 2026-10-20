// Rasterize the supplied SVG into two alpha masks, retaining its paths and proportions.
// PLAYWRIGHT_MODULE may point to the locally installed Playwright package.
const fs=require('fs');
const { chromium }=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 const page=await browser.newPage();
 const svg=fs.readFileSync('assets/RSVP-Nano-logo-color.svg','utf8');
 const layers=await page.evaluate(async svg=>{
  const layers=[];
  for(const accent of [false,true]) {
   const source=svg.replace(/fill:white;/g,accent?'fill:none;':'fill:white;').replace(/fill:rgb\(253,111,66\);/g,accent?'fill:white;':'fill:none;');
   const image=new Image();image.src='data:image/svg+xml;base64,'+btoa(source);await image.decode();
   const canvas=document.createElement('canvas');canvas.width=246;canvas.height=105;
   const ctx=canvas.getContext('2d');ctx.drawImage(image,0,0,246,105);
   const data=ctx.getImageData(0,0,246,105).data;const runs=[];
   let last=-1,count=0;
   for(let i=3;i<data.length;i+=4) { const alpha=Math.round(data[i]/17);if(alpha===last&&count<4095)++count;else{if(count)runs.push((count<<4)|last);last=alpha;count=1;} }
   if(count)runs.push((count<<4)|last);layers.push(runs);
  }
  return layers;
 },svg);
 let header='// Generated from assets/RSVP-Nano-logo-color.svg by tools/generate_logo.js.\n#pragma once\n#include <cstdint>\nnamespace ui::logo {\ninline constexpr int width=246, height=105;\n';
 layers.forEach((runs,i)=>{header+='inline constexpr uint16_t '+(i?'accent':'foreground')+'[]{\n';for(let j=0;j<runs.length;j+=18)header+=' '+runs.slice(j,j+18).map(n=>'0x'+n.toString(16)).join(',')+',\n';header+='};\n';});
 header+='}\n';fs.writeFileSync('src/ui/Logo.generated.h',header);await browser.close();
})();
