# Testing

## Local validation (CI-like)

```bash
./build/scripts/ci_helper.sh
```

The script:

1. Checks code quality (trailing whitespace, shell script permissions)
2. Builds AF Core, PCF Handler, and API Component Docker images
3. Builds integration test image
4. Runs integration tests using Docker Compose

## Integration tests (CI)

The documented integration environment uses [docker-compose/compose.yaml](../../docker-compose/compose.yaml) with the relevant profiles for the scenario under test.

<!---
TODO: Document how to run unit tests (if any) outside the integration test container.
We need to inspect CMake test targets and component-level READMEs.
-->
