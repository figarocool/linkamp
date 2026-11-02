(() => {
  const BUTTON_ID = "linkamp-youtube-add-button";

  function isVideoPage() {
    const path = window.location.pathname;
    const query = new URLSearchParams(window.location.search);
    return (path === "/watch" && query.has("v")) || path.startsWith("/shorts/");
  }

  function ensureButton() {
    let button = document.getElementById(BUTTON_ID);
    if (!button) {
      button = document.createElement("button");
      button.id = BUTTON_ID;
      button.type = "button";
      button.textContent = "♫  Aggiungi a Linkamp";
      button.title = "Aggiungi questo video alla playlist Linkamp (solo audio in streaming)";
      button.style.cssText = [
        "position:fixed",
        "top:82px",
        "right:22px",
        "z-index:2147483647",
        "padding:10px 14px",
        "border:1px solid #9c2622",
        "border-radius:6px",
        "background:#c4302b",
        "color:#fff",
        "font:600 14px Arial,sans-serif",
        "box-shadow:0 2px 8px rgba(0,0,0,.35)",
        "cursor:pointer"
      ].join(";");
      button.addEventListener("click", () => {
        const appUrl = `linkamp://add?url=${encodeURIComponent(window.location.href)}`;
        window.location.href = appUrl;
      });
      document.documentElement.appendChild(button);
    }
    button.hidden = !isVideoPage();
  }

  ensureButton();
  document.addEventListener("yt-navigate-finish", ensureButton);
  new MutationObserver(ensureButton).observe(document.documentElement, {
    childList: true,
    subtree: true
  });
})();
