# Configuration reference

## `config/af_config.yaml`

- `af_config.host`
- `af_config.port`
- `af_config.threads`
- `af_config.log_level`
- `af_config.tls_enabled`
- `af_config.cert_file`
- `af_config.key_file`
- `communication.type`
- `communication.af_core_service`
- `communication.af_core_host`
- `communication.af_core_port`
- `communication.timeout_ms`
- `logging.level`
- `logging.file`
- `logging.console`
- `logging.max_size_mb`
- `logging.max_files`
- `metrics.enabled`
- `metrics.port`
- `metrics.path`

## `af_core/config/af_core.yaml`

- `af_core.pcf_base_url`
- `af_core.use_tls`
- `af_core.api_version`
- `af_core.logging.level`
- `af_core.logging.console_output`
- `af_core.logging.file_output`
- `af_core.logging.file_path`
- `af_core.communication.server_address`
- `af_core.communication.server_port`

<!---
TODO: Validate which keys are actually read by code.
`AfOrchestrator::load_config()` currently loads YAML but includes TODOs for applying values.
Source: [af_core/src/af_orchestrator.cpp](../../af_core/src/af_orchestrator.cpp).
-->
