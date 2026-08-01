import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptsDirectory = path.dirname(fileURLToPath(import.meta.url));
const docsDirectory = path.dirname(scriptsDirectory);
const repositoryRoot = path.dirname(docsDirectory);
const failures = [];

const glossaryTerms = [
  ["Source", "one input video"],
  ["Chapter", "ordered intent"],
  ["RenderedSegment", "produced media artifact"],
  ["ChapterFile", "JSON or YAML serialization"],
  ["ChapterSource", "ChapterFile or explicit embedded chapters"],
  ["Job", "One Source"],
  ["Batch", "Multiple Jobs"],
];
const adrFiles = [
  "knowledge/architecture/decisions/0001-shared-qt-free-engine.md",
  "knowledge/architecture/decisions/0002-pinned-vcpkg-dependencies.md",
  "knowledge/architecture/decisions/0003-windows-support-portability.md",
];
const adrHeadings = [
  "Status",
  "Context",
  "Alternatives Considered",
  "Decision",
  "Consequences",
  "Migration Impact",
];
const architectureFiles = [
  "CONTEXT.md",
  "README.md",
  "VIDCHOPPER_TEAM_KNOWLEDGE_BASE.md",
  "docs/cli-config-schema.md",
  "docs/vidchopper_cli_architecture_plan.md",
  "knowledge/README.md",
  "knowledge/architecture/repo-map.md",
  "knowledge/architecture/decisions/README.md",
  ...adrFiles,
];

function check(condition, message) {
  if (!condition) failures.push(message);
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

async function readRepositoryFile(relativePath) {
  try {
    return await readFile(path.join(repositoryRoot, relativePath), "utf8");
  } catch (error) {
    failures.push(`${relativePath}: could not read file (${error.message})`);
    return "";
  }
}

function checkRequiredLink(text, source, target) {
  check(
    text.includes(`](${target})`),
    `${source}: missing canonical link target ${target}`,
  );
}

async function checkLocalLinks(source, text) {
  const pattern = /!?\[[^\]]*\]\(([^)\s]+)(?:\s+["'][^"']*["'])?\)/g;
  const sourceDirectory = path.dirname(path.join(repositoryRoot, source));
  const targets = new Set();
  for (const match of text.matchAll(pattern)) {
    const rawTarget = match[1].replace(/^<|>$/g, "");
    if (
      rawTarget.startsWith("#") ||
      rawTarget.startsWith("/") ||
      /^[A-Za-z][A-Za-z0-9+.-]*:/.test(rawTarget)
    ) {
      continue;
    }
    const pathOnly = rawTarget.split("#", 1)[0];
    if (pathOnly.length > 0) targets.add(path.resolve(sourceDirectory, pathOnly));
  }

  for (const target of targets) {
    try {
      await stat(target);
    } catch {
      failures.push(
        `${source}: local link target does not exist: ${path.relative(repositoryRoot, target)}`,
      );
    }
  }
}

const entries = await Promise.all(
  architectureFiles.map(async (relativePath) => [
    relativePath,
    await readRepositoryFile(relativePath),
  ]),
);
const content = new Map(entries);
const glossary = content.get("CONTEXT.md");

let previousHeading = -1;
for (const [term, requiredPhrase] of glossaryTerms) {
  const headingPattern = new RegExp(`^## ${escapeRegExp(term)}$`, "m");
  const headingIndex = glossary.search(headingPattern);
  check(headingIndex >= 0, `CONTEXT.md: missing canonical term heading ${term}`);
  check(
    headingIndex > previousHeading,
    `CONTEXT.md: canonical term ${term} is out of order`,
  );
  check(
    glossary.toLowerCase().includes(requiredPhrase.toLowerCase()),
    `CONTEXT.md: ${term} definition is missing required meaning: ${requiredPhrase}`,
  );
  previousHeading = headingIndex;
}

for (const implementationDetail of [
  "ChapterSegment",
  "BatchJob",
  "ResolvedExportJob",
  "src/",
  "std::",
  "VidChopperCLI.exe",
]) {
  check(
    !glossary.includes(implementationDetail),
    `CONTEXT.md: glossary contains implementation detail ${implementationDetail}`,
  );
}

