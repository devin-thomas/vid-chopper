/**
 * Bridge to the inline bootstrap guard in index.html.
 *
 * The guard reveals a static fallback when the application never mounts —
 * a purged bundle behind a cached document, a dropped mobile connection, or a
 * script error before React runs. These helpers tell it which happened so the
 * fallback appears only when it is actually needed.
 */

type BootGuard = {
  ready?: () => void;
  fail?: (reason?: string) => void;
};

declare global {
  interface Window {
    __vidchopperBoot?: BootGuard;
  }
}

/** The application mounted: retire the bootstrap fallback. */
export function markBootReady() {
  try {
    window.__vidchopperBoot?.ready?.();
  } catch {
    // The guard is best effort and must never break a successful mount.
  }
}

/** The application could not mount: show the bootstrap fallback now. */
export function markBootFailed(reason: string) {
  try {
    window.__vidchopperBoot?.fail?.(reason);
  } catch {
    // Nothing further can be done from here.
  }
}
