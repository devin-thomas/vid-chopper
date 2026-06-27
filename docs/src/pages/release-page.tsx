import appIcon from "../assets/app-icon.png";
import { releaseFacts, releaseHighlights, releaseZipUrl, releasesUrl } from "../content/site";

export function ReleasePage() {
  return (
    <main className="page-stack">
      <section className="subpage-hero release-hero">
        <div>
          <h1>Download the current portable Windows release without building Qt locally.</h1>
          <p>
            The `v0.2.0-alpha` prerelease is the intended end-user path: unzip it, launch VidChopper.exe, then point
            the app at ffmpeg and ffprobe if they are not already on your PATH.
          </p>
        </div>
        <div className="release-hero-card">
          <div className="release-mark">
            <img src={appIcon} alt="" />
            <span>Portable Windows release</span>
          </div>
          <div className="release-chip">Current release</div>
          <h2>v0.2.0-alpha</h2>
          <a className="cta-primary" href={releaseZipUrl}>
            Download Windows ZIP
          </a>
          <a className="cta-secondary" href={releasesUrl}>
            Browse release history
          </a>
        </div>
      </section>

      <section className="release-matrix">
        <article className="release-card">
          <h3>Package facts</h3>
          <dl className="release-facts-grid">
            {releaseFacts.map((fact) => (
              <div key={fact.label}>
                <dt>{fact.label}</dt>
                <dd>{fact.value}</dd>
              </div>
            ))}
          </dl>
        </article>
        <article className="release-card">
          <h3>Included in the ZIP</h3>
          <ul>
            <li>VidChopper.exe</li>
            <li>Required Qt runtime files</li>
            <li>Microsoft VC++ runtime</li>
            <li>Release readme, notices, and license files</li>
          </ul>
        </article>
        <article className="release-card">
          <h3>Still required separately</h3>
          <ul>
            <li>ffmpeg</li>
            <li>ffprobe</li>
            <li>Any optional codec/environment setup outside the portable bundle</li>
          </ul>
        </article>
      </section>

      <section className="release-callout">
        <div>
          <h2>Install in three steps</h2>
          <ol>
            <li>Download and unzip the Windows x64 release archive.</li>
            <li>Launch VidChopper.exe from any writable location.</li>
            <li>Install ffmpeg and ffprobe separately, or set custom tool paths in Advanced Settings.</li>
          </ol>
        </div>
        <div className="release-note-card">
          <strong>Why the portable route exists</strong>
          <p>
            The repo can validate the core without Qt, but the GUI build still depends on a Qt SDK. The release ZIP is
            the clean path for people who want the product rather than the build setup.
          </p>
        </div>
      </section>

      <section className="release-history">
        <div className="release-history-panel">
          <h2>Release highlights</h2>
          <ul>
            {releaseHighlights.map((item) => (
              <li key={item}>{item}</li>
            ))}
          </ul>
        </div>
        <div className="release-history-panel">
          <h2>Previous releases</h2>
          <p>
            VidChopper is still in prerelease mode. The GitHub release history is the source of truth for ZIP assets,
            notes, and future version progression.
          </p>
          <a className="ghost-link" href={releasesUrl}>
            Open GitHub releases
          </a>
        </div>
      </section>
    </main>
  );
}
