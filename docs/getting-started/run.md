# Run

## Running via Docker Compose


```bash
# OAI build environment
docker compose -f docker-compose/docker-compose-oai-build.yaml up -d

# Free5GC build environment
docker compose -f docker-compose/docker-compose-free5gc-build.yaml up -d
```

## Health checks

TODO: Document how to call the `health_check` message over the configured northbound interface.

<!---
TODO: Expand with “native run” instructions.
The root README currently states this section will be expanded.
Source: [README.md](../../README.md).
-->
