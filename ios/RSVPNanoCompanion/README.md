# RSVP Nano Companion

Native SwiftUI iPhone app for the RSVP Nano companion sync spike.

## Run

1. Open `RSVPNanoCompanion.xcodeproj` in Xcode.
2. Select the `RSVPNanoCompanion` scheme.
3. Select your iPhone as the run destination.
4. In target signing settings, choose your Apple developer team if Xcode asks.
5. Put the reader into `Companion sync`.
6. Run the app. It shows connection instructions and checks for the reader automatically.
7. Join the `RSVP-Nano-xxxxxx` Wi-Fi network shown on the reader in iPhone Settings, then return to
   the app. The library appears once the HTTP API is reachable.

The app can read `/api/info`, list `/api/books`, delete books, upload `.rsvp`/`.epub` files, and
convert EPUB, plain text, Markdown, or HTML/XHTML files into `.rsvp` before upload. The library
shows saved reading percentage when the reader reports `progressPercent` for a book.

## Add Reading Material

- `Upload File`: pick `.rsvp`, `.epub`, `.txt`, `.md`, `.markdown`, `.html`, `.htm`, or `.xhtml`
  from Files. EPUB/Text/Markdown/HTML files are converted locally when possible; unsupported EPUB
  archives are uploaded as `.epub` so the reader can convert them on open.
- `New Text`: paste text or article extracts, optionally with a title/source, and upload the
  generated `.rsvp`.
- Share Extension: from Safari or another app, share a URL or selected text to `RSVP Nano`. The
  extension extracts and formats the article, converts it to `.rsvp`, and saves it into the app's
  pending inbox. Open the companion app later, connect to the reader Wi-Fi, and use `Sync Saved
  Articles`.
