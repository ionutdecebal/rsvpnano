import {
  rsvpConvertBytesToRsvp,
  rsvpDefaultOutputMode,
  rsvpExtensionForName,
  rsvpSideCarSuffixes,
  rsvpStripExtension,
  rsvpSupportedExtensions,
} from "./generated/converter/rsvpnano_converter.mjs";

const DEFAULT_OUTPUT_MODE = rsvpDefaultOutputMode();
const SIDE_CAR_SUFFIXES = rsvpSideCarSuffixes();
const SUPPORTED_EXTENSIONS = new Set(rsvpSupportedExtensions());
const extensionForName = rsvpExtensionForName;
const stripExtension = rsvpStripExtension;

const state = {
  items: [],
  directoryHandle: null,
  folderInventory: null,
  outputMode: DEFAULT_OUTPUT_MODE,
  isBusy: false,
  dragDepth: 0,
  nextId: 1,
  jszipPromise: null,
};

const hasDocument = typeof document !== "undefined";

const elements = hasDocument
  ? {
      addButton: document.querySelector("#library-add-button"),
      clearButton: document.querySelector("#library-clear-button"),
      downloadButton: document.querySelector("#library-download-button"),
      dropzone: document.querySelector("#library-dropzone"),
      fileInput: document.querySelector("#library-file-input"),
      folderButton: document.querySelector("#library-folder-button"),
      folderCleanButton: document.querySelector("#library-folder-clean-button"),
      folderImportButton: document.querySelector("#library-folder-import-button"),
      folderLabel: document.querySelector("#library-folder-label"),
      folderSummary: document.querySelector("#library-folder-summary"),
      list: document.querySelector("#library-list"),
      empty: document.querySelector("#library-empty"),
      status: document.querySelector("#library-status"),
      summary: document.querySelector("#library-summary"),
      summaryHeader: document.querySelector("#workspace-summary-header"),
      syncButton: document.querySelector("#library-sync-button"),
    }
  : {};

if (hasDocument) {
  initialize();
}

function initialize() {
  if (!elements.addButton) {
    return;
  }

  elements.addButton.addEventListener("click", () => {
    elements.fileInput.click();
  });

  elements.fileInput.addEventListener("change", async (event) => {
    const files = Array.from(event.target.files || []);
    elements.fileInput.value = "";
    if (files.length > 0) {
      await importFiles(files, "upload");
    }
  });

  elements.clearButton.addEventListener("click", () => {
    state.items = [];
    renderLibrary();
    setStatus(
      "Library cleared",
      "The browser workspace is empty again. Add more books whenever you are ready.",
      "success",
    );
    refreshUi();
  });

  elements.downloadButton.addEventListener("click", async () => {
    await downloadLibraryZip();
  });

  elements.folderButton.addEventListener("click", async () => {
    await chooseBooksDirectory();
  });

  elements.folderImportButton.addEventListener("click", async () => {
    await importFromSelectedDirectory();
  });

  elements.folderCleanButton.addEventListener("click", async () => {
    await cleanSidecarsInSelectedDirectory();
  });

  elements.syncButton.addEventListener("click", async () => {
    await syncReadyBooksToSelectedDirectory();
  });

  elements.list.addEventListener("click", async (event) => {
    const button = event.target.closest("button[data-action]");
    if (!button) {
      return;
    }

    const itemId = Number(button.dataset.itemId);
    const item = state.items.find((entry) => entry.id === itemId);
    if (!item) {
      return;
    }

    const action = button.dataset.action;
    if (action === "remove") {
      state.items = state.items.filter((entry) => entry.id !== item.id);
      renderLibrary();
      refreshUi();
      return;
    }

    if (action === "download") {
      downloadTextBlob(item.outputName, item.outputText);
      return;
    }

    if (action === "reconvert") {
      await reconvertSingleItem(item);
    }
  });

  elements.dropzone.addEventListener("dragenter", (event) => {
    event.preventDefault();
    state.dragDepth += 1;
    elements.dropzone.classList.add("is-active");
  });

  elements.dropzone.addEventListener("dragover", (event) => {
    event.preventDefault();
  });

  elements.dropzone.addEventListener("dragleave", (event) => {
    event.preventDefault();
    state.dragDepth = Math.max(0, state.dragDepth - 1);
    if (state.dragDepth === 0) {
      elements.dropzone.classList.remove("is-active");
    }
  });

  elements.dropzone.addEventListener("drop", async (event) => {
    event.preventDefault();
    state.dragDepth = 0;
    elements.dropzone.classList.remove("is-active");

    const files = Array.from(event.dataTransfer?.files || []).filter((file) => file.size >= 0);
    if (files.length > 0) {
      await importFiles(files, "upload");
    }
  });

  if (!supportsDirectoryAccess()) {
    elements.folderButton.disabled = true;
    elements.folderImportButton.disabled = true;
    elements.folderCleanButton.disabled = true;
    elements.syncButton.disabled = true;
    setStatus(
      "Browser setup required",
      "Folder sync needs Chrome or Edge with the File System Access API. Import and ZIP download still work in supporting browsers.",
      "info",
    );
  }

  renderLibrary();
  refreshUi();
}