for (const adrFile of adrFiles) {
  const adr = content.get(adrFile);
  for (const heading of adrHeadings) {
    check(
      new RegExp(`^## ${escapeRegExp(heading)}$`, "m").test(adr),
      `${adrFile}: missing required heading ${heading}`,
    );
  }
  check(
    /^## Status\s+Accepted$/m.test(adr),
    `${adrFile}: status must be Accepted`,
  );
}

const requiredLinks = new Map([
  ["CONTEXT.md", ["knowledge/architecture/decisions/README.md"]],
  [
    "README.md",
    ["CONTEXT.md", "knowledge/architecture/decisions/README.md"],
  ],
  [
    "VIDCHOPPER_TEAM_KNOWLEDGE_BASE.md",
    ["CONTEXT.md", "knowledge/architecture/decisions/README.md"],
  ],
  [
    "docs/cli-config-schema.md",
    ["../CONTEXT.md", "../knowledge/architecture/decisions/README.md"],
  ],
  [
    "docs/vidchopper_cli_architecture_plan.md",
    [
      "../CONTEXT.md",
      "../knowledge/architecture/decisions/README.md",
      "vidchopper_cli_architecture_plan.pdf",
    ],
  ],
  [
    "knowledge/README.md",
    ["../CONTEXT.md", "architecture/decisions/README.md"],
  ],
  [
    "knowledge/architecture/repo-map.md",
    ["../../CONTEXT.md", "decisions/README.md"],
  ],
  [
    "knowledge/architecture/decisions/README.md",
    ["../../../CONTEXT.md", "../../../docs/vidchopper_cli_architecture_plan.md"],
  ],
]);

for (const [source, targets] of requiredLinks) {
  const text = content.get(source);
  for (const target of targets) checkRequiredLink(text, source, target);
}

const repoMap = content.get("knowledge/architecture/repo-map.md");
for (const implementationName of [
  "ChapterSegment",
  "BatchJob",
  "ResolvedExportJob",
]) {
  check(
    repoMap.includes(`\`${implementationName}\``),
    `knowledge/architecture/repo-map.md: missing transitional mapping for ${implementationName}`,
  );
}

const historicalPlan = content.get("docs/vidchopper_cli_architecture_plan.md");
check(
  historicalPlan.toLowerCase().includes("historical"),
  "docs/vidchopper_cli_architecture_plan.md: preserved PDF must be marked historical",
);

for (const [source, text] of content) await checkLocalLinks(source, text);

const vcpkgText = await readRepositoryFile("vcpkg.json");
try {
  const vcpkg = JSON.parse(vcpkgText);
  check(
    typeof vcpkg["builtin-baseline"] === "string" &&
      /^[0-9a-f]{40}$/.test(vcpkg["builtin-baseline"]),
    "vcpkg.json: builtin-baseline must pin a 40-character commit",
  );
  check(
    Array.isArray(vcpkg.dependencies) &&
      vcpkg.dependencies.includes("nlohmann-json") &&
      vcpkg.dependencies.includes("yaml-cpp"),
    "vcpkg.json: dependencies must include nlohmann-json and yaml-cpp",
  );
} catch (error) {
  failures.push(`vcpkg.json: invalid JSON (${error.message})`);
}

const workflow = await readRepositoryFile(".github/workflows/ci.yml");
for (const docsPath of [
  "CONTEXT.md",
  "VIDCHOPPER_TEAM_KNOWLEDGE_BASE.md",
  "knowledge/README.md",
  "knowledge/architecture/**",
]) {
  check(
    workflow.includes(`- '${docsPath}'`),
    `.github/workflows/ci.yml: docs filter is missing ${docsPath}`,
  );
}

const packageText = await readRepositoryFile("docs/package.json");
try {
  const packageData = JSON.parse(packageText);
  check(
    packageData.scripts?.["validate:architecture"] ===
      "node scripts/validate-architecture-docs.mjs",
    "docs/package.json: validate:architecture script is not wired",
  );
  check(
    packageData.scripts?.test?.includes("npm run validate:architecture"),
    "docs/package.json: test must run validate:architecture",
  );
} catch (error) {
  failures.push(`docs/package.json: invalid JSON (${error.message})`);
}

if (failures.length > 0) {
  throw new Error(`Invalid architecture documentation:\n- ${failures.join("\n- ")}`);
}

console.log(
  `Architecture documentation validation passed (${glossaryTerms.length} terms, ${adrFiles.length} accepted ADRs).`,
);
