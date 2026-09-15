import { lstat, readdir, readFile } from "node:fs/promises";
import path from "node:path";
import {
  assetPath,
  distDirectory,
  docsDirectory,
  htmlPath,
  repositoryRoot,
  routes,
} from "./site-contract.mjs";

const packagePath = path.join(docsDirectory, "package.json");
const workflowPath = path.join(
  repositoryRoot,
  ".github",
  "workflows",
  "cloudflare.yml",
);
const runbookPath = path.join(
  repositoryRoot,
  "knowledge",
  "operations",
  "cloudflare-production.md",
);
const gitignorePath = path.join(repositoryRoot, ".gitignore");
const verificationScriptPath = path.join(repositoryRoot, "tools", "verify.ps1");
const deployCommand =
  "wrangler pages deploy dist --project-name vidchopper --branch main";

function fail(message) {
  throw new Error(`Invalid Cloudflare Pages deployment contract: ${message}`);
}

function assert(condition, message) {
  if (!condition) fail(message);
}

async function requiredText(file, label) {
  try {
    return await readFile(file, "utf8");
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    fail(`${label} is missing or unreadable: ${detail}`);
  }
}

async function exists(file) {
  try {
    await lstat(file);
    return true;
  } catch (error) {
    if (error instanceof Error && "code" in error && error.code === "ENOENT") {
      return false;
    }
    throw error;
  }
}

async function walkFiles(root, relative = "") {
  const directory = path.join(root, relative);
  const entries = await readdir(directory, { withFileTypes: true });
  const files = [];
  for (const entry of entries) {
    const child = path.join(relative, entry.name);
    const portable = child.split(path.sep).join("/");
    assert(!entry.isSymbolicLink(), `dist must not contain symlink ${portable}`);
    if (entry.isDirectory()) {
      files.push(...(await walkFiles(root, child)));
      continue;
    }
    assert(entry.isFile(), `dist contains unsupported entry ${portable}`);
    const metadata = await lstat(path.join(root, child));
    files.push({ path: portable, size: metadata.size });
  }
  return files;
}

const [packageText, workflow, runbook, gitignore, verification] =
  await Promise.all([
    requiredText(packagePath, "docs/package.json"),
    requiredText(workflowPath, "Cloudflare Pages workflow"),
    requiredText(runbookPath, "Cloudflare production runbook"),
    requiredText(gitignorePath, ".gitignore"),
    requiredText(verificationScriptPath, "repository verification script"),
  ]);

let packageMetadata;
try {
  packageMetadata = JSON.parse(packageText);
} catch (error) {
  const detail = error instanceof Error ? error.message : String(error);
  fail(`docs/package.json could not be parsed: ${detail}`);
}

// vidchopper.app is a direct-upload Pages project. A Worker configuration
// would invite `wrangler deploy` to recreate the retired vidchopper Worker.
for (const legacyConfig of [
  "wrangler.jsonc",
  "wrangler.json",
  "wrangler.toml",
]) {
  assert(
    !(await exists(path.join(docsDirectory, legacyConfig))),
    `docs/${legacyConfig} must not exist; Pages direct upload needs no Worker configuration`,
  );
}

assert(
  packageMetadata.devDependencies?.wrangler === "4.120.1",
  "Wrangler must be pinned exactly to 4.120.1",
);
assert(
  packageMetadata.scripts?.["deploy:cloudflare"] ===
    `npm run build && ${deployCommand}`,
  "deploy:cloudflare must rebuild the canonical artifact immediately before upload",
);
assert(
  /^\.wrangler\/$/m.test(gitignore),
  ".gitignore must exclude every .wrangler directory",
);

const canonicalBuildCall =
  'Invoke-RepoCommand -FilePath $npm -ArgumentList @("run", "build")';
const pagesBuildCall =
  'Invoke-RepoCommand -FilePath $npm -ArgumentList @("run", "build:pages")';
const canonicalBuildIndex = verification.indexOf(canonicalBuildCall);
const pagesBuildIndex = verification.indexOf(pagesBuildCall);
assert(
  canonicalBuildIndex !== -1 && pagesBuildIndex > canonicalBuildIndex,
  "Docs verification must run the canonical build before the GitHub Pages build",
);