function supportsDirectoryAccess() {
  return typeof window.showDirectoryPicker === "function";
}

function refreshUi() {
  const readyItems = state.items.filter((item) => item.status === "ready");
  const totalWords = readyItems.reduce((sum, item) => sum + item.wordCount, 0);

  elements.summary.textContent =
    readyItems.length === 0
      ? "0 converted books ready"
      : `${readyItems.length} converted ${pluralize("book", readyItems.length)} ready, ${formatNumber(totalWords)} ${pluralize("word", totalWords)}`;

  if (elements.summaryHeader) {
    elements.summaryHeader.textContent =
      readyItems.length === 0 ? "" : elements.summary.textContent;
  }

  elements.folderLabel.textContent = state.directoryHandle
    ? `/${state.directoryHandle.name}`
    : "No /books/books folder selected";

  if (state.folderInventory) {
    const { sources, rsvp, sidecars, unsupported } = state.folderInventory;
    const parts = [
      `${sources} source ${pluralize("file", sources)}`,
      `${rsvp} .rsvp ${pluralize("file", rsvp)}`,
      `${sidecars} sidecar ${pluralize("file", sidecars)}`,
    ];
    if (unsupported > 0) {
      parts.push(`${unsupported} other ${pluralize("file", unsupported)}`);
    }
    elements.folderSummary.textContent = parts.join(", ");
  } else {
    elements.folderSummary.textContent = "Pick the SD card’s /books/books folder to scan it";
  }

  const noFolder = !state.directoryHandle;
  const noReadyItems = readyItems.length === 0;
  const noItems = state.items.length === 0;

  elements.addButton.disabled = state.isBusy;
  elements.fileInput.disabled = state.isBusy;
  elements.downloadButton.disabled = state.isBusy || noReadyItems;
  elements.clearButton.disabled = state.isBusy || noItems;
  elements.folderButton.disabled = state.isBusy || !supportsDirectoryAccess();
  elements.folderImportButton.disabled =
    state.isBusy || !supportsDirectoryAccess() || noFolder;
  elements.folderCleanButton.disabled =
    state.isBusy || !supportsDirectoryAccess() || noFolder;
  elements.syncButton.disabled =
    state.isBusy || !supportsDirectoryAccess() || noFolder || noReadyItems;
}

function renderLibrary() {
  refreshItemWarnings();
  if (state.items.length === 0) {
    elements.list.innerHTML = "";
    elements.empty.hidden = false;
    return;
  }

  elements.empty.hidden = true;
  elements.list.innerHTML = state.items
    .map((item) => {
      const stateToken =
        item.status === "ready"
          ? "ready"
          : item.status === "error"
            ? "error"
            : "working";
      const statusLabel =
        item.status === "ready"
          ? "Ready"
          : item.status === "error"
            ? "Needs attention"
            : "Converting";
      const authorPill = item.author ? `<span class="pill">${escapeHtml(item.author)}</span>` : "";
      const warningCopy = item.warning
        ? `<p class="library-item-copy">${escapeHtml(item.warning)}</p>`
        : "";
      const detailCopy =
        item.status === "ready"
          ? `Output <code>${escapeHtml(item.outputName)}</code> from <code>${escapeHtml(item.sourceName)}</code>.`
          : item.status === "error"
            ? escapeHtml(item.error || "Conversion failed.")
            : "Reading source and building .rsvp output...";

      return `
        <li class="library-item">
          <div class="library-item-head">
            <div class="library-item-title">
              <strong>${escapeHtml(item.title || stripExtension(item.sourceName))}</strong>
              <span>${escapeHtml(item.sourceName)}</span>
            </div>
            <span class="pill" data-state="${stateToken}">${statusLabel}</span>
          </div>
          <div class="library-item-meta">
            <span class="pill">${escapeHtml(item.sourceExt.slice(1).toUpperCase())}</span>
            <span class="pill">${formatNumber(item.wordCount)} ${pluralize("word", item.wordCount)}</span>
            <span class="pill">${formatNumber(item.chapterCount)} ${pluralize("chapter", item.chapterCount)}</span>
            ${authorPill}
          </div>
          <p class="library-item-copy">${detailCopy}</p>
          ${warningCopy}
          <div class="library-item-actions">
            ${
              item.status === "ready"
                ? `<button class="tool-button" type="button" data-action="download" data-item-id="${item.id}">Download</button>`
                : ""
            }
            <button class="tool-button" type="button" data-action="reconvert" data-item-id="${item.id}">
              Reconvert
            </button>
            <button class="tool-button" type="button" data-action="remove" data-item-id="${item.id}">
              Remove
            </button>
          </div>
        </li>
      `;
    })
    .join("");
}

