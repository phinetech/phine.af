// Initialize Mermaid for MkDocs pages.
// This assumes Mermaid is loaded via `extra_javascript` before this file.

(function () {
  function init() {
    if (!window.mermaid) return;

    // Mermaid v10 supports `startOnLoad`.
    window.mermaid.initialize({ startOnLoad: true });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
