import { useState } from "react";
import appIcon from "../assets/app-icon.png";
import { docsGuideposts, docsLinks, docsQuickLinks, repositoryUrl } from "../content/site";

export function DocsPage() {
  const [query, setQuery] = useState("");
  const lowered = query.trim().toLowerCase();

  const filteredLinks = docsLinks.filter((link) =>
    lowered.length === 0
      ? true
      : `${link.title} ${link.description}`.toLowerCase().includes(lowered),
  );

  const filteredGuideposts = docsGuideposts.filter((section) =>
    lowered.length === 0
      ? true
      : `${section.title} ${section.body}`.toLowerCase().includes(lowered),
  );

  return (
    <main className="page-stack docs-page-shell">
      <section className="docs-layout docs-layout-full">
        <nav className="docs-sidebar" aria-label="Documentation sections">
          <div className="docs-sidebar-brand">
            <img src={appIcon} alt="" />
            <div>
              <strong>VidChopper</strong>
              <span>Developer Docs</span>
            </div>
          </div>
          <label className="docs-search">
            <span className="sr-only">Filter docs</span>
            <input
              type="search"
              value={query}
              onChange={(event) => setQuery(event.target.value)}
              placeholder="Filter docs, guides, and links"
            />
          </label>
          <div className="docs-sidebar-section">
            <span>Guideposts</span>
            {docsGuideposts.map((section) => (
              <a key={section.title} href={`#${section.title.toLowerCase().replaceAll(" ", "-")}`}>
                {section.title}
              </a>
            ))}
          </div>
          <div className="docs-sidebar-section">
            <span>Sources</span>
            {docsLinks.map((link) => (
              <a key={link.title} href={link.href}>
                {link.title}
              </a>
            ))}
          </div>
        </nav>
        <div className="docs-content">
          <section className="docs-hero-grid">
            <div className="docs-hero-copy">
              <div className="hero-kicker">Developer documentation</div>
              <h1>Build, ship, and extend the real desktop app without re-learning the repo from scratch.</h1>
              <p>
                This page points at the source docs, the split knowledge base, the build and release workflow, and the
                future-agent handoff material that now lives with the product.
              </p>
              <div className="hero-actions">
                <a className="cta-primary" href={`${repositoryUrl}/tree/main/knowledge`}>
                  Open knowledge base
                </a>
                <a className="cta-secondary" href={repositoryUrl}>
                  View repository
                </a>
              </div>
            </div>
            <aside className="docs-hero-art">
              <img src={appIcon} alt="" />
            </aside>
          </section>

          <section className="docs-card-stack">
            {filteredLinks.map((link) => (
              <article key={link.title} className="doc-link-card">
                <h2>{link.title}</h2>
                <p>{link.description}</p>
                <a href={link.href}>Open source doc</a>
              </article>
            ))}
          </section>

          <section className="docs-guide-grid">
            {filteredGuideposts.map((section) => (
              <article
                key={section.title}
                id={section.title.toLowerCase().replaceAll(" ", "-")}
                className="guide-card"
              >
                <h2>{section.title}</h2>
                <p>{section.body}</p>
              </article>
            ))}
          </section>

          <section className="docs-callout">
            <div>
              <h2>Agent knowledge base is a first-class project artifact now.</h2>
              <ul>
                <li>`README.md` for the product, build path, and release flow.</li>
                <li>`CODING_STYLE.md` for the repo’s engineering contract.</li>
                <li>`features_plan.md` for release-task progress.</li>
                <li>`knowledge/` for architecture, workflows, progress, and installed-skill notes.</li>
              </ul>
            </div>
            <a className="cta-secondary" href={`${repositoryUrl}/tree/main/knowledge`}>
              Open knowledge base
            </a>
          </section>
        </div>
        <aside className="docs-rail">
          <div className="docs-rail-card">
            <h2>On this page</h2>
            <ul>
              {docsGuideposts.map((section) => (
                <li key={section.title}>
                  <a href={`#${section.title.toLowerCase().replaceAll(" ", "-")}`}>{section.title}</a>
                </li>
              ))}
            </ul>
          </div>
          <div className="docs-rail-card">
            <h2>Quick links</h2>
            <ul>
              {docsQuickLinks.map((link) => (
                <li key={link.label}>
                  <a href={link.href}>{link.label}</a>
                </li>
              ))}
            </ul>
          </div>
        </aside>
      </section>
    </main>
  );
}
