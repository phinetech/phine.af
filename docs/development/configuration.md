# Configuration (developer guide)

## AF Core configuration

- Default AF Core config path used by the AF Core binary: `/etc/oai/af/af_core.yaml`
- A repository-provided example config exists at `af_core/config/af_core.yaml`.


## Northbound API component configuration

The repo contains `config/af_config.yaml` with host/port, TLS, and gRPC target configuration for talking to AF Core.

<!---
TODO: Document how these config files are mounted/used in each Docker Compose environment.
We need to inspect the relevant Dockerfiles and Compose yaml files for volume mounts.
-->
