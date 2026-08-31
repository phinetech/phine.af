# Configuration reference

## Logging schema

Two schemas exist across the config files in this repository:

- **Modern (parsed today):** per-component `logging:` block with
  `level`, `console_output` (bool), `file_output` (bool), `file_path`.
  Decoded by [`common/config/src/af_typed_config.cpp`](../../common/config/src/af_typed_config.cpp).
  **Caveat:** `file_output`/`file_path` are parsed but not yet wired to a
  spdlog file sink — components log to stdout regardless. Tracked in
  issue **#13** ([TECH DEBT] Restore and enhance logging infrastructure).
- **Legacy (kept for reference, not parsed):** top-level `logging:` block
  with `level`, `file`, `console`, `max_size_mb`, `max_files`. Present in
  `config/af_config.yaml`, `config/components/*.yaml`, the trailing block
  of `config/af.yaml`, and some `docker-compose/conf/*.yaml` files. These
  values are ignored at runtime.

## `config/af.yaml` (bundled AF)

- `af_core.logging.level`
- `af_core.logging.console_output`
- `af_core.logging.file_output` *(parsed, not wired — see #13)*
- `af_core.logging.file_path` *(parsed, not wired — see #13)*
- `af_core.communication.kind` / `.listen.host` / `.listen.port` / `.timeout_ms`
- `af_core.qod.max_session_duration` / `.min_session_duration` / `.session_cleanup_interval` / `.unavailable_session_ttl` / `.enable_notifications` / `.api_base_url`
- `pcf_handler.enabled`
- `pcf_handler.pcf.base_url` / `.use_tls` / `.api_version`
- `pcf_handler.logging.*` (same shape as `af_core.logging.*`)
- `pcf_handler.communication.*`
- `nef_handler.enabled` / `udr_handler.enabled` — reserved for future handlers
- `metrics.enabled` / `.port` / `.path`

## `af_core/config/af_core.yaml`

- `af_core.pcf_base_url`
- `af_core.use_tls`
- `af_core.api_version`
- `af_core.logging.level`
- `af_core.logging.console_output`
- `af_core.logging.file_output` *(parsed, not wired — see #13)*
- `af_core.logging.file_path` *(parsed, not wired — see #13)*
- `af_core.communication.server_address`
- `af_core.communication.server_port`

## `config/af_config.yaml` *(legacy schema — not parsed)*

Kept in-repo as a reference for the legacy shape. See #13. Fields:

- `af_config.host` / `.port` / `.threads` / `.log_level` / `.tls_enabled` / `.cert_file` / `.key_file`
- `communication.type` / `.af_core_service` / `.af_core_host` / `.af_core_port` / `.timeout_ms`
- `logging.level` / `.file` / `.console` / `.max_size_mb` / `.max_files`
- `metrics.enabled` / `.port` / `.path`