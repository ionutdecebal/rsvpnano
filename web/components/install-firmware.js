const FLASH_KEY = "rsvpnano_last_flash";

function timeAgo(ts) {
  const s = Math.floor((Date.now() - ts) / 1000);
  if (s < 60) return "just now";
  const m = Math.floor(s / 60);
  if (m < 60) return m + (m === 1 ? " minute ago" : " minutes ago");
  const h = Math.floor(m / 60);
  if (h < 24) return h + (h === 1 ? " hour ago" : " hours ago");
  const d = Math.floor(h / 24);
  return d + (d === 1 ? " day ago" : " days ago");
}

class InstallFirmware extends HTMLElement {
  connectedCallback() {
    this.innerHTML = `
      <section class="card install-steps" id="install-section">
        <div class="section-header" id="install-toggle">
          <div style="flex:1;display:flex;align-items:center;gap:10px">
            <span class="step-number">1</span>
            <h2>Install Firmware</h2>
          </div>
          <span class="flash-history" id="flash-history"></span>
          <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="6 9 12 15 18 9"></polyline></svg>
        </div>
        <div class="section-body">
          <div class="section-body-inner">
            <p>Flash the current browser installer manifest, then use the next steps to prepare books and sync them onto the SD card.</p>
            <p>Put the device in boot mode before starting the installer:</p>
            <ol>
              <li>Turn the device off.</li>
              <li>Hold <code>BOOT</code> while connecting a USB data cable.</li>
              <li>If the installer cannot connect, tap reset or power-cycle, then try again.</li>
            </ol>
          </div>
          <div class="section-body-inner">
            <div class="install-option">
              <div class="install-option-head">
                <strong class="fw-version"></strong>
                <span class="latest-badge"><span class="pulse-dot"></span>Latest Release</span>
              </div>
              <ul class="feature-list"></ul>
              <esp-web-install-button manifest="firmware/manifest.json">
                <button slot="activate">Install Firmware</button>
                <span slot="unsupported">Use Chrome or Edge on desktop with Web Serial support.</span>
                <span slot="not-allowed">This page must be opened over HTTPS or localhost.</span>
              </esp-web-install-button>
              <p class="install-warning">Important: keep the device plugged in until the installer says it's done.</p>
            </div>
          </div>
        </div>
      </section>
    `;

    this._section = this.querySelector("#install-section");
    this._historyEl = this.querySelector("#flash-history");

    this.querySelector("#install-toggle").addEventListener("click", () => {
      this._section.classList.toggle("is-collapsed");
    });

    this._showFlashHistory();
    this._autoCollapse();
    this._observeInstallDialog();

    fetch("firmware/manifest.json")
      .then(r => r.json())
      .then(m => {
        this._fwVersion = m.version;
        this.querySelector(".fw-version").textContent = "Version " + m.version;
        if (m.features) {
          const ul = this.querySelector(".feature-list");
          ul.innerHTML = m.features.map(f => "<li>" + f + "</li>").join("");
        }
        this._updateButtonText();
      });
  }

  _showFlashHistory() {
    try {
      const data = JSON.parse(localStorage.getItem(FLASH_KEY));
      if (data && data.timestamp) {
        this._historyEl.textContent = "v" + data.version + " flashed " + timeAgo(data.timestamp);
      } else {
        this._historyEl.textContent = "No installations in history";
      }
    } catch (e) {
      this._historyEl.textContent = "No installations in history";
    }
  }

  _autoCollapse() {
    try {
      const data = JSON.parse(localStorage.getItem(FLASH_KEY));
      if (data && data.timestamp) {
        this._section.classList.add("is-collapsed");
      }
    } catch (e) {}
  }

  _updateButtonText() {
    try {
      const data = JSON.parse(localStorage.getItem(FLASH_KEY));
      if (data && data.version && this._fwVersion && data.version !== this._fwVersion) {
        const btn = this.querySelector('button[slot="activate"]');
        if (btn) btn.textContent = "Update Firmware";
      }
    } catch (e) {}
  }

  _observeInstallDialog() {
    new MutationObserver((mutations) => {
      mutations.forEach((m) => {
        m.addedNodes.forEach((node) => {
          if (node.nodeName !== "EWT-INSTALL-DIALOG") return;

          let saved = false;
          const pollTimer = setInterval(() => {
            if (!document.body.contains(node)) { clearInterval(pollTimer); return; }
            if (saved) return;
            try {
              const text = node.shadowRoot?.textContent || "";
              if (text.indexOf("Installation complete") !== -1) {
                saved = true;
                clearInterval(pollTimer);
                localStorage.setItem(FLASH_KEY, JSON.stringify({
                  version: this._fwVersion,
                  timestamp: Date.now(),
                }));
                this._showFlashHistory();
              }
            } catch (e) {}
          }, 500);

          setTimeout(() => clearInterval(pollTimer), 600000);
        });
      });
    }).observe(document.body, { childList: true });
  }
}

customElements.define("install-firmware", InstallFirmware);
