class PrepareSdcard extends HTMLElement {
  connectedCallback() {
    const isMac = /mac/i.test(navigator.userAgent) && !/iphone|ipad/i.test(navigator.userAgent);
    const defaultTab = isMac ? "mac" : "windows";

    this.innerHTML = `
      <section class="card install-steps" id="sdcard-section">
        <div class="section-header" id="sdcard-toggle">
          <div style="flex:1;display:flex;align-items:center;gap:10px">
            <span class="step-number">2</span>
            <h2>Prepare SD Card</h2>
          </div>
          <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"></polyline></svg>
        </div>
        <div class="section-body">
          <div class="section-body-inner">
            <p>Use a microSD card up to <strong>64 GB</strong>, formatted as <strong>FAT32</strong>.</p>

            <div class="sdcard-tabs">
              <button class="sdcard-tab${defaultTab === "windows" ? " active" : ""}" data-tab="windows">Windows</button>
              <button class="sdcard-tab${defaultTab === "mac" ? " active" : ""}" data-tab="mac">Mac</button>
            </div>

            <div class="sdcard-panel" id="sdcard-panel-windows" style="display:${defaultTab === "windows" ? "block" : "none"}">
              <ol>
                <li>Insert the SD card into your PC</li>
                <li>Open <strong>File Explorer</strong>, right-click the SD card drive</li>
                <li>Select <strong>Format…</strong></li>
                <li>Set <strong>File system</strong> to <strong>FAT32</strong><br><small>If the card is larger than 32 GB, Windows won't offer FAT32 — use a free tool like <strong>guiformat</strong> (FAT32 Format) instead.</small></li>
                <li>Leave <em>Allocation unit size</em> on Default</li>
                <li>Check <strong>Quick Format</strong>, then click <strong>Start</strong></li>
              </ol>
            </div>

            <div class="sdcard-panel" id="sdcard-panel-mac" style="display:${defaultTab === "mac" ? "block" : "none"}">
              <ol>
                <li>Insert the SD card into your Mac</li>
                <li>Open <strong>Disk Utility</strong> (Spotlight → "Disk Utility")</li>
                <li>Select the SD card in the sidebar</li>
                <li>Click <strong>Erase</strong></li>
                <li>Set <strong>Format</strong> to <strong>MS-DOS (FAT)</strong></li>
                <li>Set <strong>Scheme</strong> to <strong>Master Boot Record</strong></li>
                <li>Click <strong>Erase</strong></li>
              </ol>
            </div>
          </div>
        </div>
      </section>
    `;

    this.querySelector("#sdcard-toggle").addEventListener("click", () => {
      this.querySelector("#sdcard-section").classList.toggle("is-collapsed");
    });

    this.querySelectorAll(".sdcard-tab").forEach(btn => {
      btn.addEventListener("click", (e) => {
        e.stopPropagation();
        const tab = btn.dataset.tab;
        this.querySelectorAll(".sdcard-tab").forEach(b => b.classList.toggle("active", b === btn));
        this.querySelectorAll(".sdcard-panel").forEach(p => {
          p.style.display = p.id === `sdcard-panel-${tab}` ? "block" : "none";
        });
      });
    });
  }
}

customElements.define("prepare-sdcard", PrepareSdcard);
