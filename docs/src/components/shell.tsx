import { useEffect, type ReactNode } from "react";
import appIcon from "../assets/app-icon.png";
import { releaseZipUrl, repositoryUrl } from "../content/site";
import { HashLink, useHashLocation } from "../router";

const navItems = [
  { label: "Overview", to: "/" },
  { label: "Features", to: "/?section=features" },
  { label: "Screenshots", to: "/?section=screenshots" },
  { label: "Changelog", to: "/releases?section=changelog" },
  { label: "Docs", to: "/docs" },
  { label: "Releases", to: "/releases" },
] as const;

export function Shell({ children }: { children: ReactNode }) {
  const location = useHashLocation();
  const section = new URLSearchParams(location.search).get("section");

  useEffect(() => {
    window.scrollTo({ top: 0, behavior: "instant" as ScrollBehavior });
  }, [location.pathname]);

  const isActive = (to: string) => {
    if (to === "/") {
      return location.pathname === "/" && (section === null || section === "overview");
    }
    if (to === "/?section=features") {
      return location.pathname === "/" && section === "features";
    }
    if (to === "/?section=screenshots") {
      return location.pathname === "/" && section === "screenshots";
    }
    if (to === "/releases?section=changelog") {
      return location.pathname === "/releases" && section === "changelog";
    }
    if (to === "/releases") {
      return location.pathname === "/releases" && section !== "changelog";
    }
    return location.pathname === to;
  };

  return (
    <div className="site-shell">
      <div className="page-orb page-orb-left" />
      <div className="page-orb page-orb-right" />
      <header className="topbar">
        <HashLink to="/" className="brandmark">
          <img src={appIcon} alt="" className="brandmark-icon" />
          <span>
            <strong>VidChopper</strong>
            <small>Offline chapter export utility</small>
          </span>
        </HashLink>
        <nav className="topnav" aria-label="Primary">
          {navItems.map((item) => (
            <HashLink
              key={item.to}
              to={item.to}
              className={`topnav-link ${isActive(item.to) ? "topnav-link-active" : ""}`}
            >
              {item.label}
            </HashLink>
          ))}
        </nav>
        <div className="topbar-actions">
          <a className="ghost-link" href={repositoryUrl}>
            GitHub
          </a>
          <a className="primary-link" href={releaseZipUrl}>
            Download ZIP
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
          <a href={repositoryUrl}>Repository</a>
          <a href={releaseZipUrl}>Latest ZIP</a>
        </div>
      </footer>
    </div>
  );
}
