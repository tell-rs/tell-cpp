# Changelog

## v0.1.1

New:
- config: service field on TellConfig builder, stamped on every event and log
- encoding: service field in FlatBuffer event schema (field 2 in vtable)
- client: log service resolution chain — explicit param > config-level > "app" default

Fix:
- docs: replace placeholder API keys with consistent feed1e11 dummy key across README, examples, tests, and benchmarks
