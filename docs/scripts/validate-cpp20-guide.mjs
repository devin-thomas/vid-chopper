import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptsDirectory = path.dirname(fileURLToPath(import.meta.url));
const docsDirectory = path.dirname(scriptsDirectory);
const repositoryRoot = path.dirname(docsDirectory);
const guideRelativePath = "docs/cpp20-clarity-and-safety.md";
const guidePath = path.join(repositoryRoot, guideRelativePath);
const failures = [];

function check(condition, message) {
  if (!condition) failures.push(message);
}

function escapeRegExp(value) {
  return value.replace(/[.*+?^$()|[\]{}\\]/g, "\\$&");
}

function normalizeLineEndings(value) {
  return value.replace(/\r\n?/g, "\n");
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
        `${guideRelativePath}: missing local link ${path.relative(repositoryRoot, target)}`,
      );
    }
  }
}

const source = normalizeLineEndings(await readFile(guidePath, "utf8"));

for (const heading of [
  "Purpose and authority",
  "Manager decision frame",
  "Toolchain contract",
  "Values, ownership, and QObject lifetime",
  "Optional values, errors, and results",
  "Checked arithmetic and narrowing",
  "Enums, ranges, constexpr, and noexcept",
  "Paths, includes, and headers",
  "Qt, STL, platform, and third-party boundaries",
  "AI-agent and reviewer protocol",
  "Review checklist",
  "Phased adoption",
  "Definition of done",
]) {
  check(
    new RegExp(`^## ${escapeRegExp(heading)}$`, "m").test(source),
    `${guideRelativePath}: missing required heading ${heading}`,
  );
}

for (const target of [
  "../CODING_STYLE.md",
  "../CONTEXT.md",
  "../knowledge/architecture/decisions/0001-shared-qt-free-engine.md",
  "../knowledge/architecture/decisions/0002-pinned-vcpkg-dependencies.md",
  "../knowledge/architecture/decisions/0003-windows-support-portability.md",
]) {
  check(
    source.includes(`](${target})`),
    `${guideRelativePath}: missing required link ${target}`,
  );
}

for (const token of [
  "C++20 only",
  "C++23",
  "std::optional",
  "Q_DISABLE_COPY_MOVE",
  "std::ranges",
  "checked_add",
  "std::filesystem::path",
  "#pragma once",
  "nlohmann-json",
  "yaml-cpp",
]) {
  check(
    source.includes(token),
    `${guideRelativePath}: missing required policy token ${token}`,
  );
}

check(
  !/\bPDF\b|output\/pdf/i.test(source),
  `${guideRelativePath}: the internal guide must remain Markdown-only`,
);

// Keep the published examples honest by comparing each labeled excerpt with its source file.
const compiledExcerptPattern =
  /\*\*Compiled source excerpt - `([^`]+)`:\*\*\s*```cpp\n([\s\S]*?)\n```/g;
const compiledExcerpts = [...source.matchAll(compiledExcerptPattern)];
check(
  compiledExcerpts.length === 6,
  `${guideRelativePath}: expected six compiled source excerpts, found ${compiledExcerpts.length}`,
);

for (const match of compiledExcerpts) {
  const [, sourcePath, excerpt] = match;
  let sourceText = "";
  try {
    sourceText = normalizeLineEndings(
      await readFile(path.join(repositoryRoot, sourcePath), "utf8"),
    );
  } catch (error) {
    failures.push(`${sourcePath}: could not read compiled excerpt source (${error.message})`);
    continue;
  }
  check(
    sourceText.includes(excerpt),
    `${guideRelativePath}: compiled excerpt no longer matches ${sourcePath}`,
  );
}

const cppBlocks = [...source.matchAll(/```cpp\n[\s\S]*?\n```/g)];
const schematicBlocks = [
  ...source.matchAll(/\*\*Schematic [^\n]+:\*\*\s*```cpp\n[\s\S]*?\n```/g),
];
check(
  cppBlocks.length === compiledExcerpts.length + schematicBlocks.length,
  `${guideRelativePath}: every C++ block must be labeled compiled or schematic`,
);

await checkLocalLinks(source);

for (const [backlinkPath, target] of [
  ["CODING_STYLE.md", "docs/cpp20-clarity-and-safety.md"],
  ["knowledge/coding-style/overview.md", "../../docs/cpp20-clarity-and-safety.md"],
]) {
  const backlinkText = await readFile(path.join(repositoryRoot, backlinkPath), "utf8");
  check(
    backlinkText.includes(`](${target})`),
    `${backlinkPath}: missing C++20 guide link`,
  );
}

const cmake = await readFile(path.join(repositoryRoot, "CMakeLists.txt"), "utf8");
check(
  cmake.includes("set(CMAKE_CXX_STANDARD 20)") &&
    cmake.includes("set(CMAKE_CXX_STANDARD_REQUIRED ON)") &&
    cmake.includes("set(CMAKE_CXX_EXTENSIONS OFF)"),
  "CMakeLists.txt: documented C++20 toolchain contract changed",
);

const packageData = JSON.parse(
  await readFile(path.join(docsDirectory, "package.json"), "utf8"),
);
check(
  packageData.scripts?.["validate:cpp20-guide"] ===
    "node scripts/validate-cpp20-guide.mjs",
  "docs/package.json: validate:cpp20-guide is not wired",
);
check(
  packageData.scripts?.test?.includes("npm run validate:cpp20-guide"),
  "docs/package.json: test must run validate:cpp20-guide",
);

if (failures.length > 0) {
  throw new Error(`Invalid C++20 guide:\n- ${failures.join("\n- ")}`);
}

console.log("C++20 Markdown guide validation passed.");
