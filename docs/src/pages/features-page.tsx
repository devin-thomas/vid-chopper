import { DesktopFrame } from "../components/desktop-frame";
import { SectionHeading } from "../components/section-heading";
import { keyFeatures, workflowSteps } from "../content/site";

export function FeaturesPage() {
  return (
    <main className="page-stack">
      <section className="subpage-hero">
        <div>
          <h1>The product is a focused desktop pipeline, not a vague promise of “AI video tooling.”</h1>
          <p>
            VidChopper is opinionated about one job: importing chapters, cleaning the layout, and exporting clips
            through ffmpeg with native desktop affordances and explicit settings.
          </p>
        </div>
        <DesktopFrame title="Export coordinator" accent="#ff8b5f" detail="Sequential jobs" variant="timeline" />
      </section>

      <section className="workflow-band">
        <SectionHeading
          title="Workflow anatomy"
          body="The product stays tight by keeping each stage legible instead of pretending everything is one generic “export” button."
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

      <section className="showcase-panel">
        <SectionHeading
          title="Desktop surfaces shown as actual product anatomy"
          body="These previews are deliberately code-native mockups of the app shell, not generic marketing cards."
        />
        <div className="surface-grid">
          <DesktopFrame title="Chapter grid" accent="#8e44ff" detail="Edit names + timing" variant="editor" />
          <DesktopFrame title="Advanced settings" accent="#5fd39e" detail="Encoder + output" variant="settings" />
          <DesktopFrame title="Logs + progress" accent="#ff8b5f" detail="Curated activity" variant="timeline" />
        </div>
      </section>

      <section className="feature-river">
        <SectionHeading
          title="What the current alpha actually ships"
          body="Every item below corresponds to implemented repository behavior or release packaging that is already live."
        />
        <div className="feature-river-grid">
          {keyFeatures.map((feature, index) => (
            <article key={feature.title} className={`feature-panel feature-panel-${(index + 1) % 3}`}>
              <h3>{feature.title}</h3>
              <p>{feature.detail}</p>
            </article>
          ))}
        </div>
      </section>
    </main>
  );
}
