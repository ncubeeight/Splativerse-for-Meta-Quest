# Third-party notices

Original Splativerse application code is Apache-2.0; see LICENSE. The generated demo room and the navigation/spawn behavior come from the user's Splativerse visionOS project (source archive dated November 15, 2026).

## Khronos OpenXR

- https://github.com/KhronosGroup/OpenXR-SDK-Source
- Loader and headers: Maven `org.khronos.openxr:openxr_loader_for_android:1.1.49`, downloaded from Maven Central.
- `xr_linear.h`: Khronos `release-1.1.49`, copyright Khronos Group and Oculus VR, Apache-2.0.
- License included in `vendor/openxr/LICENSE`; original header notices retained.
- The arm64 loader binary is unmodified. Other ABI binaries are not bundled.

## Niantic SPZ

- https://github.com/nianticlabs/spz
- Tag v2.0.0, commit `cc5671d716740e02d57816d4a4a38d164926e695`.
- MIT license in `vendor/spz/LICENSE`.
- Only C++ sources are vendored. Decoding supports SPZ versions 1–3.
- Local modifications in `load-spz.cc`: share a 1,000,000-point / 128 MiB input bound with the application via `splativerse-limits.h`; validate gzip output against its SPZ header and the maximum 65,000,016-byte decoded payload; reject unsupported headers/flags, truncated or trailing data, and fractional-bit counts above 24; avoid copying vectors just to calculate byte counts; avoid taking `operator[]` on an empty SH vector for degree-zero scans. The last issue was detected and regression-tested with UndefinedBehaviorSanitizer.

## MetalSplatter reference

- https://github.com/scier/MetalSplatter
- Existing Splativerse vendor revision: `464eb37c55d90d7362a79120fdf8b50d4ae03296`.
- The GLSL Gaussian projection, SH evaluation, and compositing implementation was informed by the existing Metal implementation. It uses Vulkan's asymmetric eye projection and a different GPU buffer layout.
- MIT notice retained in `vendor/MetalSplatter-LICENSE`.

## Android NDK

The build compiles `android_native_app_glue.c` from the installed Android NDK. It is Android Open Source Project code under Apache-2.0. Android's system zlib is linked dynamically. The NDK itself is not included in the project or source archive.

## Vision Pro performance port (November 19, 2026)

The local Splativerse Vision Pro 1.1 (24) working tree supplied the live-radius/fade design, Morton-local 512-point block selection, 1024-splat indexed batching, and normalized antialiasing helper. These are adapted to native C++/GLSL/Vulkan. Its MetalSplatter MIT notice remains in `vendor/MetalSplatter-LICENSE`. The source was adapted from a local working tree that included uncommitted changes. Quest retains float32 RGB coefficients (tightly packed) and depth sorting rather than copying Metal half precision or radial sorting. The Vision Pro project was not modified.
