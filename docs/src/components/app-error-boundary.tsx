import { Component, type ErrorInfo, type ReactNode } from "react";
import {
  agentSkillUrl,
  docsUrl,
  releaseMacDmgUrl,
  releasePageUrl,
  releaseVersion,
  releaseZipUrl,
} from "../content/site";

type Props = { children: ReactNode };
type State = { failed: boolean };

/**
 * Keeps a rendering fault from emptying the document.
 *
 * Without a boundary, one throw anywhere in the tree unmounts the React root
 * and leaves a blank page with no way forward. The fallback below repeats the
 * only three things a visitor comes here for, so a broken render still ends in
 * a download, the docs, or the agent skill.
 */
export class AppErrorBoundary extends Component<Props, State> {
  override state: State = { failed: false };

  static getDerivedStateFromError(): State {
    return { failed: true };
  }

  override componentDidCatch(error: Error, info: ErrorInfo) {
    console.error("VidChopper site failed to render.", error, info);
  }

  override render() {
    if (!this.state.failed) return this.props.children;

    return (
      <main className="noscript-onboarding" data-route-focus>
        <p className="noscript-kicker">
          VidChopper works without a website account
        </p>
        <h1>This page did not render, but the downloads still work.</h1>
        <p>
          Reload to try again, or go straight to the release, the docs, or the
          agent skill. The site never receives your media or local paths.
        </p>
        <nav aria-label="Recovery starting points">
          <a href={releaseZipUrl}>Download {releaseVersion} for Windows</a>
          <a href={releaseMacDmgUrl}>Download {releaseVersion} for Mac</a>
          <a href={releasePageUrl}>View {releaseVersion} release details</a>
          <a href={docsUrl}>Read the docs</a>
          <a href={agentSkillUrl}>Inspect the full skill</a>
          <a href="/">Reload the site</a>
        </nav>
      </main>
    );
  }
}
