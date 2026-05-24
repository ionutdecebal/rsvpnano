# RSVPNanoCompanion UI/UX Plan

## Product Shape

RSVPNanoCompanion is a native utility for preparing phone content and syncing it to RSVP Nano.

The UI should make two contexts clear:

- Normal phone internet: save shared links/text, fetch article content, edit drafts, manage RSS feed URLs, and choose files.
- RSVP Nano Companion Sync Wi-Fi: connect to the reader, refresh library/settings, upload/sync content, delete reader files, edit reader settings, and configure reader home Wi-Fi.

Shared Kotlin owns business rules. SwiftUI and Compose should render native UI and adapt user actions to shared services.

## Current App Structure

Primary navigation:

- Library
- Settings

There are no separate primary Articles or RSS tabs. Article drafts, fetched articles, and Nano articles live in Library. RSS feed management is reached from the add/import flow or settings-style management surfaces.

The help entry is a toolbar/action item, not a primary tab.

## Connection Surface

The app uses a compact global connection surface.

It should show:

- Current connection state.
- Device name or Nano identity when known.
- Primary connect/check action.
- Busy indicator while connecting/checking.
- Manual connection entry when needed.

Current default flow:

- User opens Companion Sync on the Nano.
- User joins the RSVP-Nano Wi-Fi through the platform/manual flow.
- App checks the default reader address: `http://192.168.4.1`.
- User can reveal manual address entry if the reader shows a different address.
- Writes verify reader reachability before mutating device state.

Current constraints:

- Native auto-discovery/auto-connect is deferred.
- Remembered Nano / auto-connect is deferred.
- Optional auto-disconnect is deferred.
- Do not present auto-connect as implemented behavior in UI copy.

Remaining UX work:

- Make disconnect/power-off state changes feel prompt on both platforms.
- Keep phone internet, Nano Wi-Fi attachment, and Nano HTTP API availability conceptually separate.
- Avoid long persistent setup instructions in the normal UI.
- Use short actionable copy when connection fails.

## Feedback

Use transient feedback for completed operations and short failures.

Android uses Material snackbars. iOS uses a native-feeling toast/banner overlay.

Good snackbar/toast cases:

- Uploaded a book.
- Synced saved articles.
- RSS feeds synced.
- Settings saved.
- Article saved locally.
- Reader disconnected.

Avoid toasts for:

- Long instructions.
- Persistent connection state.
- Validation that belongs next to a field.
- Destructive confirmations.

Feedback tone is structured in shared code with `CompanionNotice`:

- Success.
- Attention/warning/offline/waiting.
- Error/failure.
- Neutral/progress.

Use subtle status color coding. Keep colors restrained and consistent with action buttons.

For settings saves, use copy like:

```text
Saved to Nano. Some changes apply after leaving Companion Sync.
```

If firmware later applies all settings live while Companion Sync is open, shorten this to:

```text
Saved to Nano.
```

## Library

Library is the unified content surface for:

- Books on the Nano.
- Articles on the Nano.
- Local saved article drafts.
- URL-only drafts waiting for internet fetch.
- Drafts ready to sync.

Books and articles share one list. Differentiate with icons:

- Book icon for books.
- Newspaper/article icon for articles.

Library supports:

- Pull-to-refresh where the platform supports it.
- Search by title, author, source, URL, or path.
- Filter chips: All, Books, Articles.
- Upload/add/import entry point.
- Item details and delete actions.

Pending article fetches should remain visible while unresolved. When a pending article resolves, it should move into its normal Library position. Failed fetches should remain visible with retry, edit, and delete actions.

Each list row should show available metadata:

- Title.
- Author/source when available.
- Progress percent when available.
- File size.
- Ready/pending/failed/synced state when relevant.
- Book/article icon.

Do not add word count in this pass. Current firmware `/api/books` does not return word count or chapter count.

Supported upload formats:

- `.rsvp`
- `.epub`
- `.txt`
- `.md`
- `.markdown`
- `.html`
- `.htm`
- `.xhtml`

## Add Article

Adding an article is URL-first, with manual editing available.

The Add Article sheet/dialog should show:

- URL text field.
- Title text field.
- `Edit body` action that reveals the body editor.

Behavior:

- Save shared links/text immediately.
- Fetch URL-only articles while normal phone internet is available.
- If internet is unavailable, show the draft as waiting for internet.
- Do not imply Nano sync is possible until article text exists.
- Manual connect remains available, but unresolved URL-only drafts still need internet before they can sync.

## RSS

RSS is feed-source management, not a primary reader tab.

It supports:

- Add feed URL.
- List feed URLs.
- Show synced vs pending state.
- Sync feeds to the reader.
- Reload feeds from the reader.
- Delete feed entries.

