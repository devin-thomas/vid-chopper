import { readFileSync } from "node:fs";
import path from "node:path";
import { cleanup, render, screen, within } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, describe, expect, it, vi } from "vitest";
import { AgentOnboarding } from "./agent-onboarding";
import {
  agentOnboardingPrompt,
  agentSkillUrl,
  docsUrl,
  releaseZipUrl,
} from "../content/site";
import { HomePage } from "../pages/home-page";

afterEach(() => {
  cleanup();
  vi.restoreAllMocks();
});

function setClipboard(writeText?: (value: string) => Promise<void>) {
  Object.defineProperty(navigator, "clipboard", {
    configurable: true,
    value: writeText === undefined ? undefined : { writeText },
  });
}

describe("AgentOnboarding", () => {
  it("keeps the stable skill URL, dry-run, and consequential-action gates in the prompt", () => {
    expect(agentOnboardingPrompt).toContain(agentSkillUrl);
    expect(agentOnboardingPrompt).toContain("dry-run");
    for (const gate of ["exporting", "overwriting", "uploading", "publishing", "deleting"]) {
      expect(agentOnboardingPrompt).toContain(gate);
    }
  });

  it("keeps download, human docs, and agent onboarding at equal hierarchy", () => {
    render(<HomePage />);

    const region = screen.getByRole("region", { name: "Choose how to start" });
    expect(region.closest(".hero-panel")).not.toBeNull();
    const cards = within(region).getAllByRole("article");
    expect(cards).toHaveLength(3);
    expect(within(cards[0]!).getByRole("link", { name: /download/i })).toHaveAttribute(
      "href",
      releaseZipUrl,
    );
    expect(within(cards[1]!).getByRole("link", { name: /read the docs/i })).toHaveAttribute(
      "href",
      docsUrl,
    );
    expect(within(cards[2]!).getByRole("button", { name: /copy agent prompt/i })).toBeVisible();
  });

  it("tabs through the three equal actions and activates copy from the keyboard", async () => {
    const writeText = vi.fn<(value: string) => Promise<void>>().mockResolvedValue(undefined);
    const user = userEvent.setup();
    setClipboard(writeText);
    render(<AgentOnboarding />);

    await user.tab();
    expect(screen.getByRole("link", { name: /download/i })).toHaveFocus();
    await user.tab();
    expect(screen.getByRole("link", { name: /read the docs/i })).toHaveFocus();
    await user.tab();
    expect(screen.getByRole("button", { name: /copy agent prompt/i })).toHaveFocus();
    await user.keyboard("{Enter}");

    expect(writeText).toHaveBeenCalledWith(agentOnboardingPrompt);
    expect(screen.getByRole("status")).toHaveTextContent("Prompt copied");
  });

  it("copies the exact safe prompt and announces success", async () => {
    const writeText = vi.fn<(value: string) => Promise<void>>().mockResolvedValue(undefined);
    const storageWrite = vi.spyOn(Storage.prototype, "setItem");
    const initialUrl = window.location.href;
    setClipboard(writeText);
    render(<AgentOnboarding />);

    await userEvent.click(screen.getByRole("button", { name: /copy agent prompt/i }));

    expect(writeText).toHaveBeenCalledWith(agentOnboardingPrompt);
    expect(screen.getByRole("status")).toHaveTextContent("Prompt copied");
    expect(screen.getByLabelText("Agent starter prompt")).toHaveValue(agentOnboardingPrompt);
    expect(storageWrite).not.toHaveBeenCalled();
    expect(window.location.href).toBe(initialUrl);
  });

  it("shows a manual fallback and focuses the prompt when copying fails", async () => {
    setClipboard(vi.fn().mockRejectedValue(new Error("permission denied")));
    render(<AgentOnboarding />);

    await userEvent.click(screen.getByRole("button", { name: /copy agent prompt/i }));

    const prompt = screen.getByLabelText("Agent starter prompt");
    expect(screen.getByRole("status")).toHaveTextContent("Copy failed");
    expect(prompt).toHaveFocus();
    expect(prompt).toHaveValue(agentOnboardingPrompt);
    expect(screen.getByRole("link", { name: /inspect the full skill/i })).toHaveAttribute(
      "href",
      agentSkillUrl,
    );
  });

  it("uses the same manual fallback when the Clipboard API is unavailable", async () => {
    setClipboard();
    render(<AgentOnboarding />);

    await userEvent.click(screen.getByRole("button", { name: /copy agent prompt/i }));

    const prompt = screen.getByLabelText("Agent starter prompt");
    expect(screen.getByRole("status")).toHaveTextContent("Copy failed");
    expect(prompt).toHaveFocus();
    expect(prompt).toHaveValue(agentOnboardingPrompt);
  });

  it("uses the same prompt and direct navigation in the no-JavaScript fallback", () => {
    const html = readFileSync(path.join(process.cwd(), "index.html"), "utf8");

    expect(html).toContain("<noscript>");
    expect(html.indexOf("<noscript>")).toBeLessThan(
      html.indexOf('<div id="root"></div>'),
    );
    expect(html).toContain(agentOnboardingPrompt);
    expect(html).toContain(`href="${agentSkillUrl}"`);
    expect(html).toContain(`href="${docsUrl}"`);
    expect(html).toContain(`href="${releaseZipUrl}"`);
  });
});