function setStatus(title, message, tone = "info") {
  elements.status.dataset.tone = tone;
  elements.status.innerHTML = `<strong>${escapeHtml(title)}</strong>${escapeHtml(message)}`;
}

async function withBusy(fn) {
  if (state.isBusy) {
    return;
  }
  state.isBusy = true;
  refreshUi();
  try {
    await fn();
  } catch (error) {
    setStatus(
      "Action interrupted",
      error instanceof Error ? error.message : String(error),
      "error",
    );
  } finally {
    state.isBusy = false;
    refreshUi();
  }
}

async function importFiles(files, origin) {
  const descriptors = files
    .map((file) => descriptorFromFile(file, origin))
    .filter((descriptor) => descriptor !== null);

  if (descriptors.length === 0) {
    setStatus(
      "No supported books found",
      "Bring in EPUB, TXT, Markdown, or HTML sources to convert them into .rsvp.",
      "error",
    );
    return;
  }

  await ingestDescriptors(descriptors, "Importing books");
}

function descriptorFromFile(file, origin) {
  const sourceExt = extensionForName(file.name);
  if (!SUPPORTED_EXTENSIONS.has(sourceExt)) {
    return null;
  }

  return {
    key: file.name.toLowerCase(),
    origin,
    sourceExt,
    sourceName: file.name,
    getFile: async () => file,
  };
}

function descriptorFromHandle(fileHandle) {
  const sourceExt = extensionForName(fileHandle.name);
  if (!SUPPORTED_EXTENSIONS.has(sourceExt)) {
    return null;
  }

  return {
    key: fileHandle.name.toLowerCase(),
    origin: "folder",
    sourceExt,
    sourceName: fileHandle.name,
    getFile: async () => fileHandle.getFile(),
  };
}

async function ingestDescriptors(descriptors, statusTitle) {
  await withBusy(async () => {
    const targets = [];

    for (const descriptor of descriptors) {
      let item = state.items.find((entry) => entry.key === descriptor.key);
      if (item) {
        item.descriptor = descriptor;
      } else {
        item = createLibraryItem(descriptor);
        state.items.push(item);
      }
      item.mode = state.outputMode;
      item.status = "working";
      item.error = "";
      item.warning = "";
      targets.push(item);
    }

    renderLibrary();
    refreshUi();

    for (let index = 0; index < targets.length; index += 1) {
      const item = targets[index];
      setStatus(
        statusTitle,
        `Converting ${index + 1} of ${targets.length}: ${item.sourceName}`,
        "busy",
      );
      await convertDescriptorIntoItem(item);
      renderLibrary();
      refreshUi();
    }

    const readyCount = targets.filter((item) => item.status === "ready").length;
    const failedCount = targets.length - readyCount;
    if (failedCount > 0) {
      setStatus(
        "Conversion finished with notes",
        `${readyCount} ${pluralize("book", readyCount)} converted, ${failedCount} ${pluralize("book", failedCount)} need another look.`,
        "error",
      );
    } else {
      setStatus(
        "Conversion complete",
        `${readyCount} ${pluralize("book", readyCount)} are ready to download or sync into /books/books.`,
        "success",
      );
    }
  });
}

function createLibraryItem(descriptor) {
  return {
    id: state.nextId++,
    key: descriptor.key,
    descriptor,
    sourceName: descriptor.sourceName,
    sourceExt: descriptor.sourceExt,
    status: "working",
    error: "",
    warning: "",
    title: stripExtension(descriptor.sourceName),
    author: "",
    outputName: `${stripExtension(descriptor.sourceName)}.rsvp`,
    outputText: "",
    wordCount: 0,
    chapterCount: 0,
    mode: state.outputMode,
  };
}

