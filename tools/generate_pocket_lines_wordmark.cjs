// Compile the supplied banner's actual lettering into themeable alpha masks.
// This keeps the artwork's type, spacing and highlighted n; no runtime PNG or font.
const fs = require('node:fs');
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || '../benchmark/web/node_modules/playwright');

(async () => {
  const browser = await chromium.launch({ headless: true, args: ['--no-sandbox'] });
  try {
    const page = await browser.newPage();
    const source = fs.readFileSync('assets/pocket-lines/banner-transparent.png').toString('base64');
    const result = await page.evaluate(async source => {
      const image = new Image();
      image.src = 'data:image/png;base64,' + source;
      await image.decode();
      const original = document.createElement('canvas');
      original.width = image.width; original.height = image.height;
      const ctx = original.getContext('2d'); ctx.drawImage(image, 0, 0);
      const pixels = ctx.getImageData(0, 0, image.width, image.height).data;
      const ink = new Uint8ClampedArray(image.width * image.height);
      const accent = new Uint8ClampedArray(ink.length);
      let left = image.width, top = image.height, right = 0, bottom = 0;
      let textTop = image.height, textBottom = 0;
      for (let i = 0; i < ink.length; ++i) {
        const [r, g, b, a] = pixels.subarray(i * 4, i * 4 + 4);
        // The supplied PNG has a white matte around the red n. Separate black
        // and red coverage before downsampling, so that matte is transparent.
        ink[i] = a * (255 - Math.max(r, g, b)) / 255;
        accent[i] = a * Math.max(0, r - Math.max(g, b)) / 255;
        const x = i % image.width, y = Math.floor(i / image.width);
        if (ink[i] || accent[i]) {
          left = Math.min(left, x); top = Math.min(top, y);
          right = Math.max(right, x + 1); bottom = Math.max(bottom, y + 1);
        }
        if (ink[i]) { textTop = Math.min(textTop, y); textBottom = Math.max(textBottom, y + 1); }
      }
      if (right <= left || bottom <= top || textBottom <= textTop) throw Error('Empty banner artwork');
      const width = 246, height = Math.ceil((bottom - top) * width / (right - left));
      const layers = [];
      for (const name of ['lettering', 'focus', 'guides']) {
        const layer = ctx.createImageData(image.width, image.height);
        for (let i = 0; i < ink.length; ++i) {
          const y = Math.floor(i / image.width), isText = y >= textTop && y < textBottom;
          layer.data[i * 4 + 3] = name === 'lettering' ? ink[i]
            : (name === 'focus') === isText ? accent[i] : 0;
        }
        ctx.putImageData(layer, 0, 0);
        const small = document.createElement('canvas'); small.width = width; small.height = height;
        const dst = small.getContext('2d'); dst.imageSmoothingQuality = 'high';
        const scaledHeight = (bottom - top) * width / (right - left);
        dst.drawImage(original, left, top, right - left, bottom - top, 0, (height - scaledHeight) / 2, width, scaledHeight);
        const data = dst.getImageData(0, 0, width, height).data, runs = [];
        let last = -1, count = 0;
        for (let i = 3; i < data.length; i += 4) {
          const alpha = Math.round(data[i] / 17);
          if (alpha === last && count < 4095) count++;
          else { if (count) runs.push((count << 4) | last); last = alpha; count = 1; }
        }
        if (count) runs.push((count << 4) | last);
        layers.push({ name, runs });
      }
      return { width, height, crop: [left, top, right, bottom], layers };
    }, source);
    let header = '// Generated from assets/pocket-lines/banner-transparent.png by tools/generate_pocket_lines_wordmark.cjs.\n';
    header += '#pragma once\n#include <cstdint>\nnamespace ui::pocketlines::wordmark {\n';
    header += `inline constexpr int width=${result.width}, height=${result.height};\n`;
    for (const { name, runs } of result.layers) {
      if (runs.reduce((n, run) => n + (run >> 4), 0) !== result.width * result.height) throw Error('Invalid RLE length');
      header += `inline constexpr uint16_t ${name}[]{\n`;
      for (let i = 0; i < runs.length; i += 18) header += ' ' + runs.slice(i, i + 18).join(',') + ',\n';
      header += '};\n';
    }
    header += '}\n';
    fs.writeFileSync('src/ui/PocketLinesWordmark.generated.h', header);
    console.log(`Banner crop ${result.crop.join(',')} → ${result.width}×${result.height}; ${result.layers.reduce((n, layer) => n + layer.runs.length * 2, 0)} bytes of alpha masks.`);
  } finally { await browser.close(); }
})().catch(error => { console.error(error); process.exitCode = 1; });
