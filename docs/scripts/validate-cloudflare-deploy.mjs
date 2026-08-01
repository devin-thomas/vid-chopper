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

const wranglerPath = path.join(docsDirectory, "wrangler.jsonc");
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

function fail(message) {
  throw new Error(`Invalid Cloudflare deployment contract: ${message}`);
}

function assert(condition, message) {
  if (!condition) fail(message);
}

function assertPlainObject(value, label) {
  assert(
    value !== null && typeof value === "object" && !Array.isArray(value),
    `${label} must be an object`,
  );
}

function assertExactKeys(value, expected, label) {
  assertPlainObject(value, label);
  const actual = Object.keys(value).sort();
  const wanted = [...expected].sort();
  assert(
    JSON.stringify(actual) === JSON.stringify(wanted),
    `${label} fields must be ${wanted.join(", ")}; got ${actual.join(", ")}`,
  );
}

async function requiredText(file, label) {
  try {
    return await readFile(file, "utf8");
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    fail(`${label} is missing or unreadable: ${detail}`);
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

const [wranglerText, packageText, workflow, runbook, gitignore, verification] =
  await Promise.all([
    requiredText(wranglerPath, "docs/wrangler.jsonc"),
    requiredText(packagePath, "docs/package.json"),
    requiredText(workflowPath, "Cloudflare production workflow"),
    requiredText(runbookPath, "Cloudflare production runbook"),
    requiredText(gitignorePath, ".gitignore"),
    requiredText(verificationScriptPath, "repository verification script"),
  ]);

let wrangler;
let packageMetadata;
try {
  wrangler = JSON.parse(wranglerText);
  packageMetadata = JSON.parse(packageText);
} catch (error) {
  const detail = error instanceof Error ? error.message : String(error);
  fail(`JSON configuration could not be parsed: ${detail}`);
}

assertExactKeys(
  wrangler,
  [
    "$schema",
    "assets",
    "compatibility_date",
    "name",
    "preview_urls",
    "routes",
    "workers_dev",
  ],
  "wrangler root",
);
assert(
  wrangler.$schema === "node_modules/wrangler/config-schema.json",
  "wrangler must use the repository-local schema",
);
assert(wrangler.name === "vidchopper", "Worker name must be vidchopper");
assert(
  /^\d{4}-\d{2}-\d{2}$/.test(wrangler.compatibility_date),
  "compatibility_date must be an explicit ISO date",
);
assert(wrangler.workers_dev === false, "workers.dev must remain disabled");
assert(wrangler.preview_urls === false, "public preview URLs must be disabled");
assert(
  JSON.stringify(wrangler.routes) ===
    JSON.stringify([{ pattern: "vidchopper.app", custom_domain: true }]),
  "the only production route must be the vidchopper.app custom domain",
);
assertExactKeys(
  wrangler.assets,
  ["directory", "html_handling", "not_found_handling"],
  "wrangler assets",
);
assert(
  wrangler.assets.directory === "./dist",
  "Wrangler must upload only docs/dist",
);
assert(
  wrangler.assets.html_handling === routes.delivery.htmlHandling,
  "Wrangler HTML handling must match docs/routes.json",
);
assert(
  wrangler.assets.not_found_handling === routes.delivery.notFoundHandling,
  "Wrangler 404 handling must match docs/routes.json",
);

assert(
  packageMetadata.devDependencies?.wrangler === "4.118.0",
  "Wrangler must be pinned exactly to 4.118.0",
);
assert(
  packageMetadata.scripts?.["cloudflare:dry-run"] ===
    "wrangler deploy --dry-run --outdir .wrangler/dry-run",
  "cloudflare:dry-run must inspect the built artifact without uploading",
);
assert(
  packageMetadata.scripts?.["deploy:cloudflare"] ===
    "npm run build && wrangler deploy --strict",
  "deploy:cloudflare must rebuild the canonical artifact immediately before upload",
);
assert(
  /^\.wrangler\/$/m.test(gitignore),
  ".gitignore must exclude every .wrangler directory",
);

const canonicalBuildCall =
  'Invoke-RepoCommand -FilePath $npm -ArgumentList @("run", "build")';
const dryRunCall =
  'Invoke-RepoCommand -FilePath $npm -ArgumentList @("run", "cloudflare:dry-run")';
const pagesBuildCall =
  'Invoke-RepoCommand -FilePath $npm -ArgumentList @("run", "build:pages")';
const canonicalBuildIndex = verification.indexOf(canonicalBuildCall);
const dryRunIndex = verification.indexOf(dryRunCall);
const pagesBuildIndex = verification.indexOf(pagesBuildCall);
assert(
  canonicalBuildIndex !== -1 &&
    dryRunIndex > canonicalBuildIndex &&
    pagesBuildIndex > dryRunIndex,
  "Docs verification must dry-run Wrangler after the canonical build and before the Pages build",
);

const workflowSnippets = [
  "name: Cloudflare Production",
  "workflow_dispatch:",
  "confirmation:",
  "deploy vidchopper.app",
  "DISPATCH_REF: ${{ github.ref }}",
  "CONFIRMATION: ${{ inputs.confirmation }}",
  "needs: authorize",
  "contents: read",
  "group: cloudflare-production",
  "cancel-in-progress: false",
  "name: cloudflare-environment",
  "url: https://vidchopper.app",
  "node-version: 22",
  "name: Verify Cloudflare credentials before build",
  "npx --yes wrangler@4.118.0 deployments list --name vidchopper --json",
  "npx --yes wrangler@4.118.0 versions list --name vidchopper --json",
  "Cloudflare production secrets are missing.",
  "Cloudflare credential and read-contract preflight passed.",
  "npm ci",
  "tools/agent-skill-artifacts.ps1 -Mode Check",
  "npm test",
  "npm run build",
  "npm run cloudflare:dry-run",
  "uses: cloudflare/wrangler-action@ebbaa1584979971c8614a24965b4405ff95890e0 # v4",
  "apiToken: ${{ secrets.CLOUDFLARE_API_TOKEN }}",
  "accountId: ${{ secrets.CLOUDFLARE_ACCOUNT_ID }}",
  'wranglerVersion: "4.118.0"',
  "workingDirectory: docs",
  "deploy --strict --message",
  "--tag git-${{ github.sha }}",
  "max_by(.created_on)",
  '.annotations["workers/tag"]',
  "GITHUB_RUN_ID",
  "GITHUB_RUN_ATTEMPT",
  "github.run_attempt",
  "cloudflare-production-${{ github.run_id }}-${{ github.run_attempt }}",
  "sleep 15",
  "node scripts/validate-routes.mjs --origin https://vidchopper.app",
  "actions/upload-artifact@v4",
];
for (const snippet of workflowSnippets) {
  assert(workflow.includes(snippet), `workflow is missing: ${snippet}`);
}
const credentialPreflightIndex = workflow.indexOf(
  "name: Verify Cloudflare credentials before build",
);
const installIndex = workflow.indexOf("name: Install pinned frontend dependencies");
const deployIndex = workflow.indexOf("name: Deploy the audited artifact");
assert(
  credentialPreflightIndex !== -1 &&
    credentialPreflightIndex < installIndex &&
    installIndex < deployIndex,
  "Cloudflare credentials must be checked before dependency installation and deployment",
);
assert(
  !/^\s+(?:push|pull_request):/m.test(workflow),
  "production deployment must remain manual",
);
assert(
  !workflow.includes("vars.CLOUDFLARE_ACCOUNT_ID"),
  "Cloudflare account identifiers must use a masked environment secret",
);
assert(
  !/\b[a-f0-9]{32}\b/i.test(workflow),
  "workflow must not hardcode a Cloudflare account or credential identifier",
);

const preCutoverDeployment = "5c335ae3-017e-43e3-8458-bac2f60f499f";
const preCutoverVersion = "ea79b2ce-dfc3-4f6a-8c07-e5190a423e77";
for (const required of [
  preCutoverDeployment,
  preCutoverVersion,
  "CLOUDFLARE_API_TOKEN",
  "CLOUDFLARE_ACCOUNT_ID",
  "wrangler rollback <version-id>",
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

const requiredPrivateProbes = [
  "/.assetsignore",
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
];
for (const probe of requiredPrivateProbes) {
  assert(
    routes.notFoundProbes.includes(probe),
    `docs/routes.json must prove ${probe} is not public`,
  );
}

const files = await walkFiles(distDirectory);
assert(files.length <= 20_000, "dist exceeds the Workers Free file limit");
const fileSet = new Set(files.map((file) => file.path));
const allowedFiles = new Set([
  "index.html",
  "404.html",
  "_headers",
  ".assetsignore",
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
  `Validated Cloudflare deployment contract and ${files.length} dist files for manual production publication.`,
);
