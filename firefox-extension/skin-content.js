(() => {
  const DIRECT_BUTTON_ID = "linkamp-install-skin-button";
  const SITE_BUTTON_ID = "linkamp-install-skin-site-button";
  let retryTimer = null;
  let ensureScheduled = false;

  function decodedPath(url) {
    try {
      return decodeURIComponent(url.pathname);
    } catch (_) {
      return url.pathname;
    }
  }

  function skinInfo(rawUrl) {
    let url;
    try {
      url = new URL(rawUrl, window.location.href);
    } catch (_) {
      return null;
    }
    if (url.protocol !== "https:") return null;

    const path = decodedPath(url);
    if (url.hostname === "skins.webamp.org") {
      const museumSkin = path.match(/^\/skin\/([a-f0-9]{32})\/([^/]+\.wsz)\/?$/i);
      if (museumSkin) {
        return {
          fileName: museumSkin[2],
          downloadUrl: `https://r2.webampskins.org/skins/${museumSkin[1]}.wsz`
        };
      }
    }

    if (!path.toLowerCase().endsWith(".wsz")) return null;
    return {
      fileName: path.substring(path.lastIndexOf("/") + 1),
      downloadUrl: url.href
    };
  }

  function install(button) {
    const info = skinInfo(button.dataset.linkampSkinUrl);
    if (!info) return;
    const appUrl = `linkamp://install-skin?url=${encodeURIComponent(info.downloadUrl)}&name=${encodeURIComponent(info.fileName)}`;
    window.location.href = appUrl;
  }

  function createButton(id, text, title, className) {
    const button = document.createElement("button");
    button.id = id;
    button.type = "button";
    button.className = className || "";
    button.textContent = text;
    button.title = title;
    button.setAttribute("aria-label", title);
    button.addEventListener("click", () => install(button));
    return button;
  }

  function stylePill(button, fixed) {
    button.style.cssText = [
      "appearance:none",
      "display:inline-flex",
      "align-items:center",
      "justify-content:center",
      "height:36px",
      "padding:0 14px",
      "margin:8px",
      "border:0",
      "border-radius:18px",
      "background:#c4302b",
      "color:#fff",
      "font:600 13px system-ui,Arial,sans-serif",
      "white-space:nowrap",
      "cursor:pointer",
      fixed ? "position:fixed;top:16px;right:16px;z-index:2147483647;box-shadow:0 2px 8px #0008" : ""
    ].join(";");
  }

  function ensureCurrentPageButton() {
    const isMuseumPage = window.location.hostname === "skins.webamp.org";
    const nativeDownload = isMuseumPage
      ? document.querySelector('button[aria-label="Download skin"], button[title="Download skin"]')
      : null;

    if (nativeDownload) {
      let button = document.getElementById(SITE_BUTTON_ID);
      if (!button) {
        button = createButton(SITE_BUTTON_ID, "♫ Linkamp",
                              "Installa e applica questa skin in Linkamp", "skin-action-button");
      }
      button.dataset.linkampSkinUrl = window.location.href;
      button.style.cssText = [
        "appearance:none", "display:inline-flex", "align-items:center",
        "justify-content:center", "height:36px", "padding:0 14px",
        "margin-left:8px", "border:0", "border-radius:18px",
        "background:#c4302b", "color:#fff",
        "font:600 13px system-ui,Arial,sans-serif", "white-space:nowrap", "cursor:pointer"
      ].join(";");
      if (button.parentElement !== nativeDownload.parentElement
          || button.previousElementSibling !== nativeDownload) {
        nativeDownload.insertAdjacentElement("afterend", button);
      }
      document.getElementById(DIRECT_BUTTON_ID)?.remove();
      return;
    }

    document.getElementById(SITE_BUTTON_ID)?.remove();
    const directPageInfo = skinInfo(window.location.href);
    let button = document.getElementById(DIRECT_BUTTON_ID);
    if (!directPageInfo) {
      button?.remove();
      return;
    }
    if (!button) {
      button = createButton(DIRECT_BUTTON_ID, "♫ Installa skin in Linkamp",
                            "Installa e applica questo file skin in Linkamp");
      stylePill(button, true);
      document.body.appendChild(button);
    }
    button.dataset.linkampSkinUrl = window.location.href;
  }

  function ensureLinkedSkinButtons() {
    const anchors = document.querySelectorAll("a[href]");
    for (const anchor of anchors) {
      const info = skinInfo(anchor.href);
      const existing = anchor.nextElementSibling?.matches("[data-linkamp-skin-link-button]")
        ? anchor.nextElementSibling : null;
      if (!info) {
        existing?.remove();
        continue;
      }
      let button = existing;
      if (!button) {
        button = createButton("", "Installa in Linkamp",
                              "Installa questa skin Winamp in Linkamp");
        button.dataset.linkampSkinLinkButton = "true";
        stylePill(button, false);
        button.style.height = "26px";
        button.style.padding = "0 10px";
        button.style.margin = "0 0 0 6px";
        button.style.fontSize = "12px";
        anchor.insertAdjacentElement("afterend", button);
      }
      button.dataset.linkampSkinUrl = anchor.href;
    }
  }

  function ensureButtons() {
    if (!document.body) {
      if (!retryTimer) {
        retryTimer = setTimeout(() => {
          retryTimer = null;
          ensureButtons();
        }, 250);
      }
      return;
    }
    if (retryTimer) {
      clearTimeout(retryTimer);
      retryTimer = null;
    }
    ensureCurrentPageButton();
    ensureLinkedSkinButtons();
  }

  function scheduleEnsureButtons() {
    if (ensureScheduled) return;
    ensureScheduled = true;
    setTimeout(() => {
      ensureScheduled = false;
      ensureButtons();
    }, 250);
  }

  ensureButtons();
  new MutationObserver(scheduleEnsureButtons).observe(document.documentElement, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ["href"]
  });
  window.addEventListener("popstate", scheduleEnsureButtons);
  window.addEventListener("hashchange", scheduleEnsureButtons);
})();
