---
runme:
  id: adapter-qod-tutorial
  version: v3
cwd: ../..
---

# Demo QoD Adapter Tutorial

This tutorial walks you through deploying the demo QoD adapter alongside the full 5G testbed. The adapter acts as an **Application Management** component for a ROS2-over-5G deployment, automatically requesting and managing CAMARA Quality-on-Demand sessions for different traffic types through phine.af.

> **Runme compatible**: This tutorial is designed to run with [Runme](https://runme.dev/) — both interactively in VS Code and non-interactively in CI via the Runme CLI. See the [Runme Guide](../development/runme-guide.md) for details.

By the end of this tutorial you will have:

1. Deployed a complete 5G core (free5GC), RAN simulator (UERANSIM), and the phine.af Application Function
2. Built and run the demo QoD adapter as a containerised application
3. Observed the adapter automatically creating QoD sessions for two traffic streams (video and WebRTC)
4. Monitored session status transitions from `REQUESTED` → `AVAILABLE`
5. Watched the adapter gracefully clean up all sessions on shutdown

## Prerequisites

| Requirement | Notes |
|---|---|
| Docker Engine ≥ 24.0 | With Docker Compose v2 plugin |
| Linux host | Network interface creation requires Linux kernel capabilities |
| `gtp5g` kernel module | Required by the UPF — see [prerequisites](prerequisites.md) |
| ~8 GB free RAM | The full 5G core + RAN + AF + adapter stack runs ~16 containers |
| Ports available | No services bound to `192.168.70.128/26`, `192.168.71.128/26`, or `192.168.72.128/26` subnets |

## Architecture

The adapter connects to af_core via gRPC and sends QoD session requests using the `InternalCommunication.SendMessage` RPC. Each request carries a JSON payload describing the CAMARA QoD session parameters.

```text
┌───────────────────────────────────────────────────────────────────────────┐
│                          Demo QoD Adapter (.70.143)                      │
│                                                                          │
│   config.yaml                                                            │
│   ┌─────────────────────────────────────────────────────────────────┐    │
│   │  video_stream     QOS_L   port 8554   "HD camera feed"         │    │
│   │  webrtc_control   QOS_E   port 8443   "Teleoperation commands" │    │
│   └─────────────────────────────────────────────────────────────────┘    │
│                          │                                               │
│               SessionManager                                             │
│          create → monitor → cleanup                                      │
│                          │                                               │
│                     QodClient                                            │
│                          │ gRPC (InternalMessage)                        │
└──────────────────────────┼───────────────────────────────────────────────┘
                           ▼
                    ┌──────────────┐     gRPC      ┌──────────────┐
                    │   AF Core    │──────────────▶│ PCF Handler  │
                    │   .70.141    │               │   .70.140    │
                    └──────────────┘               └──────┬───────┘
                                                          │ HTTP
                                                          ▼
                                                   ┌──────────────┐
                                                   │   PCF        │
                                                   │   .70.139    │
                                                   └──────┬───────┘
                                                          │ N7
                                                          ▼
                                                   ┌──────────────┐
                                                   │   SMF        │
                                                   │   .70.133    │
                                                   └──────┬───────┘
                                                          │ N4 (PFCP)
                                                          ▼
                   ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
                   │    UE    │────▶│   gNB    │────▶│   UPF    │────▶│  Ext DN  │
                   │ 10.60.0.1│     │ UERANSIM │     │ (OAI)    │     │          │
                   └──────────┘     └──────────┘     └──────────┘     └──────────┘
```

**Key containers**:

| Container | IP Address | Role |
|---|---|---|
| `demo-qod-adapter` | 192.168.70.143 | Adapter — creates and manages QoD sessions |
| `af-core` | 192.168.70.141 | AF Core — receives and routes QoD requests |
| `af-pcf-handler` | 192.168.70.140 | Southbound handler — translates to PCF API |
| `pcf` | 192.168.70.139 | free5GC PCF — policy control function |
| `smf` | 192.168.70.133 | free5GC SMF — session management |
| `upf` | host network | OAI UPF — user plane enforcement |
| `ue` | 192.168.70.181 (ctrl) / 10.60.0.1 (data) | UERANSIM UE |

## Configuration

Set up environment variables for this tutorial:

```bash {"name":"setup-variables","interactive":"false"}
export COMPOSE_FILE="docker-compose/docker-compose-test.yaml"
export LOGS_DIR="/tmp/phine.af/adapter-qod-tutorial/logs"
mkdir -p "$LOGS_DIR"
sudo mkdir -p "$LOGS_DIR"
sudo chmod 777 "$LOGS_DIR"

echo "Configuration set:"
echo "  COMPOSE_FILE: $COMPOSE_FILE"
echo "  CAPTURE_DIR: $LOGS_DIR"
```

## Step 1: Deploy the 5G Core and AF Stack

Install required dependencies and build the gtp5g kernel module:

```bash {"name":"install-deps","interactive":"false"}
./build/scripts/ci_helper.sh install_dependencies
./build/scripts/ci_helper.sh install_gtp5g
```

Ensure submodules are up to date
```bash {"name":"check-submodules","interactive":"false"}
./build/scripts/ci_helper.sh check_submodules
```

Build the adapter and AF images:

```bash {"name":"build-images","interactive":"false"}
docker compose -f $COMPOSE_FILE build af_core pcf_handler demo-qod-adapter
```

Start the 5G core infrastructure:

```bash {"name":"start-infrastructure","interactive":"false"}
docker compose -f $COMPOSE_FILE up -d \
  db free5gc-nrf free5gc-amf free5gc-ausf free5gc-nssf \
  free5gc-pcf free5gc-smf free5gc-udm free5gc-udr \
  free5gc-upf free5gc-webui oai-ext-dn
```

Wait for the NRF to become ready:

```bash {"name":"wait-for-nrf","interactive":"false"}
# Wait for NRF to be ready
sleep 30
```

Start the RAN simulator (gNB and UE):

```bash {"name":"start-ran","interactive":"false"}
docker compose -f $COMPOSE_FILE up -d gnb ue
echo "Waiting for UE to establish connection (20s)..."
sleep 20
```

Verify the UE has registered and obtained an IP address:

```bash {"name":"verify-ue","interactive":"false"}
docker exec ue ip addr show uesimtun0 | grep "10.60.0.1"
```

Start the AF core and PCF handler:

```bash {"name":"start-af","interactive":"false"}
docker compose -f $COMPOSE_FILE up -d af_core pcf_handler
```

Wait for the AF Core gRPC server to become ready:

```bash {"name":"wait-for-af","interactive":"false"}
# Wait for af-core to be ready
sleep 15
```

At this point the full stack is running and ready for the adapter.

## Step 2: Start Traffic Capture

Start capturing control-plane traffic to observe the QoD signalling flow:

```bash {"name":"start-capture","interactive":"false"}
PCAP_FILE="$LOGS_DIR/adapter_capture.pcapng"
PID_FILE="$LOGS_DIR/adapter_capture.pid"
LOG_FILE="$LOGS_DIR/adapter_capture.tshark.log"

sudo nohup tshark -i demo-oai \
  -f "host 192.168.70.143 or host 192.168.70.141 or host 192.168.70.140 or host 192.168.70.139 or host 192.168.70.133" \
  -w "$PCAP_FILE" \
  >"$LOG_FILE" 2>&1 &
echo $! > "$PID_FILE"

echo "Traffic capture started in background (PID: $(cat "$PID_FILE" 2>/dev/null || echo unknown))"
sleep 2
```

This captures:
- **gRPC/HTTP2** traffic between adapter (.143) and af_core (.141)
- **HTTP** traffic between PCF handler (.140) and PCF (.139)
- **PFCP** traffic between SMF (.133) and UPF

## Step 3: Review the Adapter Configuration

The adapter ships with a default configuration at `adapters/demo-qod-adapter/config.yaml`. For Docker deployments, an override is mounted from `docker-compose/conf/adapter_config.yaml`:

```bash {"name":"show-config","interactive":"false"}
cat docker-compose/conf/adapter_config.yaml
```

Key configuration fields:

| Field | Value | Purpose |
|---|---|---|
| `af_core.address` | `192.168.70.141:50051` | AF Core gRPC endpoint |
| `monitor.interval_seconds` | `5` | Seconds between status checks |
| `monitor.iterations` | `3` | Number of monitoring cycles (`-1` for indefinite) |
| `streams[0].qos_profile` | `QOS_L` | Low-latency profile for video |
| `streams[1].qos_profile` | `QOS_E` | Ultra-low-latency profile for WebRTC |

Each stream entry maps to a CAMARA QoD `createSession` request. The adapter translates YAML fields into the JSON payload expected by the `InternalCommunication.SendMessage` RPC.

### Customising the Configuration

To modify streams, edit the mounted config file before starting the adapter:

```bash {"name":"edit-config","excludeFromRunAll":"true","interactive":"true"}
vi docker-compose/conf/adapter_config.yaml
```

For example, to run indefinitely until stopped with `docker compose stop`:

```yaml
monitor:
  interval_seconds: 10
  iterations: -1  # Run until SIGINT/SIGTERM
```

## Step 4: Run the Adapter

Start the adapter container. It connects to af_core, creates QoD sessions, monitors them, and exits after the configured number of iterations:

```bash {"name":"run-adapter","interactive":"false"}
docker compose -f $COMPOSE_FILE up --exit-code-from demo-qod-adapter demo-qod-adapter
```

### Expected Output

You should see output similar to this:

```text
demo-qod-adapter  | [2026-02-16 17:01:45.569] [] [info] Demo QoD Adapter starting — config: /app/config.yaml
demo-qod-adapter  | [2026-02-16 17:01:45.569] [] [info] Loaded 2 stream definition(s)
demo-qod-adapter  | [2026-02-16 17:01:45.570] [] [info] [QodClient] Created — target: 192.168.70.141:50051
demo-qod-adapter  | [2026-02-16 17:01:45.570] [] [info] [QodClient] Waiting for af_core at 192.168.70.141:50051 (timeout 30s)…
demo-qod-adapter  | [2026-02-16 17:01:45.572] [] [info] [QodClient] Channel READY
demo-qod-adapter  | [2026-02-16 17:01:45.572] [] [info] [SessionManager] Initialised
demo-qod-adapter  | [2026-02-16 17:01:45.572] [] [info] [SessionManager] Creating 2 session(s)…
demo-qod-adapter  | [2026-02-16 17:01:45.572] [] [info] [SessionManager] Requesting QoS for 'video_stream' (profile=QOS_L, duration=300s)
demo-qod-adapter  | [2026-02-16 17:01:45.598] [] [info] [SessionManager] Session created for 'video_stream': id=01c6135b-..., status=REQUESTED
demo-qod-adapter  | [2026-02-16 17:01:45.598] [] [info] [SessionManager] Requesting QoS for 'webrtc_control' (profile=QOS_E, duration=300s)
demo-qod-adapter  | [2026-02-16 17:01:45.621] [] [info] [SessionManager] Session created for 'webrtc_control': id=09978f8b-..., status=REQUESTED
demo-qod-adapter  | [2026-02-16 17:01:45.621] [] [info] [SessionManager] Created 2/2 session(s) successfully
demo-qod-adapter  | [2026-02-16 17:01:50.621] [] [info] [SessionManager] Monitoring 2 session(s)…
demo-qod-adapter  | [2026-02-16 17:01:50.622] [] [info] [SessionManager] Session 01c6135b-... ('video_stream') status: REQUESTED → AVAILABLE
demo-qod-adapter  | [2026-02-16 17:01:50.623] [] [info] [SessionManager] Session 09978f8b-... ('webrtc_control') status: REQUESTED → AVAILABLE
demo-qod-adapter  | [2026-02-16 17:01:55.623] [] [info] [SessionManager] Monitoring 2 session(s)…
demo-qod-adapter  | [2026-02-16 17:02:00.624] [] [info] [SessionManager] Monitoring 2 session(s)…
demo-qod-adapter  | [2026-02-16 17:02:00.628] [] [info] Shutting down — cleaning up sessions…
demo-qod-adapter  | [2026-02-16 17:02:00.628] [] [info] [SessionManager] Cleaning up 2 session(s)…
demo-qod-adapter  | [2026-02-16 17:02:00.628] [] [info] [SessionManager] Deleting session 01c6135b-... ('video_stream')
demo-qod-adapter  | [2026-02-16 17:02:00.651] [] [info] [SessionManager] Deleting session 09978f8b-... ('webrtc_control')
demo-qod-adapter  | [2026-02-16 17:02:00.673] [] [info] [SessionManager] Cleanup complete: 2/2 deleted
demo-qod-adapter  | [2026-02-16 17:02:00.673] [] [info] Demo QoD Adapter finished.
demo-qod-adapter exited with code 0
```

### Understanding the Output

The adapter follows a five-phase lifecycle:

| Phase | Log Indicators | What Happens |
|---|---|---|
| **1. Connect** | `Channel READY` | gRPC channel to af_core established |
| **2. Create** | `Session created for '...'` | `qod_create_session` sent for each stream |
| **3. Monitor** | `Monitoring N session(s)…` | Periodic `qod_get_session` polls for status changes |
| **4. Transition** | `REQUESTED → AVAILABLE` | PCF confirmed the QoS policy is active |
| **5. Cleanup** | `Cleanup complete: N/N deleted` | `qod_delete_session` sent for every tracked session |

The adapter exits with code **0** on success. A non-zero exit code indicates a failure (e.g., af_core unreachable, no streams configured).

Stop the traffic capture:

```bash {"name":"stop-capture","interactive":"false"}
PCAP_FILE="$LOGS_DIR/adapter_capture.pcapng"
PID_FILE="$LOGS_DIR/adapter_capture.pid"

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

## Step 5: Analyze the Signalling Path

Review the captured traffic to trace the QoD signalling flow:

```bash {"name":"view-capture","interactive":"false","excludeFromRunAll":"true"}
PCAP_FILE="$LOGS_DIR/adapter_capture.pcapng"
# Open file with wireshark
wireshark $PCAP_FILE
```

### Expected Signalling Sequence

When the adapter creates a session, the following chain of events occurs:

```text
demo-qod-adapter (.143)
       │
       │  gRPC: InternalCommunication.SendMessage
       │  message_type: "qod_create_session"
       │  payload: { device, applicationServer, qosProfile, ... }
       ▼
  AF Core (.141)
       │
       │  Routes request to QoD handler → PCF Handler
       ▼
  PCF Handler (.140)
       │
       │  HTTP POST /npcf-policyauthorization/v1/app-sessions
       ▼
    PCF (.139)
       │
       │  N7: Policy update to SMF
       ▼
    SMF (.133)
       │
       │  PFCP Session Modification Request
       ▼
    UPF (host)
       │
       │  Installs QoS Enforcement Rule (QER)
       │  PFCP Session Modification Response
       ▼
    SMF (.133)
```

This sequence repeats for each stream defined in the configuration. When the adapter cleans up, the same chain runs in reverse with `qod_delete_session` messages.

### What to Look For in the Capture

Key indicators:
- **gRPC messages** with `qod_create_session` / `qod_get_session` / `qod_delete_session`
- **HTTP POST** to `/npcf-policyauthorization/v1/app-sessions`
- **HTTP POST** to SM Policy Update Notification Callback `/<callback_url>/update` i.e., for free5gc `/nsmf-callback/sm-policies/<xxxx>/update`
- **PFCP Session Modification** requests/responses

## Step 6: Collect Logs

After the adapter run, collect logs from all containers for analysis:

```bash {"name":"collect-logs","interactive":"false"}
./build/scripts/ci_helper.sh collect_logs $LOGS_DIR
echo "Logs collected to $LOGS_DIR"
ls -la $LOGS_DIR
```

View the adapter log:

```bash {"name":"view-adapter-log","interactive":"false"}
cat $LOGS_DIR/demo_qod_adapter.log
```

View the AF Core log to see how it processed the adapter's requests:

```bash {"name":"view-afcore-log","excludeFromRunAll":"true","interactive":"false"}
cat $LOGS_DIR/af_core.log | tail -50
```

## Step 7: Run with Indefinite Monitoring

To keep the adapter running continuously (e.g., for a long-running demo or integration with real ROS2 nodes), update the config to use indefinite monitoring:

```bash {"name":"configure-indefinite","excludeFromRunAll":"true","interactive":"false"}
cat > /tmp/adapter_config_indefinite.yaml << 'EOF'
af_core:
  address: "192.168.70.141:50051"
  timeout_seconds: 30

retry:
  max_retries: 3
  initial_delay_ms: 1000

monitor:
  interval_seconds: 10
  iterations: -1  # Run indefinitely until SIGINT/SIGTERM

streams:
  - name: "video_stream"
    description: "HD camera feed for remote monitoring/processing"
    device:
      ipv4_address:
        public_address: "10.60.0.1"
        public_port: 8554
    application_server:
      ipv4_address: "0.0.0.0/0"
    device_ports:
      ports: [8554]
    qos_profile: "QOS_L"
    duration_seconds: 3600

  - name: "webrtc_control"
    description: "Real-time teleoperation commands and status"
    device:
      ipv4_address:
        public_address: "10.60.0.1"
        public_port: 8443
    application_server:
      ipv4_address: "0.0.0.0/0"
    device_ports:
      ports: [8443]
    qos_profile: "QOS_E"
    duration_seconds: 3600
EOF
```

Run the adapter in the background with the indefinite config:

```bash {"name":"run-adapter-indefinite","excludeFromRunAll":"true","background":"true","interactive":"false"}
docker compose -f $COMPOSE_FILE run --rm \
  -v /tmp/adapter_config_indefinite.yaml:/app/config.yaml:ro \
  demo-qod-adapter
```

The adapter will keep monitoring sessions until you stop it:

```bash {"name":"stop-adapter","excludeFromRunAll":"true","interactive":"false"}
docker compose -f $COMPOSE_FILE stop demo-qod-adapter
```

When stopped, the adapter catches SIGTERM, cleans up all sessions, and exits gracefully.

## Cleanup

Ensure traffic capture is stopped:

```bash {"name":"ensure-capture-stopped","interactive":"false"}
PCAP_FILE="$LOGS_DIR/adapter_capture.pcapng"
PID_FILE="$LOGS_DIR/adapter_capture.pid"

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
echo "All tshark processes stopped"
```

Stop and remove all containers:

```bash {"name":"cleanup","interactive":"false"}
docker compose -f $COMPOSE_FILE down
```

To also remove built images:

```bash {"name":"cleanup-all","excludeFromRunAll":"true","interactive":"false"}
docker compose -f $COMPOSE_FILE down --rmi all
```

## CI Quick Run

The entire tutorial can be run non-interactively via the CI helper script:

```bash {"name":"ci-run","excludeFromRunAll":"true","interactive":"false"}
./build/scripts/ci_helper.sh run_adapter_test
```

This builds all required images, deploys the full stack, runs the adapter, collects logs, and reports success or failure.

## Troubleshooting

### Adapter exits immediately with "af_core not reachable"

The af_core gRPC server isn't ready yet. Ensure the `wait-for-af` step completed successfully and that the `af-core` container is running:

```bash {"name":"check-af-core","excludeFromRunAll":"true","interactive":"false"}
docker ps --filter name=af-core --format "table {{.Names}}\t{{.Status}}"
grpcurl -plaintext 192.168.70.141:50051 list
```

### Sessions stay in REQUESTED state

This is expected if the PCF backend has not fully processed the policy. Check the PCF handler and PCF logs:

```bash {"name":"check-pcf-logs","excludeFromRunAll":"true","interactive":"false"}
docker logs af-pcf-handler --tail 20
docker logs pcf --tail 20
```

### Port conflict on Docker network

If containers fail to start with address-already-in-use errors, ensure no other Docker networks are using the `192.168.70.128/26` subnet:

```bash {"name":"check-networks","excludeFromRunAll":"true","interactive":"false"}
docker network ls
docker network inspect demo-oai-public-net 2>/dev/null || echo "Network not found"
```

## Next Steps

- Explore the adapter source code and design: [DESIGN.md](../../adapters/demo-qod-adapter/DESIGN.md)
- Try the end-to-end QoS enforcement tutorial with iperf3 verification: [QoS Enforcement Tutorial](qos-enforcement-tutorial.md)
- Learn how to add new CAMARA APIs: [Add a CAMARA API](../development/add-camara-api.md)
- Review the architecture: [Architecture Overview](../architecture/overview.md)
