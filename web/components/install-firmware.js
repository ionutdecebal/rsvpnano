const FLASH_KEY = "rsvpnano_last_flash";

const FIRMWARE_OPTIONS = [
  {
    manifest: "firmware/manifest.json",
    title: "Waveshare 3.49 rev1",
    badge: "Default",
    note: "Use this build first. It keeps the standard GPIO8 backlight profile.",
    defaultOption: true,
  },
  {
    manifest: "firmware/manifest-rev2.json",
    title: "Waveshare 3.49 rev2",
    badge: "GPIO42",
    note: "Use this if the device boots but brightness or backlight control does not respond.",
    defaultOption: false,
  },
];

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
    const firmwareOptions = FIRMWARE_OPTIONS.map((option, index) => ({ ...option, index }));
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
            <div class="install-options">
              ${firmwareOptions.map((option) => `
                <div class="install-option" data-option-index="${option.index}">
                  <div class="install-option-head">
                    <strong class="fw-version">${option.title}</strong>
                    <span class="latest-badge"><span class="pulse-dot"></span>${option.badge}</span>
                  </div>
                  <p class="install-option-note">${option.note}</p>
                  <ul class="feature-list"></ul>
                  <div class="uptodate-badge" hidden>
                    <span class="uptodate-left">
                      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"></polyline></svg>
                      Up to date
                    </span>
                    <span class="uptodate-right">
                      <span class="uptodate-version"></span>
                      <span class="uptodate-ago"></span>
                    </span>
                  </div>
                  <esp-web-install-button manifest="${option.manifest}">
                    <button slot="activate">Install Firmware</button>
                    <span slot="unsupported">Use Chrome or Edge on desktop with Web Serial support.</span>
                    <span slot="not-allowed">This page must be opened over HTTPS or localhost.</span>
                  </esp-web-install-button>
                  <p class="install-warning">Important: keep the device plugged in until the installer says it's done.</p>
                </div>
              `).join("")}
            </div>
          </div>
        </div>
      </section>
    `;

    this._section = this.querySelector("#install-section");
    this._historyEl = this.querySelector("#flash-history");
    this._optionViews = firmwareOptions.map((option) => {
      const root = this.querySelector(`[data-option-index="${option.index}"]`);
      const button = root.querySelector('button[slot="activate"]');
      const espButton = button.closest("esp-web-install-button");
      const view = {
        ...option,
        root,
        button,
        espButton,
        versionLabel: root.querySelector(".fw-version"),
        featureList: root.querySelector(".feature-list"),
        uptodateBadge: root.querySelector(".uptodate-badge"),
        uptodateVersion: root.querySelector(".uptodate-version"),
        uptodateAgo: root.querySelector(".uptodate-ago"),
        version: "",
      };

      button.addEventListener("click", () => {
        this._activeInstall = view;
      });

      return view;
    });

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

    this._optionViews.forEach((view) => {
      fetch(view.manifest, { cache: "no-store" })
        .then(r => r.json())
        .then(m => {
          view.version = m.version;
          view.versionLabel.textContent = view.title + " - " + m.version;
          if (m.features) {
            view.featureList.innerHTML = m.features.map(f => "<li>" + f + "</li>").join("");
          }
          this._refreshInstallButtons();
          this._showFlashHistory();
        })
        .catch(() => {
          view.root.hidden = true;
          this._showFlashHistory();
        });
    });
  }

  _readFlashData() {
    try {
      return JSON.parse(localStorage.getItem(FLASH_KEY));
    } catch (e) {
      return null;
    }
  }

  _viewForFlashData(data) {
    if (!data) return null;
    if (data.manifest) {
      const manifestMatch = this._optionViews.find((view) => view.manifest === data.manifest);
      if (manifestMatch) return manifestMatch;
    }
    return this._optionViews.find((view) => view.defaultOption) || this._optionViews[0] || null;
  }

  _isSameInstallOption(data, view) {
    if (!data || !view) return false;
    if (data.manifest) return data.manifest === view.manifest;
    return view.defaultOption;
  }

  _showFlashHistory() {
    const data = this._readFlashData();
    if (!data || !data.timestamp) {
      this._historyEl.textContent = "No installations in history";
      this._historyEl.classList.remove("update-available");
      return;
    }

    const view = this._viewForFlashData(data);
    const hasUpdate = data.version && view?.version && data.version !== view.version;
    if (hasUpdate) {
      this._historyEl.textContent = "Update available";
      this._historyEl.classList.add("update-available");
    } else {
      const versionLabel = data.version ? data.version + " " : "";
      const titleLabel = data.title ? data.title + " " : "";
      this._historyEl.textContent = titleLabel + versionLabel + "flashed " + timeAgo(data.timestamp);
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

  _refreshInstallButtons() {
    const data = this._readFlashData();
    this._optionViews.forEach((view) => this._refreshInstallButton(view, data));
  }

  _refreshInstallButton(view, data) {
    if (!view.button || !view.uptodateBadge || !view.espButton) return;

    const sameOption = this._isSameInstallOption(data, view);
    const isUpToDate = sameOption && data?.version && view.version && data.version === view.version;
    const hasUpdate = sameOption && data?.version && view.version && data.version !== view.version;

    if (isUpToDate) {
      view.uptodateBadge.hidden = false;
      view.espButton.hidden = true;
      view.uptodateVersion.textContent = data.version;
      view.uptodateAgo.textContent = timeAgo(data.timestamp);

      if (!view.reinstallLink) {
        view.reinstallLink = document.createElement("button");
        view.reinstallLink.className = "uptodate-reinstall";
        view.reinstallLink.addEventListener("click", () => {
          view.espButton.style.position = "absolute";
          view.espButton.style.opacity = "0";
          view.espButton.style.pointerEvents = "none";
          view.espButton.hidden = false;
          view.button.click();
          view.espButton.hidden = true;
          view.espButton.style.position = "";
          view.espButton.style.opacity = "";
          view.espButton.style.pointerEvents = "";
        });
        view.uptodateBadge.insertAdjacentElement("afterend", view.reinstallLink);
      }
      view.reinstallLink.textContent = "Install Firmware · " + view.version;
      view.reinstallLink.hidden = false;
    } else {
      view.uptodateBadge.hidden = true;
      view.espButton.hidden = false;
      if (view.reinstallLink) view.reinstallLink.hidden = true;

      if (hasUpdate) {
        view.button.innerHTML =
          '<span>' +
          '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" style="vertical-align:-2px;margin-right:4px"><path d="M12 19V5"/><path d="M5 12l7-7 7 7"/></svg>' +
          "Update Firmware</span>" +
          '<span class="btn-version"><span>' + view.version + "</span>" +
          "<span>" + timeAgo(data.timestamp) + "</span></span>";
      } else {
        const versionTag = view.version
          ? '<span class="btn-version">' + view.version + "</span>"
          : "";
        view.button.innerHTML = "<span>Install Firmware</span>" + versionTag;
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

            const active = this._activeInstall || this._optionViews.find((view) => view.defaultOption);
            saved = true;
            clearInterval(pollTimer);
            localStorage.setItem(FLASH_KEY, JSON.stringify({
              version: active?.version,
              title: active?.title,
              manifest: active?.manifest,
              timestamp: Date.now(),
            }));
            this._showFlashHistory();
            this._refreshInstallButtons();
          }, 500);

          setTimeout(() => clearInterval(pollTimer), 600000);
        });
      });
    }).observe(document.body, { childList: true });
  }
}

customElements.define("install-firmware", InstallFirmware);
