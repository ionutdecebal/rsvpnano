#!/usr/bin/env python3
"""Convert OEWN's official JSON release to an SD dictionary pack.

Pack v1: 24-byte header <8sIIII>, N fixed 16-byte <IIII> records,
UTF-8 keys then UTF-8 JSON payloads. Records sort by key bytes; no RAM index
is required on the device. Each payload is bounded to 6 KiB.
"""
import argparse, hashlib, json, pathlib, struct, zipfile

SOURCE = 'Open English Wordnet 2025'
ATTRIBUTION = ('Adapted from Open English Wordnet 2025 by the Open English Wordnet '
               'Community, derived from Princeton WordNet. CC BY 4.0. '
               'https://en-word.net/ | https://creativecommons.org/licenses/by/4.0/ '
               'Changes: compact definitions, examples, synonyms and form aliases; '
               'up to 8 senses per entry on device. Original data remains available at source.')

def build(archive, output):
    output.mkdir(parents=True, exist_ok=True)
    synsets, entries = {}, {}
    with zipfile.ZipFile(archive) as z:
        for name in z.namelist():
            if not name.endswith('.json'): continue
            data = json.loads(z.read(name))
            (entries if name.startswith('entries-') else synsets).update(data)
    results = {}
    aliases = {}
    pos_labels={'n':'noun','v':'verb','a':'adjective','s':'adjective','r':'adverb'}
    for lemma, variants in entries.items():
        senses = []
        for variant in variants.values():
            for sense in variant.get('sense', []):
                s = synsets.get(sense['synset'], {})
                if not s.get('definition'): continue
                example=next(iter(s.get('example', [])), '')
                if isinstance(example,dict):
                    example=example.get('text','') + (' — '+example['source'] if example.get('source') else '')
                senses.append({'pos': pos_labels.get(s.get('partOfSpeech',''),''),
                               'definition': ' '.join(s['definition']),
                               'example': example,
                               'synonyms': s.get('members', [])[:8]})
            for form in variant.get('form', []): aliases.setdefault(form.lower(), lemma.lower())
        if senses:
            key = lemma.lower()
            if key in results: senses=results[key]['senses']+senses
            unique={ (v['pos'],v['definition']):v for v in senses }
            senses=list(unique.values())
            results[key] = {'word': lemma, 'source': SOURCE, 'license': 'CC BY 4.0',
                            'url': 'https://en-word.net/', 'senses': senses}
    for alias, target in aliases.items():
        if alias not in results and target in results: results[alias] = results[target]
    items = sorted(results.items(), key=lambda v: v[0].encode())
    records, keys, payloads = bytearray(), bytearray(), bytearray()
    start = 24 + 16 * len(items)
    for word, payload in items:
        compact = dict(payload, senses=payload['senses'][:8])
        encoded = json.dumps(compact, ensure_ascii=False, separators=(',', ':')).encode()
        while len(encoded) > 6144 and len(compact['senses']) > 1:
            compact['senses'].pop(); encoded = json.dumps(compact, ensure_ascii=False, separators=(',', ':')).encode()
        if len(encoded) > 6144: raise ValueError(f'Oversized entry: {word}')
        key = word.encode()
        records.extend(struct.pack('<IIII', len(keys), len(key), len(payloads), len(encoded)))
        keys.extend(key); payloads.extend(encoded)
    pack = output / 'english.oewn'
    pack.write_bytes(struct.pack('<8sIIII', b'RSVPDIC1', len(items), 24, start, start + len(keys)) + records + keys + payloads)
    manifest = {'format': 1, 'language': 'en', 'entries': len(items), 'source': SOURCE,
                'license': 'CC BY 4.0', 'attribution': ATTRIBUTION, 'bytes': pack.stat().st_size,
                'sha256': hashlib.sha256(pack.read_bytes()).hexdigest(),
                'sourceSha256': hashlib.sha256(archive.read_bytes()).hexdigest()}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (output / 'ATTRIBUTION.txt').write_text(ATTRIBUTION + '\n')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('archive', type=pathlib.Path); p.add_argument('output', type=pathlib.Path)
    a = p.parse_args(); build(a.archive, a.output)
