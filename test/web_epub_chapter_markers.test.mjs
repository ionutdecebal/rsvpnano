import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { test } from "node:test";
import { URL as NodeURL } from "node:url";
import vm from "node:vm";

const libraryUrl = new NodeURL("../web/library.js", import.meta.url);
const sandbox = {
  console,
  document: {
    createElement() {
      return {
        click() {},
        remove() {},
        set download(value) {
          this._download = value;
        },
        set href(value) {
          this._href = value;
        },
      };
    },
    body: {
      append() {},
    },
    querySelector() {
      return null;
    },
  },
  DOMParser: class TestDOMParser {
    parseFromString(text) {
      return parseXml(text);
    }
  },
  setTimeout,
  TextDecoder,
  URL: {
    createObjectURL() {
      return "blob:test";
    },
    revokeObjectURL() {},
  },
};
sandbox.globalThis = sandbox;

const librarySource = await readFile(libraryUrl, "utf8");
vm.runInNewContext(
  `${librarySource}
globalThis.__libraryTestHooks = {
  inferredChapterEvents,
  parsePackage,
  RsvpWriter,
};`,
  sandbox,
  { filename: "web/library.js" },
);
const { __libraryTestHooks } = sandbox;
const { inferredChapterEvents, parsePackage, RsvpWriter } = __libraryTestHooks;

test("uses NCX-only TOC titles when spine files have no HTML headings", async () => {
  const { spinePaths, tocTitles } = await parsePackage(
    fakeZip({
      "EPUB/package.opf": packageXml({
        manifest: `
<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
<item id="c1" href="text/chapter-one.xhtml" media-type="application/xhtml+xml"/>`,
        spine: `<itemref idref="c1"/>`,
      }),
      "EPUB/toc.ncx": `
<ncx><navMap><navPoint>
  <navLabel><text>NCX Chapter</text></navLabel>
  <content src="text/chapter-one.xhtml#start"/>
</navPoint></navMap></ncx>`,
    }),
    "EPUB/package.opf",
    "unicode",
  );

  assert.deepEqual(plain(spinePaths), ["EPUB/text/chapter-one.xhtml"]);
  assert.equal(tocTitles.get("EPUB/text/chapter-one.xhtml"), "NCX Chapter");
});

test("uses nav TOC links with fragments as spine titles", async () => {
  const { tocTitles } = await parsePackage(
    fakeZip({
      "EPUB/package.opf": packageXml({
        manifest: `
<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
<item id="c1" href="chapter-one.xhtml" media-type="application/xhtml+xml"/>`,
        spine: `<itemref idref="c1"/>`,
      }),
      "EPUB/nav.xhtml": `
<html xmlns:epub="http://www.idpf.org/2007/ops"><body>
  <nav epub:type="toc"><ol><li><a href="chapter-one.xhtml#start">Real Chapter</a></li></ol></nav>
</body></html>`,
    }),
    "EPUB/package.opf",
    "unicode",
  );

  assert.equal(tocTitles.get("EPUB/chapter-one.xhtml"), "Real Chapter");
});

test("infers OCR chapter lines without creating page filename chapters", () => {
  const events = inferredChapterEvents(
    [
      ["text", "Chapter 1 LOG ENTRY: SOL 6"],
      ["text", "Body one."],
      ["text", "Chapter 2"],
      ["text", "Body two."],
    ],
    "unicode",
  );

  assert.deepEqual(plain(events), [
    ["chapter", "Chapter 1 LOG ENTRY: SOL 6"],
    ["text", "Body one."],
    ["chapter", "Chapter 2"],
    ["text", "Body two."],
  ]);
});

test("preserves prose after an inline chapter marker", () => {
  const events = inferredChapterEvents(
    [["text", "Chapter 1. The first sentence starts here and should remain readable."]],
    "unicode",
  );

  assert.deepEqual(plain(events), [
    ["chapter", "Chapter 1"],
    ["text", "The first sentence starts here and should remain readable."],
  ]);
});

test("writer adds exactly one book-title fallback when no chapters exist", () => {
  const writer = new RsvpWriter({
    title: "Plain Book",
    author: "",
    source: "plain.epub",
    mode: "unicode",
  });
  writer.beginParagraph();
  writer.addText("Body one.");
  writer.beginParagraph();
  writer.addText("Body two.");

  const output = writer.finalize("Plain Book");

  assert.deepEqual(output.match(/^@chapter .+$/gm), ["@chapter Plain Book"]);
});

function plain(value) {
  return JSON.parse(JSON.stringify(value));
}

function packageXml({ title = "Test Book", manifest, spine }) {
  return `<?xml version="1.0" encoding="utf-8"?>
<package xmlns:dc="http://purl.org/dc/elements/1.1/" version="3.0">
  <metadata>
    <dc:title>${title}</dc:title>
    <dc:creator>Example Author</dc:creator>
  </metadata>
  <manifest>
    ${manifest}
  </manifest>
  <spine toc="ncx">
    ${spine}
  </spine>
</package>`;
}

function fakeZip(files) {
  const encoder = new TextEncoder();
  const entries = {};
  for (const [name, text] of Object.entries(files)) {
    entries[name] = {
      name,
      async: async (type) => {
        assert.equal(type, "uint8array");
        return encoder.encode(text);
      },
    };
  }
  return {
    files: entries,
    file(name) {
      return entries[name] || null;
    },
  };
}

function parseXml(xml) {
  const root = new TestDocument();
  const stack = [root];
  const tokenPattern = /<!--[\s\S]*?-->|<[^>]+>|[^<]+/g;
  for (const match of xml.matchAll(tokenPattern)) {
    const token = match[0];
    if (token.startsWith("<!--") || token.startsWith("<?") || token.startsWith("<!")) {
      continue;
    }
    if (token.startsWith("</")) {
      if (stack.length > 1) {
        stack.pop();
      }
      continue;
    }
    if (token.startsWith("<")) {
      const selfClosing = /\/\s*>$/.test(token);
      const content = token.replace(/^</, "").replace(/\/?>$/, "").trim();
      const [name] = content.split(/\s+/, 1);
      const node = new TestElement(name, parseAttributes(content.slice(name.length)));
      stack[stack.length - 1].children.push(node);
      if (!selfClosing) {
        stack.push(node);
      }
      continue;
    }
    stack[stack.length - 1].text += token;
  }
  root.documentElement = root.children[0] || null;
  return root;
}

function parseAttributes(value) {
  const attrs = new Map();
  const pattern = /([^\s=]+)\s*=\s*(?:"([^"]*)"|'([^']*)')/g;
  for (const match of value.matchAll(pattern)) {
    attrs.set(match[1], match[2] ?? match[3] ?? "");
  }
  return attrs;
}

class TestDocument {
  constructor() {
    this.children = [];
    this.documentElement = null;
    this.text = "";
  }

  querySelector() {
    return null;
  }

  getElementsByTagName() {
    return this.children.flatMap((child) => [child, ...child.getElementsByTagName("*")]);
  }
}

class TestElement {
  constructor(name, attrs) {
    this.nodeName = name;
    this.localName = name.includes(":") ? name.split(":").pop() : name;
    this.attrs = attrs;
    this.children = [];
    this.text = "";
  }

  getAttribute(name) {
    return this.attrs.get(name) || "";
  }

  getAttributeNS(_namespace, localName) {
    return this.attrs.get(`epub:${localName}`) || this.attrs.get(localName) || "";
  }

  get textContent() {
    return this.text + this.children.map((child) => child.textContent).join("");
  }

  getElementsByTagName() {
    return this.children.flatMap((child) => [child, ...child.getElementsByTagName("*")]);
  }
}
