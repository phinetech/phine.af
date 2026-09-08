# Run

## Running via Docker Compose


```bash
# Simple getting-started stack
docker compose up -d --build
```

Use [docker-compose/compose.yaml](../../docker-compose/compose.yaml) only when you need profile-based variants such as split AF, standalone QoD, or OAI-core.

## Health checks

Every component exposes a `health_check` message. Over HTTP/2 that is a
`GET /health` on the `api_component` port (default `:8080`):

```bash
curl -k http://localhost:8080/health
# {"status":"up"}
```

Over gRPC (internal or for testing), send an `InternalCommunication.SendMessage`
with `message_type: "health_check"` — see the [First Request](04-first-request.md)
example for the invocation pattern.

<!--- Native (non-Docker) run instructions live in [02-build.md](02-build.md);
this page focuses on the Compose flow. -->