Only promote RSS to a primary tab if the app itself later shows actual feed timelines, unread counts, per-feed browsing, and item triage.

## Settings

Mirror the Nano settings groups:

- Word Pacing
- Display
- Typography
- Wi-Fi

Do not include Firmware Update in the app for now.

Shared Kotlin owns setting IDs, ranges, snapping, clamping, and appearance-mode behavior through `NanoSettingsSchema` and `NanoSettings` helper methods. Platform UI should call the existing shared setters such as `withWpm`, `withPacingLongWordMs`, `withBrightnessIndex`, and `withAppearanceMode`.

### Word Pacing

Controls:

- Reading mode: `One Word` / `Scroll`.
- Pause behavior: `Sentence End` / `Instant`.
- Accurate time estimate toggle.
- Base speed slider.
- Long words slider.
- Complexity slider.
- Punctuation slider.
- Reset pacing button.

Shared ranges:

- WPM: 10-1000.
- WPM snapping: 10-step up to 100, then 25-step above 100.
- Pacing delays: 0-600 ms, step 50.

### Display

Controls:

- Display mode: `Light` / `Dark` / `Night`.
- Brightness: 5-step slider.
- Reader hand: `Right` / `Left`.
- Footer label picker/menu.
- Battery label picker/menu.
- Show battery while reading toggle.
- Show chapter while reading toggle.
- Show book percent while reading toggle.

Firmware/API follow-up:

- Screensaver exists on the device settings menu but does not appear in current `/api/settings`.
- Language is exposed as an integer. Defer app UI until labels/localization are safe to present.

### Typography

Controls:

- Typeface picker/menu.
- Font size slider.
- Tracking slider.
- Anchor slider.
- Guide width slider.
- Guide gap slider.
- Focus highlight toggle.
- Phantom words toggle.

Shared ranges:

- Font size: 0-2.
- Tracking: -2 to 3.
- Anchor: 30-40%.
- Guide width: 12-30, step 2.
- Guide gap: 2-8.

### Wi-Fi

This is reader home Wi-Fi, used by the reader for RSS and OTA. It is separate from the RSVP Nano Companion Sync AP.

Controls:

- Saved network summary.
- SSID text field.
- Password secure field.
- Save Wi-Fi button.
- Forget Wi-Fi button.
- Refresh Wi-Fi via pull-to-refresh or a platform fallback button.

The app cannot read the saved password back from the reader, so the UI should say when a password is already saved.

## Platform Notes

Android:

- Uses Compose UI split into focused files.
- Uses Android DataStore only from `androidMain` for app settings.
- Routes UI actions through shared controllers and shared settings helpers.

iOS:

- Uses SwiftUI split into focused files.
- Uses shared JSON app settings logic over iOS app-group file storage.
- Does not depend on AndroidX DataStore.
- Native hotspot joining remains deferred until entitlement/API constraints are validated.

## Icons

Use icons where the meaning is familiar:

- Library: book.
- Articles: newspaper/article icon inside Library rows and upload picker actions.
- RSS: RSS icon.
- Settings: sliders.
- Connect: Wi-Fi/link.
- Refresh: refresh.
- Upload: upload.
- Sync: sync arrows.
- Edit: pencil.
- Delete: trash.
- Save: check/save.
- Warning/disconnected: alert.

Avoid icon-only controls for destructive or ambiguous actions unless context is obvious and accessibility labels are present.

## Visual Direction

This should feel like a quiet native utility.

Prefer:

- Dense but readable lists.
- Clear hierarchy.
- Native controls.
- Compact status surfaces.
- Icons paired with labels for major actions.
- Search and filters where they save time.

Avoid:

- Large decorative hero sections.
- Always-visible instructional blocks.
- Card-heavy nested layouts.
- One-note color palettes.
- Repeating obvious type labels in every row when section context or icons are enough.

## Surface And Color Hierarchy

Use surface roles to guide behavior:

- App/page background: lowest emphasis neutral surface.
- Lists and normal rows: neutral surface/container colors.
- Grouped settings and form sections: slightly raised neutral containers.
- Global connection bar: higher-emphasis surface because it affects the whole app.
- Connected state: subtle primary/success tint.
- Needs attention/disconnected state: subtle tertiary/warning tint, not destructive red.

Use action colors sparingly:

- Primary/prominent buttons: main action in a region.
- Secondary/tonal buttons: refresh, reconnect/check, edit, reset, and supporting actions.
- Text buttons: low-emphasis actions like reveal manual connection or cancel.
- Error/destructive color: delete, forget Wi-Fi, and confirmed destructive actions only.
