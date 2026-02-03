# Documentation (MkDocs)

This repository includes an MkDocs configuration at `mkdocs.yml` and Markdown content under `docs/`.

## Serve locally

```bash
pip install mkdocs pymdown-extensions
mkdocs serve
```

Open the URL printed by MkDocs (typically `http://127.0.0.1:8000/`).

## Build static site

```bash
mkdocs build
```

The generated site is written to `site/` by default.

## Mermaid diagrams

MkDocs does not render Mermaid diagrams by default. This repo enables Mermaid rendering via `pymdown-extensions` (configured in `mkdocs.yml`).

You can write Mermaid diagrams in Markdown using fenced blocks like:

```text
```mermaid
graph TD
	A --> B
```
```

Mermaid diagrams are kept as single-source files under `docs/diagrams/` and synced into Markdown pages.

Edit:

- `docs/diagrams/architecture.mmd`

Then run:

```bash
chmod +x build/scripts/sync_diagrams.sh
./build/scripts/sync_diagrams.sh
```
