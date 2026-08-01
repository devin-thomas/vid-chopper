import { useEffect, useRef, useState, type ReactNode } from "react";
import routeContract from "../../routes.json";
import appIcon from "../assets/app-icon.png";
import {
  docsUrl,
  releaseZipUrl,
  repositoryUrl,
  siteUrl,
} from "../content/site";
import { legacyPagesBuild, SiteLink, useSiteLocation } from "../router";
import { Icon } from "./icon";

const navItems = [
  { label: "Overview", to: "/" },
  { label: "Features", to: "/?section=features" },
  { label: "Docs", to: "/docs" },
  { label: "Releases", to: "/releases" },
] as const;

const routeTitles = new Map(Object.entries(routeContract.htmlTitles));

export function Shell({ children }: { children: ReactNode }) {
  const location = useSiteLocation();
  const [menuOpen, setMenuOpen] = useState(false);
  const section = new URLSearchParams(location.search).get("section");
  const previousRoute = useRef(`${location.pathname}${location.search}`);

  useEffect(() => {
    setMenuOpen(false);
    if (section === null) {
      window.scrollTo({ top: 0, behavior: "instant" as ScrollBehavior });
    }
  }, [location.pathname, section]);

  useEffect(() => {
    const featuresView = location.pathname === "/" && section === "features";
    const titleRoute = featuresView ? "/features" : location.pathname;
    const title =
      routeTitles.get(titleRoute) ?? "Page not found | VidChopper";
    const canonicalPath = featuresView
      ? "/?section=features"
      : location.pathname;
    const canonicalUrl = new URL(canonicalPath, siteUrl).href;
    document.title = title;
    document
      .querySelector<HTMLLinkElement>('link[rel="canonical"]')
      ?.setAttribute("href", canonicalUrl);
    document
      .querySelector<HTMLMetaElement>('meta[property="og:title"]')
      ?.setAttribute("content", title);
    document
      .querySelector<HTMLMetaElement>('meta[property="og:url"]')
      ?.setAttribute("content", canonicalUrl);
  }, [location.pathname, section]);

  useEffect(() => {
    const route = `${location.pathname}${location.search}`;
    if (previousRoute.current === route) return;
    previousRoute.current = route;

    const frame = window.requestAnimationFrame(() => {
      const target =
        document.querySelector<HTMLElement>("[data-route-focus]") ??
        document.querySelector<HTMLElement>("main");
      if (target === null) return;
      if (!target.hasAttribute("tabindex")) {
        target.setAttribute("tabindex", "-1");
      }
      target.focus({ preventScroll: true });
    });

    return () => window.cancelAnimationFrame(frame);
  }, [location.pathname, location.search]);

  const isActive = (to: string) => {
    if (to === "/") {
      return (
        location.pathname === "/" &&
        (section === null || section === "overview")
      );
    }
    if (to === "/?section=features") {
      return location.pathname === "/" && section === "features";
    }
    if (to === "/releases") {
      return location.pathname === "/releases";
    }
    if (to === "/docs") {
      return (
        location.pathname === "/docs" || location.pathname.startsWith("/docs/")
      );
    }
    return location.pathname === to;
  };

  return (
    <div className="site-shell">
      <header className="topbar">
        <SiteLink to="/" className="brandmark">
          <img src={appIcon} alt="" className="brandmark-icon" />
          <span>
            <strong>VidChopper</strong>
            <small>Offline chapter export utility</small>
          </span>
        </SiteLink>
        <button
          className="menu-toggle"
          type="button"
          aria-expanded={menuOpen}
          aria-controls="primary-navigation"
          onClick={() => setMenuOpen((open) => !open)}
        >
          <span className="sr-only">Toggle navigation</span>
          <Icon name={menuOpen ? "close" : "menu"} />
        </button>
        <nav
          id="primary-navigation"
          className={`topnav ${menuOpen ? "topnav-open" : ""}`}
          aria-label="Primary"
        >
          {navItems.map((item) => (
            <SiteLink
              key={item.to}
              to={item.to}
              className={`topnav-link ${isActive(item.to) ? "topnav-link-active" : ""}`}
              aria-current={isActive(item.to) ? "page" : undefined}
              onClick={() => setMenuOpen(false)}
            >
              {item.label}
            </SiteLink>
          ))}
        </nav>
        <div className="topbar-actions">
          <a className="primary-link" href={releaseZipUrl}>
            <Icon name="download" /> Download ZIP
          </a>
        </div>
      </header>
      {legacyPagesBuild ? (
        <aside className="legacy-site-notice" aria-label="Legacy site notice">
          GitHub Pages is the legacy mirror. The canonical documentation home is{" "}
          <a href={docsUrl}>{docsUrl}</a>.
        </aside>
      ) : null}
      {children}
      <footer className="site-footer">
        <div>
          <h3>VidChopper</h3>
          <p>
            Windows-first desktop tooling for turning one source video into
            precise chapter clips with ffmpeg.
          </p>
        </div>
        <div className="footer-links">
          <SiteLink to="/releases?section=changelog">Changelog</SiteLink>
          <SiteLink to="/docs">Docs</SiteLink>
          <a href={repositoryUrl}>
            <Icon name="github" /> Repository
          </a>
          <a href={releaseZipUrl}>Latest ZIP</a>
        </div>
      </footer>
    </div>
  );
}
