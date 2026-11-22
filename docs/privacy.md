---
layout: default
title: Privacy policy
permalink: /privacy/
description: How Splativerse for Meta Quest handles scans, saved Views, controller tracking, local settings and support information.
---

# Privacy policy

**Effective date: November 21, 2026**

This policy describes Splativerse for Meta Quest, developed by Christopher Arnold (ncubeeight), and this project website. It covers the current app, version 0.1.14. Independently modified versions may behave differently. The [visionOS edition has its own policy](https://ncubeeight.github.io/Splativerse-for-visionOS/privacy/).

## Your scans stay under your control

Splativerse is a viewer for Gaussian splat files you create or obtain yourself. The app does not require an account, operate a hosted scan library, provide public galleries, or upload scans to the developer. It contains no advertising, tracking or third-party analytics SDKs. The app does not sell personal data or use scans or movement data for advertising or model training.

The current Quest app does not request Android's Internet permission. It does not provide cloud synchronization, automatic sharing or companion-content downloads.

## Files and information stored on your headset

When you choose files in Android's system file picker, Splativerse reads the selected supported scans and imports copies into its local app storage. It also reads supported files you place in its Scans folder. Importing does not modify your source file.

The local library uses filenames, sizes, file fingerprints and scan settings to identify and display files. It stores scan titles, last-opened timestamps, scale and orientation settings, viewing radius and appearance settings. Local preferences store the controls panel's height and the fingerprints of scans you have hidden from navigation.

If you select a file through a third-party storage provider, that provider may download the file or process information under its own policies. Splativerse does not operate that provider. The current Android manifest disables automatic app backup; separately copied files and platform or provider records may still exist outside the app's storage.

## Saved Views

When you save a View, the app stores its identifier, name, position and horizontal viewing direction in the scan's coordinate system. It associates these with the scan's content fingerprint and can store an optional starting View. These are local bookmarks, not a continuous recording of your physical movements or a scan of your room.

Opening a View changes your position in the displayed scene. Hiding a scan from navigation does not delete its file or its Views. This Quest version does not implement the visionOS edition's sharing-package interface or send your Views to other users.

## Headset and controller tracking

The app uses the headset and Touch controller poses and button events provided by the OpenXR runtime to render the scene, navigate and point at menu controls. This data is processed locally during use. It does not transmit your poses to the developer or keep a continuous movement history.

Saving a View stores the scene position described above. Development diagnostics can record scene positions in local logs. The app does not request raw camera images, microphone access, contacts, location services, eye tracking or hand tracking.

## Diagnostics and support

The app writes local system logs about loading, errors, performance, menu actions and View placement. These logs may include filenames, file paths, scan fingerprints, View names and saved scene coordinates. Development screenshot and comparison tools can create local rendered images and camera metadata. There is no app-operated service that automatically uploads these logs or images.

Meta may process device, store, crash or usage information through its platform services according to your settings and its own policies. See [Meta's Privacy Policy](https://www.facebook.com/privacy/policy/).

If you choose to contact support or submit an issue, the information you send may be used to investigate and respond. Support email is handled through Google Groups; visibility and retention depend on the group's and Google's settings. GitHub issues are public. Please avoid sharing private scans, personal information or sensitive logs in public channels.

## Retention, deletion and your choices

Imported scans, library metadata, preferences and saved Views remain in app storage until removed. You can delete individual saved Views through the library's View management controls. The scan hide control only hides an entry; it is not a delete command.

You can remove imported copies using a file-management method that has access to the app's storage. Clearing app data or uninstalling removes app-owned data according to the operating system's behavior. Back up any scans or Views you want to keep first. Original files elsewhere, exported diagnostic images, independently copied files and provider copies are not removed by deleting an app-owned copy; manage them separately.

Operating-system log retention is controlled by the headset. Support messages and platform reports may remain in the service where they were submitted under its retention settings. There is no Splativerse user account to delete and no developer-hosted scan collection. Contact us about information you have sent directly for support.

## This website and external services

This website is hosted on GitHub Pages. We do not add analytics, advertising, tracking pixels, cookies or third-party embedded media. GitHub may process technical information such as IP addresses and request details to operate and secure hosting. See the [GitHub General Privacy Statement](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement).

Links to Meta, GitHub, Google Groups, Arrival Space, RadianceFields and other services lead to sites governed by their own policies. Those services are separate from the local Splativerse viewer.

## Children

Splativerse does not knowingly collect personal information from children through the app. The same local processing and file controls described above apply to all users. If you believe a child has shared personal information through a support request, contact us so it can be addressed.

## Changes to this policy

Changes will be published on this page with a revised effective date. If a future version adds online features or changes data handling, this policy will be updated to describe those changes.

## Contact

For privacy questions or requests concerning information shared directly for support, contact Christopher Arnold and the Splativerse project at [splativerse@googlegroups.com](mailto:splativerse@googlegroups.com). Send only the information needed to describe the request. Do not send private scans or sensitive diagnostics unless a suitable way to share them has been agreed.
