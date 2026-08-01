import { createHash } from "node:crypto";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptsDirectory = path.dirname(fileURLToPath(import.meta.url));
const docsDirectory = path.dirname(scriptsDirectory);
const repositoryRoot = path.dirname(docsDirectory);
const markdownPath = path.join(docsDirectory, "shared-engine-boundary-guide.md");
const pdfPath = path.join(docsDirectory, "shared-engine-boundary-guide.pdf");
const failures = [];

function check(condition, message) {
  if (!condition) failures.push(message);
}

function normalizeSource(text) {
  return text.replace(/\r\n/g, "\n").replace(/\r/g, "\n").trimEnd() + "\n";
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^$()|[\]{}\\]/g, "\\$&");
}

async function checkLocalLinks(source) {
  const pattern = /!?\[[^\]]*\]\(([^)\s]+)(?:\s+["'][^"']*["'])?\)/g;
  for (const match of source.matchAll(pattern)) {
    const rawTarget = match[1].replace(/^<|>$/g, "");
    if (
      rawTarget.startsWith("#") ||
      rawTarget.startsWith("/") ||
      /^[A-Za-z][A-Za-z0-9+.-]*:/.test(rawTarget)
    ) {
      continue;
    }
    const target = path.resolve(docsDirectory, rawTarget.split("#", 1)[0]);
    try {
      await stat(target);
    } catch {
      failures.push(
        "shared-engine-boundary-guide.md: missing local link " +
          path.relative(repositoryRoot, target),
      );
    }
  }
}

const source = await readFile(markdownPath, "utf8");
const pdf = await readFile(pdfPath);

for (const heading of [
  "Decision at a Glance",
  "Dependency Direction",
  "Public C++20 Contracts",
  "ChapterSource to RenderedSegment",
  "Cancellation and Partial Work",
  "Failure and Exit-Code Mapping",
  "Synchronous Engine, Asynchronous Qt",
  "Windows Process Boundary",
  "Migration Sequence and Gates",
  "Repository and Developer Map",
  "Non-Goals",
]) {
  check(
    new RegExp("^## " + escapeRegExp(heading) + "$", "m").test(source),
    "shared-engine-boundary-guide.md: missing required heading " + heading,
  );
}

for (const target of [
  "../CONTEXT.md",
  "../knowledge/architecture/decisions/0001-shared-qt-free-engine.md",
  "../knowledge/architecture/repo-map.md",
  "../CODING_STYLE.md",
  "verification.md",
]) {
  check(
    source.includes("](" + target + ")"),
    "shared-engine-boundary-guide.md: missing required link " + target,
  );
}

for (const token of [
  "enum class ProcessExitState",
  "struct ProcessRequest",
  "struct ProcessResult",
  "using ProcessExecutor",
  "struct ProbeResult",
  "class ProbeService",
  "plan_outputs",
  "class ExportRunner",
  "std::stop_token",
  "write_manifests",
  "Exit values remain `0` success",
  "Runtime code changes in VID-38",
]) {
  check(
    source.includes(token),
    "shared-engine-boundary-guide.md: missing required contract token " + token,
  );
}
await checkLocalLinks(source);

const sourceHash = createHash("sha256")
  .update(normalizeSource(source), "utf8")
  .digest("hex");
check(
  pdf.includes(Buffer.from("Source-SHA256:" + sourceHash, "ascii")),
  "shared-engine-boundary-guide.pdf is stale; regenerate it from the Markdown source",
);
const pageCount = (pdf.toString("latin1").match(/\/Type\s*\/Page\b/g) ?? []).length;
check(
  pageCount >= 7 && pageCount <= 20,
  "shared-engine-boundary-guide.pdf page count " + pageCount + " is outside 7-20",
);
check(pdf.length >= 25_000, "shared-engine-boundary-guide.pdf is unexpectedly small");

for (const backlink of [
  ["README.md", "docs/shared-engine-boundary-guide.md"],
  ["knowledge/README.md", "../docs/shared-engine-boundary-guide.md"],
  [
    "knowledge/architecture/repo-map.md",
    "../../docs/shared-engine-boundary-guide.md",
  ],
]) {
  const backlinkText = await readFile(path.join(repositoryRoot, backlink[0]), "utf8");
  check(
    backlinkText.includes("](" + backlink[1] + ")"),
    backlink[0] + ": missing shared-engine guide link",
  );
}

const workflow = await readFile(
  path.join(repositoryRoot, ".github", "workflows", "ci.yml"),
  "utf8",
);
check(
  workflow.includes("- 'docs/**'"),
  ".github/workflows/ci.yml: docs changes must route through the docs lane",
);

const packageData = JSON.parse(
  await readFile(path.join(docsDirectory, "package.json"), "utf8"),
);
check(
  packageData.scripts?.["validate:shared-engine-guide"] ===
    "node scripts/validate-shared-engine-guide.mjs",
  "docs/package.json: validate:shared-engine-guide is not wired",
);
check(
  packageData.scripts?.test?.includes("npm run validate:shared-engine-guide"),
  "docs/package.json: test must run validate:shared-engine-guide",
);
check(
  packageData.scripts?.["render:shared-engine-guide"]?.startsWith(
    "python scripts/render-guide-pdf.py ",
  ),
  "docs/package.json: render:shared-engine-guide is not wired",
);

if (failures.length > 0) {
  throw new Error("Invalid shared-engine guide:\n- " + failures.join("\n- "));
}

console.log(
  "Shared-engine guide validation passed (" +
    pageCount +
    " pages, source SHA-256 " +
    sourceHash +
    ").",
);
