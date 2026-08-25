globalThis.rsvpNanoInstallFirmware = async (manifestJson, firmwareUrl, eraseFirst, onState, onFinished, onError) => {
  const objectUrls = [];
  let failed = false;

  try {
    const [{ flash }, response] = await Promise.all([
      import("https://unpkg.com/esp-web-tools@10.2.1/dist/flash.js?module"),
      fetch(firmwareUrl),
    ]);
    if (!response.ok) throw new Error("Could not load the firmware file.");

    const firmware = await response.blob();
    if (firmwareUrl.startsWith("blob:")) URL.revokeObjectURL(firmwareUrl);
    if (firmware.size <= 65536) throw new Error("The full installer firmware is incomplete.");

    const manifest = JSON.parse(manifestJson);
    const ranges = new Map([
      [0, [0, 32768]],
      [32768, [32768, 36864]],
      [57344, [57344, 65536]],
      [65536, [65536, firmware.size]],
    ]);

    for (const part of manifest.builds[0].parts) {
      const range = ranges.get(part.offset);
      if (!range) throw new Error("Unsupported firmware offset.");
      part.path = URL.createObjectURL(firmware.slice(range[0], range[1]));
      objectUrls.push(part.path);
    }

    const manifestUrl = URL.createObjectURL(new Blob([JSON.stringify(manifest)], { type: "application/json" }));
    objectUrls.push(manifestUrl);
    const port = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a }] });

    await flash(state => {
      if (state.state === "error") {
        failed = true;
        onError(state.message || "Installation failed.");
        return;
      }

      const percentage = state.state === "writing" ? state.details.percentage : -1;
      onState(state.message, percentage);
    }, port, manifestUrl, manifest, eraseFirst);

    if (!failed) onFinished();
  } catch (error) {
    if (!failed) onError(error?.message || String(error));
  } finally {
    for (const url of objectUrls) URL.revokeObjectURL(url);
    if (firmwareUrl.startsWith("blob:")) URL.revokeObjectURL(firmwareUrl);
  }
};
