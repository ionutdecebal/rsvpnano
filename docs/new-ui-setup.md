# New UI 0.2.0: setup, updates and dictionary

[Back to the project README](../README.md)

## Get started


You need a supported RSVP Nano board, a USB-C data cable, and a FAT32 microSD card. An 8 GB to
32 GB card is the safest choice.

1. Open the [web companion](https://ionutdecebal.github.io/rsvpnano/) in a desktop browser.
2. Choose your board and firmware: **Classic** is the default. **New UI (preview) · 0.2.0**
   is available for LCD 3.49 Rev1.
3. Follow setup and verify the reader. The New UI includes an on-device introduction.
4. Add an EPUB, RSVP, text, Markdown, HTML, or XHTML file from the companion's Library screen.
5. Open the book on the Nano and start reading. For offline definitions in the New UI,
   [install the dictionary pack](#dictionary-setup-new-ui).

Wi-Fi is optional for reading and offline dictionary use. Use USB to transfer books if you
prefer; the experimental new phone app is not required.

Chrome or Edge is required for browser flashing and USB companion access because those features use
Web Serial. The site still supports local-network management in browsers that can reach the reader.

The web installer keeps Classic and New UI downloads separate. You can also select a compatible
merged `.bin` file manually. Use the application-only `-ota.bin` for an OTA update, not the merged
image. Always match the firmware to your board revision.

## Updates and switching

On a compatible reader, **Firmware updates** offers:

- **Check current software** — get updates for the interface you already use.
- **Return to Classic** from the New UI, or **Try New UI** from Classic.

The New UI shows the installed version and the selected version before you confirm.
Classic’s selector build shows the selected version before installation.
It downloads the selected firmware over Wi-Fi, shows progress, and restarts when installation
is complete. Switching is always intentional. Custom repository/tag settings remain available for normal
updates. An explicit interface switch backs up that advanced source in device storage and uses
the official channel; it does not automatically restore a custom source later.

The selector is included in Classic v0.1.3 and later on LCD 3.49 Rev1 so you can switch in both directions.
If an older version reports `rsvp-nano-ota.bin missing`, retry the official update
to Classic v0.1.4 first. For manual browser flashing, extract your board's full
image from `rsvp-nano-browser-installers.zip`; do not select an `-ota.bin` file.
Older Classic builds can use their existing **Release tag** setting to select `v0.2.0` after
publication, or use the web installer. New UI choices will only appear for supported hardware.

Classic and the New UI have separate update selections. The New UI can remain a GitHub
prerelease while still being offered in the device's update menu.

Back up the SD card, including settings and reading progress, before switching. Both firmwares use
the existing library layout, but Classic may ignore new settings or replace them when saving.
Keep the backup to restore your exact New UI preferences. Switching must not format the SD card.

The web installer remains an alternative: choose the firmware for the same board and leave
**Erase first** off. Use the web companion's installer; a generic whole-chip erase can remove
saved device settings.

## Dictionary setup (New UI)

The dictionary lives on the SD card, separately from the firmware, so you can keep or replace it
without reflashing the reader. English is available now; more languages are planned.

### Install from the reader

During onboarding, or from **Dictionary → ? → Install / manage**, choose
**Install dictionary** (about 48 MiB). The reader will check the SD card and available
space, connect over Wi-Fi, then download and verify the pack with visible progress.
You can skip this during setup and install it later. Dictionary packs are hosted on GitHub
and versioned separately from the firmware.

An existing dictionary stays available until a replacement has downloaded and passed
verification. Replacing a manually installed pack requires an explicit choice. Press the middle
button to cancel a download; retry starts a fresh transfer.

### Install manually or use a custom pack

1. Download `RSVP-Nano-English-Dictionary.zip` from the
   [English dictionary release](https://github.com/ionutdecebal/rsvpnano/releases/tag/dictionary-en-2025.1).
2. Extract it and copy the `dictionaries` folder to the root of the SD card using a card reader
   or USB mass-storage mode.
3. From a book, press the **middle button → Dictionary**. Use **Search** to look up another word.
   The **?** button inside Dictionary opens setup help.

The complete release ZIP includes the same dictionary folder alongside the firmware
and installation instructions. Downloading that ZIP to a computer does not automatically copy
the dictionary to the reader's SD card.

The resulting files should be:

```text
/dictionaries/english.oewn
/dictionaries/ATTRIBUTION.txt
/dictionaries/manifest.json
```

The pack uses about **48 MiB** and contains English definitions. It works without Wi-Fi, a phone,
an account, or a server. It does not install interface translations.

For a custom pack, use the compatible indexed format expected at `/dictionaries/english.oewn`.
The [dictionary build tool](https://github.com/ionutdecebal/rsvpnano/tree/release/ui-dictionary/tools/dictionary)
documents the current Open English Wordnet conversion. Arbitrary dictionary files are not
automatically compatible; additional formats and dictionary selection would need further support.

**Look online** makes a request directly to Wiktionary over configured Wi-Fi when you choose it.
The lookup word is sent, not the book. Offline lookup remains available when an online request fails.

The offline pack adapts [Open English Wordnet 2025](https://en-word.net/), by the Open English
Wordnet Community and derived from Princeton WordNet, under
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). It includes compact definitions,
examples, form aliases, and up to eight senses per entry. Keep its attribution files when
redistributing it. Online definitions come from Wiktionary contributors under
[CC BY-SA 4.0](https://en.wiktionary.org/wiki/Wiktionary:Copyrights).

## Need help?

Include the board revision, firmware version and steps to reproduce when reporting
an issue. Join the [RSVP Nano Discord](https://discord.gg/mB5xv2PG53) for setup help.
