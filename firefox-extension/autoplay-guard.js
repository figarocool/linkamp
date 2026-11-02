(() => {
  const playerSelector = "#movie_player, ytd-player, .html5-video-player";
  let userPlaybackAllowed = false;

  function isPlayerInteraction(event) {
    const path = event.composedPath ? event.composedPath() : [];
    return path.some(node => node && node.nodeType === Node.ELEMENT_NODE
          && typeof node.matches === "function" && node.matches(playerSelector))
        || Boolean(event.target?.closest?.(playerSelector));
  }

  function allowManualPlayback(event) {
    if (event.isTrusted && isPlayerInteraction(event))
      userPlaybackAllowed = true;
  }

  function stopUnrequestedPlayback(event) {
    const video = event.target;
    if (video?.tagName === "VIDEO" && !userPlaybackAllowed)
      video.pause();
  }

  function pauseCurrentVideo() {
    const video = document.querySelector("video.html5-main-video") || document.querySelector("video");
    if (!video)
      return false;

    video.autoplay = false;
    video.removeAttribute("autoplay");
    if (!userPlaybackAllowed && !video.paused)
      video.pause();
    return true;
  }

  const videoObserver = new MutationObserver(() => {
    if (pauseCurrentVideo())
      videoObserver.disconnect();
  });

  document.addEventListener("pointerdown", allowManualPlayback, true);
  document.addEventListener("keydown", event => {
    const playerShortcut = event.key.toLowerCase() === "k" || event.code === "Space";
    if (event.isTrusted && (playerShortcut || isPlayerInteraction(event))) {
      userPlaybackAllowed = true;
    }
  }, true);
  document.addEventListener("play", stopUnrequestedPlayback, true);
  document.addEventListener("playing", stopUnrequestedPlayback, true);
  document.addEventListener("yt-navigate-finish", () => {
    userPlaybackAllowed = false;
    if (!pauseCurrentVideo())
      videoObserver.observe(document.documentElement || document, { childList: true, subtree: true });
  });

  if (!pauseCurrentVideo())
    videoObserver.observe(document.documentElement || document, { childList: true, subtree: true });
})();
