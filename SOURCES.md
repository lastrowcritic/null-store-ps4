# Sources

NULL_store ships no catalogue. The library is whatever your `sources.json` describes,
refreshed from those sites' public listing pages.

## Installing a source list

**From your phone (easiest).** Open **Sources** in the app (L3). The screen shows a QR code
and an address like `http://192.168.1.40:8090/418302`. Scan the code with your phone
camera on the same Wi-Fi, or type the address. Fill in the form, press **Test** to check
the page parses, then **Add source**. Press **Options** to build the library.

**From a USB stick.** Create the file on a computer, put it at
`NULL_store/sources.json` on the stick, insert it, and start NULL_store. The app copies it
on first run, when it has no list of its own.

The phone page writes the same file, so both routes mix freely. Deleting `sources.json`
returns the app to the empty state.

## Format

```json
{
  "version": 1,
  "sources": [
    {
      "id": "example",
      "label": "Example Store",
      "base": "https://games.example.org",
      "listing": "/games/",
      "pageListing": "/games/page/{page}/",
      "maxPages": 20,
      "enabled": true,
      "shape": {
        "item": "article.card",
        "title": ".card-title",
        "size": ".card-size",
        "imageAttributes": ["data-src", "src"],
        "linkClasses": ["download-button"],
        "linkMarkers": [".torrent", "do=download"],
        "blocked": ["data-locked=1"]
      }
    }
  ]
}
```

| Field | Meaning |
|---|---|
| `id` | Short slug. Names the cache file, so keep it stable. |
| `label` | Name shown in the console UI. |
| `base` | Site origin. Must be `https://`. A port is allowed. |
| `listing` | Path of listing page 1, resolved against `base`. |
| `pageListing` | Path template for later pages; `{page}` is replaced. Omit it for single-page sources. |
| `maxPages` | Hard stop, default 20. The scan also stops at the first empty page. |
| `enabled` | Set false to keep the entry but skip it. |
| `shape.item` | Selector for one game card: `tag`, `.class`, `tag.class` or `#id`. |
| `shape.title` | Selector for the title text inside a card. Falls back to the link text, then its `title` attribute. |
| `shape.size` | Selector for a size label. Optional. |
| `shape.imageAttributes` | Attribute names tried in order for artwork. Defaults to `data-src`, `src`. |
| `shape.linkClasses` | Classes that mark a download link on a game page. |
| `shape.linkMarkers` | Substrings that mark a download URL, for example `.torrent`. |
| `shape.blocked` | `attribute=value` pairs that mean gated, paid or delayed. Those links are skipped. |

Selectors are matched against the tag text, so `article.card` means "an `article` element
whose class list contains `card`". Only class and id matching is supported; there are no
descendant or pseudo selectors.

## Rules the app enforces

- Every link must stay on the source's own host, or on `www.` of it. Offsite links are dropped.
- The listing must be plain HTML anchors. Pages rendered by JavaScript are not followed.
- Responses are not decompressed, so a server that ignores `Accept-Encoding: identity`
  cannot be parsed.
- Redirects are followed up to six hops.
- A game id is derived from its page URL, so it survives restarts and reordering.
- Downloads need a `.torrent` file. Magnet links are not supported.

## Checking a source

Press **Test** on the phone page: it fetches page 1 and reports how many cards matched, so
a wrong selector shows up as `0 games found` before you save anything.

On the console, press **Options** and watch the status line. `0 updated / 0 cached / 1 failed` with an empty
library means the page was fetched but no card matched `shape.item`, or the URL was not
`https://` on that host. A working source reports `1 updated` and fills the grid.

Only add sources you are allowed to use, and respect each site's terms.
