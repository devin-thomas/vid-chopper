import { useEffect } from "react";
import { NavLink, Outlet, useLocation } from "react-router-dom";
import appIcon from "../assets/app-icon.png";
import { releaseZipUrl, repositoryUrl } from "../content/site";

const navItems = [
  { label: "Product", to: "/" },
  { label: "Features", to: "/features" },
  { label: "Release", to: "/download" },
  { label: "Docs", to: "/docs" },
] as const;

export function Shell() {
  const location = useLocation();

  useEffect(() => {
    window.scrollTo({ top: 0, behavior: "instant" as ScrollBehavior });
  }, [location.pathname]);

  return (
    <div className="site-shell">
      <div className="page-orb page-orb-left" />
      <div className="page-orb page-orb-right" />
      <header className="topbar">
        <NavLink to="/" className="brandmark">
          <img src={appIcon} alt="" className="brandmark-icon" />
          <span>
            <strong>VidChopper</strong>
            <small>Desktop chapter export</small>
          </span>
        </NavLink>
        <nav className="topnav" aria-label="Primary">
          {navItems.map((item) => (
            <NavLink
              key={item.to}
              to={item.to}
              className={({ isActive }) => `topnav-link ${isActive ? "topnav-link-active" : ""}`}
            >
              {item.label}
            </NavLink>
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
      <Outlet />
      <footer className="site-footer">
        <div>
          <h3>VidChopper</h3>
          <p>
            Windows-first desktop tooling for turning one source video into precise chapter clips with ffmpeg.
          </p>
        </div>
        <div className="footer-links">
          <a href={releaseZipUrl}>Latest ZIP</a>
          <a href={repositoryUrl}>Repository</a>
          <a href={`${repositoryUrl}/tree/main/knowledge`}>Knowledge Base</a>
        </div>
      </footer>
    </div>
  );
}
