# Observability

<!---
Got logging configuration keys from [af_core/config/af_core.yaml](../../af_core/config/af_core.yaml) and [config/af_config.yaml](../../config/af_config.yaml).
-->

## Logging

- AF Core config includes logging level and file path options.
- API component config includes logging level, log file path, and log rotation settings.

<!---
TODO: Document runtime log locations for Docker deployments.
We need to inspect Dockerfiles and Compose volume mounts.
-->

## Metrics

<!---
Got metrics configuration keys from [config/af_config.yaml](../../config/af_config.yaml).
-->

The API component config includes a `metrics` section with `enabled`, `port`, and `path`.

<!---
TODO: Confirm whether metrics endpoint is implemented and which component serves it.
-->
