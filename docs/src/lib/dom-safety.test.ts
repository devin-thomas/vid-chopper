import { afterEach, describe, expect, it, vi } from "vitest";
import {
  focusWithoutScroll,
  pushHistoryEntry,
  replaceHistoryEntry,
  scrollSectionIntoView,
  scrollToTop,
} from "./dom-safety";

afterEach(() => {
  vi.restoreAllMocks();
});

describe("dom-safety", () => {
  it("falls back when the browser rejects the instant scroll behavior", () => {
    const accepted: unknown[] = [];
    vi.spyOn(window, "scrollTo").mockImplementation(((...args: unknown[]) => {
      const [options] = args;
      if (
        typeof options === "object" &&
        options !== null &&
        (options as ScrollToOptions).behavior === ("instant" as ScrollBehavior)
      ) {
        throw new TypeError("Type error");
      }
      accepted.push(options);
    }) as typeof window.scrollTo);

    expect(() => scrollToTop()).not.toThrow();
    expect(accepted).toEqual([{ top: 0, left: 0 }]);
  });

  it("scrolls a section into view without options when the browser refuses them", () => {
    const element = document.createElement("section");
    const calls: unknown[] = [];
    element.scrollIntoView = ((options?: unknown) => {
      if (options !== undefined) throw new TypeError("Type error");
      calls.push(options);
    }) as Element["scrollIntoView"];

    expect(() => scrollSectionIntoView(element)).not.toThrow();
    expect(calls).toEqual([undefined]);
  });

  it("focuses without options when preventScroll is unsupported", () => {
    const element = document.createElement("div");
    const calls: unknown[] = [];
    element.focus = ((options?: unknown) => {
      if (options !== undefined) throw new TypeError("Type error");
      calls.push(options);
    }) as HTMLElement["focus"];

    expect(() => focusWithoutScroll(element)).not.toThrow();
    expect(calls).toEqual([undefined]);
  });

  it("reports a refused history write instead of throwing", () => {
    const securityError = Object.assign(new Error("throttled"), {
      name: "SecurityError",
    });
    vi.spyOn(window.history, "replaceState").mockImplementation(() => {
      throw securityError;
    });
    vi.spyOn(window.history, "pushState").mockImplementation(() => {
      throw securityError;
    });

    expect(replaceHistoryEntry("/docs")).toBe(false);
    expect(pushHistoryEntry("/docs")).toBe(false);
  });

  it("reports an accepted history write", () => {
    expect(replaceHistoryEntry("/docs")).toBe(true);
    expect(window.location.pathname).toBe("/docs");
    expect(replaceHistoryEntry("/")).toBe(true);
  });
});
