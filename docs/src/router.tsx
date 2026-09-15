import {
  useEffect,
  useSyncExternalStore,
  type ComponentPropsWithoutRef,
  type MouseEvent,
} from "react";
import routeContract from "../routes.json";
import { docsGuideposts } from "./content/site";
import {
  notifyLocationChanged,
  pushHistoryEntry,
  replaceHistoryEntry,
} from "./lib/dom-safety";

export type SiteLocation = {
  pathname: string;
  search: string;
};

const currentDocsSections = new Map<string, string>(
  docsGuideposts.map((section) => [section.slug, section.path] as const),
);

const retiredDocsSections = new Map(
  Object.entries(routeContract.legacyDocsRoutes),
);

function subscribe(listener: () => void) {
  window.addEventListener("hashchange", listener);
  window.addEventListener("popstate", listener);
  return () => {
    window.removeEventListener("hashchange", listener);
    window.removeEventListener("popstate", listener);
  };
}

function snapshot() {
  if (window.location.hash.startsWith("#/")) {
    return `hash:${window.location.hash.slice(1)}`;
  }
  return `path:${window.location.pathname}${window.location.search}`;
}

/**
 * Recover a route without the URL parser.
 *
 * Only reached when parsing threw, so it keeps the visitor on the page they
 * asked for instead of silently returning them to the home page. Query
 * handling is dropped, which costs the section anchors and nothing else.
 */
function fallbackRoute(routeSnapshot: string): SiteLocation {
  const route = routeSnapshot.slice(routeSnapshot.indexOf(":") + 1);
  const rawPath = route.split(/[?#]/)[0] ?? "/";
  if (!rawPath.startsWith("/")) return { pathname: "/", search: "" };
  const pathname = rawPath.length > 1 ? rawPath.replace(/\/+$/, "") : "/";
  return { pathname, search: "" };
}

function parseRoute(routeSnapshot: string): SiteLocation {
  const route = routeSnapshot.slice(routeSnapshot.indexOf(":") + 1);
  const url = new URL(route, "https://vidchopper.app");
  let pathname = url.pathname;
  const search = new URLSearchParams(url.search);

  if (pathname.length > 1) pathname = pathname.replace(/\/+$/, "");
  if (pathname === "/features") {
    pathname = "/";
    search.set("section", "features");
  } else if (pathname === "/download") {
    pathname = "/releases";
  } else if (pathname === "/docs" && search.has("section")) {
    const legacySection = search.get("section");
    if (legacySection !== null) {
      pathname =
        currentDocsSections.get(legacySection) ??
        retiredDocsSections.get(legacySection) ??
        "/docs";
    }
    search.delete("section");
  }

  const query = search.toString();
  return { pathname, search: query.length > 0 ? `?${query}` : "" };
}

// Every component reading the location must observe the same object identity,
// otherwise effects keyed on the location re-run on every render and repeatedly
// mutate history or scroll position.
const locationCache = new Map<string, SiteLocation>();

function normalizeRoute(routeSnapshot: string): SiteLocation {
  const cached = locationCache.get(routeSnapshot);
  if (cached !== undefined) return cached;
  let location: SiteLocation;
  try {
    location = parseRoute(routeSnapshot);
  } catch {
    // A malformed address must still render the site rather than nothing.
    location = fallbackRoute(routeSnapshot);
  }
  if (locationCache.size > 64) locationCache.clear();
  locationCache.set(routeSnapshot, location);
  return location;
}

// Readers share one instance per query string, so treat the result as
// read-only: mutating it would rewrite what every other component sees.
const searchParamsCache = new Map<string, URLSearchParams>();

function parsedSearchParams(search: string) {
  const cached = searchParamsCache.get(search);
  if (cached !== undefined) return cached;
  let params: URLSearchParams;
  try {
    params = new URLSearchParams(search);
  } catch {
    params = new URLSearchParams();
  }
  if (searchParamsCache.size > 64) searchParamsCache.clear();
  searchParamsCache.set(search, params);
  return params;
}

/** Read the current in-app location. Safe to call from any component. */
export function useSiteLocation(): SiteLocation {
  const routeSnapshot = useSyncExternalStore(
    subscribe,
    snapshot,
    () => "path:/",
  );
  return normalizeRoute(routeSnapshot);
}

/**
 * Rewrite the address bar to the canonical form of the current route.
 *
 * Mounted exactly once, at the application root. Running it from several
 * components would multiply history writes per navigation, and WebKit starts
 * throwing `SecurityError` once a document mutates history too often.
 */
export function useCanonicalLocation(location: SiteLocation) {
  useEffect(() => {
    const canonicalPath = `${location.pathname}${location.search}`;
    const currentPath = `${window.location.pathname}${window.location.search}`;
    if (!window.location.hash.startsWith("#/") && currentPath === canonicalPath) {
      return;
    }
    if (replaceHistoryEntry(canonicalPath)) notifyLocationChanged();
  }, [location.pathname, location.search]);
}

export function useSiteSearchParams() {
  return parsedSearchParams(useSiteLocation().search);
}

type SiteLinkProps = Omit<ComponentPropsWithoutRef<"a">, "href"> & {
  to: string;
};

export function SiteLink({ to, onClick, target, ...props }: SiteLinkProps) {
  const navigate = (event: MouseEvent<HTMLAnchorElement>) => {
    onClick?.(event);
    if (
      event.defaultPrevented ||
      event.button !== 0 ||
      event.metaKey ||
      event.ctrlKey ||
      event.shiftKey ||
      event.altKey ||
      target !== undefined
    ) {
      return;
    }

    // Let the browser perform a full navigation when history is unavailable,
    // so a refused history write never turns a link into a dead control.
    if (!pushHistoryEntry(to)) return;
    event.preventDefault();
    notifyLocationChanged();
  };

  return <a href={to} target={target} onClick={navigate} {...props} />;
}
