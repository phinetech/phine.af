---
name: Bug report
about: Report something broken or misbehaving in phine.af
title: ''
labels: bug
assignees: ''
---

**Describe the bug**
A clear and concise description of what the bug is.

**To Reproduce**
Steps to reproduce the behaviour:

1. Deployment used (bundled `af`, microservice, docker compose profile …)
2. Command / API call
3. What happened

**Expected behaviour**
A clear and concise description of what you expected to happen.

**Logs**
Paste relevant logs (`af_core`, `pcf_handler`, `api_component`, …). Trim
to the minimum needed to see the failure and use fenced code blocks. Or attach the logs as files.

```text
paste logs here
```

**Environment**

- phine.af version / commit: <!-- e.g. main @ abc1234 or v0.1.0 -->
- Deployment: <!-- bundled / microservice / docker compose profile -->
- 5G Core: <!-- free5gc x.y.z / OAI CN5G x.y.z / other -->
- OS: <!-- e.g. Ubuntu 22.04 -->
- Docker: <!-- docker --version -->
- Docker Compose: <!-- docker compose version -->

**Additional context**
Anything else that might help — related PRs/issues, upstream OAI/free5gc
config, network topology, screenshots.
