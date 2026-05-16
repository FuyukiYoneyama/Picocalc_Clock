# Picocalc_Clock Agent Rules

This file records repository-local development rules for AI coding agents and
maintainers. It is safe to publish with the repository.

## Build Handoff

- Before handing a UF2 to a tester, build it once after committing the intended
  source changes.
- Confirm the firmware startup log includes:
  - `Picocalc_Clock version ...`
  - `BUILD ID git=... dirty=... time="..." purpose="..."`
- Do not debug a hardware log unless its `BUILD ID` line is present, or the
  missing build ID is explicitly noted.

## Quality Rule

- Always aim for complete handling of the current requested scope, not a minimal
  proof that merely compiles.
- When implementing a human-facing UART command interface, treat it as a real UI:
  - print a prompt such as `> `;
  - echo printable user input;
  - support Backspace visually and internally;
  - handle an empty Return by showing the next prompt;
  - show startup help when useful;
  - return to a prompt after every command;
  - make command success and failure messages clear.
- Do not regress previously working interaction quality when moving from
  tests/diagnostics to the formal app.
