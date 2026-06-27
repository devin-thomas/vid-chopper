import appIcon from "../assets/app-icon.png";
import { docsLinks, repositoryUrl } from "../content/site";

const docSections = [
  {
    title: "Getting started",
    body: "Download the Windows ZIP if you want the app, or build from source with CMake and a Qt 6.9 SDK if you are working on the desktop codebase.",
  },
  {
    title: "Architecture",
    body: "The repo is intentionally split into a Qt-free `src/core` and a Qt Widgets `src/qt` shell so the export logic stays testable without Qt.",
  },
  {
    title: "Coding style",
    body: "C++20, trailing return types, project integer aliases, designated initializers, aggressive const-correctness, and a hard Qt/core boundary are not optional house style preferences.",
  },
  {
    title: "Quality gates",
    body: "The release path depends on clang-format, clang-tidy over the core, fast and slow core tests, GUI build validation, and Pages/release workflows.",
  },
  {
    title: "Knowledge base",
    body: "Future-agent continuity now lives in the split `knowledge/` directory, mirrored into Linear documents for the VidChopper team.",
  },
] as const;

export function DocsPage() {
  return (
    <main className="page-stack">
      <section className="subpage-hero docs-hero">
        <div>
          <h1>Developer docs for the shipped desktop app, not generic scaffolding notes.</h1>
          <p>
            This page points at the real repository docs, the split knowledge base, and the current build, testing,
            and release workflow shape so a future agent or contributor can re-enter the project quickly.
          </p>
        </div>
        <aside className="toc-panel">
          <div className="toc-art">
            <img src={appIcon} alt="" />
          </div>
          <h2>On this page</h2>
          <ul>
            <li>Source docs</li>
            <li>Project guideposts</li>
            <li>Build + release path</li>
            <li>Agent knowledge base</li>
          </ul>
        </aside>
      </section>

      <section className="docs-layout">
        <nav className="docs-sidebar" aria-label="Documentation sections">
          {docSections.map((section) => (
            <a key={section.title} href={`#${section.title.toLowerCase().replaceAll(" ", "-")}`}>
              {section.title}
            </a>
          ))}
        </nav>
        <div className="docs-content">
          <section className="docs-card-stack">
            {docsLinks.map((link) => (
              <article key={link.title} className="doc-link-card">
                <h2>{link.title}</h2>
                <p>{link.description}</p>
                <a href={link.href}>Open source doc</a>
              </article>
            ))}
          </section>

          <section className="docs-guide-grid">
            {docSections.map((section) => (
              <article key={section.title} id={section.title.toLowerCase().replaceAll(" ", "-")} className="guide-card">
                <h2>{section.title}</h2>
                <p>{section.body}</p>
              </article>
            ))}
          </section>

          <section className="docs-callout">
            <div>
              <h2>Repository entry points</h2>
              <ul>
                <li>`README.md` for the product, build path, and release flow.</li>
                <li>`CODING_STYLE.md` for the repo’s engineering contract.</li>
                <li>`features_plan.md` for release-task progress.</li>
                <li>`knowledge/` for architecture, workflows, progress, and installed-skill notes.</li>
              </ul>
            </div>
            <a className="cta-secondary" href={repositoryUrl}>
              Open the repository
            </a>
          </section>
        </div>
      </section>
    </main>
  );
}
