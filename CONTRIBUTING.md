# Contributing to phine.af

Thanks for your interest in improving phine.af!

For complete instructions on setting up your local environment, running code formatters, executing tests, and understanding CI pipelines, read our **[Contributor Guide](docs/contributing.md)** (also published at **[af.phine.tech/contributing/](https://af.phine.tech/contributing/)**).

---

### 1. Developer Certificate of Origin (DCO) Sign-Off
We require a **DCO sign-off** on every commit. Add `-s` (or `--signoff`) to your commit command:

```bash
git commit -s -m "feat(pcf): add policy rule"
```

This appends `Signed-off-by: Your Name <your.email@example.com>` to the commit message.

### 2. Start PRs as Drafts
Always open Pull Requests as **Drafts**. Draft PRs run fast-tier CI checks (format, unit tests, clang-tidy) while you iterate.

### 3. Run Pre-Push Checks
Before marking a PR as **Ready for review**, run local formatting, static analysis, and test suites:

```bash
.github/scripts/pre-push.sh --fix
```

---

## Reporting Issues

Please use the provided GitHub issue templates:
* **Bug report:** Something broken or misbehaving.
* **Feature Implementation:** New capabilities using **shall / will / should** requirements.
* **Task Scope:** Smaller, well-scoped work items.

---

## Governance & Licensing

* **Code of Conduct:** Participation in this project is governed by our [Code of Conduct](CODE_OF_CONDUCT.md). Reports: `<tariro.mukute@phine.tech>`.
* **License:** By contributing, you agree that your contributions will be licensed under the [Apache License 2.0](LICENSE).