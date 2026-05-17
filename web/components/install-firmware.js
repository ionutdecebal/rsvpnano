const FLASH_KEY = "rsvpnano_last_flash";

function timeAgo(ts) {
  const s = Math.max(0, Math.floor((Date.now() - ts) / 1000));
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
      <section class="card step-card" id="install-section">
        <button class="step-card-toggle" id="install-toggle" type="button" aria-expanded="true" aria-controls="install-content">
          <span class="section-header-main">
            <span class="step-number">1</span>
            <span class="section-header-label">
              <span class="section-kicker">Browser Flasher</span>
              <span class="section-title">Install Firmware</span>
            </span>
          </span>
          <span class="flash-history" id="flash-history"></span>
          <svg class="chevron" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><polyline points="6 9 12 15 18 9"></polyline></svg>
        </button>
        <div class="section-body" id="install-content">
          <div class="section-body-inner">
            <p>Flash the current browser installer manifest, then use the next steps to prepare books and sync them onto the SD card.</p>
            <p>Put the device in boot mode before starting the installer:</p>
            <ol>
              <li>Turn the device off.</li>
              <li>Hold <code>BOOT</code> while connecting a USB data cable.</li>
              <li>On Linux, if you use Chromium from Snap, run <code>sudo snap connect chromium:raw-usb</code> once, then restart Chromium.</li>
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
              <div id="uptodate-badge" class="uptodate-badge" hidden>
                <span class="uptodate-left">
                  <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>
                  Up to date
                </span>
                <span class="uptodate-right">
                  <span id="uptodate-version"></span>
                  <span id="uptodate-ago"></span>
                </span>
              </div>
              <esp-web-install-button manifest="firmware/manifest.json">
                <button id="firmware-install-btn" slot="activate">Install Firmware</button>
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
    this._installBtn = this.querySelector("#firmware-install-btn");

    const toggle = this.querySelector("#install-toggle");
    const content = this.querySelector("#install-content");
    toggle.addEventListener("click", () => {
      const collapsed = !this._section.classList.contains("is-collapsed");
      this._section.classList.toggle("is-collapsed", collapsed);
      toggle.setAttribute("aria-expanded", collapsed ? "false" : "true");
      content.hidden = collapsed;
    });

    this._showFlashHistory();
    this._autoCollapse();
    this._observeInstallDialog();

    fetch("firmware/manifest.json", { cache: "no-store" })
      .then(r => r.json())
      .then(m => {
        this._fwVersion = m.version;
        this.querySelector(".fw-version").textContent = "Version " + m.version;
        if (m.features) {
          const ul = this.querySelector(".feature-list");
          ul.innerHTML = m.features.map(f => "<li>" + f + "</li>").join("");
        }
        this._refreshInstallButton();
        this._showFlashHistory();
      });
  }

  _readFlashData() {
    try {
      return JSON.parse(localStorage.getItem(FLASH_KEY));
    } catch (e) {
      return null;
    }
  }

  _showFlashHistory() {
    const data = this._readFlashData();
    if (!data || !data.timestamp) {
      this._historyEl.textContent = "No installations in history";
      this._historyEl.classList.remove("update-available");
      return;
    }

    const hasUpdate = data.version && this._fwVersion && data.version !== this._fwVersion;
    if (hasUpdate) {
      this._historyEl.textContent = "Update available";
      this._historyEl.classList.add("update-available");
    } else {
      const versionLabel = data.version ? data.version + " " : "";
      this._historyEl.textContent = versionLabel + "flashed " + timeAgo(data.timestamp);
      this._historyEl.classList.remove("update-available");
    }
  }

  _autoCollapse() {
    const data = this._readFlashData();
    if (data && data.timestamp) {
      this._section.classList.add("is-collapsed");
      this.querySelector("#install-toggle").setAttribute("aria-expanded", "false");
      this.querySelector("#install-content").hidden = true;
    }
  }

  _refreshInstallButton() {
    const data = this._readFlashData();
    const uptodateBadge = this.querySelector("#uptodate-badge");
    const espButton = this._installBtn?.closest("esp-web-install-button");
    if (!this._installBtn || !uptodateBadge || !espButton) return;

    const isUpToDate = data?.version && this._fwVersion && data.version === this._fwVersion;
    const hasUpdate = data?.version && this._fwVersion && data.version !== this._fwVersion;

    if (isUpToDate) {
      uptodateBadge.hidden = false;
      espButton.hidden = true;
      const utdVersion = this.querySelector("#uptodate-version");
      const utdAgo = this.querySelector("#uptodate-ago");
      if (utdVersion) utdVersion.textContent = data.version;
      if (utdAgo) utdAgo.textContent = timeAgo(data.timestamp);

      let reinstallLink = this.querySelector("#uptodate-reinstall");
      if (!reinstallLink) {
        reinstallLink = document.createElement("button");
        reinstallLink.id = "uptodate-reinstall";
        reinstallLink.className = "uptodate-reinstall";
        reinstallLink.addEventListener("click", () => {
          espButton.style.position = "absolute";
          espButton.style.opacity = "0";
          espButton.style.pointerEvents = "none";
          espButton.hidden = false;
          this._installBtn.click();
          espButton.hidden = true;
          espButton.style.position = "";
          espButton.style.opacity = "";
          espButton.style.pointerEvents = "";
        });
        uptodateBadge.insertAdjacentElement("afterend", reinstallLink);
      }
      reinstallLink.textContent = "Install Firmware · " + this._fwVersion;
      reinstallLink.hidden = false;
    } else {
      uptodateBadge.hidden = true;
      espButton.hidden = false;
      const reinstallLink = this.querySelector("#uptodate-reinstall");
      if (reinstallLink) reinstallLink.hidden = true;

      if (hasUpdate) {
        this._installBtn.innerHTML =
          '<span>' +
          '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="vertical-align:-2px;margin-right:4px"><path d="M12 19V5"/><path d="M5 12l7-7 7 7"/></svg>' +
          "Update Firmware</span>" +
          '<span class="btn-version"><span>' + this._fwVersion + "</span>" +
          "<span>" + timeAgo(data.timestamp) + "</span></span>";
      } else {
        const versionTag = this._fwVersion
          ? '<span class="btn-version">' + this._fwVersion + "</span>"
          : "";
        this._installBtn.innerHTML = "<span>Install Firmware</span>" + versionTag;
      }
    }
  }

  _observeInstallDialog() {
    new MutationObserver((mutations) => {
      mutations.forEach((m) => {
        m.addedNodes.forEach((node) => {
          if (node.nodeName !== "EWT-INSTALL-DIALOG") return;

          let saved = false;
          const pollTimer = setInterval(() => {
            if (!document.body.contains(node)) { clearInterval(pollTimer); return; }
            if (saved || !node.shadowRoot) return;

            const msg = node.shadowRoot.querySelector("ewt-page-message");
            if (!msg || !msg.label || String(msg.label).indexOf("complete") === -1) return;

            saved = true;
            clearInterval(pollTimer);
            localStorage.setItem(FLASH_KEY, JSON.stringify({
              version: this._fwVersion,
              timestamp: Date.now(),
            }));
            this._showFlashHistory();
            this._refreshInstallButton();
          }, 500);

          setTimeout(() => clearInterval(pollTimer), 600000);
        });
      });
    }).observe(document.body, { childList: true });
  }
}

customElements.define("install-firmware", InstallFirmware);
