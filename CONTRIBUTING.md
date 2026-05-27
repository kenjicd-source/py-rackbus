# Contributing

Thanks for your interest. Here's how to help.

## What we welcome

- **Compatibility reports** — successful or unsuccessful tests with
  other Rackbus instruments. Use the
  [compatibility report](.github/ISSUE_TEMPLATE/compatibility_report.md)
  issue template.
- **Captured frames** — wire dumps with known parameter values from
  any Rackbus device. Sanitize any commercially sensitive data
  (tag names, customer identifiers) first.
- **Bug fixes** — with a minimal reproduction case ideally.
- **New features** — discuss in an issue first if substantial.
- **Documentation improvements** — including translations and
  clarifications.
- **Hardware ports** — ESP32, Arduino, STM32, RP2040, etc.
- **Tooling** — Wireshark dissector, GUI configurators, MQTT bridges,
  Modbus TCP gateway.
- **Response-CRC reverse engineering** — we have the captured material
  but the formula isn't found yet.

## What we don't accept

- **Proprietary code or documentation** from any manufacturer
- **Reverse-engineered firmware** or anything derived from
  decompiling vendor software
- **Write functionality in the core library** — forks may add this,
  but the upstream stays read-only for safety reasons. If you need
  write support, discuss alternative approaches (e.g., a separate
  optional package with extra safeguards).

## Code style

- Python: PEP 8, type hints encouraged
- Formatting: `black` (line length 100)
- Linting: `ruff` (see `pyproject.toml`)
- Commit messages: imperative mood ("Add density decoder", not "Added
  density decoder")
- One logical change per PR

## Testing

All PRs need to pass the existing test suite:

```bash
pip install -e ".[dev]"
pytest
```

Adding tests for new functionality is strongly encouraged. If your
change affects protocol decoding, please include byte-level fixtures
(captured frames with known values) so tests remain reproducible
without real hardware.

## Legal

By submitting a contribution, you confirm that:

1. You wrote the contribution yourself, OR have appropriate rights to
   submit it
2. You agree to license your contribution under the project's MIT
   license
3. Your contribution does not include any proprietary code or
   confidential information from any manufacturer
4. If your contribution relates to reverse engineering, it was done
   through lawful clean-room methods (observation of traffic, public
   datasheets, etc.) and not through decompilation of proprietary
   firmware or software

## Code of Conduct

See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md). TL;DR — be respectful,
constructive, and welcoming.

## Questions

Open an issue with the "question" label, or start a discussion in the
GitHub Discussions tab.
