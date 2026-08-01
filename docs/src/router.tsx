import { useSyncExternalStore, type ComponentPropsWithoutRef } from "react";

type HashLocation = {
  pathname: string;
  search: string;
};

function subscribe(listener: () => void) {
  window.addEventListener("hashchange", listener);
  return () => window.removeEventListener("hashchange", listener);
}

function snapshot() {
  return window.location.hash.slice(1) || "/";
}

export function useHashLocation(): HashLocation {
  const route = useSyncExternalStore(subscribe, snapshot, () => "/");
  const url = new URL(route, window.location.origin);
  if (url.pathname === "/features") {
    return { pathname: "/", search: "?section=features" };
  }
  if (url.pathname === "/download") {
    return { pathname: "/releases", search: "" };
  }
  return { pathname: url.pathname, search: url.search };
}

export function useHashSearchParams() {
  return new URLSearchParams(useHashLocation().search);
}

type HashLinkProps = Omit<ComponentPropsWithoutRef<"a">, "href"> & {
  to: string;
};

export function HashLink({ to, ...props }: HashLinkProps) {
  return <a href={`#${to}`} {...props} />;
}
