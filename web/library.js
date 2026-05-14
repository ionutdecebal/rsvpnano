const RSVP_VERSION = "1";
const WRAP_WIDTH = 96;

const SUPPORTED_EXTENSIONS = new Set([
  ".epub",
  ".html",
  ".htm",
  ".xhtml",
  ".md",
  ".markdown",
  ".txt",
]);

const SIDE_CAR_SUFFIXES = [".rsvp.failed", ".rsvp.tmp", ".rsvp.converting"];

const BLOCK_TAGS = new Set([
  "address",
  "article",
  "aside",
  "blockquote",
  "body",
  "br",
  "dd",
  "div",
  "dl",
  "dt",
  "figcaption",
  "figure",
  "footer",
  "header",
  "hr",
  "li",
  "main",
  "ol",
  "p",
  "pre",
  "section",
  "table",
  "tbody",
  "td",
  "tfoot",
  "th",
  "thead",
  "tr",
  "ul",
]);

const HEADING_TAGS = new Set(["h1", "h2", "h3", "h4", "h5", "h6"]);
const SKIP_TAGS = new Set(["head", "math", "nav", "script", "style", "svg"]);
const ASCII_REPLACEMENTS = {
  "\u00A0": " ",
  "\u1680": " ",
  "\u180E": " ",
  "\u2000": " ",
  "\u2001": " ",
  "\u2002": " ",
  "\u2003": " ",
  "\u2004": " ",
  "\u2005": " ",
  "\u2006": " ",
  "\u2007": " ",
  "\u2008": " ",
  "\u2009": " ",
  "\u200A": " ",
  "\u2028": " ",
  "\u2029": " ",
  "\u202F": " ",
  "\u205F": " ",
  "\u3000": " ",
  "\u2018": "'",
  "\u2019": "'",
  "\u201A": "'",
  "\u201B": "'",
  "\u2032": "'",
  "\u2035": "'",
  "\u201C": '"',
  "\u201D": '"',
  "\u201E": '"',
  "\u201F": '"',
  "\u00AB": '"',
  "\u00BB": '"',
  "\u2039": "'",
  "\u203A": "'",
  "\u2033": '"',
  "\u2036": '"',
  "\u300C": '"',
  "\u300D": '"',
  "\u300E": '"',
  "\u300F": '"',
  "\u2010": "-",
  "\u2011": "-",
  "\u2012": "-",
  "\u2013": "-",
  "\u2014": "-",
  "\u2015": "-",
  "\u2043": "-",
  "\u2212": "-",
  "\u2026": "...",
  "\u2022": "*",
  "\u00B7": "*",
  "\u2219": "*",
  "\u207D": "(",
  "\u208D": "(",
  "\u2768": "(",
  "\u276A": "(",
  "\u207E": ")",
  "\u208E": ")",
  "\u2769": ")",
  "\u276B": ")",
  "\u2045": "[",
  "\u2308": "[",
  "\u230A": "[",
  "\u3010": "[",
  "\u3014": "[",
  "\u3016": "[",
  "\u3018": "[",
  "\u301A": "[",
  "\u2046": "]",
  "\u2309": "]",
  "\u230B": "]",
  "\u3011": "]",
  "\u3015": "]",
  "\u3017": "]",
  "\u3019": "]",
  "\u301B": "]",
  "\u2774": "{",
  "\u2776": "{",
  "\u2775": "}",
  "\u2777": "}",
  "\u2329": "<",
  "\u27E8": "<",
  "\u3008": "<",
  "\u300A": "<",
  "\u232A": ">",
  "\u27E9": ">",
  "\u3009": ">",
  "\u300B": ">",
  "\u00A9": "(c)",
  "\u00AE": "(r)",
  "\u2122": "TM",
  "\uFB00": "ff",
  "\uFB01": "fi",
  "\uFB02": "fl",
  "\uFB03": "ffi",
  "\uFB04": "ffl",
  "\uFB05": "st",
  "\uFB06": "st",
  "\uFFFD": "",
};

const SPACE_LIKE_RE =
  /[\u00A0\u1680\u180E\u2000-\u200A\u2028\u2029\u202F\u205F\u3000\r\n\t]/g;
const COMBINING_MARKS_RE = /[\u0300-\u036f]/g;
const DEFAULT_OUTPUT_MODE = "unicode";

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

