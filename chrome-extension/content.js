(() => {
  const BUTTON_ID = "linkamp-youtube-add-button";

  function isVideoPage() {
    const path = window.location.pathname;
    const query = new URLSearchParams(window.location.search);
    return (path === "/watch" && query.has("v")) || path.startsWith("/shorts/");
  }

  function findLikeControl() {
    const segmented = document.querySelector("ytd-segmented-like-dislike-button-renderer");
    if (segmented) {
      return segmented;
    }

    const candidates = document.querySelectorAll(
      "ytd-toggle-button-renderer, button[aria-label], yt-button-view-model"
    );
    for (const candidate of candidates) {
      const label = [
        candidate.getAttribute("aria-label"),
        candidate.querySelector("button")?.getAttribute("aria-label")
      ].filter(Boolean).join(" ").toLowerCase();
      if (/(^|\s)like(\s|$)|mi piace/.test(label)) {
        return candidate;
      }
    }
    return null;
  }

  function ensureButton() {
    let button = document.getElementById(BUTTON_ID);
    if (!isVideoPage()) {
      button?.remove();
      return;
    }

    const likeControl = findLikeControl();
    if (!likeControl) {
      button?.remove();
      return;
    }

    if (!button) {
      button = document.createElement("button");
      button.id = BUTTON_ID;
      button.type = "button";
      button.textContent = "♫  Linkamp";
      button.title = "Aggiungi questo video alla playlist Linkamp (solo audio in streaming)";
      button.setAttribute("aria-label", "Aggiungi a Linkamp");
      button.style.cssText = [
        "appearance:none",
        "display:inline-flex",
        "align-items:center",
        "justify-content:center",
        "height:36px",
        "margin-left:8px",
        "padding:0 14px",
        "border:0",
        "border-radius:18px",
        "background:#c4302b",
        "color:#fff",
        "font:500 14px Roboto,Arial,sans-serif",
        "white-space:nowrap",
        "cursor:pointer"
      ].join(";");
      button.addEventListener("click", () => {
        const appUrl = `linkamp://add?url=${encodeURIComponent(window.location.href)}`;
        window.location.href = appUrl;
      });
    }

    const row = likeControl.closest("#top-level-buttons-computed")
      || likeControl.closest("ytd-menu-renderer")?.querySelector("#top-level-buttons-computed")
      || likeControl.parentElement;
    if (!row) {
      button.remove();
      return;
    }

    let rowControl = likeControl;
    while (rowControl.parentElement && rowControl.parentElement !== row) {
      rowControl = rowControl.parentElement;
    }
    if (rowControl.parentElement !== row) {
      button.remove();
      return;
    }
    if (button.parentElement !== row || button.previousElementSibling !== rowControl) {
      rowControl.insertAdjacentElement("afterend", button);
    }
    button.hidden = false;
  }

  ensureButton();
  document.addEventListener("yt-navigate-finish", ensureButton);
  new MutationObserver(ensureButton).observe(document.documentElement, {
    childList: true,
    subtree: true
  });
})();
