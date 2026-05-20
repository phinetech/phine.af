# Quickstart

## Option A: Run using Docker Compose

```bash
# Start the simple getting-started stack
docker compose up -d --build
```

For advanced topologies such as split AF, standalone QoD, or OAI-core variants, use [docker-compose/compose.yaml](../../docker-compose/compose.yaml) with profiles.

## Send a test request to AF Core

```bash
docker run --rm --network host -v ./common/protos:/var/protos/ fullstorydev/grpcurl -plaintext \
  -proto message.proto \
  -import-path /var/protos \
  -d '{
     "message_type": "qod_create_session",
     "correlation_id": "12345",
     "payload": "'$(cat ./af_core/tests/requests/qod/qod_create_session.json | base64 -w 0)'",
     "metadata": {
      "source": "command_line",
      "priority": "high"
    }
  }' \
  192.168.70.141:50051 \
  af.proto.InternalCommunication/SendMessage
```

<!---
TODO: Confirm the correct AF Core host/IP for your environment.
The README example uses `192.168.70.141:50051`, but the repo also uses `localhost:50051` in multiple places.
Sources: [README.md](../../README.md), [config/af_config.yaml](../../config/af_config.yaml), [af_core/config/af_core.yaml](../../af_core/config/af_core.yaml), and [af_core/src/af_orchestrator.cpp](../../af_core/src/af_orchestrator.cpp).
-->
