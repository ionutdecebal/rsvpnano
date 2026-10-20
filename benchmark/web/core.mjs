export const CASES = ['normal', 'large'];
export const EXPECTED_SIZE = {normal:1, large:0};
export const SIZE_LABELS = ['Large','Medium','Small','Compact'];
export const REQUIRED = ['ok', 'wpm', 'expected', 'displayed', 'skipped', 'deadline_us',
  'deadline_misses', 'max_cycle_us', 'max_gap_us', 'elapsed_us', 'complete'];

export function validCase(r, name) {
  return !!r && REQUIRED.every(k => Number.isSafeInteger(r[k])) && r.ok === 1 && r.complete === 1 &&
    r.wpm === 1000 && r.expected === 1000 && r.displayed === 1000 && r.skipped === 0 &&
    r.deadline_us === 60000 && r.deadline_misses === 0 && r.max_cycle_us > 0 &&
    r.max_cycle_us <= 60000 && r.max_gap_us > 0 && r.elapsed_us >= 59880000 && r.elapsed_us <= 60120000 &&
    Object.hasOwn(EXPECTED_SIZE,name) && r.font_size === EXPECTED_SIZE[name];
}

export function fontDescription(row, name) {
  const expected = SIZE_LABELS[EXPECTED_SIZE[name]], actual = SIZE_LABELS[row?.font_size] || 'Unknown font';
  const pixels = Number.isSafeInteger(row?.font_px) && row.font_px > 0 ? ` · ${row.font_px} px` : '';
  return actual + pixels + (row?.font_size === EXPECTED_SIZE[name] ? '' : ` · expected ${expected}`);
}

