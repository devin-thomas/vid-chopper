type DesktopFrameProps = {
  title: string;
  accent: string;
  detail: string;
  variant?: "editor" | "timeline" | "settings";
};

export function DesktopFrame({ title, accent, detail, variant = "editor" }: DesktopFrameProps) {
  return (
    <article className="desktop-frame">
      <div className="desktop-chrome">
        <span className="chrome-dot bg-[#ff6b7e]" />
        <span className="chrome-dot bg-[#f9b04f]" />
        <span className="chrome-dot bg-[#5fd39e]" />
        <span className="chrome-title">{title}</span>
      </div>
      <div className="desktop-body">
        <aside className="desktop-rail">
          <div className="rail-badge" style={{ background: accent }} />
          <div className="rail-line w-9" />
          <div className="rail-line w-7" />
          <div className="rail-line w-10" />
          <div className="rail-line w-6" />
        </aside>
        <div className="desktop-canvas">
          <div className="desktop-toolbar">
            <div className="toolbar-pill" style={{ borderColor: accent, color: accent }}>
              {detail}
            </div>
            <div className="toolbar-line w-24" />
            <div className="toolbar-line w-16" />
          </div>
          {variant === "editor" ? <EditorVariant accent={accent} /> : null}
          {variant === "timeline" ? <TimelineVariant accent={accent} /> : null}
          {variant === "settings" ? <SettingsVariant accent={accent} /> : null}
        </div>
      </div>
    </article>
  );
}

function EditorVariant({ accent }: { accent: string }) {
  return (
    <div className="variant-stack">
      <div className="hero-track">
        <div className="track-wave" style={{ background: `linear-gradient(90deg, ${accent}, rgba(255,255,255,0.08))` }} />
      </div>
      <div className="data-grid">
        {["Cold open", "Chapter plan", "Timing pass", "Output folder"].map((label, index) => (
          <div key={label} className={`data-row ${index === 1 ? "data-row-active" : ""}`}>
            <span>{label}</span>
            <span>{index === 3 ? "Ready" : `0${index + 1}`}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

function TimelineVariant({ accent }: { accent: string }) {
  return (
    <div className="variant-stack">
      <div className="timeline-grid">
        {[0, 1, 2, 3].map((row) => (
          <div key={row} className="timeline-row">
            <div className="timeline-label" />
            <div className="timeline-lane">
              <div
                className="timeline-bar"
                style={{
                  background: `linear-gradient(135deg, ${accent}, rgba(255,255,255,0.22))`,
                  width: `${58 + row * 9}%`,
                }}
              />
            </div>
          </div>
        ))}
      </div>
      <div className="desktop-caption">Sequential clip export keeps progress and validation legible for long jobs.</div>
    </div>
  );
}

function SettingsVariant({ accent }: { accent: string }) {
  return (
    <div className="variant-stack">
      <div className="settings-grid">
        {[0, 1, 2, 3].map((row) => (
          <div key={row} className="setting-pair">
            <div className="setting-label" />
            <div className="setting-control">
              <div className="setting-track">
                <div className="setting-thumb" style={{ background: accent, marginLeft: row % 2 === 0 ? "auto" : 0 }} />
              </div>
            </div>
          </div>
        ))}
      </div>
      <div className="metric-strip">
        <div className="metric-card">
          <span>Encoder</span>
          <strong>HEVC NVENC</strong>
        </div>
        <div className="metric-card">
          <span>Container</span>
          <strong>MP4</strong>
        </div>
      </div>
    </div>
  );
}
