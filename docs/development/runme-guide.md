# Runme Guide

[Runme](https://runme.dev/) lets you execute the code blocks in our Markdown documentation — both interactively in VS Code and non-interactively via the CLI or GitHub Actions. This ensures that tutorials, runbooks, and setup guides stay validated and never go stale.

## Why We Use Runme

- **Living documentation** — code blocks in Markdown are the source of truth and are tested in CI
- **One workflow** — the same document works in VS Code notebooks, the terminal, and GitHub Actions
- **No duplication** — we don't maintain separate scripts alongside docs; the docs _are_ the scripts

## Installation

### VS Code Extension

Install the [Runme extension](https://marketplace.visualstudio.com/items?itemName=stateful.runme) from the VS Code marketplace. Once installed, `.md` files open as interactive notebooks where each fenced code block becomes a runnable cell.

### CLI

**Recommended (Homebrew - macOS):**

```bash {"name":"install-runme-brew","excludeFromRunAll":"true","interactive":"false"}
brew install runme
runme --version
```

**Linux (Debian/Ubuntu):**

```bash {"name":"install-runme-linux","interactive":"false"}
# Download and install .deb package
RUNME_VERSION="3.16.5"
wget https://downloads.runme.dev/runme/${RUNME_VERSION}/runme_linux_x86_64.deb
sudo dpkg -i runme_linux_x86_64.deb
runme --version
```

**Alternative (manual binary installation):**

```bash {"name":"install-runme-manual","excludeFromRunAll":"true","interactive":"false"}
# Download the binary directly
RUNME_VERSION="3.16.5"
wget https://downloads.runme.dev/runme/${RUNME_VERSION}/runme_linux_x86_64.tar.gz
tar -xzf runme_linux_x86_64.tar.gz
sudo mv runme /usr/local/bin/
runme --version
```

## How Code Blocks Are Annotated

We add runme attributes directly in the Markdown fenced code block header. The most common attributes:

| Attribute | Default | Purpose |
|---|---|---|
| `name` | auto-generated | Identifies the cell for `runme run <name>` |
| `interactive` | `true` | `false` = capture output inline (no TTY needed) |
| `background` | `false` | `true` = run as a background process |
| `excludeFromRunAll` | `false` | `true` = skip when running all cells |

### Syntax Example

````markdown
```bash {"name":"greet","interactive":"false"}
echo "Hello from phine.af"
```
````

Non-executable blocks (diagrams, sample output, config snippets) use `text` or `json` as the language identifier — runme will not try to execute them.

## Running Documentation Locally

### Run a Single Named Cell

```bash {"name":"example-list","interactive":"false"}
runme ls --filename docs/getting-started/06-qos-http-tutorial.md
```

To run a specific cell by name:

```sh {"excludeFromRunAll":"true","interactive":"false"}
runme run deploy-stack --filename docs/getting-started/06-qos-http-tutorial.md
```

### Run All Cells

Runs every cell except those marked `excludeFromRunAll`:

```sh {"excludeFromRunAll":"true","interactive":"false"}
runme run --all --skip-prompts --filename docs/getting-started/06-qos-http-tutorial.md
```

### Run in VS Code

1. Open the `.md` file — it automatically renders as a notebook
2. Click **Run All** in the toolbar, or run individual cells with the play button
3. Use the **Configure** gear icon on a cell to inspect or change its attributes

## Writing Runme-Compatible Documentation

Follow these conventions when adding or updating Markdown documentation in this project.

### 1. Name Every Executable Cell

Give each code block a descriptive, kebab-case `name`. This is the identifier used by the CLI and CI:

````markdown
```bash {"name":"build-phine.af-core","interactive":"false"}
docker compose build af_core
```
````

### 2. Mark Non-Interactive Commands

Commands that produce output and require no user input should set `interactive` to `false`. This lets runme capture the output and is required for CI:

````markdown
```bash {"name":"check-status","interactive":"false"}
docker ps --format "table {{.Names}}\t{{.Status}}"
```
````

### 3. Handle Wait Scenarios with Retry Loops

Do not use bare `sleep` for waiting on services. Use retry loops with a bounded number of attempts so the script self-terminates on failure:

````markdown
```bash {"name":"wait-for-service","interactive":"false"}
retries=30
for i in $(seq 1 $retries); do
  if curl -sf http://localhost:8080/health; then
    echo "Service is ready"
    break
  fi
  echo "Waiting... ($i/$retries)"
  sleep 2
done
curl -sf http://localhost:8080/health
```
````

The final command outside the loop acts as an assertion — if the service never became ready, it will fail the cell.

### 4. Exclude Optional / Interactive Cells

Cells that are interactive (e.g., `tshark` capture), optional (e.g., baseline comparison), or destructive (e.g., `--rmi all`) should be excluded from automated runs:

````markdown
```bash {"name":"start-tshark","background":"true","excludeFromRunAll":"true","interactive":"true"}
sudo tshark -i demo-oai ...
```
````

### 5. Use Background for Long-Running Processes

Servers or watchers that should run in the background:

````markdown
```bash {"name":"start-server","background":"true"}
docker compose up -d
```
````

### 6. Use `text` for Non-Executable Blocks

Diagrams, example output, and configuration snippets should use the `text` language identifier to prevent runme from treating them as executable:

````markdown
```text
┌──────────┐     ┌──────────┐
│  Client  │────▶│  Server  │
└──────────┘     └──────────┘
```
````

### 7. Hide Complex CI Scripts in Collapsible Sections

When you need both a simple command for users and a complex validation script for CI, use this pattern:

1. **Show a simple, user-friendly command** with `excludeFromRunAll:"true"` and `interactive:"true"`
2. **Hide the CI validation script** in a `<details>` collapsible section with a proper `name` attribute

**Example:**

````markdown
Run the iperf3 client to measure bandwidth:

```bash {"excludeFromRunAll":"true","interactive":"true"}
docker exec oai-ext-dn iperf3 -c 10.60.0.1 -p 5070 -t 10
```

Observe the throughput in the output. With QoS enforcement active, you should see the bitrate capped around 5-10 Mbps.

<details>
<summary><b>Automated Validation Script (for CI/Testing)</b></summary>

The following script automatically validates that bandwidth is being rate-limited. This runs in CI but can also be executed manually:

```bash {"name":"run-iperf3-client","interactive":"false"}
IPERF_OUTPUT=$(docker exec oai-ext-dn iperf3 -c 10.60.0.1 -p 5070 -t 10 --json)
BITRATE_BPS=$(echo "$IPERF_OUTPUT" | jq -r '.end.sum_received.bits_per_second')
BITRATE_MBPS=$(echo "scale=2; $BITRATE_BPS / 1000000" | bc)

if (( $(echo "$BITRATE_MBPS < 3 || $BITRATE_MBPS > 12" | bc -l) )); then
  echo "FAIL: Bitrate ${BITRATE_MBPS} Mbps outside expected range"
  exit 1
fi
echo "PASS: Bitrate ${BITRATE_MBPS} Mbps within expected range"
```

</details>
````

**Benefits:**

- **User experience**: Users see clean, readable commands they can copy/paste
- **CI automation**: Named cell inside `<details>` is still discoverable and executable by runme
- **Documentation clarity**: Complex parsing logic doesn't clutter the tutorial narrative
- **Flexibility**: Users can expand the collapsible if they want to understand the validation logic

**When to use this pattern:**

- Scripts with multiple error handling branches
- Complex assertions that are important for CI but distracting in tutorials

## CI Integration

We use the Runme CLI directly in GitHub Actions to execute named cells from tutorial documents. Each named cell becomes a workflow step.

### Installing Runme in CI

First, install the Runme CLI in your workflow:

```yaml {"excludeFromRunAll":"true"}
- name: Install Runme CLI
  run: |
    RUNME_VERSION="3.16.5"
    wget https://downloads.runme.dev/runme/${RUNME_VERSION}/runme_linux_x86_64.deb
    sudo dpkg -i runme_linux_x86_64.deb
    runme --version
```

### Workflow Structure

Execute named cells using `runme run`:

```yaml {"excludeFromRunAll":"true"}
- name: Run tutorial step
  run: runme run deploy-stack --filename docs/getting-started/06-qos-http-tutorial.md
```

Multiple steps can be sequenced:

```yaml {"excludeFromRunAll":"true"}
- name: Deploy
  run: runme run deploy-stack --filename docs/getting-started/06-qos-http-tutorial.md

- name: Verify
  run: runme run verify-ue-ip --filename docs/getting-started/06-qos-http-tutorial.md

- name: Cleanup
  if: always()
  run: runme run cleanup --filename docs/getting-started/06-qos-http-tutorial.md
  continue-on-error: true
```

### Best Practices for CI

- **Always use `--filename`**: Explicitly specify the markdown file path
- **Use `if: always()`**: For cleanup steps that should run even if previous steps fail
- **Add `continue-on-error: true`**: For optional cleanup steps that shouldn't fail the workflow
- **Sequential execution**: Run cells in order; runme respects the document's working directory from frontmatter

### Existing CI Workflows

| Workflow | File | What It Validates |
|---|---|---|
| Tutorials Validation | `.github/workflows/test-tutorials.yml` | All runme-enabled getting-started tutorials |
| Integration Tests | `.github/workflows/integration-tests.yml` | AF Core integration test suite |
| Build | `.github/workflows/build.yml` | Compilation and unit tests |

## Runme-Enabled Documentation

| Document | Description |
|---|---|
| [QoS Enforcement Tutorial (HTTP)](../getting-started/06-qos-http-tutorial.md) | Full end-to-end 5G QoS tutorial with iperf3 verification |
| [QoS Enforcement Tutorial (gRPC)](../getting-started/07-qos-grpc-tutorial.md) | Same tutorial over the gRPC northbound interface |
| [Demo QoD Adapter Tutorial](../getting-started/08-demo-adapter-tutorial.md) | Automated adapter-driven QoD session lifecycle |

## Additional Resources

- [Runme documentation](https://docs.runme.dev/)
- [Runme CLI reference](https://docs.runme.dev/getting-started/cli)
- [Cell-level configuration](https://docs.runme.dev/configuration/cell-level)
- [Runme installation guide](https://docs.runme.dev/installation)
