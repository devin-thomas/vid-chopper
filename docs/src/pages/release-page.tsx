import { useEffect, useRef } from "react";
import { Link, useSearchParams } from "react-router-dom";
import appIcon from "../assets/app-icon.png";
import releaseShot from "../assets/vidchopper-real-export.png";
import {
  changelogEntries,
  previousReleases,
  releaseChecklist,
  releaseFacts,
  releaseHighlights,
  releaseVersion,
  releaseZipUrl,
  releasesUrl,
} from "../content/site";

export function ReleasePage() {
  const [searchParams] = useSearchParams();
  const changelogRef = useRef<HTMLElement | null>(null);

  useEffect(() => {
    if (searchParams.get("section") === "changelog") {
      changelogRef.current?.scrollIntoView({ block: "start", behavior: "smooth" });
    }
  }, [searchParams]);

  return (
    <main className="page-stack">
      <section className="subpage-hero release-hero">
        <div className="release-hero-copy">
          <div className="hero-kicker">Release portal</div>
          <h1>Download the current portable Windows release without building Qt locally.</h1>
          <p>
            The current prerelease is the intended end-user path: unzip it, launch VidChopper.exe, then point the app
            at ffmpeg and ffprobe if they are not already on your PATH.
          </p>
          <div className="hero-actions">
            <a className="cta-primary" href={releaseZipUrl}>
              Download {releaseVersion} ZIP
            </a>
            <Link className="cta-secondary" to="/docs">
              Read docs first
            </Link>
          </div>
        </div>
        <div className="release-hero-stack">
          <div className="product-shot product-shot-release">
            <div className="shot-badge shot-badge-top">Current shipped UI</div>
            <img src={releaseShot} alt="Real VidChopper export controls and chapter table from the Windows release." />
          </div>
          <div className="release-hero-card">
            <div className="release-mark">
              <img src={appIcon} alt="" />
              <span>Portable Windows release</span>
            </div>
            <div className="release-chip">Current release</div>
            <h2>{releaseVersion}</h2>
            <p>
              Download the same packaged app surface shown here: Qt runtime bundled, VC++ runtime bundled, `ffmpeg`
              and `ffprobe` configured separately.
            </p>
          </div>
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
            {releaseChecklist.map((step) => (
              <li key={step}>{step}</li>
            ))}
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
          <div className="release-list">
            {previousReleases.map((release) => (
              <div key={release.version} className="release-list-row">
                <strong>{release.version}</strong>
                <span>{release.date}</span>
                <p>{release.note}</p>
              </div>
            ))}
          </div>
          <a className="ghost-link" href={releasesUrl}>
            Open GitHub releases
          </a>
        </div>
      </section>

      <section ref={changelogRef} className="release-history changelog-panel">
        <div className="release-history-panel">
          <h2>Changelog</h2>
          <p>The current prerelease story is grounded in actual shipped repo behavior, not placeholder bullets.</p>
        </div>
        <div className="release-history-panel changelog-list">
          {changelogEntries.map((entry) => (
            <article key={entry.title} className="changelog-entry">
              <div className="release-chip">{entry.tag}</div>
              <h3>{entry.title}</h3>
              <p>{entry.detail}</p>
            </article>
          ))}
        </div>
      </section>
    </main>
  );
}
