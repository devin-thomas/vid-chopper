import {
  useEffect,
  useSyncExternalStore,
  type ComponentPropsWithoutRef,
  type MouseEvent,
} from "react";
import routeContract from "../routes.json";
import { docsGuideposts } from "./content/site";

export type SiteLocation = {
  pathname: string;
  search: string;
};

export const legacyPagesBuild = import.meta.env.MODE === "pages";

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

function stripPagesBase(pathname: string) {
  if (!legacyPagesBuild) return pathname;
  const base = import.meta.env.BASE_URL.replace(/\/$/, "");
  if (pathname === base || pathname === `${base}/`) return "/";
  return pathname.startsWith(`${base}/`)
    ? pathname.slice(base.length)
    : pathname;
}

function snapshot() {
  if (window.location.hash.startsWith("#/")) {
    return `hash:${window.location.hash.slice(1)}`;
  }
  return `path:${stripPagesBase(window.location.pathname)}${window.location.search}`;
}

function normalizeRoute(routeSnapshot: string): SiteLocation {
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

export function useSiteLocation(): SiteLocation {
  const routeSnapshot = useSyncExternalStore(
    subscribe,
    snapshot,
    () => "path:/",
  );
  const location = normalizeRoute(routeSnapshot);

  useEffect(() => {
    if (legacyPagesBuild) return;
    const canonicalPath = `${location.pathname}${location.search}`;
    const currentPath = `${window.location.pathname}${window.location.search}`;
    if (
      window.location.hash.startsWith("#/") ||
      currentPath !== canonicalPath
    ) {
      window.history.replaceState(null, "", canonicalPath);
      window.dispatchEvent(new PopStateEvent("popstate"));
    }
  }, [location.pathname, location.search]);

  return location;
}

export function useSiteSearchParams() {
  return new URLSearchParams(useSiteLocation().search);
}

type SiteLinkProps = Omit<ComponentPropsWithoutRef<"a">, "href"> & {
  to: string;
};

export function SiteLink({ to, onClick, target, ...props }: SiteLinkProps) {
  const href = legacyPagesBuild ? `${import.meta.env.BASE_URL}#${to}` : to;

  const navigate = (event: MouseEvent<HTMLAnchorElement>) => {
    onClick?.(event);
    if (
      event.defaultPrevented ||
      legacyPagesBuild ||
      event.button !== 0 ||
      event.metaKey ||
      event.ctrlKey ||
      event.shiftKey ||
      event.altKey ||
      target !== undefined
    ) {
      return;
    }

    event.preventDefault();
    window.history.pushState(null, "", to);
    window.dispatchEvent(new PopStateEvent("popstate"));
  };

  return <a href={href} target={target} onClick={navigate} {...props} />;
}
