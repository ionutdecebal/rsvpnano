#!/usr/bin/env node

const fs = require("node:fs");
const path = require("node:path");

function usage() {
  console.error("Usage: node web/converter_cli.cjs text|html|book <input> <output> [title]");
}

const [, , command, inputPath, outputPath, title] = process.argv;

if (!["text", "html", "book"].includes(command) || !inputPath || !outputPath) {
  usage();
  process.exit(2);
}

(async () => {
  const {
    rsvpConvertHtmlToRsvp,
    rsvpConvertBytesToRsvp,
    rsvpConvertTextToRsvp,
  } = await import("./generated/converter/rsvpnano_converter.mjs");
  const filename = path.basename(inputPath);
  let result;
  if (command === "book") {
    result = rsvpConvertBytesToRsvp(filename, new Uint8Array(fs.readFileSync(inputPath)));
  } else {
    const content = fs.readFileSync(inputPath, "utf8");
    result =
      command === "html"
        ? rsvpConvertHtmlToRsvp(filename, title, filename, content)
        : rsvpConvertTextToRsvp(filename, title, filename, content);
  }

  fs.writeFileSync(outputPath, result.text, "utf8");
})().catch((error) => {
  console.error(error);
  process.exit(1);
});
