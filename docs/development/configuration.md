# Configuration (developer guide)

## AF Core configuration

- Default AF Core config path used by the AF Core binary:
  `/etc/phine.af/af_core.yaml`.
- A repository-provided example config exists at
  [`af_core/config/af_core.yaml`](../../af_core/config/af_core.yaml).

The bundled `af` binary uses [`config/af.yaml`](../../config/af.yaml)
which contains both `af_core:` and `pcf_handler:` sub-configs in one file.

## Northbound API component configuration

The example config lives at
[`northbound/api_component/config/api_adapter.yaml`](../../northbound/api_component/config/api_adapter.yaml).
It sets HTTP/2 listen host/port, TLS toggles, and the AF Core communication
target. The `logging:` block follows the modern schema
(`console_output` / `file_output` / `file_path`) — see the
[configuration reference](../reference/configuration-reference.md#logging-schema)
for details on how logging is currently wired.

## Docker Compose mounts

Each Compose service in [`docker-compose/compose.yaml`](../../docker-compose/compose.yaml)
mounts its config from `docker-compose/conf/<component>.yaml` into
`/etc/phine.af/<component>.yaml` inside the container, matching the binary
default path. Profile-specific variants (`compose.grpc.yaml`,
`compose.http.yaml`) mount the corresponding `*.grpc.yaml` / `*.http.yaml`
files. Change any config value by editing the file under
`docker-compose/conf/` — no rebuild required.