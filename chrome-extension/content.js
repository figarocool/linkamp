(() => {
  const BUTTON_ID = "linkamp-youtube-add-button";
  let retryTimer = null;

  function isVideoPage() {
    const path = window.location.pathname;
    const query = new URLSearchParams(window.location.search);
    return (path === "/watch" && query.has("v")) || path.startsWith("/shorts/");
  }

  function findLikeControl(container) {
    const segmented = container.querySelector("ytd-segmented-like-dislike-button-renderer");
    if (segmented) {
      return segmented;
    }

    const candidates = container.querySelectorAll(
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

  function findLikePlacement() {
    const metadata = document.querySelector("ytd-watch-metadata");
    const actionRows = metadata
      ? metadata.querySelectorAll("#top-level-buttons-computed")
      : [];
    for (const actionRow of actionRows) {
      const likeControl = findLikeControl(actionRow) || actionRow.firstElementChild;
      if (likeControl) {
        return { actionRow, likeControl };
      }
    }

    const actionContainer = metadata?.querySelector("#actions-inner, #actions, ytd-menu-renderer");
    if (actionContainer) {
      const actionRow = actionContainer.querySelector("#top-level-buttons-computed") || actionContainer;
      const likeControl = findLikeControl(actionContainer) || actionRow.firstElementChild;
      if (likeControl) {
        return { actionRow, likeControl };
      }
    }

    for (const row of document.querySelectorAll("#top-level-buttons-computed")) {
      const likeControl = findLikeControl(row);
      if (likeControl) {
        return { actionRow: row, likeControl };
      }
    }
    return null;
  }

  function ensureButton() {
    let button = document.getElementById(BUTTON_ID);
    if (!isVideoPage()) {
      button?.remove();
      if (retryTimer) {
        clearTimeout(retryTimer);
        retryTimer = null;
      }
      return;
    }

    const placement = findLikePlacement();
    if (!placement) {
      button?.remove();
      if (!retryTimer) {
        retryTimer = setTimeout(() => {
          retryTimer = null;
          ensureButton();
        }, 500);
      }
      return;
    }
    if (retryTimer) {
      clearTimeout(retryTimer);
      retryTimer = null;
    }

    if (!button) {
      button = document.createElement("button");
      button.id = BUTTON_ID;
      button.type = "button";
      button.addEventListener("click", () => {
        const appUrl = `linkamp://add?url=${encodeURIComponent(window.location.href)}`;
        window.location.href = appUrl;
      });
    }

    button.textContent = "♫  Linkamp";
    button.title = "Aggiungi questo video alla playlist Linkamp (solo audio in streaming)";
    button.setAttribute("aria-label", "Aggiungi a Linkamp");
    button.style.cssText = [
      "appearance:none",
      "display:inline-flex",
      "align-items:center",
      "justify-content:center",
      "height:36px",
      "flex-shrink:0",
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

    const { actionRow, likeControl } = placement;
    let rowControl = likeControl;
    while (rowControl.parentElement && rowControl.parentElement !== actionRow) {
      rowControl = rowControl.parentElement;
    }
    if (rowControl.parentElement !== actionRow) {
      button.remove();
      return;
    }
    if (button.parentElement !== actionRow || button.previousElementSibling !== rowControl) {
      rowControl.insertAdjacentElement("afterend", button);
    }
    button.hidden = false;
  }

  ensureButton();
  document.addEventListener("yt-navigate-finish", () => {
    if (retryTimer) {
      clearTimeout(retryTimer);
      retryTimer = null;
    }
    ensureButton();
  });
  new MutationObserver(ensureButton).observe(document.documentElement, {
    childList: true,
    subtree: true
  });
})();
