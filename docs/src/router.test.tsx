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
