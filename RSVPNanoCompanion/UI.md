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
- Settings

Do not keep separate Articles and RSS tabs for the first redesign pass. Article drafts and fetched article entries should live in Library. RSS should only get a primary tab later if the app becomes an actual RSS reader with feed timelines, unread counts, per-feed browsing, and refresh controls.

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

Library is the unified content surface for books and articles. It should cover both local saved/draft content and content currently on the Nano, with clear sync/status indicators rather than separate primary tabs.

Books and articles should share one list. Differentiate with icons instead of repeating text labels on every row:

- Book icon for books.
- Newspaper/article icon for articles.

Use a single Upload action for creation/import:

- Upload book.
- Add article.
- Add RSS feed.

Place Upload as the first Library row, above search and filters. It should be simple and list-native:

- Label: `Upload`.
- Upload icon.
- No long explanatory subtitle.
- Opens the creation/import picker.

Avoid a title-bar Add button because the action is Library-scoped, not global. Avoid a bottom-right FAB by default because it competes with the bottom navigation and can cover list content. If testing later shows Upload must stay reachable while deep-scrolled, revisit a modest FAB.

Tapping Upload should open a compact action picker, not an expanding speed-dial. Use a platform-native popup, menu, confirmation dialog, or bottom sheet with:

- Upload book.
- Add article.
- Add RSS feed.

The picker should lead to the chosen workflow. Do not put the article URL field directly in the first Upload picker. Keep the menu shallow so the common path does not feel hidden or cumbersome.

Use minimal filtering/search:

- Search by title, author, source, URL, or path.
- Filter chips: All, Books, Articles.

Avoid adding filters that behave like replacement tabs, such as Pending, Feeds, Unread, Synced, or Failed. Add those only if the list becomes hard to scan in real use.

Pending article fetches should appear in a small section at the top of Library only while they are unresolved. When a pending article resolves, it should move into its normal place in the list. Failed fetches should remain visible with retry, edit, and delete actions.

Each list row should show the metadata available for that item:

- Title.
- Author/source when available.
- Progress percent when available.
- File size.
- Date for local drafts/articles when useful.
- Ready/pending/failed/synced state when relevant.
- Book/article icon.

Do not add word count in this pass. Current firmware `/api/books` does not return word count or chapter count. The firmware has word count internally for indexed reading/progress, but exposing it to the app requires a firmware/API change first.

Tapping an item should open a detail sheet with:

- Title.
- Author/source.
- Progress.
- Size.
- Path/id.
- Content type inferred from category/path.
- Local article status and source URL when relevant.
- Delete action.

No replace/reupload workflow is needed now.

Primary library actions:

- Refresh.
- Add/import content.
- Delete selected item.

Where the platform supports it cleanly, refresh reader-backed lists with pull-to-refresh instead of a persistent refresh button. Keep explicit reconnect/check controls in the global connection bar because they affect the app connection state, not just one list.

Pull-to-refresh should use the standard refresh/loop-arrow indicator:

- While pulling, show the loop-arrow indicator and let it rotate/progress with pull distance.
- Once the threshold is crossed, switch the indicator into active refreshing state.
- Keep the indicator visible until the underlying refresh operation finishes, not just until the gesture ends.
- If refresh completes too quickly to perceive, keep the indicator visible briefly so the action has feedback.
- Collapse the indicator when refresh completes.

Show a short snackbar/toast when useful:

- `Library refreshed` if the list did not visibly change.
- `Refresh failed` if the request failed, with retry where appropriate.

Supported upload formats:

- `.rsvp`
- `.epub`
- `.txt`
- `.md`
- `.markdown`
- `.html`
- `.htm`
- `.xhtml`

### Add Article

Adding an article should be URL-first, with manual editing available but not presented as the main workload.

The Add article sheet/dialog should initially show:

- URL text field.
- Title text field that is auto-filled after fetch and remains editable.
- `Edit body` action that reveals the article body editor.

The body editor should stay behind `Edit body` so the normal case is quick: paste URL, fetch, optionally adjust title, save.

Behavior:

- If a URL is provided, fetch article metadata/content.
- While fetch is running, show a pending item at the top of Library.
- If fetch succeeds, auto-fill the title and body, then place the article in its normal Library position.
- If fetch fails, keep a visible failed item with retry, edit, and delete actions.
- The title should remain editable after fetch.
- The body should be editable through `Edit body`.
- Manual creation should be possible by entering title/body, but it should not dominate the default flow.

Article workflows that should be represented in Library:

- Shared URLs.
- Shared text.
- Text-like shared files.
- Manually created article drafts.
- Drafts waiting to sync to the Nano.
- Synced articles currently on the Nano.

## RSS

RSS is feed-source management unless the app grows into an RSS reader.

It should support:

- Add feed URL.
- List feed URLs.
- Show synced vs pending state.
- Sync feeds to the reader.
- Reload feeds from the reader.
- Delete local feed entries.

If RSS only creates article entries, those entries belong in Library and RSS should not be a primary tab. Feed URL management can live behind Add RSS feed or in a small feed/source management sheet.

Only promote RSS to a primary tab if the app itself shows the actual feed experience:

- Feed timelines.
- Unread counts.
- Per-feed browsing.
- Refresh controls.
- Feed item triage separate from saved Library content.

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
- Articles: newspaper/article icon inside Library rows and Upload picker actions.
- RSS: RSS icon for Add RSS feed and feed/source management.
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

## Surface And Color Hierarchy

Use surface roles to guide behavior:

- App/page background: lowest emphasis neutral surface.
- Lists and normal rows: neutral surface/container colors so content is readable but not screaming for action.
- Grouped settings and form sections: slightly raised neutral containers.
- Global connection bar: a higher-emphasis surface because it affects the whole app.
- Connected state: subtle primary container tint.
- Needs attention/disconnected state: subtle tertiary/warning tint, not destructive red.

Use action colors sparingly:

- Primary/prominent buttons: only the main action in a region, such as Wi-Fi, upload, save, or sync.
- Secondary/tonal buttons: refresh, reconnect/check, edit, reset, and supporting actions.
- Text buttons: low-emphasis actions like reveal manual connection or cancel.
- Error/destructive color: delete, forget Wi-Fi, and confirmed destructive actions only.
- Destructive icon buttons should use an error-container background, not just a red icon, so the action reads differently from ordinary row tools.

Use type hierarchy with the same restraint:

- Section titles: title styles.
- Row titles: title/body emphasis.
- Metadata, helper text, and status detail: secondary/on-surface-variant styles.
- Avoid large text inside dense controls and settings rows.
