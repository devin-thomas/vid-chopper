import { useState } from "react";
import appIcon from "../assets/app-icon.png";
import { Icon } from "../components/icon";
import {
  agentSkillUrl,
  docsGuideposts,
  docsLinks,
  docsQuickLinks,
  repositoryUrl,
} from "../content/site";
import { SiteLink, useSiteLocation } from "../router";

type Guidepost = (typeof docsGuideposts)[number];

function GuideDetails({ section }: { section: Guidepost }) {
  return (
    <>
      <p>{section.summary}</p>
      <ul>
        {section.points.map((point) => (
          <li key={point}>{point}</li>
        ))}
      </ul>
      {"command" in section ? (
        <pre>
          <code>{section.command}</code>
        </pre>
      ) : null}
      {"links" in section ? (
        <div className="docs-inline-links">
          {section.links.map((link) => (
            <a key={link.href} href={link.href}>
              {link.label}
            </a>
          ))}
        </div>
      ) : null}
    </>
  );
}

export function DocsPage() {
  const [query, setQuery] = useState("");
  const { pathname } = useSiteLocation();
  const activeGuidepost = docsGuideposts.find(
    (section) => section.path === pathname,
  );
  const docsIndex = activeGuidepost === undefined;
  const lowered = query.trim().toLowerCase();

  const filteredLinks = docsLinks.filter((link) =>
    lowered.length === 0
      ? true
      : `${link.title} ${link.description}`.toLowerCase().includes(lowered),
  );

  const filteredGuideposts = docsGuideposts.filter((section) => {
    const searchable =
      `${section.title} ${section.summary} ${section.points.join(" ")}`.toLowerCase();
    return lowered.length === 0 || searchable.includes(lowered);
  });
  const hasIndexResults =
    filteredLinks.length > 0 || filteredGuideposts.length > 0;

  const docsNavigation = (
    <nav className="docs-sidebar" aria-label="Documentation sections">
      <div className="docs-sidebar-brand">
        <img src={appIcon} alt="" />
        <div>
          <strong>VidChopper</strong>
          <span>Product + CLI Docs</span>
        </div>
      </div>
      {docsIndex ? (
        <label className="docs-search">
          <span className="sr-only">Filter docs</span>
          <input
            type="search"
            value={query}
            onChange={(event) => setQuery(event.target.value)}
            placeholder="Filter docs, guides, and links"
          />
        </label>
      ) : (
        <SiteLink className="docs-index-link" to="/docs">
          Back to documentation home
        </SiteLink>
      )}
      <div className="docs-sidebar-section">
        <span>Guideposts</span>
        <SiteLink
          to="/docs"
          aria-current={pathname === "/docs" ? "page" : undefined}
        >
          Documentation home
        </SiteLink>
        {docsGuideposts.map((section) => (
          <SiteLink
            key={section.title}
            to={section.path}
            aria-current={
              activeGuidepost?.slug === section.slug ? "page" : undefined
            }
          >
            {section.title}
          </SiteLink>
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
  );

  return (
    <main className="page-stack docs-page-shell">
      <section
        className={`docs-layout docs-layout-full ${docsIndex ? "docs-layout-index" : "docs-layout-route"}`}
      >
        {docsIndex ? docsNavigation : null}
        <div className="docs-content">
          {docsIndex ? (
            <>
              <section className="docs-hero-grid">
                <div className="docs-hero-copy">
                  <div className="hero-kicker">
                    Product and CLI documentation
                  </div>
                  <h1 tabIndex={-1} data-route-focus>
                    Move from a local video to verified chapter clips without
                    guessing.
                  </h1>
                  <p>
                    Follow the released CLI contract from prerequisites and
                    chapter discovery through a safe dry-run, confirmed export,
                    and manifest verification. Stable routes work for both
                    people and automation.
                  </p>
                  <div className="hero-actions">
                    <SiteLink
                      className="cta-primary"
                      to="/docs/getting-started"
                    >
                      Start with setup
                    </SiteLink>
                    <a
                      className="cta-secondary"
                      href={`${repositoryUrl}/tree/main/docs`}
                    >
                      <Icon name="github" /> View repository
                    </a>
                  </div>
                </div>
                <aside className="docs-hero-art">
                  <img src={appIcon} alt="" />
                </aside>
              </section>

              {hasIndexResults ? (
                <>
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
                        id={section.slug}
                        className="guide-card"
                      >
                        <h2>{section.title}</h2>
                        <GuideDetails section={section} />
                        <SiteLink className="guide-card-link" to={section.path}>
                          Open stable section URL
                        </SiteLink>
                      </article>
                    ))}
                  </section>
                </>
              ) : (
                <section className="docs-empty-state" role="status">
                  <h2>No documentation matches "{query.trim()}".</h2>
                  <p>
                    Try a broader term or clear the filter to see every guide.
                  </p>
                  <button
                    className="cta-secondary"
                    type="button"
                    onClick={() => setQuery("")}
                  >
                    Clear filter
                  </button>
                </section>
              )}
            </>
          ) : (
            <article
              id={activeGuidepost.slug}
              className="docs-section-page"
              aria-labelledby={`${activeGuidepost.slug}-title`}
            >
              <div className="hero-kicker">VidChopper documentation</div>
              <h1
                id={`${activeGuidepost.slug}-title`}
                tabIndex={-1}
                data-route-focus
              >
                {activeGuidepost.title}
              </h1>
              <GuideDetails section={activeGuidepost} />
              <SiteLink className="guide-card-link" to="/docs">
                Browse all documentation
              </SiteLink>
            </article>
          )}

          <section className="docs-callout">
            <div>
              <h2>Machine-readable contracts stay beside the human journey.</h2>
              <ul>
                <li>Versioned ChapterFile schema and JSON/YAML samples.</li>
                <li>Immutable beta release metadata with package checksum.</li>
                <li>
                  Strict not-found behavior for unpublished or unknown machine
                  resources.
                </li>
                <li>
                  The first-party agent skill is staged at{" "}
                  <a href={agentSkillUrl}>{agentSkillUrl}</a>; VID-55 owns
                  production publication and live verification.
                </li>
              </ul>
            </div>
            <a
              className="cta-secondary"
              href={`${repositoryUrl}/tree/main/docs`}
            >
              Open source docs
            </a>
          </section>
        </div>
        {docsIndex ? null : docsNavigation}
        <aside className="docs-rail">
          <div className="docs-rail-card">
            <h2>On this page</h2>
            <ul>
              {docsGuideposts.map((section) => (
                <li key={section.title}>
                  <SiteLink
                    to={section.path}
                    aria-current={
                      activeGuidepost?.slug === section.slug
                        ? "page"
                        : undefined
                    }
                  >
                    {section.title}
                  </SiteLink>
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
