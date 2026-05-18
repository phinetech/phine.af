# Docker Compose environments

## Build environments

- `docker-compose/docker-compose-oai-build.yaml`
- `docker-compose/docker-compose-free5gc-build.yaml`
- `docker-compose/docker-compose-bundled.yaml`

### Bundled deployment

The bundled deployment keeps:

- `af`: bundled AF Core + southbound PCF handler
- `api_component`: standalone northbound HTTP/2 API

In other words, “bundled” means **core + southbound** are packaged together. Northbound adapters are still deployed separately.


## Test environment

- `docker-compose/docker-compose-test.yaml`


<!---
TODO: Document which services are included in each environment and which volumes/config files are mounted.
We should inspect each Compose file and its referenced config files.
-->