// Parse complete protocol records, never a partial USB chunk. A truncated, repeated
// or malformed record cannot turn the overall result green.
export function parseReaderLog(text) {
  text = text.replace(/\x1b\[[0-9;]*m/g, '');
  const cases = {}, seen = new Set(), configuredSizes = {};
  let invalid = false, completed = 0, done = 0;
  const records = [...text.matchAll(/\[bench\] ([^\r\n]*?)(?=\[bench\]|\r?\n|$)/g)].map(m => m[1].trim());
  let closed = false;
  for (const record of records) {
    if (record.startsWith('reader_case ')) {
      const name = /(?:^| )name=(normal|large)(?: |$)/.exec(record)?.[1];
      const size = /(?:^| )font_size=(\d+)(?: |$)/.exec(record)?.[1];
      if (name && size !== undefined) {
        if (Object.hasOwn(configuredSizes,name) || Object.hasOwn(cases,name)) invalid = true;
        configuredSizes[name] = Number(size);
      }
    } else if (record.startsWith('reader_result ')) {
      if (closed) invalid = true;
      const fields = {};
      for (const token of record.slice(14).split(/\s+/)) {
        const match = /^([a-z_]+)=([\w-]+)$/.exec(token);
        if (!match || Object.hasOwn(fields, match[1])) { invalid = true; continue; }
        fields[match[1]] = match[2];
      }
      const name = fields.case;
      if (!CASES.includes(name) || seen.has(name)) invalid = true;
      seen.add(name);
      const row = {};
      for (const key of REQUIRED) {
        if (!/^-?\d+$/.test(fields[key] ?? '')) invalid = true;
        row[key] = Number(fields[key]);
      }
      row.font_size = fields.font_size === undefined ? configuredSizes[name] : Number(fields.font_size);
      if (fields.font_size !== undefined && !/^\d+$/.test(fields.font_size)) invalid = true;
      if (Object.hasOwn(configuredSizes,name) && configuredSizes[name] !== row.font_size) invalid = true;
      if (fields.font_px !== undefined) row.font_px = Number(fields.font_px);
      if (CASES.includes(name)) cases[name] = row;
    } else if (record.startsWith('reader_complete')) {
      if (closed || CASES.some(name => !Object.hasOwn(cases, name))) invalid = true;
      if (record === 'reader_complete ok=1') ++completed;
      else invalid = true;
    } else if (record === 'done') {
      if (completed !== 1) invalid = true;
      ++done; closed = true;
    }
  }
  const passed = !invalid && completed === 1 && done === 1 && CASES.every(name => validCase(cases[name],name));
  const fontIssues = CASES.filter(name => cases[name] && cases[name].font_size !== EXPECTED_SIZE[name]);
  return { cases, passed, finished: done > 0, invalid, fontIssues };
}

export function readerFixture() {
  const vocabulary = ['a','the','river','quietly','reading','window','beautiful','understand','wonderful','light','remember','chapter'];
  const words = Array.from({length:1000}, (_,i) => vocabulary[i % vocabulary.length] + String(i+1).padStart(4,'0'));
  const lines = Array.from({length:50}, (_,i) => words.slice(i*20, i*20+20).join(' '));
  return '@rsvp 1\n@title USB reader speed test\n@language en\n@direction ltr\n@chapter USB reader test\n\n' + lines.join('\n') + '\n';
}

export async function prepareDrive(root, boardId) {
  let directory, marker;
  try {
    directory = await root.getDirectoryHandle('benchmark');
    marker = await (await directory.getFileHandle('.rsvpnano-benchmark')).getFile();
  } catch {
    throw new Error('Choose the RSVP Nano drive that appears after installing the diagnostic. No benchmark marker was found; no files were changed.');
  }
  const text = await marker.text();
  if (!text.startsWith('RSVP Nano benchmark\n') || !text.split(/\r?\n/).includes('board=' + boardId)) {
    throw new Error('This drive is not the LCD 3.49 rev1 benchmark device. No files were changed.');
  }
  for (const [name, content] of [['reader-1000.rsvp', readerFixture()], ['.run-ready', 'reader-only\n']]) {
    const file = await directory.getFileHandle(name, {create:true});
    const stream = await file.createWritable();
    try { await stream.write(content); await stream.close(); }
    catch (error) { await stream.abort().catch(() => {}); throw error; }
  }
}

export async function sha256(bytes) {
  const digest = await crypto.subtle.digest('SHA-256', bytes);
  return [...new Uint8Array(digest)].map(b => b.toString(16).padStart(2,'0')).join('');
}

export async function firmwareParts(bytes, expectedHash) {
  if (!(bytes instanceof Uint8Array) || bytes.length <= 0x10000 || bytes.length > 0x650000)
    throw new Error('The included firmware has an invalid size.');
  if (await sha256(bytes) !== expectedHash) throw new Error('Firmware integrity check failed. Nothing was flashed.');
  for (const offset of [0, 0x10000]) {
    if (bytes[offset] !== 0xe9 || bytes[offset+12] !== 9 || bytes[offset+13] !== 0)
      throw new Error('The included firmware is not an ESP32-S3 image.');
  }
  if (bytes[0x8000] !== 0xaa || bytes[0x8001] !== 0x50) throw new Error('Missing partition table.');
  // Deliberately skip NVS/settings at 0x9000..0xdfff. Never flash the whole
  // merged image at zero, which would erase that gap with its FF padding.
  return [[0,0x8000],[0x8000,0x9000],[0xe000,0x10000],[0x10000,bytes.length]]
    .map(([address,end]) => ({address, data:bytes.slice(address,end)}));
}

export function reportMarkdown(text, meta, source = 'USB device') {
  const result = parseReaderLog(text);
  const lines = ['# RSVP Nano · 1,000 WPM USB test', '', '**' + (result.passed ? 'PASS' : 'FAIL / INCOMPLETE') + '**', '',
    `Source: ${source}. Diagnostic SHA-256 bundled with this page: ${meta.diagnostic.sha256}`, '',
    '| Font | Result | Words | Skipped | Late cycles | Worst cycle | Longest gap | Playback |',
    '| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |'];
  for (const name of CASES) {
    const r = result.cases[name];
    const number = (value, divisor, digits) => Number.isFinite(value) ? (value/divisor).toFixed(digits) : '—';
    lines.push(r ? `| ${fontDescription(r,name)} | ${validCase(r,name) ? 'PASS' : 'FAIL'} | ${r.displayed} | ${r.skipped} | ${r.deadline_misses} | ${number(r.max_cycle_us,1000,2)} ms | ${number(r.max_gap_us,1000,2)} ms | ${number(r.elapsed_us,1e6,3)} s |`
      : `| ${name} | INCOMPLETE | — | — | — | — | — | — |`);
  }
  if (result.fontIssues.length) lines.push('', 'Font mismatch or missing font metadata: reinstall the corrected diagnostic and repeat the test. The first diagnostic incorrectly selected Compact for its Large case; that run cannot certify the Large font.');
  lines.push('', 'Two 1,000-word sessions in the current Word-mode reader, built-in Literata, SD-backed words, phantom words and an updating Focus badge. Extra pacing delays are zero.', '',
    'Times cover reader update, draw and display flush. A pass requires both complete cases, all words in order, no cycle over 60 ms and playback within 120 ms of one minute per case.', '',
    'This does not certify other fonts/scripts, chunk pacing, full-app Wi-Fi/battery/persistence overhead, or physical panel scanout. Browser timing is not a firmware measurement.', '',
    'Raw serial log is available separately. Imported logs are not authenticated firmware identities.');
  return lines.join('\n') + '\n';
}