async function convertDescriptorIntoItem(item) {
  item.status = "working";
  item.error = "";
  item.warning = "";

  try {
    const file = await item.descriptor.getFile();
    const result = rsvpConvertBytesToRsvp(file.name, new Uint8Array(await file.arrayBuffer()));

    item.title = result.title || stripExtension(file.name);
    item.author = "";
    item.outputName = `${stripExtension(file.name)}.rsvp`;
    item.outputText = result.text;
    item.wordCount = result.wordCount;
    item.chapterCount = result.chapterCount;
    item.mode = state.outputMode;
    item.status = "ready";
  } catch (error) {
    item.status = "error";
    item.error = error instanceof Error ? error.message : String(error);
    item.outputText = "";
    item.wordCount = 0;
    item.chapterCount = 0;
  }
}

async function reconvertSingleItem(item) {
  await withBusy(async () => {
    setStatus("Re-converting book", `Building ${item.sourceName} again.`, "busy");
    await convertDescriptorIntoItem(item);
    renderLibrary();
    refreshUi();
    if (item.status === "ready") {
      setStatus("Book refreshed", `${item.sourceName} is ready again.`, "success");
    } else {
      setStatus("Could not rebuild book", item.error || "Conversion failed.", "error");
    }
  });
}

async function chooseBooksDirectory() {
  if (!supportsDirectoryAccess()) {
    return;
  }

  try {
    const directoryHandle = await window.showDirectoryPicker({ mode: "readwrite" });
    state.directoryHandle = directoryHandle;
    await scanSelectedDirectory(false);

    if (directoryHandle.name.toLowerCase() === "books") {
      setStatus(
        "Books folder selected",
        "The page can now scan, clean, and sync files directly inside /books/books.",
        "success",
      );
    } else {
      setStatus(
        "Folder selected",
        `You picked /${directoryHandle.name}. For best results, point this at the SD card’s /books/books folder.`,
        "info",
      );
    }
  } catch (error) {
    if (error?.name === "AbortError") {
      return;
    }
    setStatus(
      "Could not open folder",
      error instanceof Error ? error.message : String(error),
      "error",
    );
  } finally {
    refreshUi();
  }
}

async function importFromSelectedDirectory() {
  if (!state.directoryHandle) {
    return;
  }

  const descriptors = await scanSelectedDirectory(true);
  if (descriptors.length === 0) {
    setStatus(
      "No supported sources in folder",
      "The selected folder does not contain EPUB, TXT, Markdown, or HTML source files that need conversion.",
      "info",
    );
    return;
  }

  await ingestDescriptors(descriptors, "Importing from selected folder");
  await scanSelectedDirectory(false);
}

async function scanSelectedDirectory(includeDescriptors) {
  if (!state.directoryHandle) {
    state.folderInventory = null;
    refreshUi();
    return [];
  }

  const inventory = {
    sources: 0,
    rsvp: 0,
    sidecars: 0,
    unsupported: 0,
  };
  const descriptors = [];

  for await (const entry of state.directoryHandle.values()) {
    if (entry.kind !== "file" || entry.name.startsWith(".")) {
      continue;
    }

    const lowerName = entry.name.toLowerCase();
    if (SIDE_CAR_SUFFIXES.some((suffix) => lowerName.endsWith(suffix))) {
      inventory.sidecars += 1;
      continue;
    }
    if (lowerName.endsWith(".rsvp")) {
      inventory.rsvp += 1;
      continue;
    }

    const descriptor = descriptorFromHandle(entry);
    if (descriptor) {
      inventory.sources += 1;
      if (includeDescriptors) {
        descriptors.push(descriptor);
      }
    } else {
      inventory.unsupported += 1;
    }
  }

  state.folderInventory = inventory;
  refreshUi();
  return descriptors;
}

async function cleanSidecarsInSelectedDirectory() {
  if (!state.directoryHandle) {
    return;
  }

  await withBusy(async () => {
    const removableNames = [];
    let removed = 0;
    setStatus("Cleaning sidecars", "Removing leftover .failed, .tmp, and .converting files.", "busy");

    for await (const entry of state.directoryHandle.values()) {
      if (entry.kind !== "file") {
        continue;
      }
      const lowerName = entry.name.toLowerCase();
      if (!SIDE_CAR_SUFFIXES.some((suffix) => lowerName.endsWith(suffix))) {
        continue;
      }

      removableNames.push(entry.name);
    }

    for (const entryName of removableNames) {
      await state.directoryHandle.removeEntry(entryName);
      removed += 1;
    }

    await scanSelectedDirectory(false);
    setStatus(
      "Sidecars cleaned",
      removed === 0
        ? "There were no leftover conversion sidecars to remove."
        : `Removed ${removed} ${pluralize("sidecar file", removed)} from the selected folder.`,
      "success",
    );
  });
}

