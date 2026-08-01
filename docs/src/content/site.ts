export const repositoryUrl = "https://github.com/devin-thomas/vid-chopper";
export const releasesUrl = `${repositoryUrl}/releases`;
export const siteUrl = "https://vidchopper.app";
export const docsUrl = `${siteUrl}/docs`;
export const chapterFileSchemaUrl = `${siteUrl}/schemas/chapter-config/v1/schema.json`;
export const chapterFileSampleUrl = `${siteUrl}/samples/chapter-config/v1/chapter-config.json`;
export const releaseMetadataUrl = `${siteUrl}/releases/v0.3.0-beta/manifest.json`;
export const agentSkillUrl = `${siteUrl}/agents/vidchopper-cli/SKILL.md`;
export const releaseVersion = "v0.3.0-beta";
export const releaseZipUrl =
  "https://github.com/devin-thomas/vid-chopper/releases/download/v0.3.0-beta/VidChopper-0.3.0-beta-windows-x64.zip";

export const docsLinks = [
  {
    title: "ChapterFile schema",
    description:
      "Version 1 machine-readable JSON Schema for ChapterFiles consumed by the released CLI.",
    href: chapterFileSchemaUrl,
  },
  {
    title: "Safe ChapterFile sample",
    description:
      "A versioned JSON sample with deterministic sibling output naming and explicit chapter ranges.",
    href: chapterFileSampleUrl,
  },
  {
    title: "VidChopper CLI agent skill",
    description:
      "First-party local-media workflow with dry-run, collision confirmation, and manifest verification gates.",
    href: agentSkillUrl,
  },
  {
    title: "Release metadata",
    description:
      "Version, source commit, package URL, size, checksum, and schema compatibility for v0.3.0-beta.",
    href: releaseMetadataUrl,
  },
  {
    title: "Repository source docs",
    description:
      "Detailed settings, schema, verification, architecture, and future-agent evidence in source control.",
    href: `${repositoryUrl}/tree/main/docs`,
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
  {
    version: "v0.2.0-alpha",
    date: "Desktop alpha",
    note: "Portable Qt workflow and Pages release portal.",
  },
  {
    version: "v0.1.1-alpha",
    date: "Alpha maintenance",
    note: "Early portable-package corrections.",
  },
  {
    version: "v0.1.0-alpha",
    date: "Initial alpha release",
    note: "First portable GitHub prerelease.",
  },
] as const;

export const changelogEntries = [
  {
    title: "Complete Qt-free CLI",
    detail:
      "Direct and chop modes now support explicit ChapterFiles, dry runs, exports, progress summaries, and manifests.",
    tag: "CLI",
  },
  {
    title: "Proven portable package",
    detail:
      "A second clean Windows runner extracts and tests the exact ZIP before GitHub publishes the prerelease.",
    tag: "Release",
  },
  {
    title: "ChapterBuilder compatibility",
    detail:
      "The game-neutral contract is covered by a 16-chapter public tournament fixture and packaged CLI smoke test.",
    tag: "Contract",
  },
] as const;

export const keyFeatures = [
  {
    title: "Chapter-aware import",
    detail:
      "Start from embedded metadata when it exists, or seed a clean six-part layout when it does not.",
  },
  {
    title: "Timestamp editing",
    detail:
      "Work in millisecond or frame-based time displays while keeping the chapter plan internally consistent.",
  },
  {
    title: "Advanced export control",
    detail:
      "Tune naming, container, encoder, seek mode, audio handling, manifest output, and overwrite behavior.",
  },
  {
    title: "GPU-aware defaults",
    detail:
      "Prefer HEVC NVENC when supported, then fall back to x264 for predictable compatibility.",
  },
  {
    title: "Portable config path",
    detail:
      "Store VidChopper.ini beside the executable when writable, with a safe AppData fallback otherwise.",
  },
  {
    title: "Release-ready workflow",
    detail:
      "Download a portable ZIP, point the app at ffmpeg/ffprobe, and start exporting without a local Qt build.",
  },
] as const;

export const roadmap = [
  { item: "Desktop feature round through v0.2.0-alpha", status: "Shipped" },
  {
    item: "Complete Qt-free CLI and safety gates for v0.3.0-beta",
    status: "Shipped",
  },
  {
    item: "Verify the ChapterBuilder companion contract end to end",
    status: "Shipped",
  },
  {
    item: "Converge GUI and CLI on one shared engine for stable v1.0.0",
    status: "Later",
  },
] as const;

export const docsGuideposts = [
  {
    slug: "getting-started",
    path: "/docs/getting-started",
    title: "Getting started",
    summary:
      "Install the portable Windows package and verify the local tools before reading or changing user media.",
    points: [
      "Use the v0.3.0-beta Windows x64 ZIP from the verified GitHub prerelease.",
      "VidChopperCLI.exe and the GUI are bundled; ffmpeg and ffprobe are installed or configured separately.",
      "Keep the source video, ChapterFile, manifests, and clips local. The documentation site never receives them.",
    ],
    command: `& "C:\\Tools\\VidChopper\\VidChopperCLI.exe" --version
ffmpeg -version
ffprobe -version`,
  },
  {
    slug: "cli",
    path: "/docs/cli",
    title: "CLI invocation",
    summary:
      "Pass explicit local paths and select exactly one chapter source. Direct and chop forms share the same released behavior.",
    points: [
      "A ChapterFile path and --embedded are mutually exclusive.",
      "Use --crf for the released standalone CLI's tested x264 path; do not assume --cq selects NVENC.",
      "Run --help on the packaged binary when a command or flag is uncertain.",
    ],
    command: `& "C:\\Tools\\VidChopper\\VidChopperCLI.exe" "C:\\Media\\source.mp4" "C:\\Media\\chapters.json" --dry-run`,
  },
  {
    slug: "chapterfiles",
    path: "/docs/chapterfiles",
    title: "ChapterFiles",
    summary:
      "JSON and YAML use schema version 1 with ordered, non-overlapping chapter ranges and optional output/encoder overrides.",
    points: [
      "Validate against the canonical version 1 schema before dry-run, then let the CLI apply timeline validation.",
      "The published sample writes to the deterministic %source%_chapters sibling folder.",
      "Unknown fields, empty names, invalid timestamps, overlaps, and end-before-start ranges are errors.",
    ],
    links: [
      { label: "Open schema", href: chapterFileSchemaUrl },
      { label: "Open JSON sample", href: chapterFileSampleUrl },
    ],
  },
  {
    slug: "embedded-chapters",
    path: "/docs/embedded-chapters",
    title: "Embedded chapters",
    summary:
      "Select embedded metadata explicitly and inspect the resulting plan before any export.",
    points: [
      "Use --embedded only when ffprobe reports a usable embedded chapter set.",
      "If embedded chapters are absent, the CLI stops and reports the explicit ChapterFile alternative.",
      "Dry-run prints provenance, planned output names, and collisions without writing settings or output files.",
    ],
    command: `& "C:\\Tools\\VidChopper\\VidChopperCLI.exe" "C:\\Media\\source-with-chapters.mkv" --embedded --dry-run`,
  },
  {
    slug: "chapterbuilder",
    path: "/docs/chapterbuilder",
    title: "ChapterBuilder handoff",
    summary:
      "Use ChapterBuilder's version 1 JSON output directly; do not translate it into a new format or invent integration flags.",
    points: [
      "The release gate accepts the byte-identical 16-chapter TNS 2XKO #36 fixture without edits.",
      "Keep downloaded VODs and generated clips outside the repository and off remote services.",
      "Confirm the source duration covers every ChapterBuilder range before export.",
    ],
  },
  {
    slug: "dry-run-and-export",
    path: "/docs/dry-run-and-export",
    title: "Dry-run and confirmed export",
    summary:
      "Treat dry-run as a read-only plan and export as a separate consequential action that needs explicit confirmation.",
    points: [
      "Require exit code 0, Planned chapters: N, exact destination paths, and Existing output: no before an automatic handoff.",
      "The beta CLI defaults to overwrite and has no overwrite flag. List every collision and obtain explicit approval.",
      "Immediately recheck every planned path after approval. If a path appeared or changed, stop and re-gate.",
      "Run the identical command without --dry-run only after the review is accepted.",
    ],
    command: `& "C:\\Tools\\VidChopper\\VidChopperCLI.exe" chop "C:\\Media\\source.mp4" "C:\\Media\\chapters.json" --aggregate-json "C:\\Media\\source_chapters\\run-manifest.json" --dry-run
& "C:\\Tools\\VidChopper\\VidChopperCLI.exe" chop "C:\\Media\\source.mp4" "C:\\Media\\chapters.json" --aggregate-json "C:\\Media\\source_chapters\\run-manifest.json"`,
  },
  {
    slug: "manifests",
    path: "/docs/manifests",
    title: "Manifest verification",
    summary:
      "Exit code zero is necessary but not sufficient: verify the reported files and machine-readable job state.",
    points: [
      "The CLI prints Summary: exported=N, failed=0, skipped=0 for a fully successful run.",
      "Each job manifest must report jobStatus: success and every segment processState: success.",
      "Match segment count and output-file count to the dry-run chapter count, then probe every clip.",
      "Aggregate JSON/CSV manifests are optional run-level summaries; a write failure makes the run nonzero.",
    ],
  },
  {
    slug: "settings-and-failures",
    path: "/docs/settings-and-failures",
    title: "Settings and failures",
    summary:
      "CLI flags win over ChapterFile, CLI settings, optional GUI import, and built-in defaults; failures stay actionable.",
    points: [
      "Dry-runs never create VidChopperCLI.ini. A normal run may create it beside the executable.",
      "Exit 1 is usage or validation, exit 2 is export/manifest failure, and exit 3 is probe/tool startup failure.",
      "After exit 2, verify and report individually preserved successful clips but withhold overall success.",
      "Do not retry with guessed paths, changed settings, or unsupported flags.",
    ],
  },
  {
    slug: "safety",
    path: "/docs/safety",
    title: "Safety boundaries",
    summary:
      "Local inspection is read-only by default; overwrite, install, upload, publish, spend, and external-system changes remain human gates.",
    points: [
      "Never upload source media, ChapterFiles, private paths, manifests, or clips merely to make an agent workflow easier.",
      "Ask before downloads, installations, PATH edits, settings changes, exports with collisions, or external publication.",
      "A broad instruction does not authorize unlisted paths or a new class of consequential action.",
      "Report exact local output locations and bounded error context without leaking private data into remote logs.",
    ],
  },
] as const;

export const docsQuickLinks = [
  { label: "ChapterFile schema", href: chapterFileSchemaUrl },
  { label: "JSON sample", href: chapterFileSampleUrl },
  { label: "Release metadata", href: releaseMetadataUrl },
  { label: "GitHub release", href: `${releasesUrl}/tag/${releaseVersion}` },
] as const;
