export const repositoryUrl = "https://github.com/devin-thomas/vid-chopper";
export const releasesUrl = `${repositoryUrl}/releases`;
export const releaseZipUrl =
  "https://github.com/devin-thomas/vid-chopper/releases/download/v0.2.0-alpha/VidChopper-windows-x64.zip";

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
    title: "Feature Plan",
    description: "Task-by-task status for the v0.2.0-alpha round and the Pages rewrite milestone.",
    href: `${repositoryUrl}/blob/main/features_plan.md`,
  },
  {
    title: "Knowledge Base",
    description: "Structured future-agent documentation for architecture, workflows, progress, and installed skills.",
    href: `${repositoryUrl}/tree/main/knowledge`,
  },
] as const;

export const releaseFacts = [
  { label: "Version", value: "v0.2.0-alpha" },
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
  "Qt runtime and VC++ runtime bundled beside VidChopper.exe",
  "Dark Qt desktop workflow for chapter editing and export orchestration",
  "Pure C++ core for validation, planning, naming, and ffmpeg command generation",
  "GitHub Actions coverage for lint, core tests, GUI build, release packaging, and Pages deployment",
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
  { item: "Portable ZIP release packaging and release workflow", status: "Shipped" },
  { item: "GitHub Pages rewrite to React, TypeScript, and Tailwind", status: "Shipped" },
  { item: "Next focus: polish the docs surface and release-story continuity over future releases", status: "Next" },
] as const;
