/**
 * Browser APIs that are specified but not uniformly implemented.
 *
 * WebKit validates IDL dictionary members strictly and throws `TypeError` for
 * enum values it does not know, and it throws `SecurityError` once a document
 * exceeds its history mutation budget. Both faults surface inside React
 * effects, where an uncaught throw unmounts the whole tree and leaves a blank
 * document. Every call the shell makes into these APIs goes through here so a
 * hostile browser degrades one interaction instead of the entire site.
 */

function ignoreFailure(action: () => void) {
  try {
    action();
    return true;
  } catch {
    return false;
  }
}

/** Jump to the top of the document without the smooth-scroll animation. */
export function scrollToTop() {
  // "instant" is the only value that overrides `html { scroll-behavior: smooth }`.
  // Browsers that reject the enum value fall back to an animated jump.
  const instant = ignoreFailure(() => {
    window.scrollTo({ top: 0, left: 0, behavior: "instant" as ScrollBehavior });
  });
  if (instant) return;
  if (ignoreFailure(() => window.scrollTo({ top: 0, left: 0 }))) return;
  ignoreFailure(() => window.scrollTo(0, 0));
}

/** Bring an element into view, animating only when the browser allows it. */
export function scrollSectionIntoView(element: Element) {
  const smooth = ignoreFailure(() => {
    element.scrollIntoView({ block: "start", behavior: "smooth" });
  });
  if (smooth) return;
  ignoreFailure(() => element.scrollIntoView());
}

/** Move focus for assistive technology without stealing the scroll position. */
export function focusWithoutScroll(element: HTMLElement) {
  if (ignoreFailure(() => element.focus({ preventScroll: true }))) return;
  ignoreFailure(() => element.focus());
}

/**
 * Replace the current history entry. Returns false when the browser refused,
 * which keeps callers from assuming `window.location` now matches the request.
 */
export function replaceHistoryEntry(url: string) {
  return ignoreFailure(() => window.history.replaceState(null, "", url));
}

/** Push a new history entry, reporting whether the browser accepted it. */
export function pushHistoryEntry(url: string) {
  return ignoreFailure(() => window.history.pushState(null, "", url));
}

/** Tell the in-app router that `window.location` changed. */
export function notifyLocationChanged() {
  if (ignoreFailure(() => window.dispatchEvent(new PopStateEvent("popstate")))) {
    return;
  }
  ignoreFailure(() => window.dispatchEvent(new Event("popstate")));
}
