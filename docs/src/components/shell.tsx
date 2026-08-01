import { useEffect, useState, type ReactNode } from "react";
import appIcon from "../assets/app-icon.png";
import { releaseZipUrl, repositoryUrl } from "../content/site";
import { HashLink, useHashLocation } from "../router";
import { Icon } from "./icon";

const navItems = [
  { label: "Overview", to: "/" },
  { label: "Features", to: "/?section=features" },
  { label: "Docs", to: "/docs" },
  { label: "Releases", to: "/releases" },
] as const;

export function Shell({ children }: { children: ReactNode }) {
  const location = useHashLocation();
  const [menuOpen, setMenuOpen] = useState(false);
  const section = new URLSearchParams(location.search).get("section");

  useEffect(() => {
    setMenuOpen(false);
    if (section === null) {
      window.scrollTo({ top: 0, behavior: "instant" as ScrollBehavior });
    }
  }, [location.pathname, section]);

  const isActive = (to: string) => {
    if (to === "/") {
      return location.pathname === "/" && (section === null || section === "overview");
    }
    if (to === "/?section=features") {
      return location.pathname === "/" && section === "features";
    }
    if (to === "/releases") {
      return location.pathname === "/releases";
    }
    return location.pathname === to;
  };

  return (
    <div className="site-shell">
      <header className="topbar">
        <HashLink to="/" className="brandmark">
          <img src={appIcon} alt="" className="brandmark-icon" />
          <span>
            <strong>VidChopper</strong>
            <small>Offline chapter export utility</small>
          </span>
        </HashLink>
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
            <HashLink
              key={item.to}
              to={item.to}
              className={`topnav-link ${isActive(item.to) ? "topnav-link-active" : ""}`}
              aria-current={isActive(item.to) ? "page" : undefined}
              onClick={() => setMenuOpen(false)}
            >
              {item.label}
            </HashLink>
          ))}
        </nav>
        <div className="topbar-actions">
          <a className="primary-link" href={releaseZipUrl}>
            <Icon name="download" /> Download ZIP
          </a>
        </div>
      </header>
      {children}
      <footer className="site-footer">
        <div>
          <h3>VidChopper</h3>
          <p>
            Windows-first desktop tooling for turning one source video into precise chapter clips with ffmpeg.
          </p>
        </div>
        <div className="footer-links">
          <HashLink to="/releases?section=changelog">Changelog</HashLink>
          <HashLink to="/docs">Docs</HashLink>
          <a href={repositoryUrl}>
            <Icon name="github" /> Repository
          </a>
          <a href={releaseZipUrl}>Latest ZIP</a>
        </div>
      </footer>
    </div>
  );
}
