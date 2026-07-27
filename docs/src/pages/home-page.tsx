import { useEffect, useRef } from "react";
import { Link, useSearchParams } from "react-router-dom";
import heroMain from "../assets/vidchopper-real-main.png";
import heroExport from "../assets/vidchopper-real-export.png";
import { SectionHeading } from "../components/section-heading";
import { Icon } from "../components/icon";
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
          <h1>Turn long-form video into clean chapter clips.</h1>
          <p>
            Import a source video, edit exact chapter ranges, and export clean clips with ffmpeg from a focused native
            desktop workflow.
          </p>
          <div className="hero-actions">
            <a className="cta-primary" href={releaseZipUrl}>
              <Icon name="download" /> Download {releaseVersion}
            </a>
            <Link className="cta-secondary" to="/?section=screenshots">
              <Icon name="arrow" /> Explore the workflow
            </Link>
          </div>
          <div className="trust-line">No installer. No telemetry. Local files stay on your machine.</div>
        </div>
        <div className="hero-visual">
          <div className="shot-stack">
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
          <Icon name="folder" />
          <strong>Local first</strong>
          <span>No account flow, no browser dependency, no service round-trip.</span>
        </div>
        <div>
          <Icon name="terminal" />
          <strong>FFmpeg powered</strong>
          <span>Import embedded metadata or seed a clean starter layout from one source file.</span>
        </div>
        <div>
          <Icon name="list" />
          <strong>Precise control</strong>
          <span>Keep the output path, settings, progress row, and logs in the same native window.</span>
        </div>
      </section>

      <section ref={screenshotsRef} className="work-area">
        <article className="workshot-frame">
          <img
            src={heroExport}
            alt="VidChopper chapter table and export controls from the real desktop application."
            loading="lazy"
          />
        </article>
        <div className="workflow-column">
          <SectionHeading
            title="A focused workflow"
            body="VidChopper keeps the edit-export loop small: one source video, exact chapter boundaries, reliable clip outputs."
          />
          <div className="workflow-list">
            {workflowSteps.map((step, index) => (
              <article key={step.title} className="workflow-card">
                <span className="step-number">{index + 1}</span>
                <h3>{step.title}</h3>
                <p>{step.text}</p>
              </article>
            ))}
          </div>
        </div>
      </section>

      <section ref={featuresRef} className="feature-layout">
        <div className="feature-map-panel">
          <SectionHeading
            title="Feature map"
            body="Current shipped behavior, organized around the work instead of decorative product claims."
            aside={<Link to="/releases?section=changelog">Read the changelog <Icon name="arrow" /></Link>}
          />
          <div className="feature-river-grid">
            {keyFeatures.map((feature, index) => (
              <article key={feature.title} className={`feature-panel feature-panel-${index % 3}`}>
                <Icon name={index % 3 === 0 ? "list" : index % 3 === 1 ? "code" : "map"} />
                <h3>{feature.title}</h3>
                <p>{feature.detail}</p>
              </article>
            ))}
          </div>
        </div>
        <aside className="roadmap-panel">
          <div className="roadmap-copy">
            <h2>Roadmap</h2>
            <p>What is next is visible in the repo history and release gates.</p>
          </div>
          <div className="roadmap-list">
            {roadmap.map((entry) => (
              <div key={entry.item} className="roadmap-row">
                <strong>{entry.item}</strong>
                <span>{entry.status}</span>
              </div>
            ))}
          </div>
        </aside>
      </section>

      <section className="download-band">
        <div className="download-band-copy">
          <h2>Ready to ship clean clips?</h2>
          <p>
            The release portal includes the ZIP, install checklist, ffmpeg/ffprobe requirement notes, and changelog.
          </p>
        </div>
        <div className="download-band-actions">
          <a className="cta-primary" href={releaseZipUrl}>
            <Icon name="download" /> Download {releaseVersion}
          </a>
          <a className="cta-secondary" href={releasesUrl}>
            View all GitHub releases
          </a>
        </div>
      </section>
    </main>
  );
}
