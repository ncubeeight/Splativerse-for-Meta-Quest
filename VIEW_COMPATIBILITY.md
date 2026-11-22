# Saved View compatibility contract

Compared with the Vision Pro 1.1 (24) working tree on 2026-11-19. The Vision Pro project was read, not modified.

## Identity and JSON

A scan is identified by the lowercase, 64-character SHA-256 of the complete original file. Filenames, paths, and scan titles are not portable identity. Renaming or copying the same bytes preserves identity; re-exporting or editing a scan creates a new identity, even if its appearance is similar.

`Library/viewpoints.json` uses the same schema as Vision Pro's `viewpoints.json`:

```json
{
  "<64-character SHA-256>": {
    "views": [{
      "id": "8A2E7674-65E7-4F09-8BD1-C94DB16FD53D",
      "name": "Entrance",
      "pose": {"position": [12.5, -1.75, 3], "forward": [0, 0, 1]}
    }],
    "defaultID": "8A2E7674-65E7-4F09-8BD1-C94DB16FD53D"
  }
}
```

UUIDs compare without case sensitivity. The optional defaultID must name a member. At most 100 Views per scan; nonblank names, three finite components per vector, absolute values below 1e12, and squared forward length above 1e-10. Quest names are limited to 80 Unicode code points and exclude control characters; this is a stricter subset of Vision's 80 extended grapheme clusters. New names are trimmed. Readers preserve unknown JSON fields when editing existing Views.

## Coordinates

Position/forward are in decoded source coordinates **before** units and up-axis transforms. SPZ means the decoder's RUB-to-RDF conversion has already happened, as in both existing renderers. They are not raw packed SPZ RUB coordinates, tracking coordinates, physical-room anchors, or offsets from a dense-region spawn.

| Up axis | Source → renderer |
| --- | --- |
| yDown | (x, -y, -z) |
| yUp | (x, y, z) |
| zUp | (x, z, -y) |

Quest geometry uses orientation × metresPerUnit. Saving inverts that transform. Heading uses horizontal world facing, with projected head-right fallback near vertical gaze; pitch/roll are not saved. Restoring maps the current tracked eye to the saved source position and applies only yaw. Vision also inverts/applies its source-alignment translation, so a different spawn or floor alignment does not change the meaning of the stored source position.

## Settings and future sharing

Quest metadata keeps the portable settings fields: `metresPerUnit`, `upAxis`, `floorOffset`, `bubbleRadius`, `spawnMode`, `splatFilter`. Current library UI supports units/up axis/filter/radius; floorOffset=0 and spawnMode=denseRegion are the defaults. Non-default floor/spawn settings from external packages are not imported or applied in this release.

Portable bubbleRadius stays in Vision's current 60–360 m range. Quest's 30 m and Full scan (0) choices are separate local `liveRadius` values; they retain the last portable radius, initially 60 m. Future sharing must make this distinction visible instead of promising identical radius behavior. The Quest `library.json` index, display titles, paths and timestamps are platform-specific.

Future `.splativerse` export should carry the unchanged scan bytes, digest, compatible settings and this `ScanViewpoints` object. Implement package validation, transactional import, and duplicate-View merge before exposing sharing. Do not copy either platform's entire library index into the other. No network service or package exchange is part of 0.1.5.

## Regression checks

- C++ pose roundtrips: every up axis, metres/centimetres/millimetres, different tracking heights/positions/yaws, near-vertical heading, finite bounds.
- Java storage: SHA identity, rename/reimport, changed scan rejection, concurrent saves, View CRUD/default/delete, 100-View limit, source preservation, invalid/corrupt metadata.
- Optional Swift interop: compile the actual Vision core sources, encode a Vision fixture, load/edit it with Quest LibraryStore, decode Quest output with Vision's actual `ScanViewpoints` and `ScanSettings`; restore through different source transforms.
- Android runtime: run the same Java model suite against Android's own `org.json`, separate from any application data.

These checks establish data compatibility. A physical Vision Pro ↔ Quest package transfer remains a later integration test.
