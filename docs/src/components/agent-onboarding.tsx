import { useRef, useState } from "react";
import {
  agentOnboardingPrompt,
  agentSkillUrl,
  docsUrl,
  releaseVersion,
  releaseZipUrl,
} from "../content/site";
import { Icon } from "./icon";

type CopyState = "idle" | "copied" | "failed";

const copyMessages: Record<CopyState, string> = {
  idle: "Copy the prompt or select it manually.",
  copied: "Prompt copied. Paste it into your agent to begin with a dry-run.",
  failed:
    "Copy failed. Select the prompt manually, then copy it with your keyboard.",
};

export function AgentOnboarding() {
  const [copyState, setCopyState] = useState<CopyState>("idle");
  const promptRef = useRef<HTMLTextAreaElement>(null);

  const copyPrompt = async () => {
    try {
      if (navigator.clipboard?.writeText === undefined) {
        throw new Error("Clipboard API unavailable");
      }
      await navigator.clipboard.writeText(agentOnboardingPrompt);
      setCopyState("copied");
    } catch {
      setCopyState("failed");
      promptRef.current?.focus();
      promptRef.current?.select();
    }
  };

  return (
    <section className="start-paths" aria-labelledby="start-paths-title">
      <header className="start-paths-heading">
        <span>Start your way</span>
        <h2 id="start-paths-title">Choose how to start</h2>
        <p>
          Download the app, read the human guide, or copy one safe request for
          your agent.
        </p>
      </header>
      <div className="start-path-grid">
        <article className="start-card" aria-label="Download VidChopper">
          <Icon name="download" />
          <span className="start-card-label">Portable app</span>
          <a
            className="start-card-action"
            href={releaseZipUrl}
            aria-label={`Download VidChopper ${releaseVersion}`}
          >
            Download
          </a>
        </article>

        <article className="start-card" aria-label="Read the VidChopper docs">
          <Icon name="code" />
          <span className="start-card-label">Human guide</span>
          <a
            className="start-card-action"
            href={docsUrl}
            aria-label="Read the docs"
          >
            Read docs
          </a>
        </article>

        <article className="start-card" aria-label="Onboard your agent">
          <Icon name="terminal" />
          <span className="start-card-label">Agent handoff</span>
          <button
            className="start-card-action"
            type="button"
            onClick={copyPrompt}
            aria-label="Copy agent prompt"
          >
            Copy prompt
          </button>
        </article>
      </div>

      <div className="agent-handoff-detail">
        <div className="agent-handoff-heading">
          <p>
            Works with skill-aware and generic coding agents. The prompt keeps
            dry-run and consequential-action gates explicit.
          </p>
          <a className="start-card-detail" href={agentSkillUrl}>
            Inspect the full skill
          </a>
        </div>
        <label className="agent-prompt-label" htmlFor="agent-starter-prompt">
          Agent starter prompt
        </label>
        <textarea
          id="agent-starter-prompt"
          ref={promptRef}
          className="agent-prompt"
          rows={5}
          readOnly
          value={agentOnboardingPrompt}
          onFocus={(event) => event.currentTarget.select()}
        />
        <p
          className={`copy-status copy-status-${copyState}`}
          role="status"
          aria-live="polite"
        >
          {copyMessages[copyState]}
        </p>
        <p className="agent-privacy-boundary">
          The agent runs your local CLI. This site receives no media, paths, or
          clipboard contents.
        </p>
      </div>
    </section>
  );
}
