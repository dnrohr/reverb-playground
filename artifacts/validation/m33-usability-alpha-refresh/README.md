# M33 usability alpha refresh qualification

`workflow-matrix.json` is the integrated M28-M32 release matrix. It maps every
required user journey to expected graph/audio state, persistence, history,
accessibility, real-time safety, recovery behavior, automated coverage, and
reviewed UI evidence.

The alpha.2 publication gate requires:

- complete local Release verification;
- standalone and VST3 exact-commit builds;
- standalone cold-start and crash-report fixtures;
- pluginval strictness 10 and Steinberg extensive validation;
- a deterministic exact-commit ZIP and SHA-256 file;
- successful clean-main Verify and package jobs;
- a successful `v0.1.0-alpha.2` release workflow and GitHub prerelease.

The three M7.5 non-implementer sessions and M24.1 comparative listening notes
remain outstanding. They are not represented as passed by this internal matrix.
