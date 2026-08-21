---
runme:
  id: qos-grpc-tutorial
  version: v3
cwd: ../..
---

# End-to-End QoS Enforcement Tutorial (gRPC Transport)

This tutorial deploys the 5G testbed, sends a CAMARA QoD session request over **gRPC**, and verifies downlink bandwidth enforcement with `iperf3`.

This page only covers what differs from the HTTP tutorial. For the full background on the testbed topology, traffic capture, the complete CAMARA session lifecycle, and the `iperf3` teardown verification, see the [HTTP QoS Enforcement Tutorial](06-qos-http-tutorial.md) — everything there applies here except the transport.

By the end of the run you will:

1. Start free5GC, UERANSIM, and the phine.af Application Function with its gRPC northbound interface active
2. Send a CAMARA QoD `createSession` request through `grpcurl`
3. Optionally inspect the policy signalling path: **AF → PCF → SMF → UPF**
4. Verify that the UPF enforces downlink bandwidth with `iperf3`

## Prerequisites

Same as the [HTTP tutorial's prerequisites](06-qos-http-tutorial.md#prerequisites).

## Configuration

As with the HTTP tutorial, you only need to choose the AF topology (`AF_PROFILE=af` or `AF_PROFILE=afs`) — this tutorial always runs over **gRPC** (`COMPOSE_OVERRIDE_FILE=docker-compose/compose.grpc.yaml`):

```bash {"name":"setup-variables","interactive":"false"}
export COMPOSE_FILE="${COMPOSE_FILE:-docker-compose/compose.yaml}"
export COMPOSE_OVERRIDE_FILE="${COMPOSE_OVERRIDE_FILE:-docker-compose/compose.grpc.yaml}"
export AF_PROFILE="${AF_PROFILE:-af}"
export COMPOSE_PROFILES="${COMPOSE_PROFILES:---profile free5gc --profile $AF_PROFILE}"
export RAN_SERVICES="${RAN_SERVICES:-ueransim-gnb ueransim-ue}"
export LOGS_DIR="${LOGS_DIR:-/tmp/phine.af/qos-grpc-tutorial/logs}"
mkdir -p "$LOGS_DIR"
sudo mkdir -p "$LOGS_DIR"
sudo chmod 777 "$LOGS_DIR"

export UE_IP="${UE_IP:-10.60.0.1}"
export EXT_DN_IP="${EXT_DN_IP:-192.168.72.135}"
export MIN_MBPS="${MIN_MBPS:-3}"
export MAX_MBPS="${MAX_MBPS:-12}"
echo "Configuration set:"
echo "  COMPOSE_FILE: $COMPOSE_FILE"
echo "  COMPOSE_OVERRIDE_FILE: $COMPOSE_OVERRIDE_FILE"
echo "  AF_PROFILE: $AF_PROFILE"
echo "  COMPOSE_PROFILES: $COMPOSE_PROFILES"
echo "  RAN_SERVICES: $RAN_SERVICES"
echo "  LOGS_DIR: $LOGS_DIR"
echo "  UE_IP: $UE_IP"
echo "  EXT_DN_IP: $EXT_DN_IP"
echo "  Bandwidth validation range: ${MIN_MBPS}-${MAX_MBPS} Mbps"
```

The gRPC entrypoint is `192.168.70.141:50051` and `af_client` uses `grpcurl` — see [docker-compose/compose.grpc.yaml](../../docker-compose/compose.grpc.yaml). For the service/address reference table, see the [HTTP tutorial](06-qos-http-tutorial.md#step-1-deploy-the-setup).

## Step 1: Deploy the Setup

Identical to the HTTP tutorial's [Step 1](06-qos-http-tutorial.md#step-1-deploy-the-setup):

```bash {"name":"install-deps","interactive":"false"}
./build/scripts/ci_helper.sh install_dependencies
./build/scripts/ci_helper.sh install_gtp5g
```

```bash {"name":"check-submodules","interactive":"false"}
./build/scripts/ci_helper.sh check_submodules
```

```bash {"name":"deploy-stack","interactive":"false"}
# Tear down all AF topology profiles before deploying to avoid host-port
# conflicts — 'af' and 'af_core' are mutually exclusive and both bind the
# same host ports (8080 for HTTP, 50051 for gRPC).
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE \
  --profile af --profile afs --profile free5gc down 2>/dev/null || true

# Forcibly stop any container still holding port 8080 or 50051 in case the
# compose down did not release them (e.g. after a partial or crashed run).
docker ps -q --filter publish=8080  | xargs -r docker stop 2>/dev/null || true
docker ps -q --filter publish=50051 | xargs -r docker stop 2>/dev/null || true

docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE $COMPOSE_PROFILES up -d --build

sleep 30

docker compose -f $COMPOSE_FILE up -d $RAN_SERVICES
```

```bash {"name":"wait-for-healthy","interactive":"false"}
sleep 30
docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES ps
```

```bash {"name":"verify-ue-ip","interactive":"false"}
sleep 15
docker exec ue ip addr show uesimtun0 | grep "10.60.0.1"
```

```bash {"name":"verify-connectivity","interactive":"false"}
docker exec ue ping -I uesimtun0 $EXT_DN_IP -c 3
```

## Step 2: Start Traffic Capture

```bash {"name":"start-capture","interactive":"false"}
PCAP_FILE="$LOGS_DIR/capture.pcapng"
PID_FILE="$LOGS_DIR/capture.pid"
LOG_FILE="$LOGS_DIR/capture.tshark.log"

sudo nohup tshark -i demo-oai \
  -f "host 192.168.70.141 or host 192.168.70.140 or host 192.168.70.139 or host 192.168.70.133" \
  -w "$PCAP_FILE" \
  >"$LOG_FILE" 2>&1 &
echo $! > "$PID_FILE"

echo "Traffic capture started in background (PID: $(cat "$PID_FILE" 2>/dev/null || echo unknown))"
sleep 2
```

## Step 3: Create a Session

This tutorial uses the same `premium` QoS profile as the HTTP tutorial (5 Mbps guaranteed / 10 Mbps maximum, 5QI 7, 3600s duration) — see [af_core/tests/requests/qod/qod_create_session.json](../../af_core/tests/requests/qod/qod_create_session.json).

Send the request. The `af-client` service (defined in [docker-compose/compose.grpc.yaml](../../docker-compose/compose.grpc.yaml)) base64-encodes the payload and sends it via `grpcurl` as a `qod_create_session` message on the `InternalCommunication.SendMessage` RPC:

```bash {"name":"send-qod-request","interactive":"false"}
docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE --profile af-client up -d
```

Because the request runs detached (`up -d`), view the response with `docker logs af-client`. The gRPC response is an `InternalMessage` whose `payload` field is itself base64-encoded CAMARA JSON, so extracting `sessionId` needs one extra unwrap step compared to the HTTP tutorial:

```bash {"name":"extract-session-id","interactive":"false"}
# Wait for af-client to exit, then unwrap the base64 payload to read the sessionId
docker wait af-client > /dev/null 2>&1 || true
RESPONSE_PAYLOAD=$(docker logs af-client 2>/dev/null | jq -r '.payload // empty')
SESSION_ID=$(echo "$RESPONSE_PAYLOAD" | base64 -d 2>/dev/null | jq -r '.sessionId // empty')
if [ -z "$SESSION_ID" ]; then
  echo "ERROR: Could not extract sessionId — inspect the response with: docker logs af-client"
  docker logs af-client 2>&1 || true
else
  export SESSION_ID
  echo "SESSION_ID: $SESSION_ID"
fi
```

```bash {"name":"wait-for-qos-policy","interactive":"false"}
echo "Waiting for QoS policy to propagate through signalling chain..."
sleep 10
echo "QoS policy propagation wait complete"
```

```bash {"name":"stop-capture","interactive":"false"}
PCAP_FILE="$LOGS_DIR/capture.pcapng"
PID_FILE="$LOGS_DIR/capture.pid"

if [ -f "$PID_FILE" ]; then
  PID="$(cat "$PID_FILE" 2>/dev/null || true)"
  if [ -n "$PID" ]; then
    sudo kill -TERM "$PID" 2>/dev/null || true
    sleep 1
    sudo kill -KILL "$PID" 2>/dev/null || true
  fi
  rm -f "$PID_FILE"
fi
sudo chmod a+r "$PCAP_FILE" 2>/dev/null || true
echo "Capture stopped"
```

### Session lifecycle beyond creation

`af_core` registers gRPC handlers for `qod_get_session`, `qod_retrieve_sessions`, `qod_extend_session`, and `qod_delete_session` (the same operations exercised over REST in the HTTP tutorial), but the `af-client` service in [docker-compose/compose.grpc.yaml](../../docker-compose/compose.grpc.yaml) only wires up `qod_create_session` today. To exercise retrieve, extend, or delete, use the [HTTP tutorial's session lifecycle steps](06-qos-http-tutorial.md#step-3-session-lifecycle) instead, or invoke `grpcurl` directly (see [First Request](04-first-request.md) for the invocation pattern).

Because this tutorial never deletes the session, `docker compose down` in [Cleanup](#cleanup) tears down the stack with the session still active — this is expected.

## Step 4: Trace the Signalling Path

This step is optional, but useful if you want to confirm how the QoD request moved through the control plane.

```bash {"name":"view-capture","interactive":"false","excludeFromRunAll":"true"}
PCAP_FILE="$LOGS_DIR/capture.pcapng"
# Open file with wireshark
wireshark $PCAP_FILE
```

See [What you should see in the capture](06-qos-http-tutorial.md#step-4-trace-the-signalling-path) in the HTTP tutorial — the AF → PCF → SMF → UPF sequence is identical regardless of transport.

## Step 5: Verify QoS Enforcement with iperf3

This setup enforces QoS on **downlink** traffic, so the measurement is taken from the external data network toward the UE.

Start the `iperf3` server on the UE:

```bash {"name":"start-iperf3-server","background":"true","interactive":"false"}
docker exec -d ue iperf3 -s -B $UE_IP -p 5070
```

Run the client from the external data network:

```bash {"excludeFromRunAll":"true","interactive":"true"}
docker exec oai-ext-dn iperf3 -c $UE_IP -p 5070 -t 10
echo
```

You should see the throughput capped roughly in the **5-10 Mbps** range.

<details>
<summary><b>Automated Validation Script (for CI/Testing)</b></summary>

The following script automatically validates that bandwidth is being rate-limited. This runs in CI but can also be executed manually:

```bash {"name":"run-iperf3-client","interactive":"false"}
sleep 2

# Run iperf3 client and capture output
IPERF_OUTPUT=$(docker exec oai-ext-dn iperf3 -c $UE_IP -p 5070 -t 10)

echo "$IPERF_OUTPUT"

# Extract receiver bitrate (Mbits/sec) from the summary line
BITRATE_MBPS=$(echo "$IPERF_OUTPUT" | awk '/receiver/{print $7}')

if [ -z "$BITRATE_MBPS" ]; then
  echo "ERROR: Failed to extract bitrate from iperf3 output"
  exit 1
fi

echo "Measured receiver bitrate: ${BITRATE_MBPS} Mbps"

# Premium profile: GBR=5Mbps, MBR=10Mbps
# Allow a tolerance range from MIN_MBPS to MAX_MBPS to account for encapsulation overhead
# The key assertion is that traffic IS being rate-limited (not running at full link speed)
PASS=$(echo "$BITRATE_MBPS >= $MIN_MBPS && $BITRATE_MBPS <= $MAX_MBPS" | bc)
if [ "$PASS" -eq 1 ]; then
  echo "PASS: Bitrate ${BITRATE_MBPS} Mbps is within expected range (${MIN_MBPS}-${MAX_MBPS} Mbps)"
else
  echo "FAIL: Bitrate ${BITRATE_MBPS} Mbps is outside expected range (${MIN_MBPS}-${MAX_MBPS} Mbps)"
  exit 1
fi
```

</details>

Stop the server after the test:

```bash {"name":"stop-iperf3-server","interactive":"false"}
docker exec ue pkill iperf3 || true
```

## Step 6: Collect Logs

```bash {"name":"collect-logs","interactive":"false"}
# Each `runme run <name>` invocation starts a fresh shell, so setup-variables'
# exports aren't inherited here — re-apply the same defaults.
export LOGS_DIR="${LOGS_DIR:-/tmp/phine.af/qos-grpc-tutorial/logs}"

./build/scripts/ci_helper.sh collect_logs $LOGS_DIR
echo "Logs collected to $LOGS_DIR"
ls -la $LOGS_DIR
```

## Cleanup

Stop and remove all containers:

```bash {"name":"cleanup","interactive":"false"}
# Each `runme run <name>` invocation starts a fresh shell, so setup-variables'
# exports aren't inherited here — re-apply the same defaults.
export COMPOSE_FILE="${COMPOSE_FILE:-docker-compose/compose.yaml}"
export COMPOSE_OVERRIDE_FILE="${COMPOSE_OVERRIDE_FILE:-docker-compose/compose.grpc.yaml}"
export AF_PROFILE="${AF_PROFILE:-af}"
export COMPOSE_PROFILES="${COMPOSE_PROFILES:---profile free5gc --profile $AF_PROFILE}"
export RAN_SERVICES="${RAN_SERVICES:-ueransim-gnb ueransim-ue}"

docker compose -f $COMPOSE_FILE down $RAN_SERVICES

docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES down

docker compose -f $COMPOSE_FILE -f $COMPOSE_OVERRIDE_FILE --profile af-client down
```

To also remove built images:

```bash {"name":"cleanup-all","excludeFromRunAll":"true","interactive":"false"}
export COMPOSE_FILE="${COMPOSE_FILE:-docker-compose/compose.yaml}"
export AF_PROFILE="${AF_PROFILE:-af}"
export COMPOSE_PROFILES="${COMPOSE_PROFILES:---profile free5gc --profile $AF_PROFILE}"

docker compose -f $COMPOSE_FILE $COMPOSE_PROFILES down --rmi all
```

## Next Steps

- Exercise the full session lifecycle (retrieve, extend, delete) over HTTP: [HTTP QoS Enforcement Tutorial](06-qos-http-tutorial.md)
- Run the automated adapter demo, which drives the same gRPC interface programmatically: [Demo QoD Adapter Tutorial](08-demo-adapter-tutorial.md)
- Review the [Architecture Overview](../architecture/overview.md) for more detail on AF internals
