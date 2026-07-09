# Docker Compose environments

The repository now documents a single Compose entrypoint:

- `docker-compose/compose.yaml`

Deployment selection is controlled by Compose profiles.

## Profiles

- `free5gc`: 5G core network + UERANSIM
- `afs`: separate `af_core` + `pcf_handler` + `api_component`
- `af`: bundled `af` + `api_component`
- `standalone-qod`: standalone `demo-qod-adapter`
- `demo-qod`: bundled `af_demo_qod` + `api_component`
- `af-client`: helper Ubuntu container for manual testing

## Common combinations

- Bundled AF + free5GC:
	- `docker compose -f docker-compose/compose.yaml --profile free5gc --profile af up -d`
- Split AF + free5GC:
	- `docker compose -f docker-compose/compose.yaml --profile free5gc --profile afs up -d`
- Split AF + standalone QoD adapter + free5GC:
	- `docker compose -f docker-compose/compose.yaml --profile free5gc --profile afs --profile standalone-qod up -d`
- Bundled AF + standalone QoD adapter + free5GC:
	- `docker compose -f docker-compose/compose.yaml --profile free5gc --profile af --profile standalone-qod up -d`

### Bundled deployment

The bundled deployment keeps:

- `af`: bundled AF Core + southbound PCF handler
- `api_component`: standalone northbound HTTP/2 API

In other words, “bundled” means **core + southbound** are packaged together. Northbound adapters are still deployed separately.


<!---
TODO: Document which services are included in each environment and which volumes/config files are mounted.
We should inspect each Compose file and its referenced config files.
-->
