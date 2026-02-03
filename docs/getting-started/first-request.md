# First request

This page shows one known working request format used by the repository documentation.

## QoD create session (gRPC)

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
TODO: Document expected response schema for the above.
We need to inspect the proto (`common/protos`) and/or AF core handlers to document response types.
Sources: the command is from [README.md](../../README.md); handler wiring is in [af_core/src/af_orchestrator.cpp](../../af_core/src/af_orchestrator.cpp).
-->