for (const snippet of [
  "name: Cloudflare Pages",
  "push:",
  "- main",
  "workflow_dispatch:",
  "contents: read",
  "group: cloudflare-production",
  "cancel-in-progress: false",
  "name: cloudflare-environment",
  "url: https://vidchopper.app",
  "node-version: 22",
  "npm ci",
  "CLOUDFLARE_API_TOKEN: ${{ secrets.CLOUDFLARE_API_TOKEN }}",
  "CLOUDFLARE_ACCOUNT_ID: ${{ secrets.CLOUDFLARE_ACCOUNT_ID }}",
  "npx --no-install wrangler pages deployment list --project-name vidchopper",
  "tools/agent-skill-artifacts.ps1 -Mode Check",
  "npm test",
  "npm run build",
  `npx --no-install ${deployCommand}`,
  "node scripts/validate-routes.mjs --origin https://vidchopper.app",
]) {
  assert(workflow.includes(snippet), `workflow is missing: ${snippet}`);
}
const stepOrder = [
  "name: Install pinned frontend dependencies",
  "name: Verify Cloudflare credentials before build",
  "name: Build and audit the canonical artifact",
  "name: Deploy to Cloudflare Pages",
  "name: Validate production",
].map((step) => workflow.indexOf(step));
assert(
  stepOrder.every(
    (index, position) =>
      index !== -1 && (position === 0 || index > stepOrder[position - 1]),
  ),
  "workflow must install, check credentials, build, deploy, then validate production in that order",
);
assert(
  !/^\s+pull_request(?:_target)?:/m.test(workflow),
  "production deployment must never run for pull requests",
);
assert(
  !/wrangler deploy\b/.test(workflow),
  "workflow must deploy to Pages, not a Worker",
);
assert(
  !workflow.includes("vars.CLOUDFLARE_ACCOUNT_ID"),
  "Cloudflare account identifiers must use a masked environment secret",
);
assert(
  !/\b[a-f0-9]{32}\b/i.test(workflow),
  "workflow must not hardcode a Cloudflare account or credential identifier",
);

for (const required of [
  "Cloudflare Pages",
  "CLOUDFLARE_API_TOKEN",
  "CLOUDFLARE_ACCOUNT_ID",
  "npm run deploy:cloudflare",
  "node scripts/validate-routes.mjs --origin https://vidchopper.app",
]) {
  assert(runbook.includes(required), `runbook is missing: ${required}`);
}
assert(
  !/[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}/i.test(runbook),
  "runbook must not publish an email address",
);
assert(
  !/\b[a-f0-9]{32}\b/i.test(runbook),
  "runbook must not publish a Cloudflare account identifier",
);

for (const probe of [
  "/.dev.vars",
  "/.env",
  "/.npmrc",
  "/.wrangler/cache/wrangler-account.json",
  "/_headers",
  "/_redirects",
  "/package.json",
  "/routes.json",
  "/wrangler.jsonc",
  "/wrangler.toml",
  "/README.md",
]) {
  assert(
    routes.notFoundProbes.includes(probe),
    `docs/routes.json must prove ${probe} is not public`,
  );
}

const files = await walkFiles(distDirectory);
assert(files.length <= 20_000, "dist exceeds the Pages Free file limit");
const fileSet = new Set(files.map((file) => file.path));
const allowedFiles = new Set([
  "index.html",
  "404.html",
  "_headers",
  "_redirects",
]);
for (const route of routes.htmlRoutes) {
  const expected = path
    .relative(distDirectory, htmlPath(distDirectory, route))
    .split(path.sep)
    .join("/");
  allowedFiles.add(expected);
  assert(fileSet.has(expected), `dist is missing HTML route ${route}`);
}
for (const asset of routes.assets) {
  const expected = path
    .relative(distDirectory, assetPath(distDirectory, asset.route))
    .split(path.sep)
    .join("/");
  allowedFiles.add(expected);
  assert(fileSet.has(expected), `dist is missing static asset ${asset.route}`);
}
for (const required of allowedFiles) {
  assert(fileSet.has(required), `dist is missing ${required}`);
}

const fingerprintedAssetPattern =
  /^assets\/[A-Za-z0-9][A-Za-z0-9._-]*-[A-Za-z0-9_-]{8,}\.(?:avif|css|gif|ico|jpe?g|js|png|svg|webp|woff2?)$/;
const fingerprintedAssets = [];
for (const file of files) {
  assert(file.size <= 25 * 1024 * 1024, `${file.path} exceeds 25 MiB`);
  if (allowedFiles.has(file.path)) continue;
  assert(
    fingerprintedAssetPattern.test(file.path),
    `dist contains an undeclared upload: ${file.path}`,
  );
  fingerprintedAssets.push(file.path);
}
assert(
  fingerprintedAssets.some((file) => file.endsWith(".css")) &&
    fingerprintedAssets.some((file) => file.endsWith(".js")),
  "dist must contain fingerprinted Vite CSS and JavaScript",
);

console.log(
  `Validated Cloudflare Pages deployment contract and ${files.length} dist files.`,
);
