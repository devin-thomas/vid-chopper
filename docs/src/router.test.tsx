import { renderHook } from "@testing-library/react";
import { afterEach, describe, expect, it } from "vitest";
import { useSiteLocation, useSiteSearchParams } from "./router";

afterEach(() => {
  window.history.replaceState(null, "", "/");
});

describe("useSiteLocation", () => {
  it("canonicalizes retired routes", () => {
    window.history.replaceState(null, "", "/features");
    expect(renderHook(() => useSiteLocation()).result.current).toEqual({
      pathname: "/",
      search: "?section=features",
    });

    window.history.replaceState(null, "", "/download");
    expect(renderHook(() => useSiteLocation()).result.current).toEqual({
      pathname: "/releases",
      search: "",
    });

    window.history.replaceState(null, "", "/docs?section=build-from-source");
    expect(renderHook(() => useSiteLocation()).result.current).toEqual({
      pathname: "/docs/getting-started",
      search: "",
    });
  });

  it("keeps the requested page when the URL parser refuses the address", () => {
    const RealUrl = globalThis.URL;
    window.history.replaceState(null, "", "/docs/cli?section=x");
    globalThis.URL = function FailingUrl() {
      throw new TypeError("Type error");
    } as unknown as typeof URL;
    try {
      expect(renderHook(() => useSiteLocation()).result.current).toEqual({
        pathname: "/docs/cli",
        search: "",
      });
    } finally {
      globalThis.URL = RealUrl;
    }
  });

  it("hands every reader the same object so location effects do not re-run", () => {
    window.history.replaceState(null, "", "/docs");
    const first = renderHook(() => useSiteLocation());
    const second = renderHook(() => useSiteLocation());
    const before = first.result.current;
    first.rerender();

    expect(first.result.current).toBe(before);
    expect(second.result.current).toBe(before);
    expect(renderHook(() => useSiteSearchParams()).result.current).toBe(
      renderHook(() => useSiteSearchParams()).result.current,
    );
  });
});