const elements = {
  addButton: document.querySelector("#library-add-button"),
  clearButton: document.querySelector("#library-clear-button"),
  downloadButton: document.querySelector("#library-download-button"),
  dropzone: document.querySelector("#library-dropzone"),
  dropzoneLarge: document.querySelector("#library-dropzone-large"),
  dropzoneWrapper: document.querySelector("#library-dropzone-wrapper"),
  fileInput: document.querySelector("#library-file-input"),
  folderCleanButton: document.querySelector("#library-folder-clean-button"),
  folderCleanVisible: document.querySelector("#library-folder-clean-visible"),
  folderImportButton: document.querySelector("#library-folder-import-button"),
  folderLabel: document.querySelector("#library-folder-label"),
  folderSummary: document.querySelector("#library-folder-summary"),
  folderFileList: document.querySelector("#library-folder-files"),
  folderFileDesc: document.querySelector("#library-folder-files-desc"),
  folderFileSection: document.querySelector("#library-folder-files-section"),
  list: document.querySelector("#library-list"),
  empty: document.querySelector("#library-empty"),
  status: document.querySelector("#library-status"),
  summary: document.querySelector("#library-summary"),
  summaryHeader: document.querySelector("#workspace-summary-header"),
  syncButton: document.querySelector("#library-sync-button"),
  syncDropzone: document.querySelector("#sync-dropzone"),
};

