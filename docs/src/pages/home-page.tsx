import { Link } from "react-router-dom";
import { DesktopFrame } from "../components/desktop-frame";
import { SectionHeading } from "../components/section-heading";
import {
  keyFeatures,
  releaseFacts,
  releaseZipUrl,
  releasesUrl,
  roadmap,
  workflowSteps,
} from "../content/site";

export function HomePage() {
  return (
    <main className="page-stack">
      <section className="hero-panel">
        <div className="hero-copy">
          <h1>Turn one long source video into clean chapter clips without leaving a native desktop workflow.</h1>
          <p>
            VidChopper is a Qt 6 Windows app for importing chapter data, editing exact timing, and exporting clips
            through ffmpeg with GPU-aware defaults, portable release packaging, and a testable C++ core.
          </p>
          <div className="hero-actions">
            <a className="cta-primary" href={releaseZipUrl}>
              Download v0.2.0-alpha ZIP
            </a>
            <a className="cta-secondary" href={releasesUrl}>
              Browse all releases
            </a>
          </div>
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
          <DesktopFrame title="VidChopper.exe" accent="#8e44ff" detail="Chapter editor" variant="editor" />
        </div>
      </section>

      <section className="showcase-panel">
        <SectionHeading
          title="See the desktop flow before you download it"
          body="The first screen is about certainty: probe the source file, inspect the chapter layout, edit exact ranges, and keep the export path visible."
          aside={<Link to="/features">View the full feature breakdown</Link>}
        />
        <div className="showcase-grid">
          <DesktopFrame title="Import + inspect" accent="#5fd39e" detail="ffprobe pass" variant="settings" />
          <DesktopFrame title="Edit timeline" accent="#8e44ff" detail="Chapter timeline" variant="timeline" />
          <div className="showcase-copy">
            <h3>Screenshot-led persuasion, not vague claims</h3>
            <p>
              The product pitch is the actual workflow: a dark desktop utility that exposes chapter timing, settings,
              and export state clearly enough to trust on long-form media jobs.
            </p>
            <ul>
              <li>Import embedded chapters or seed a clean starter layout.</li>
              <li>Edit names and boundaries in a dense utility-first desktop interface.</li>
              <li>Export sequential clips with logs, checks, and predictable output control.</li>
            </ul>
          </div>
        </div>
      </section>

      <section className="workflow-band">
        <SectionHeading
          title="Built around a three-step desktop rhythm"
          body="VidChopper is not trying to be a full NLE. It is optimized for taking one source file and turning chapters into reliable clip outputs."
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

      <section className="feature-river">
        <SectionHeading
          title="Desktop capabilities that matter to the export path"
          body="The current alpha is already a serious release artifact: portable packaging, clear settings, release automation, and a native core/UI split."
          aside={<Link to="/download">Read the release notes</Link>}
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
            The `v0.2.0-alpha` round shipped the desktop feature pass, the release packaging path, and this Pages
            rewrite. The next work is polish, continuity, and future release communication rather than re-explaining
            the product from scratch.
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
    </main>
  );
}
