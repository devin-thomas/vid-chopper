import { SiteLink } from "../router";

export function NotFoundPage() {
  return (
    <main className="page-stack">
      <section className="subpage-hero not-found-panel">
        <div>
          <div className="hero-kicker">404 / Not found</div>
          <h1>This route is not part of the VidChopper site.</h1>
          <p>
            Use the canonical documentation index for supported product, CLI,
            ChapterFile, and release routes.
          </p>
          <div className="hero-actions">
            <SiteLink className="cta-primary" to="/docs">
              Open documentation
            </SiteLink>
            <SiteLink className="cta-secondary" to="/">
              Return home
            </SiteLink>
          </div>
        </div>
      </section>
    </main>
  );
}
