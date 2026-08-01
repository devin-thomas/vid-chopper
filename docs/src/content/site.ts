export const repositoryUrl = "https://github.com/devin-thomas/vid-chopper";
export const releasesUrl = `${repositoryUrl}/releases`;
export const releaseVersion = "v0.3.0-beta";
export const releaseZipUrl =
  "https://github.com/devin-thomas/vid-chopper/releases/download/v0.3.0-beta/VidChopper-0.3.0-beta-windows-x64.zip";

export const docsLinks = [
  {
    title: "Repository Overview",
    description: "Windows-first Qt 6 desktop app, release distribution, architecture notes, and constraints.",
    href: `${repositoryUrl}/blob/main/README.md`,
  },
  {
    title: "Coding Style",
    description: "The source of truth for the C++20, Qt, testing, and quality-gate conventions used in this repo.",
    href: `${repositoryUrl}/blob/main/CODING_STYLE.md`,
  },
  {
    title: "Release History",
    description: "Archived task-by-task record for the completed v0.2.0-alpha feature round.",
    href: `${repositoryUrl}/blob/main/features_plan.md`,
  },
  {
    title: "Knowledge Base",
    description: "Structured future-agent documentation for architecture, workflows, progress, and installed skills.",
    href: `${repositoryUrl}/tree/main/knowledge`,
  },
] as const;

export const releaseFacts = [
  { label: "Version", value: releaseVersion },
  { label: "Platform", value: "Windows 10/11 x64" },
  { label: "Package", value: "Portable ZIP release" },
  { label: "Runtime", value: "Qt + VC++ bundled" },
] as const;

export const workflowSteps = [
  {
    title: "Import a source video",
    text: "Probe a local file with ffprobe and start from embedded chapters or a generated starter layout.",
  },
  {
    title: "Edit exact chapter ranges",
    text: "Rename, retime, and inspect chapters in a dense desktop workflow built for long-form media cleanup.",
  },
  {
    title: "Export clean clips",
    text: "Run sequential ffmpeg exports with GPU-aware defaults, progress visibility, and explicit output control.",
  },
] as const;

export const releaseHighlights = [
  "Portable Windows x64 ZIP attached to the GitHub prerelease",
  "Complete Qt-free VidChopperCLI.exe packaged beside the Qt desktop app",
  "Explicit JSON/YAML ChapterFiles, dry-run planning, exports, and manifests",
  "ChapterBuilder compatibility verified with a public tournament fixture",
  "Clean Windows archive smoke testing before prerelease publication",
] as const;

export const releaseChecklist = [
  "Download the ZIP archive from the current GitHub release.",
  "Extract the portable folder to any writable location.",
  "Install ffmpeg and ffprobe separately, or set their paths in Advanced Settings.",
  "Launch VidChopper.exe and export from the native desktop workflow.",
] as const;

export const previousReleases = [
  { version: "v0.2.0-alpha", date: "Desktop alpha", note: "Portable Qt workflow and Pages release portal." },
  { version: "v0.1.1-alpha", date: "Alpha maintenance", note: "Early portable-package corrections." },
  { version: "v0.1.0-alpha", date: "Initial alpha release", note: "First portable GitHub prerelease." },
] as const;

export const changelogEntries = [
  {
    title: "Complete Qt-free CLI",
    detail: "Direct and chop modes now support explicit ChapterFiles, dry runs, exports, progress summaries, and manifests.",
    tag: "CLI",
  },
  {
    title: "Proven portable package",
    detail: "A second clean Windows runner extracts and tests the exact ZIP before GitHub publishes the prerelease.",
    tag: "Release",
  },
  {
    title: "ChapterBuilder compatibility",
    detail: "The game-neutral contract is covered by a 16-chapter public tournament fixture and packaged CLI smoke test.",
    tag: "Contract",
  },
] as const;

export const keyFeatures = [
  {
    title: "Chapter-aware import",
    detail: "Start from embedded metadata when it exists, or seed a clean six-part layout when it does not.",
  },
  {
    title: "Timestamp editing",
    detail: "Work in millisecond or frame-based time displays while keeping the chapter plan internally consistent.",
  },
  {
    title: "Advanced export control",
    detail: "Tune naming, container, encoder, seek mode, audio handling, manifest output, and overwrite behavior.",
  },
  {
    title: "GPU-aware defaults",
    detail: "Prefer HEVC NVENC when supported, then fall back to x264 for predictable compatibility.",
  },
  {
    title: "Portable config path",
    detail: "Store VidChopper.ini beside the executable when writable, with a safe AppData fallback otherwise.",
  },
  {
    title: "Release-ready workflow",
    detail: "Download a portable ZIP, point the app at ffmpeg/ffprobe, and start exporting without a local Qt build.",
  },
] as const;

export const roadmap = [
  { item: "Desktop feature round through v0.2.0-alpha", status: "Shipped" },
  { item: "Complete Qt-free CLI and safety gates for v0.3.0-beta", status: "Shipped" },
  { item: "Verify the ChapterBuilder companion contract end to end", status: "Shipped" },
  { item: "Converge GUI and CLI on one shared engine for stable v1.0.0", status: "Later" },
] as const;

export const docsGuideposts = [
  {
    title: "Getting Started",
    body: "Download the release ZIP if you want the app, or build from source with CMake and a Qt 6.9 SDK if you are working on the desktop codebase.",
  },
  {
    title: "Build from Source",
    body: "Use the core-only preset for fast validation, and the Qt preset when you need the real GUI executable or seeded demo captures.",
  },
  {
    title: "Architecture",
    body: "The repo is intentionally split into a Qt-free `src/core` and a Qt Widgets `src/qt` shell so domain logic stays testable without Qt.",
  },
  {
    title: "Coding Style",
    body: "C++20, trailing return types, designated initializers, project integer aliases, and a hard Qt/core boundary are part of the repo contract.",
  },
  {
    title: "Test Strategy",
    body: "Fast native tests protect the core logic while slower ffmpeg integration coverage validates the real media pipeline and packaging assumptions.",
  },
  {
    title: "Release Workflows",
    body: "CI, release packaging, Pages deployment, and the knowledge-base docs all need to stay aligned so future agents can keep shipping safely.",
  },
  {
    title: "Agent Knowledge Base",
    body: "The `knowledge/` tree is the future-agent handoff: architecture, workflows, progress, publishing notes, and installed-skill guidance.",
  },
] as const;

export const docsQuickLinks = [
  { label: "Repository", href: repositoryUrl },
  { label: "Releases", href: releasesUrl },
  { label: "Knowledge Base", href: `${repositoryUrl}/tree/main/knowledge` },
  { label: "Coding Style", href: `${repositoryUrl}/blob/main/CODING_STYLE.md` },
] as const;
