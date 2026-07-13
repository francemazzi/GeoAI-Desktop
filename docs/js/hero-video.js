(function () {
  const video = document.getElementById("hero-demo-video");
  if (!video) return;

  const webmSrc = video.dataset.webmSrc;
  if (!webmSrc) return;

  const prefersReducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  if (prefersReducedMotion) return;

  function startPlayback() {
    const source = document.createElement("source");
    source.src = webmSrc;
    source.type = "video/webm";
    video.appendChild(source);
    video.load();
    video.play().catch(() => {});
  }

  if ("requestIdleCallback" in window) {
    requestIdleCallback(startPlayback, { timeout: 1200 });
  } else {
    setTimeout(startPlayback, 1200);
  }
})();
