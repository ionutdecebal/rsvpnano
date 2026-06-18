# Calibre-Server AJAX — Firmware Integration Notes

Probed against calibre 8.7.0, library `rsvplib`, book id 1 ("Test Book" by Jane Doe,
formats: epub + rsvp). Server started with:

```
calibre-server --port 8099 /tmp/rsvplib
```

---

## Exact JSON field paths the firmware must read

### 1. `GET /ajax/library-info`

```
.default_library          → string  e.g. "rsvplib"
.library_map              → object  keys = valid library_id strings
                            e.g. { "rsvplib": "rsvplib" }
```

Firmware use: pick `default_library` when no library is configured; validate
a stored library_id exists in `library_map` before issuing further requests.

---

### 2. `GET /ajax/search?query=<q>&library_id=<lib>`

```
.book_ids                 → array of integers   e.g. [1]
.total_num                → integer  total matching books
.num                      → integer  books returned in this page
.offset                   → integer  starting offset of this page
.library_id               → string   echoed back
.query                    → string   echoed back
```

Actual values (query="Test", library_id="rsvplib"):
```json
{
    "total_num": 1,
    "num": 1,
    "offset": 0,
    "book_ids": [1]
}
```

Firmware use: iterate `.book_ids`; if `total_num > num` there are more pages —
use `?num=<n>&offset=<o>` to paginate (both are query parameters calibre accepts).

---

### 3. `GET /ajax/book/<id>?library_id=<lib>`

```
.title                            → string
.authors                          → array of strings
.last_modified                    → ISO-8601 string  e.g. "2026-06-17T15:37:34+00:00"
.formats                          → array of strings e.g. ["epub", "rsvp"]
.other_formats.rsvp               → string  relative URL path  e.g. "/get/rsvp/1/rsvplib"
.format_metadata.rsvp.size        → integer  bytes              e.g. 475
.format_metadata.rsvp.mtime       → ISO-8601 string  e.g. "2026-06-17T15:37:34.528479+00:00"
.main_format                      → object  { "<fmt>": "<path>" }  — first/preferred format
```

Actual values (book id 1):
```json
{
    "last_modified": "2026-06-17T15:37:34+00:00",
    "formats": ["epub", "rsvp"],
    "other_formats": {
        "rsvp": "/get/rsvp/1/rsvplib"
    },
    "format_metadata": {
        "rsvp": {
            "size": 475,
            "mtime": "2026-06-17T15:37:34.528479+00:00"
        }
    }
}
```

---

### 4. Download URL template

```
GET /get/rsvp/<book_id>/<library_id>
```

Example: `GET /get/rsvp/1/rsvplib`

The server also returns the full relative path in `.other_formats.rsvp` —
firmware can use that directly (prepend base URL) rather than constructing
the template itself.  Both are equivalent.

HTTP response headers on success:
```
HTTP/1.1 200 OK
Content-Type: application/x-rsvp
Content-Length: 475
Content-Disposition: attachment; filename="Test Book - Jane Doe_1.rsvp"; filename*=utf-8''...
ETag: "85db54f36db17540996d6db2f666ff6c44f880d5"
```

---

## Edge cases for the firmware JSON parser

### A. Book has no rsvp format
`.formats` will not contain `"rsvp"`.  
`.other_formats` will not have a `"rsvp"` key (it will be absent, not null).  
`.format_metadata` will not have a `"rsvp"` key.  
**Firmware must check for key presence before dereferencing, not just null-check.**

### B. Authentication required (401)
When the server has `--enable-auth`, unauthenticated requests return:
```
HTTP/1.1 401 Unauthorized
WWW-Authenticate: Basic realm="calibre"
```
The body is HTML, not JSON. Firmware must check HTTP status before attempting
`json_parse()` — parsing HTML as JSON will crash or produce garbage.

### C. Empty search results
`.book_ids` is an empty array `[]` and `.total_num` is `0`.  
Verified: query `tag:rsvp` against this library returns `{"book_ids": [], "total_num": 0}`.  
Firmware must guard against iterating an empty array.

### D. Pagination in `/ajax/search`
Default page size is determined by the server (returned as `.num`).  
To request a specific page size and offset, append `&num=<n>&offset=<o>`.  
If `total_num > num + offset`, there are more pages.  
**Firmware should not assume all book_ids arrive in one response.**

### E. `mtime` vs `last_modified` timestamp formats
- `.format_metadata.rsvp.mtime` has **sub-second precision**:  
  `"2026-06-17T15:37:34.528479+00:00"` — 6 decimal digits in seconds.
- `.last_modified` (book-level) has **second precision**:  
  `"2026-06-17T15:37:34+00:00"` — no decimal fraction.  
  Both are ISO-8601 with UTC offset `+00:00` (not `Z`).  
  Firmware ISO-8601 parser must handle both forms; the fractional part is optional.

### F. `pubdate` field can be the string `"None"` (not JSON null)
The `.pubdate` field in `/ajax/book/<id>` can be the literal string `"None"`
(a Python artefact), not JSON `null`. Do not parse it as a date without first
checking for this sentinel.

### G. `formats` array key casing
Format names in `.formats` are **lowercase**: `"rsvp"`, `"epub"`.  
Keys in `.format_metadata` and `.other_formats` are also **lowercase**.  
Firmware string comparisons must be case-insensitive or consistently lowercase.

### H. Library ID in URL path vs query param
The `/get/<fmt>/<book_id>/<library_id>` endpoint takes library_id as a **path
segment**, not a query parameter. The `/ajax/*` endpoints take it as
`?library_id=<lib>` query parameter. Do not mix these up.

---

## Fixture files (for firmware unit tests)

| File | Endpoint |
|------|----------|
| `fixtures/library-info.json` | `/ajax/library-info` |
| `fixtures/search.json` | `/ajax/search?query=Test&library_id=rsvplib` |
| `fixtures/book-1.json` | `/ajax/book/1?library_id=rsvplib` |
