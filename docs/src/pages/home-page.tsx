import { useEffect, useRef } from "react";
import { Link, useSearchParams } from "react-router-dom";
import heroMain from "../assets/vidchopper-real-main.png";
import heroSummary from "../assets/vidchopper-real-summary.png";
import heroExport from "../assets/vidchopper-real-export.png";
import { SectionHeading } from "../components/section-heading";
import {
  keyFeatures,
  releaseFacts,
  releaseVersion,
  releaseZipUrl,
  releasesUrl,
  roadmap,
  workflowSteps,
} from "../content/site";

export function HomePage() {
  const [searchParams] = useSearchParams();
  const featuresRef = useRef<HTMLElement | null>(null);
  const screenshotsRef = useRef<HTMLElement | null>(null);

  useEffect(() => {
    const section = searchParams.get("section");
    if (section === "features") {
      featuresRef.current?.scrollIntoView({ block: "start", behavior: "smooth" });
    }
    if (section === "screenshots") {
      screenshotsRef.current?.scrollIntoView({ block: "start", behavior: "smooth" });
    }
  }, [searchParams]);

  return (
    <main className="page-stack">
      <section className="hero-panel hero-panel-overview">
        <div className="hero-copy">
          <div className="hero-kicker">Real Qt desktop capture. No fake product shell.</div>
          <h1>Turn one long source video into clean chapter clips from a native desktop workflow.</h1>
          <p>
            VidChopper is a Windows-first Qt app for importing chapter data, retiming chapter boundaries, and
            exporting sequential clips through ffmpeg with portable release packaging, offline defaults, and a
            testable C++ core.
          </p>
          <div className="hero-actions">
            <a className="cta-primary" href={releaseZipUrl}>
              Download {releaseVersion} ZIP
            </a>
            <Link className="cta-secondary" to="/releases">
              Browse release portal
            </Link>
          </div>
          <div className="trust-line">No installer. No telemetry. Local files stay on your machine.</div>
          <dl className="fact-ribbon">
            {releaseFacts.map((fact) => (
              <div key={fact.label}>
                <dt>{fact.label}</dt>
                <dd>{fact.value}</dd>
              </div>
            ))}
          </dl>
          <p className="fine-print">
            The portable release bundles VidChopper.exe, the Qt runtime, and the Microsoft VC++ runtime. ffmpeg and
            ffprobe still need to be installed separately or configured in-app.
          </p>
        </div>
        <div className="hero-visual">
          <div className="shot-stack">
            <div className="shot-badge shot-badge-top">Seeded real-app hero capture</div>
            <div className="shot-badge shot-badge-bottom">Qt 6 desktop workflow</div>
            <div className="product-shot product-shot-hero">
              <img
                src={heroMain}
                alt="Real VidChopper desktop window showing the source, summary, chapter table, and export controls."
              />
            </div>
          </div>
        </div>
      </section>

      <section className="benefit-ribbon" aria-label="Core product benefits">
        <div>
          <strong>Offline by default</strong>
          <span>No account flow, no browser dependency, no service round-trip.</span>
        </div>
        <div>
          <strong>Chapter planning</strong>
          <span>Import embedded metadata or seed a clean starter layout from one source file.</span>
        </div>
        <div>
          <strong>Export visibility</strong>
          <span>Keep the output path, settings, progress row, and logs in the same native window.</span>
        </div>
      </section>

      <section ref={screenshotsRef} className="showcase-panel">
        <SectionHeading
          title="Built for real workflow"
          body="The site leans on the actual product surface: import the file, inspect the session summary, retime the chapter list, then export with the path and logs still visible."
          aside={<Link to="/releases">Open the release portal</Link>}
        />
        <div className="showcase-grid">
          <article className="screenshot-card">
            <div className="screenshot-card-label">Source + summary</div>
            <img
              src={heroSummary}
              alt="VidChopper source and output section with chapter count controls and session summary."
              loading="lazy"
            />
          </article>
          <article className="screenshot-card">
            <div className="screenshot-card-label">Chapter table + export</div>
            <img
              src={heroExport}
              alt="VidChopper chapter table and export controls from the real desktop application."
              loading="lazy"
            />
          </article>
          <div className="showcase-copy">
            <h3>Screenshot-led proof, not concept-only copy</h3>
            <p>
              The screenshots are narrow because the app is a dense utility, not a cinematic editor. The design job on
              this site is to frame that truth well, not to imply a preview canvas or timeline that does not ship.
            </p>
            <ul>
              <li>Inspect the source, output path, and starter chapter plan at a glance.</li>
              <li>Edit timing and naming in a native table built for utility-first desktop work.</li>
              <li>Export through a visible status row with explicit controls and collapsible logs.</li>
            </ul>
          </div>
        </div>
        <div className="secondary-shot-row">
          <article className="secondary-shot-copy">
            <span className="eyebrow">How it works</span>
            <h3>One source file in. Ordered chapter clips out.</h3>
            <p>
              VidChopper is intentionally smaller than an NLE. Its job is to keep the chapter plan, output naming,
              export state, and settings legible while ffmpeg does the actual encode work.
            </p>
          </article>
          <article className="secondary-shot-frame">
            <img
              src={heroMain}
              alt="Full real VidChopper application window used as the primary seeded overview screenshot."
              loading="lazy"
            />
          </article>
        </div>
      </section>

      <section className="workflow-band">
        <SectionHeading
          title="Built around a three-step desktop rhythm"
          body="VidChopper is not trying to be a full editor. It is tuned for taking one source video and turning chapters into reliable clip outputs."
        />
        <div className="workflow-list">
          {workflowSteps.map((step, index) => (
            <article key={step.title} className="workflow-card">
              <span>{`0${index + 1}`}</span>
              <h3>{step.title}</h3>
              <p>{step.text}</p>
            </article>
          ))}
        </div>
      </section>

      <section ref={featuresRef} className="feature-river">
        <SectionHeading
          title="Powerful features. Zero fluff."
          body="Every point below maps to current shipped behavior: chapter import, timing edits, export controls, release packaging, or repo-level workflow discipline."
          aside={<Link to="/releases?section=changelog">Read the changelog</Link>}
        />
        <div className="feature-river-grid">
          {keyFeatures.map((feature, index) => (
            <article key={feature.title} className={`feature-panel feature-panel-${index % 3}`}>
              <h3>{feature.title}</h3>
              <p>{feature.detail}</p>
            </article>
          ))}
        </div>
      </section>

      <section className="roadmap-panel">
        <div className="roadmap-copy">
          <h2>Release momentum is already visible in the repo history.</h2>
          <p>
            The `v0.2.0-alpha` round shipped the desktop feature pass, the release packaging path, and the Pages
            rewrite. What remains is polish, seeded capture continuity, and stronger release storytelling over time.
          </p>
        </div>
        <div className="roadmap-list">
          {roadmap.map((entry) => (
            <div key={entry.item} className="roadmap-row">
              <strong>{entry.item}</strong>
              <span>{entry.status}</span>
            </div>
          ))}
        </div>
      </section>

      <section className="download-band">
        <div className="download-band-copy">
          <span className="eyebrow">Ready when you are</span>
          <h2>Download the portable Windows ZIP and work locally.</h2>
          <p>
            The release portal includes the ZIP, install checklist, ffmpeg/ffprobe requirement notes, and changelog.
          </p>
        </div>
        <div className="download-band-actions">
          <a className="cta-primary" href={releaseZipUrl}>
            Download ZIP
          </a>
          <a className="cta-secondary" href={releasesUrl}>
            View all GitHub releases
          </a>
        </div>
      </section>
    </main>
  );
}
