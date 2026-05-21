# RSVPNanoCompanion UI/UX Plan

## Product Shape

The companion app is an operational tool for preparing content on the phone and syncing it to RSVP Nano.

The UI should make the two working contexts clear:

- Normal phone internet: save shared links/text, edit articles, manage RSS feed URLs, choose files.
- RSVP Nano Companion Sync Wi-Fi: connect to the reader, upload/sync content, delete reader files, edit reader settings.

Most functionality already exists. The redesign should focus on clarity, speed, and native-feeling controls rather than adding large new workflows.

## App Structure

Use bottom navigation for the primary sections:

- Library
- Articles
- RSS
- Settings

Use a help icon instead of a Help tab. It can open contextual help for the current section, or a single help sheet with grouped topics.

## Connection Bar

Use a compact top bar for global connection state and actions.

The bar should show:

- App/device title.
- Current state: connected, checking, offline, busy, or disconnected.
- Device name when connected.
- Primary action: connect, reconnect, or current busy indicator.
- Help icon.

Avoid always-visible setup instructions. The normal UI should be obvious enough without a permanent instruction panel.

During connection attempts or failures, show a small `Connect manually` action. Tapping it expands an address field for the reader AP/IP.

Settings should also include a reader connection section near the top:

- Default reader address.
- Reset to `http://192.168.4.1`.
- Short explanation that this is only needed when the reader shows a different address.

## Feedback

Use transient feedback for completed operations and short failures.

Android should use Material snackbars. iOS should use a native-feeling toast/banner overlay.

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

Use confirmation dialogs for destructive actions such as deleting reader files or forgetting Wi-Fi.

For settings saves, show:

```text
Saved to Nano. Some changes apply after leaving Companion Sync.
```

If firmware later applies all settings live while Companion Sync is open, shorten this to:

```text
Saved to Nano.
```

## Library

Library is the reader-side collection. It should show content currently on the Nano.

Books and synced articles can share one library list. Differentiate with icons instead of repeating text labels on every row:

- Book icon for books.
- Newspaper/article icon for articles.

Add filtering/search:

- Search by title, author, or path.
- Filter chips: All, Books, Articles.

Each list row should show metadata already available from the reader API:

- Title.
- Author/source when available.
- Progress percent when available.
- File size.
- Book/article icon.

Do not add word count in this pass. Current firmware `/api/books` does not return word count or chapter count. The firmware has word count internally for indexed reading/progress, but exposing it to the app requires a firmware/API change first.

Tapping an item should open a detail sheet with:

- Title.
- Author/source.
- Progress.
- Size.
- Path/id.
- Content type inferred from category/path.
- Delete action.

No replace/reupload workflow is needed now.

Primary library actions:

- Refresh.
- Upload supported files.
- Delete selected item.

Supported upload formats:

- `.rsvp`
- `.epub`
- `.txt`
- `.md`
- `.markdown`
- `.html`
- `.htm`
- `.xhtml`

## Articles

Articles is the local article/draft workflow.

It should manage:

- Shared URLs.
- Shared text.
- Text-like shared files.
- Manually created article drafts.
- Drafts waiting to sync to the Nano.

Draft rows should clearly show:

- Title.
- Source host/url when available.
- Date.
- Size or word count from local draft body.
- Ready-to-sync state vs needs article text.

Actions:

- Edit draft.
- Delete draft.
- Sync one ready article.
- Sync all ready articles.
- Save/paste article text manually.

URL-only drafts should show that article text is needed before sync. If background fetching succeeded, show the draft as ready.

## RSS

RSS is feed-source management, not article content management.

It should support:

- Add feed URL.
- List feed URLs.
- Show synced vs pending state.
- Sync feeds to the reader.
- Reload feeds from the reader.
- Delete local feed entries.

Keep the copy short. Make clear that RSS feeds are saved to the reader and used by the reader to download articles when it has configured home Wi-Fi.

## Settings

Mirror the Nano settings groups:

- Word Pacing
- Display
- Typography Tune
- Wi-Fi

Do not include Firmware Update in the app for now.

Every setting that can be set from the Nano should be settable from the companion app. If the firmware API already exposes a setting, add it to shared models and UI. If the firmware menu has a setting that the API does not expose, track it as a firmware/API follow-up.

### Word Pacing

Controls:

- Reading mode: segmented control, `One Word` / `Scroll`.
- Pause behaviour: segmented control, `Sentence End` / `Instant`.
- Base speed: slider with value.
- Long words: slider.
- Complexity: slider.
- Punctuation: slider.
- Reset pacing: secondary button.

Ranges:

- WPM: 10-1000.
- WPM snapping should match firmware: 10-step up to 100, then 25-step above 100.
- Pacing delays: 0-600 ms, step 50.

### Display

Controls:

- Display mode: segmented control, `Light` / `Dark` / `Night`.
- Brightness: 5-step slider.
- Reader hand: segmented control, `Right` / `Left`.
- Footer label: picker/menu.
- Battery label: picker/menu.
- Show battery while reading: toggle.
- Show chapter while reading: toggle.
- Show book percent while reading: toggle.

Current shared model follow-up:

- Add `readingBattery`.
- Add `readingChapter`.
- Add `readingProgress`.

Firmware/API follow-up:

- Screensaver exists on the device settings menu, but does not appear in current `/api/settings`. Add firmware API support before exposing it in the app.

Language is exposed as an integer in `/api/settings`. Defer app UI until labels/localization are clean enough to present safely.

### Typography Tune

Controls:

- Typeface: segmented control if it fits, otherwise menu.
- Font size: 3-step segmented control or slider.
- Tracking: slider.
- Anchor: slider.
- Guide width: slider.
- Guide gap: slider.
- Focus highlight: toggle.
- Phantom words: toggle.

Ranges:

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
- Refresh Wi-Fi button.

The app cannot read the saved password back from the reader, so the UI should say when a password is already saved.

## Hot Apply

The app currently saves settings to the reader while connected. Current firmware API behavior appears to persist settings but not apply every reader/display change immediately while Companion Sync is open. The built-in web companion says to exit sync mode to apply all reader changes.

For now:

- Save settings immediately through the existing API.
- Show save feedback.
- Tell users some changes apply after leaving Companion Sync.

Future firmware work:

- After `/api/settings` PATCH, apply runtime settings using the same paths used by the on-device settings menu.
- Then remove the Companion Sync apply-delay message from the app.

## Icons

Use icons where the meaning is familiar:

- Library: book.
- Articles: newspaper/article.
- RSS: RSS icon.
- Settings: sliders.
- Connect: Wi-Fi/link.
- Refresh: refresh.
- Upload: upload.
- Sync: sync arrows.
- Edit: pencil.
- Delete: trash.
- Save: check/save.
- Warning/disconnected: alert circle.

Avoid icon-only controls for destructive or ambiguous actions unless the context is obvious and accessibility labels are present.

## Visual Direction

This should feel like a quiet native utility, not a marketing page.

Prefer:

- Dense but readable lists.
- Clear hierarchy.
- Native controls.
- Compact status surfaces.
- Search and filters where they save time.
- Icons paired with labels for major actions.

Avoid:

- Large decorative hero sections.
- Always-visible instructional blocks.
- Card-heavy nested layouts.
- One-note color palettes.
- Repeating obvious type labels in every row when section context or icons are enough.
