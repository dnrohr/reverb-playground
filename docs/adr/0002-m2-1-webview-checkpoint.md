# ADR 0002: M2.1 webview checkpoint

- Status: provisionally accepted
- Date: 2026-08-08

## Context

ADR 0001 selected React, TypeScript, React Flow, and a JUCE WebBrowserComponent, with an explicit checkpoint before production commitment. M2.1 must prove the editor interaction model without coupling it prematurely to real-time DSP state.

## Decision

Retain the web stack through M2.2. The production web bundle is generated into `src/ui/WebAssets`, compiled into the binary, and served by JUCE's resource provider. The shipped editor therefore has no localhost or network dependency.

The native JUCE layer continues to own the audible-reference controls. The web editor owns schematic navigation, selection, and inspection. Native-to-web runtime identity and parameter transport are intentionally deferred to M2.2.

## Evidence and limits

The browser prototype demonstrates fitted rendering, selection-driven inspection, pan/zoom controls, keyboard-focusable graph elements, and non-colour signal semantics. The embedded Standalone build is the first packaging check.

VST3 validation in two hosts remains a release checkpoint before treating the webview choice as final. Until that test is recorded, this decision is provisional and the native audition strip provides a usable fallback surface if a host cannot construct WebView2.

## Consequences

- One UI implementation serves the standalone application and plugin formats.
- Generated web assets and their pinned lockfile are verified in CI.
- M2.2 must define a versioned message boundary and detect UI/runtime identity drift.
- Host compatibility, accessibility bridging, memory use, and WebView2 availability remain explicit risks.
