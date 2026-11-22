# Splativerse for Meta Quest

A native Gaussian splat viewer for Meta Quest, built with C++, Vulkan, OpenXR and an Android scan library.

[Project website](https://ncubeeight.github.io/Splativerse-for-Meta-Quest/) · [Privacy policy](https://ncubeeight.github.io/Splativerse-for-Meta-Quest/privacy/) · [Support](https://ncubeeight.github.io/Splativerse-for-Meta-Quest/support/)

## Features

- Import SPZ, PLY and SPLAT scans from the Android file picker.
- Explore scenes with tracked headset movement and Touch controller navigation.
- Search, sort, and hide or restore library entries without deleting scan files.
- Save named Views, choose a starting View, and jump between saved Views inside a scene.
- Adjust rendering radius and filtering, and reposition or hide the controls panel.
- A small synthetic demonstration scene is included.

This is the Quest counterpart to [Splativerse for visionOS](https://github.com/ncubeeight/Splativerse-for-visionOS). Saved View coordinates and scan fingerprints use compatible models; cross-device package sharing is not implemented in this Quest version. See [VIEW_COMPATIBILITY.md](VIEW_COMPATIBILITY.md).

## Status

Version **0.1.14**, code **15**, is a development preview. Library, saved Views, controller menu interaction and return to the system library have been tested on Quest Pro. Quest 2, Quest 3 and Quest 3S have not yet been physically tested. Large-scan performance remains below the store rendering requirement and needs further optimization and sustained testing. Store submission preparation is in progress; this repository does not claim store approval or availability.

Scans up to 1 million splats / 128 MiB are accepted, but file acceptance is not a performance guarantee. Other scenes and optional companion downloads are not included. There are no in-app accounts, ads, analytics services or network downloads.

## Controls

| Control | Action |
| --- | --- |
| Left stick | Move / strafe |
| Right stick | Snap turn / change height |
| Left-stick click | Save a View |
| Right-stick click | Change radius |
| X | Open / close saved Views |
| Y | Original scan appearance / sharper detail |
| A | Return to starting View |
| B | Return to library |
| Right trigger | Select the pointed menu control or View |
| Left trigger | Restore hidden controls |

The optional paired-detail comparison mode uses X to switch detail and left-stick click to capture comparison images. Artificial movement has no collision detection.

## Build

Requires Python 3, a JDK, Android SDK platform 36 and Build Tools 36.0.0, and Android NDK r27d (27.3.13750724). Configure `ANDROID_HOME`, `ANDROID_NDK_HOME` and `JAVA_HOME` for your machine.

```sh
python3 scripts/build.py
python3 scripts/test.py
python3 scripts/test_library.py --json-jar /path/to/org.json.jar
```

The build creates a non-debuggable `Splativerse-Quest-preview.apk`, signed with a locally generated development key in `.build/`. Keep that key securely for subsequent updates; never commit it. A clone generates its own key and cannot update an installation signed by a different key. Production key management is the distributor's responsibility.

For sideloading, enable Developer Mode, allow USB debugging, then run:

```sh
python3 scripts/device.py install
python3 scripts/device.py launch
```

The scripts directly invoke Android tools; Gradle is not required. Tested build host: Apple Silicon macOS. Linux paths are supported by the helper but are not validated here.

## Website and license

`docs/` is a Jekyll site, published through GitHub Pages from the `main` branch. The layout mirrors the visionOS project, with Quest-specific usage and privacy text.

Apache License 2.0. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for bundled dependency licenses. Signing keys, personal scans, device logs, saved View catalogs, and local validation captures are excluded from this repository.