async function syncReadyBooksToSelectedDirectory() {
  if (!state.directoryHandle) {
    return;
  }

  const readyItems = state.items.filter((item) => item.status === "ready");
  if (readyItems.length === 0) {
    return;
  }
  const conflictingOutputs = duplicateOutputNamesFor(readyItems);
  if (conflictingOutputs.length > 0) {
    setStatus(
      "Resolve duplicate outputs first",
      `More than one source currently maps to ${conflictingOutputs.map((name) => `"${name}"`).join(", ")}. Remove or rename one of them before syncing to the SD card.`,
      "error",
    );
    return;
  }

  await withBusy(async () => {
    let written = 0;
    setStatus(
      "Syncing library",
      `Writing ${readyItems.length} ${pluralize("book", readyItems.length)} into the selected folder.`,
      "busy",
    );

    for (let index = 0; index < readyItems.length; index += 1) {
      const item = readyItems[index];
      setStatus(
        "Syncing library",
        `Writing ${index + 1} of ${readyItems.length}: ${item.outputName}`,
        "busy",
      );

      const fileHandle = await state.directoryHandle.getFileHandle(item.outputName, { create: true });
      const writable = await fileHandle.createWritable();
      await writable.write(item.outputText);
      await writable.close();

      await cleanupSidecarsForOutput(item.outputName);
      written += 1;
    }

    await scanSelectedDirectory(false);
    setStatus(
      "Sync complete",
      `Wrote ${written} ${pluralize("book", written)} into the selected folder.`,
      "success",
    );
  });
}

async function cleanupSidecarsForOutput(outputName) {
  const sidecarNames = [`${outputName}.failed`, `${outputName}.tmp`, `${outputName}.converting`];
  for (const sidecarName of sidecarNames) {
    try {
      await state.directoryHandle.removeEntry(sidecarName);
    } catch (error) {
      if (error?.name !== "NotFoundError") {
        throw error;
      }
    }
  }
}

async function downloadLibraryZip() {
  const readyItems = state.items.filter((item) => item.status === "ready");
  if (readyItems.length === 0) {
    return;
  }

  await withBusy(async () => {
    setStatus(
      "Building archive",
      `Packing ${readyItems.length} converted ${pluralize("book", readyItems.length)} into one ZIP download.`,
      "busy",
    );

    const JSZip = await loadJsZip();
    const archive = new JSZip();
    for (const item of readyItems) {
      archive.file(item.outputName, item.outputText);
    }

    const blob = await archive.generateAsync({ type: "blob" });
    downloadBlob("rsvp-nano-library.zip", blob);
    setStatus(
      "ZIP ready",
      "The converted library archive has been downloaded to your computer.",
      "success",
    );
  });
}

async function loadJsZip() {
  if (!state.jszipPromise) {
    state.jszipPromise = import("https://cdn.jsdelivr.net/npm/jszip@3.10.1/+esm").then(
      (module) => module.default,
    );
  }
  return state.jszipPromise;
}

function refreshItemWarnings() {
  const duplicateNames = new Set(duplicateOutputNamesFor(state.items.filter((item) => item.status === "ready")));
  for (const item of state.items) {
    const warnings = [];
    if (item.status === "ready" && duplicateNames.has(item.outputName)) {
      warnings.push(
        `Another source in the workspace also outputs ${item.outputName}. Sync is blocked until the collision is resolved.`,
      );
    }
    item.warning = warnings.join(" ");
  }
}

function duplicateOutputNamesFor(items) {
  const counts = new Map();
  for (const item of items) {
    counts.set(item.outputName, (counts.get(item.outputName) || 0) + 1);
  }
  return Array.from(counts.entries())
    .filter(([, count]) => count > 1)
    .map(([name]) => name);
}

function pluralize(noun, count) {
  return count === 1 ? noun : `${noun}s`;
}

function formatNumber(value) {
  return Number(value || 0).toLocaleString();
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function downloadTextBlob(filename, text) {
  const blob = new Blob([text], { type: "text/plain;charset=utf-8" });
  downloadBlob(filename, blob);
}

function downloadBlob(filename, blob) {
  const objectUrl = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = objectUrl;
  anchor.download = filename;
  document.body.append(anchor);
  anchor.click();
  anchor.remove();
  setTimeout(() => {
    URL.revokeObjectURL(objectUrl);
  }, 1_000);
}