initialize();

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

  elements.folderImportButton.addEventListener("click", async () => {
    await importFromSelectedDirectory();
  });

  elements.folderCleanButton.addEventListener("click", async () => {
    await cleanSidecarsInSelectedDirectory();
  });

  elements.folderCleanVisible.addEventListener("click", async () => {
    await cleanSidecarsInSelectedDirectory();
  });

  elements.syncButton.addEventListener("click", async () => {
    if (!state.directoryHandle) {
      await chooseBooksDirectory();
    } else {
      await syncReadyBooksToSelectedDirectory();
    }
  });

  elements.syncDropzone.addEventListener("click", async () => {
    if (!state.directoryHandle) {
      await chooseBooksDirectory();
    } else {
      await syncReadyBooksToSelectedDirectory();
    }
  });

  elements.syncDropzone.addEventListener("keydown", async (event) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      if (!state.directoryHandle) {
        await chooseBooksDirectory();
      } else {
        await syncReadyBooksToSelectedDirectory();
      }
    }
  });

  elements.list.addEventListener("click", async (event) => {
    const button = event.target.closest("[data-action]");
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

    if (action === "retry") {
      await convertDescriptorIntoItem(item);
      renderLibrary();
      refreshUi();
      return;
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

  elements.dropzoneLarge.addEventListener("click", () => {
    elements.fileInput.click();
  });

  elements.dropzoneLarge.addEventListener("keydown", (event) => {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      elements.fileInput.click();
    }
  });

  elements.dropzoneLarge.addEventListener("dragenter", (event) => {
    event.preventDefault();
    state.dragDepth += 1;
    elements.dropzoneLarge.classList.add("is-active");
  });

  elements.dropzoneLarge.addEventListener("dragover", (event) => {
    event.preventDefault();
  });

  elements.dropzoneLarge.addEventListener("dragleave", (event) => {
    event.preventDefault();
    state.dragDepth = Math.max(0, state.dragDepth - 1);
    if (state.dragDepth === 0) {
      elements.dropzoneLarge.classList.remove("is-active");
    }
  });

  elements.dropzoneLarge.addEventListener("drop", async (event) => {
    event.preventDefault();
    state.dragDepth = 0;
    elements.dropzoneLarge.classList.remove("is-active");

    const files = Array.from(event.dataTransfer?.files || []).filter((file) => file.size >= 0);
    if (files.length > 0) {
      await importFiles(files, "upload");
    }
  });

  if (!supportsDirectoryAccess()) {
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
    : "";

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
    elements.folderSummary.textContent = "";
  }

  const hasFolder = !!state.directoryHandle;

  const sidecarFiles = hasFolder
    ? (state.folderInventory?.sidecarNames || []).slice().sort((a, b) => a.localeCompare(b, undefined, { sensitivity: "base" }))
    : [];

  elements.folderFileSection.hidden = sidecarFiles.length === 0;
  elements.folderFileDesc.textContent = `You have ${sidecarFiles.length} old ${pluralize("file", sidecarFiles.length)} that can be removed.`;
  elements.folderFileList.innerHTML = sidecarFiles
    .map((name) => `<li><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" width="13" height="13" style="vertical-align:-1px;margin-right:5px;opacity:.55"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>${escapeHtml(name)}</li>`)
    .join("");
  elements.folderCleanVisible.disabled = state.isBusy;

  const noFolder = !state.directoryHandle;
  const noReadyItems = readyItems.length === 0;
  const noItems = state.items.length === 0;

  elements.addButton.disabled = state.isBusy;
  elements.fileInput.disabled = state.isBusy;
  elements.downloadButton.hidden = noReadyItems;
  elements.downloadButton.disabled = state.isBusy;
  elements.clearButton.disabled = state.isBusy || noItems;
  elements.folderImportButton.disabled =
    state.isBusy || !supportsDirectoryAccess() || noFolder;
  elements.folderCleanButton.disabled =
    state.isBusy || !supportsDirectoryAccess() || noFolder;
  elements.syncButton.hidden = noReadyItems && noFolder;
  if (noFolder) {
    elements.syncButton.textContent = "Connect SD";
    elements.syncButton.disabled = state.isBusy || !supportsDirectoryAccess();
  } else {
    elements.syncButton.textContent = "Sync folder";
    elements.syncButton.disabled = state.isBusy || noReadyItems;
  }
}

function renderLibrary() {
  refreshItemWarnings();
  const hasItems = state.items.length > 0;
  elements.dropzone.hidden = !hasItems;
  elements.dropzoneLarge.hidden = hasItems;
  elements.dropzoneWrapper.hidden = hasItems;

  if (!hasItems) {
    elements.list.innerHTML = "";
    return;
  }

  elements.empty.hidden = true;
  elements.list.innerHTML = state.items
    .map((item) => {
      const isWorking = item.status === "working";
      const isQueued = item.status === "queued";
      const isError = item.status === "error";
      const warningCopy = item.warning
        ? `<p class="library-item-copy">${escapeHtml(item.warning)}</p>`
        : "";
      const progressWidth = isWorking && item.progress != null ? Math.round(item.progress * 100) : 0;
      const detailCopy =
        item.status === "ready"
          ? ""
          : isError
            ? escapeHtml(item.error || "Conversion failed.")
            : isQueued
              ? "Next..."
              : `<span class="spinner" aria-hidden="true"></span>Converting...`;
      const metaCopy =
        item.status === "ready"
          ? (() => {
              const parts = [
                `${formatNumber(item.wordCount)} ${pluralize("word", item.wordCount)} · ${formatNumber(item.chapterCount)} ${pluralize("chapter", item.chapterCount)}`,
              ];
              if (item.author) parts.push(escapeHtml(item.author));
              return `<p class="library-item-copy">${parts.join(" · ")}</p>`;
            })()
          : "";

      return `
        <li class="library-item" id="library-item-${item.id}">
          <div class="library-item-left">
            <img class="library-item-icon" src="./icons/file.svg" alt="" aria-hidden="true">
            <div class="library-item-body">
              <strong>${escapeHtml(item.sourceName)}</strong>
            </div>
            <div class="library-item-actions">
              <button class="icon-button-ghost" type="button" data-action="remove" data-item-id="${item.id}" data-danger title="Remove" aria-label="Remove">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" width="16" height="16">
                  <line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>
                </svg>
              </button>
            </div>
          </div>
          <div class="library-item-right${item.status === "ready" || isError ? " library-item-right-clickable" : ""}"
            id="library-item-right-${item.id}"
            ${isWorking ? `data-state="working" style="--progress:${progressWidth}%"` : ""}
            ${item.status === "ready" ? `data-action="download" data-item-id="${item.id}" role="button" tabindex="0" title="Download"` : ""}
            ${isError ? `data-action="retry" data-item-id="${item.id}" role="button" tabindex="0" title="Riprova"` : ""}>
            ${isWorking || isQueued || isError ? "" : `<img class="library-item-icon" src="./icons/book.svg" alt="" aria-hidden="true">`}
            <div class="library-item-body">
              ${isWorking || isQueued || isError ? "" : `<strong class="library-item-detected-title">${escapeHtml(item.title || stripExtension(item.sourceName))}</strong>`}
              ${detailCopy ? `<p class="library-item-copy">${detailCopy}</p>` : ""}
              ${metaCopy}
              ${warningCopy}
            </div>
            ${item.status === "ready" ? `<span class="library-item-download-icon" aria-hidden="true">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" width="18" height="18">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>
              </svg></span>` : ""}
            ${isError ? `<span class="library-item-download-icon" aria-hidden="true">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" width="18" height="18">
                <polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/>
              </svg></span>` : ""}
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
      item.status = "queued";
      item.error = "";
      item.warning = "";
      targets.push(item);
    }

    renderLibrary();
    refreshUi();

    for (let index = 0; index < targets.length; index += 1) {
      const item = targets[index];
      item.status = "working";
      setStatus(
        statusTitle,
        `Converting ${index + 1} of ${targets.length}: ${item.sourceName}`,
        "busy",
      );
      renderLibrary();
      await convertDescriptorIntoItem(item);
      renderLibrary();
      refreshUi();
      if (item.status === "ready") {
        const right = elements.list.querySelector(`#library-item-right-${item.id}`);
        if (right) right.classList.add("is-converting-done");
      }
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
        `${readyCount} ${pluralize("book", readyCount)} are ready to download or sync into /books.`,
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
    progress: null,
  };
}

function updateItemProgress(item, done, total) {
  item.progress = total > 0 ? done / total : null;
  const right = elements.list.querySelector(`#library-item-right-${item.id}`);
  if (right) {
    right.style.setProperty("--progress", item.progress != null ? `${Math.round(item.progress * 100)}%` : "0%");
  }
}

async function convertDescriptorIntoItem(item) {
  item.status = "working";
  item.error = "";
  item.warning = "";
  item.progress = null;

  try {
    const file = await item.descriptor.getFile();
    const { title, author, events } = await eventsForFile(file, state.outputMode, (done, total) => {
      updateItemProgress(item, done, total);
    });
    const writer = new RsvpWriter({
      title: title || stripExtension(file.name),
      author,
      source: file.name,
      mode: state.outputMode,
    });

    for (const [kind, value] of events) {
      if (kind === "chapter") {
        writer.addChapter(value);
        continue;
      }

      writer.beginParagraph();
      writer.addText(value);
    }

    item.title = writer.title;
    item.author = writer.author;
    item.outputName = buildOutputName(writer.title || stripExtension(file.name), writer.author);
    item.outputText = writer.finalize(title || stripExtension(file.name));
    item.wordCount = writer.wordCount;
    item.chapterCount = writer.chapterCount;
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
        "The page can now scan, clean, and sync files directly inside /books.",
        "success",
      );
    } else {
      setStatus(
        "Folder selected",
        `You picked /${directoryHandle.name}. For best results, point this at the SD card’s /books folder.`,
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
    sidecarNames: [],
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
      inventory.sidecarNames.push(entry.name);
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

async function eventsForFile(file, mode, onProgress) {
  const sourceExt = extensionForName(file.name);
  if (sourceExt === ".epub") {
    return epubEventsAndMetadata(file, mode, onProgress);
  }
  if (sourceExt === ".html" || sourceExt === ".htm" || sourceExt === ".xhtml") {
    const markup = await readTextFile(file);
    const { title, events } = htmlEventsAndTitle(markup, stripExtension(file.name), mode);
    return { title, author: "", events };
  }
  if (sourceExt === ".txt" || sourceExt === ".md" || sourceExt === ".markdown") {
    const raw = await readTextFile(file);
    const { title, author, body } = parseFrontmatter(raw, stripExtension(file.name));
    return { title, author, events: await textEventsAsync(body, mode, onProgress) };
  }
  throw new Error(`Unsupported extension: ${sourceExt}`);
}

async function readTextFile(file) {
  const bytes = new Uint8Array(await file.arrayBuffer());
  return decodeTextBytes(bytes);
}

function decodeTextBytes(bytes) {
  if (bytes.length >= 3 && bytes[0] === 0xef && bytes[1] === 0xbb && bytes[2] === 0xbf) {
    return new TextDecoder("utf-8").decode(bytes.subarray(3));
  }
  if (bytes.length >= 2 && bytes[0] === 0xff && bytes[1] === 0xfe) {
    return new TextDecoder("utf-16le").decode(bytes.subarray(2));
  }
  if (bytes.length >= 2 && bytes[0] === 0xfe && bytes[1] === 0xff) {
    return new TextDecoder("utf-16be").decode(bytes.subarray(2));
  }

  const guessedUtf16 = detectUtf16WithoutBom(bytes);
  if (guessedUtf16) {
    return decodeWithDeclaredEncoding(bytes, guessedUtf16);
  }

  return decodeWithDeclaredEncoding(bytes, "utf-8");
}

function detectUtf16WithoutBom(bytes) {
  if (bytes.length < 4) {
    return null;
  }
  if (bytes[0] === 0x3c && bytes[1] === 0x00 && bytes[2] === 0x3f && bytes[3] === 0x00) {
    return "utf-16le";
  }
  if (bytes[0] === 0x00 && bytes[1] === 0x3c && bytes[2] === 0x00 && bytes[3] === 0x3f) {
    return "utf-16be";
  }
  return null;
}

function decodeWithDeclaredEncoding(bytes, initialEncoding) {
  let decoded = tryDecode(bytes, initialEncoding);
  if (decoded === null) {
    decoded = tryDecode(bytes, "windows-1252");
  }
  if (decoded === null) {
    decoded = new TextDecoder("utf-8").decode(bytes);
  }

  const declaredEncoding = sniffDeclaredEncoding(decoded);
  if (
    declaredEncoding &&
    declaredEncoding.toLowerCase() !== initialEncoding.toLowerCase()
  ) {
    const redecode = tryDecode(bytes, declaredEncoding);
    if (redecode !== null) {
      return redecode;
    }
  }

  return decoded;
}

function tryDecode(bytes, encoding) {
  try {
    return new TextDecoder(encoding, { fatal: true }).decode(bytes);
  } catch (error) {
    return null;
  }
}

function sniffDeclaredEncoding(text) {
  const head = text.slice(0, 512);
  const xmlMatch = head.match(/encoding\s*=\s*["']([^"']+)["']/i);
  if (xmlMatch?.[1]) {
    return xmlMatch[1].trim();
  }
  const htmlMatch = head.match(/charset\s*=\s*["']?\s*([^"'>\s/]+)/i);
  if (htmlMatch?.[1]) {
    return htmlMatch[1].trim();
  }
  return null;
}

async function epubEventsAndMetadata(file, mode, onProgress) {
  const JSZip = await loadJsZip();
  const zip = await JSZip.loadAsync(file);
  const opfPath = await containerRootfile(zip);
  const { title, author, spinePaths } = await parsePackage(zip, opfPath, mode);

  const events = [];
  for (let index = 0; index < spinePaths.length; index += 1) {
    onProgress?.(index, spinePaths.length);
    await new Promise(requestAnimationFrame);
    const spinePath = spinePaths[index];
    const chapterMarkup = await readZipText(zip, spinePath);
    const chapterEvents = htmlEvents(chapterMarkup, mode);
    if (!chapterEvents.some(([kind]) => kind === "text")) {
      continue;
    }
    if (!chapterEvents.some(([kind]) => kind === "chapter")) {
      chapterEvents.unshift(["chapter", fallbackChapterTitle(spinePath, index + 1, mode)]);
    }
    events.push(...chapterEvents);
  }

  if (events.length === 0) {
    throw new Error("EPUB spine does not contain readable XHTML/HTML content.");
  }

  return {
    title: title || stripExtension(file.name),
    author,
    events,
  };
}

async function containerRootfile(zip) {
  const containerXml = await readZipText(zip, "META-INF/container.xml");
  const doc = parseXmlDocument(containerXml, "EPUB container.xml");

  for (const node of Array.from(doc.getElementsByTagName("*"))) {
    if (localName(node) !== "rootfile") {
      continue;
    }
    const fullPath = node.getAttribute("full-path");
    if (fullPath) {
      return normalizeZipPath(fullPath);
    }
  }

  throw new Error("EPUB container.xml does not name an OPF package file.");
}

async function parsePackage(zip, opfPath, mode) {
  const packageXml = await readZipText(zip, opfPath);
  const doc = parseXmlDocument(packageXml, "EPUB package");
  const title = firstNodeText(doc, "title", mode);
  const author = firstNodeText(doc, "creator", mode);

  const manifest = new Map();
  const manifestContentPaths = [];

  for (const node of Array.from(doc.getElementsByTagName("*"))) {
    if (localName(node) !== "item") {
      continue;
    }
    const itemId = node.getAttribute("id");
    const href = node.getAttribute("href");
    const mediaType = node.getAttribute("media-type") || "";
    if (!itemId || !href) {
      continue;
    }

    const resolvedPath = zipJoin(opfPath, href);
    manifest.set(itemId, {
      path: resolvedPath,
      mediaType,
    });

    if (isContentDocument(resolvedPath, mediaType)) {
      manifestContentPaths.push(resolvedPath);
    }
  }

  const spinePaths = [];
  for (const node of Array.from(doc.getElementsByTagName("*"))) {
    if (localName(node) !== "itemref") {
      continue;
    }
    const idref = node.getAttribute("idref");
    if (!idref || !manifest.has(idref)) {
      continue;
    }
    const item = manifest.get(idref);
    if (isContentDocument(item.path, item.mediaType)) {
      spinePaths.push(item.path);
    }
  }

  const readingOrder = spinePaths.length > 0 ? spinePaths : manifestContentPaths;
  if (readingOrder.length === 0) {
    throw new Error("EPUB spine does not contain readable XHTML/HTML documents.");
  }

  return { title, author, spinePaths: readingOrder };
}

function firstNodeText(doc, name, mode) {
  for (const node of Array.from(doc.getElementsByTagName("*"))) {
    if (localName(node) !== name) {
      continue;
    }
    const text = cleanText(node.textContent || "", mode);
    if (text) {
      return text;
    }
  }
  return "";
}

function localName(node) {
  if (node.localName) {
    return node.localName.toLowerCase();
  }
  return node.nodeName.toLowerCase().split(":").pop();
}

function isContentDocument(path, mediaType) {
  const loweredPath = path.toLowerCase();
  const loweredType = mediaType.toLowerCase();
  return (
    loweredType === "application/xhtml+xml" ||
    loweredType === "text/html" ||
    loweredPath.endsWith(".xhtml") ||
    loweredPath.endsWith(".html") ||
    loweredPath.endsWith(".htm")
  );
}

function parseXmlDocument(text, label) {
  const parser = new DOMParser();
  const doc = parser.parseFromString(text, "application/xml");
  const parserError = doc.querySelector("parsererror");
  if (parserError) {
    throw new Error(`${label} could not be parsed.`);
  }
  return doc;
}

async function readZipText(zip, requestedPath) {
  const entry = findZipEntry(zip, requestedPath);
  if (!entry) {
    throw new Error(`Missing EPUB member: ${requestedPath}`);
  }
  const bytes = await entry.async("uint8array");
  return decodeTextBytes(bytes);
}

function findZipEntry(zip, requestedPath) {
  const normalizedRequested = normalizeZipPath(requestedPath);
  const exact = zip.file(normalizedRequested);
  if (exact) {
    return exact;
  }

  const loweredRequested = normalizedRequested.toLowerCase();
  return (
    Object.values(zip.files).find(
      (entry) => normalizeZipPath(entry.name).toLowerCase() === loweredRequested,
    ) || null
  );
}

function normalizeZipPath(path) {
  return path.replace(/\\/g, "/").replace(/^\/+/, "");
}

function zipJoin(base, href) {
  const withoutFragment = href.split("#", 1)[0].split("?", 1)[0];
  let decoded = withoutFragment;
  try {
    decoded = decodeURIComponent(withoutFragment);
  } catch (error) {
    decoded = withoutFragment;
  }

  if (decoded.startsWith("/")) {
    decoded = decoded.replace(/^\/+/, "");
  } else {
    decoded = `${zipDirname(base)}${decoded}`;
  }

  return collapseZipPath(decoded);
}

function zipDirname(path) {
  const normalized = normalizeZipPath(path);
  const slashIndex = normalized.lastIndexOf("/");
  if (slashIndex < 0) {
    return "";
  }
  return normalized.slice(0, slashIndex + 1);
}

function collapseZipPath(path) {
  const parts = [];
  for (const part of normalizeZipPath(path).split("/")) {
    if (!part || part === ".") {
      continue;
    }
    if (part === "..") {
      parts.pop();
      continue;
    }
    parts.push(part);
  }
  return parts.join("/");
}

function fallbackChapterTitle(path, index, mode) {
  const base = stripExtension(path.split("/").pop() || `chapter-${index}`);
  const cleaned = cleanText(base.replace(/[_-]+/g, " "), mode);
  return cleaned || `Chapter ${index}`;
}

function htmlEventsAndTitle(markup, fallbackTitle, mode) {
  const parser = new DOMParser();
  const doc = parser.parseFromString(markup, "text/html");
  const title = cleanText(doc.title || fallbackTitle, mode) || fallbackTitle;
  return {
    title,
    events: htmlEvents(markup, mode),
  };
}

function htmlEvents(markup, mode) {
  const parser = new DOMParser();
  const doc = parser.parseFromString(markup, "text/html");
  const events = [];
  const textParts = [];

  const flushText = () => {
    const text = cleanText(textParts.join(" "), mode);
    textParts.length = 0;
    if (text) {
      events.push(["text", text]);
    }
  };

  const visit = (node) => {
    if (node.nodeType === Node.TEXT_NODE) {
      textParts.push(node.nodeValue || "");
      return;
    }

    if (node.nodeType !== Node.ELEMENT_NODE) {
      return;
    }

    const tag = node.tagName.toLowerCase();
    if (SKIP_TAGS.has(tag)) {
      return;
    }

    if (HEADING_TAGS.has(tag)) {
      flushText();
      const chapterTitle = cleanText(node.textContent || "", mode);
      if (chapterTitle) {
        events.push(["chapter", chapterTitle]);
      }
      return;
    }

    if (tag === "br") {
      flushText();
      return;
    }

    const isBlock = BLOCK_TAGS.has(tag);
    if (isBlock) {
      flushText();
    }

    for (const child of Array.from(node.childNodes)) {
      visit(child);
    }

    if (isBlock) {
      flushText();
    }
  };

  const root = doc.body || doc.documentElement;
  visit(root);
  flushText();
  return events;
}

function textEvents(text, mode) {
  const events = [];
  const paragraphParts = [];

  const flushParagraph = () => {
    if (paragraphParts.length === 0) {
      return;
    }
    const paragraph = cleanText(paragraphParts.join(" "), mode);
    paragraphParts.length = 0;
    if (paragraph) {
      events.push(["text", paragraph]);
    }
  };

  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trim();
    const chapter = looksLikeChapter(line, mode);
    if (chapter) {
      flushParagraph();
      events.push(["chapter", chapter]);
      continue;
    }

    if (!line) {
      flushParagraph();
      continue;
    }

    paragraphParts.push(line);
  }

  flushParagraph();
  return events;
}

async function textEventsAsync(text, mode, onProgress) {
  const CHUNK = 200;
  const lines = text.split(/\r?\n/);
  const events = [];
  const paragraphParts = [];

  const flushParagraph = () => {
    if (paragraphParts.length === 0) {
      return;
    }
    const paragraph = cleanText(paragraphParts.join(" "), mode);
    paragraphParts.length = 0;
    if (paragraph) {
      events.push(["text", paragraph]);
    }
  };

  for (let i = 0; i < lines.length; i += 1) {
    if (i > 0 && i % CHUNK === 0) {
      onProgress?.(i, lines.length);
      await new Promise(requestAnimationFrame);
    }

    const line = lines[i].trim();
    const chapter = looksLikeChapter(line, mode);
    if (chapter) {
      flushParagraph();
      events.push(["chapter", chapter]);
      continue;
    }

    if (!line) {
      flushParagraph();
      continue;
    }

    paragraphParts.push(line);
  }

  flushParagraph();
  return events;
}

function parseFrontmatter(text, fallbackTitle) {
  if (!text.startsWith("---")) {
    return { title: fallbackTitle, author: "", body: text };
  }
  const end = text.indexOf("\n---", 3);
  if (end < 0) {
    return { title: fallbackTitle, author: "", body: text };
  }
  const block = text.slice(3, end);
  const body = text.slice(end + 4).replace(/^\r?\n/, "");
  let title = fallbackTitle;
  let author = "";
  for (const line of block.split(/\r?\n/)) {
    const colon = line.indexOf(":");
    if (colon < 0) continue;
    const key = line.slice(0, colon).trim().toLowerCase();
    const value = line.slice(colon + 1).trim().replace(/^["']|["']$/g, "");
    if (key === "title" && value) title = value;
    if (key === "author" && value) author = value;
  }
  return { title, author, body };
}

function looksLikeChapter(line, mode) {
  const trimmed = cleanText(line, mode);
  if (!trimmed || trimmed.length > 64) {
    return null;
  }

  if (trimmed.startsWith("#")) {
    const title = cleanText(trimmed.replace(/^#+/, "").trim(), mode);
    return title || null;
  }

  if (/^(chapter|part|book)\b/i.test(trimmed)) {
    return trimmed;
  }

  return null;
}

class RsvpWriter {
  constructor({ title, author, source, mode }) {
    this.title = directiveText(title, mode);
    this.author = directiveText(author, mode);
    this.mode = mode;
    this.lines = [`@rsvp ${RSVP_VERSION}`, `@title ${this.title}`];
    if (this.author) {
      this.lines.push(`@author ${this.author}`);
    }
    this.lines.push(`@source ${directiveText(source, mode)}`);
    this.lines.push("");
    this.wordCount = 0;
    this.chapterCount = 0;
    this.lineWords = [];
    this.lineLength = 0;
    this.lastChapter = "";
  }

  addChapter(title) {
    const value = directiveText(title, this.mode);
    if (!value || value === this.lastChapter) {
      return;
    }
    this.flushLine();
    if (this.lines.length > 0 && this.lines[this.lines.length - 1] !== "") {
      this.lines.push("");
    }
    this.lines.push(`@chapter ${value}`);
    this.chapterCount += 1;
    this.lastChapter = value;
  }

  beginParagraph() {
    this.flushLine();
    if (this.wordCount > 0) {
      if (this.lines.length > 0 && this.lines[this.lines.length - 1] !== "") {
        this.lines.push("");
      }
      this.lines.push("@para");
    }
  }

  addText(text) {
    for (const word of iterCleanWords(text, this.mode)) {
      const projected =
        this.lineWords.length === 0 ? word.length : this.lineLength + 1 + word.length;
      if (this.lineWords.length > 0 && projected > WRAP_WIDTH) {
        this.flushLine();
      }

      this.lineWords.push(word);
      this.lineLength =
        this.lineWords.length === 1 ? word.length : this.lineLength + 1 + word.length;
      this.wordCount += 1;
    }
  }

  flushLine() {
    if (this.lineWords.length === 0) {
      return;
    }
    let line = this.lineWords.join(" ");
    if (line.startsWith("@")) {
      line = `@${line}`;
    }
    this.lines.push(line);
    this.lineWords = [];
    this.lineLength = 0;
  }

  finalize(fallbackChapterTitle) {
    this.flushLine();
    if (this.wordCount === 0) {
      throw new Error("No readable words found in this source.");
    }

    if (this.chapterCount === 0) {
      const chapter = `@chapter ${directiveText(fallbackChapterTitle, this.mode)}`;
      const insertIndex = this.lines.findIndex((line) => line === "");
      if (insertIndex >= 0) {
        this.lines.splice(insertIndex, 0, chapter);
      } else {
        this.lines.push(chapter);
      }
    }

    return `${this.lines.join("\n").trim()}\n`;
  }
}

function iterCleanWords(text, mode) {
  const cleaned = cleanText(text, mode);
  if (!cleaned) {
    return [];
  }

  return cleaned
    .split(/\s+/)
    .filter((token) => token && /[\p{L}\p{N}]/u.test(token));
}

function cleanText(text, mode) {
  let value = text || "";
  value = value.replace(SPACE_LIKE_RE, " ").replace(/\uFFFD/g, "");

  if (mode === "ascii") {
    value = Array.from(value, (character) => ASCII_REPLACEMENTS[character] ?? character).join("");
    value = value.replace(/[\uFF01-\uFF5E]/g, (character) =>
      String.fromCharCode(character.charCodeAt(0) - 0xfee0),
    );
    value = value.normalize("NFKD").replace(COMBINING_MARKS_RE, "");
    value = value.replace(/[^\x20-\x7E]/g, "");
  } else {
    value = value.normalize("NFC");
  }

  return value.replace(/\s+/g, " ").trim();
}

function directiveText(text, mode) {
  return cleanText(text, mode).replace(/[\r\n]+/g, " ");
}

function extensionForName(name) {
  const lastDot = name.lastIndexOf(".");
  return lastDot >= 0 ? name.slice(lastDot).toLowerCase() : "";
}

function stripExtension(name) {
  const lastSlash = Math.max(name.lastIndexOf("/"), name.lastIndexOf("\\"));
  const base = lastSlash >= 0 ? name.slice(lastSlash + 1) : name;
  const lastDot = base.lastIndexOf(".");
  return lastDot > 0 ? base.slice(0, lastDot) : base;
}

function buildOutputName(title, author) {
  const slug = (s) => s.trim().normalize("NFD").replace(/[\u0300-\u036f]/g, "").replace(/[^\w]+/g, "_").replace(/^_|_$/g, "");
  const t = slug(title);
  const a = author ? slug(author) : "";
  return a ? `${t}__${a}.rsvp` : `${t}.rsvp`;
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
