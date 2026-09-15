import { cleanup, render, screen } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import { AppErrorBoundary } from "./app-error-boundary";
import { docsUrl, releaseZipUrl } from "../content/site";

afterEach(() => {
  cleanup();
  vi.restoreAllMocks();
});

function Exploding(): never {
  throw new TypeError("Type error");
}

describe("AppErrorBoundary", () => {
  it("renders children while nothing fails", () => {
    render(
      <AppErrorBoundary>
        <p>Chapter clips</p>
      </AppErrorBoundary>,
    );

    expect(screen.getByText("Chapter clips")).toBeInTheDocument();
  });

  it("keeps the downloads and docs reachable when a render throws", () => {
    vi.spyOn(console, "error").mockImplementation(() => {});

    render(
      <AppErrorBoundary>
        <Exploding />
      </AppErrorBoundary>,
    );

    expect(
      screen.getByRole("heading", {
        name: /downloads still work/i,
        level: 1,
      }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole("link", { name: /Download v1\.2\.0 for Windows/ }),
    ).toHaveAttribute("href", releaseZipUrl);
    expect(screen.getByRole("link", { name: "Read the docs" })).toHaveAttribute(
      "href",
      docsUrl,
    );
  });
});
