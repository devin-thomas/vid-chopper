import { useEffect, useRef, useState, type ReactNode } from "react";
import routeContract from "../../routes.json";
import appIcon from "../assets/app-icon.png";
import {
  releasePageUrl,
  repositoryUrl,
  siteUrl,
} from "../content/site";
import {
  applyRouteMetadata,
  focusWithoutScroll,
  scrollToTop,
} from "../lib/dom-safety";
import {
  SiteLink,
  useSiteLocation,
  useSiteSearchParams,
} from "../router";
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
  const section = useSiteSearchParams().get("section");
  const previousRoute = useRef(`${location.pathname}${location.search}`);

  useEffect(() => {
    setMenuOpen(false);
    if (section === null) {
      scrollToTop();
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
    applyRouteMetadata(title, canonicalPath, siteUrl);
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
      focusWithoutScroll(target);
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
          <a className="primary-link" href={releasePageUrl}>
            <Icon name="download" /> Download 1.2.0
          </a>
        </div>
      </header>
      {children}
      <footer className="site-footer">
        <div>
          <h3>VidChopper</h3>
          <p>
            Native Windows and Apple Silicon Mac tooling for turning one
            source video into precise chapter clips with ffmpeg.
          </p>
        </div>
        <div className="footer-links">
          <SiteLink to="/releases?section=changelog">Changelog</SiteLink>
          <SiteLink to="/docs">Docs</SiteLink>
          <a href={repositoryUrl}>
            <Icon name="github" /> Repository
          </a>
          <a href={releasePageUrl}>Latest release</a>
        </div>
      </footer>
    </div>
  );
}
