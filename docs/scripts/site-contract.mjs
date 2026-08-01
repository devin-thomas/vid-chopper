import { readFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptsDirectory = path.dirname(fileURLToPath(import.meta.url));
const routeSegmentPattern = /^[A-Za-z0-9._~-]+$/;
const htmlSegmentPattern = /^[a-z0-9-]+$/;
const sourceRoots = [
  ".agents/skills/vidchopper-cli/",
  "docs/schemas/",
  "examples/",
  "packaging/releases/",
];
const contentTypesByExtension = new Map([
  [
    ".json",
    new Set([
      "application/json; charset=utf-8",
      "application/schema+json; charset=utf-8",
    ]),
  ],
  [".md", new Set(["text/markdown; charset=utf-8"])],
  [".yaml", new Set(["application/yaml; charset=utf-8"])],
  [".yml", new Set(["application/yaml; charset=utf-8"])],
  [".zip", new Set(["application/zip"])],
]);
const contractKeys = new Set([
  "canonicalOrigin",
  "pagesBasePath",
  "delivery",
  "htmlRoutes",
  "htmlTitles",
  "legacyDocsRoutes",
  "assets",
  "deferredAssets",
  "notFoundProbes",
]);

export const docsDirectory = path.dirname(scriptsDirectory);
export const repositoryRoot = path.dirname(docsDirectory);
export const distDirectory = path.join(docsDirectory, "dist");
export const stagedPublicDirectory = path.join(docsDirectory, ".staged-public");
export const routesPath = path.join(docsDirectory, "routes.json");
export const assetsIgnoreText =
  ".assetsignore\n**/.DS_Store\n**/node_modules\n";

function fail(message) {
  throw new Error(`Invalid docs route contract: ${message}`);
}

function assertPlainObject(value, label) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    fail(`${label} must be an object`);
  }
}

function assertExactKeys(value, allowed, label) {
  const unexpected = Object.keys(value).filter((key) => !allowed.has(key));
  if (unexpected.length > 0) {
    fail(`${label} has unsupported fields: ${unexpected.join(", ")}`);
  }
  for (const key of allowed) {
    if (!(key in value)) fail(`${label} is missing ${key}`);
  }
}

function assertAllowedKeys(value, required, allowed, label) {
  const unexpected = Object.keys(value).filter((key) => !allowed.has(key));
  if (unexpected.length > 0) {
    fail(`${label} has unsupported fields: ${unexpected.join(", ")}`);
  }
  for (const key of required) {
    if (!(key in value)) fail(`${label} is missing ${key}`);
  }
}

function assertRoute(route, label, html = false) {
  if (typeof route !== "string" || !route.startsWith("/")) {
    fail(`${label} must be an absolute URL path`);
  }
  if (route !== "/" && route.endsWith("/")) {
    fail(`${label} must not have a trailing slash: ${route}`);
  }
  if (route.includes("\\") || /[%?#\u0000-\u001f\u007f]/.test(route)) {
    fail(`${label} contains a forbidden character: ${route}`);
  }
  if (route === "/") {
    if (!html) fail(`${label} may use the root route only for HTML`);
    return;
  }
  const segments = route.slice(1).split("/");
  if (
    segments.some(
      (segment) =>
        !routeSegmentPattern.test(segment) ||
        segment === "." ||
        segment === "..",
    )
  ) {
    fail(`${label} has an unsafe path segment: ${route}`);
  }
  if (
    html &&
    route !== "/" &&
    segments.some((segment) => !htmlSegmentPattern.test(segment))
  ) {
    fail(`${label} must use lowercase extensionless HTML segments: ${route}`);
  }
}

function assertSource(source, label) {
  if (
    typeof source !== "string" ||
    source.startsWith("/") ||
    source.includes("\\")
  ) {
    fail(`${label} must be a relative POSIX path`);
  }
  const segments = source.split("/");
  if (
    segments.some(
      (segment) =>
        !routeSegmentPattern.test(segment) ||
        segment === "." ||
        segment === "..",
    )
  ) {
    fail(`${label} has an unsafe path segment: ${source}`);
  }
  if (!sourceRoots.some((root) => source.startsWith(root))) {
    fail(`${label} must be under an approved source root: ${source}`);
  }
}

function assertMime(route, contentType, label) {
  const allowed = contentTypesByExtension.get(path.posix.extname(route));
  if (!allowed?.has(contentType)) {
    fail(`${label} has unsupported MIME type ${contentType} for ${route}`);
  }
}

function normalizedVersion(route) {
  const match = route.match(
    /(?:^|\/)(v\d+(?:\.\d+){0,2}(?:-[a-z0-9.-]+)?)(?:\/|$)/i,
  );
  return match?.[1].toLowerCase().replace(/^v/, "");
}

function assertCachePolicy(entry, label) {
  if (entry.cache !== "stable" && entry.cache !== "immutable") {
    fail(`${label} has unsupported cache policy ${entry.cache}`);
  }
  if (entry.cache !== "immutable") return;
  const version = normalizedVersion(entry.route);
  if (!version) fail(`${label} is immutable but its route is not versioned`);
  if ("source" in entry && !entry.source.toLowerCase().includes(version)) {
    fail(
      `${label} immutable source does not contain pinned version ${version}`,
    );
  }
}

function outputPath(route, html) {
  if (html)
    return route === "/" ? "index.html" : `${route.slice(1)}/index.html`;
  return route.slice(1);
}

function validateContract(contract) {
  assertPlainObject(contract, "root");
  assertExactKeys(contract, contractKeys, "root");
  if (contract.canonicalOrigin !== "https://vidchopper.app") {
    fail("canonicalOrigin must be https://vidchopper.app");
  }
  if (!/^\/[a-z0-9-]+\/$/.test(contract.pagesBasePath)) {
    fail(
      "pagesBasePath must be one lowercase path segment with a trailing slash",
    );
  }
  assertPlainObject(contract.delivery, "delivery");
  assertExactKeys(
    contract.delivery,
    new Set(["htmlHandling", "notFoundHandling"]),
    "delivery",
  );
  if (contract.delivery.htmlHandling !== "drop-trailing-slash") {
    fail("delivery.htmlHandling must be drop-trailing-slash");
  }
  if (contract.delivery.notFoundHandling !== "404-page") {
    fail("delivery.notFoundHandling must be 404-page");
  }
  for (const key of ["htmlRoutes", "assets", "notFoundProbes"]) {
    if (!Array.isArray(contract[key]) || contract[key].length === 0)
      fail(`${key} must be a non-empty array`);
  }
  if (!Array.isArray(contract.deferredAssets)) {
    fail("deferredAssets must be an array");
  }

  const urlOwners = new Map();
  const outputs = new Map([["404.html", "reserved custom 404"]]);
  const registerRoute = (route, owner, html = false, emitted = false) => {
    assertRoute(route, owner, html);
    const urlKey = route.toLowerCase();
    if (urlOwners.has(urlKey))
      fail(`${owner} collides with ${urlOwners.get(urlKey)} at ${route}`);
    urlOwners.set(urlKey, owner);
    if (!emitted) return;
    const file = outputPath(route, html).toLowerCase();
    if (outputs.has(file))
      fail(`${owner} output collides with ${outputs.get(file)} at ${file}`);
    outputs.set(file, owner);
  };

  for (const route of contract.htmlRoutes)
    registerRoute(route, `HTML route ${route}`, true, true);

  assertPlainObject(contract.htmlTitles, "htmlTitles");
  const titleRoutes = Object.keys(contract.htmlTitles).sort();
  const htmlRoutes = [...contract.htmlRoutes].sort();
  if (JSON.stringify(titleRoutes) !== JSON.stringify(htmlRoutes)) {
    fail("htmlTitles keys must exactly match htmlRoutes");
  }
  for (const [route, title] of Object.entries(contract.htmlTitles)) {
    if (
      typeof title !== "string" ||
      title.length === 0 ||
      title.length > 120 ||
      /[<>\u0000-\u001f\u007f]/.test(title)
    ) {
      fail(`htmlTitles[${route}] must be a safe non-empty title`);
    }
  }

  assertPlainObject(contract.legacyDocsRoutes, "legacyDocsRoutes");
  if (Object.keys(contract.legacyDocsRoutes).length === 0) {
    fail("legacyDocsRoutes must not be empty");
  }
  for (const [slug, destination] of Object.entries(
    contract.legacyDocsRoutes,
  )) {
    if (!/^[a-z0-9-]+$/.test(slug)) {
      fail(`legacyDocsRoutes has an invalid slug: ${slug}`);
    }
    assertRoute(destination, `legacyDocsRoutes.${slug}`, true);
    if (!contract.htmlRoutes.includes(destination)) {
      fail(`legacyDocsRoutes.${slug} points outside htmlRoutes`);
    }
  }

  const requiredAssetKeys = new Set([
    "source",
    "route",
    "contentType",
    "cache",
  ]);
  const allowedAssetKeys = new Set([...requiredAssetKeys, "sha256"]);
  for (const [index, asset] of contract.assets.entries()) {
    const label = `assets[${index}]`;
    assertPlainObject(asset, label);
    assertAllowedKeys(asset, requiredAssetKeys, allowedAssetKeys, label);
    assertSource(asset.source, `${label}.source`);
    registerRoute(asset.route, label, false, true);
    assertMime(asset.route, asset.contentType, label);
    if (
      path.posix.extname(asset.source).toLowerCase() !==
      path.posix.extname(asset.route).toLowerCase()
    ) {
      fail(`${label} source and route extensions disagree`);
    }
    assertCachePolicy(asset, label);
    if (asset.cache === "immutable") {
      if (!/^[a-f0-9]{64}$/.test(asset.sha256 ?? "")) {
        fail(`${label} immutable asset must pin a lowercase SHA-256 digest`);
      }
    } else if ("sha256" in asset) {
      fail(`${label} stable asset must not pin mutable alias bytes`);
    }
  }

  const deferredKeys = new Set(["route", "owner", "contentType", "cache"]);
  for (const [index, asset] of contract.deferredAssets.entries()) {
    const label = `deferredAssets[${index}]`;
    assertPlainObject(asset, label);
    assertExactKeys(asset, deferredKeys, label);
    registerRoute(asset.route, label);
    if (!/^VID-\d+$/.test(asset.owner))
      fail(`${label}.owner must be a Linear issue ID`);
    assertMime(asset.route, asset.contentType, label);
    assertCachePolicy(asset, label);
  }

  for (const [index, route] of contract.notFoundProbes.entries()) {
    registerRoute(route, `notFoundProbes[${index}]`);
  }
}

export const routes = JSON.parse(await readFile(routesPath, "utf8"));
validateContract(routes);

export function assertInside(root, target, label) {
  const relative = path.relative(root, target);
  if (
    relative === "" ||
    relative.startsWith(`..${path.sep}`) ||
    path.isAbsolute(relative)
  ) {
    throw new Error(`${label} must stay inside ${root}: ${target}`);
  }
}

export function approvedSourceRoot(source) {
  const prefix = sourceRoots.find((root) => source.startsWith(root));
  if (!prefix)
    throw new Error(`Static asset source is outside approved roots: ${source}`);
  return path.resolve(repositoryRoot, prefix);
}

export function assetPath(root, route) {
  const target = path.join(root, route.replace(/^\/+/, ""));
  assertInside(root, target, `Asset route ${route}`);
  return target;
}

export function htmlPath(root, route) {
  if (route === "/") return path.join(root, "index.html");
  const target = path.join(root, route.replace(/^\/+|\/+$/g, ""), "index.html");
  assertInside(root, target, `HTML route ${route}`);
  return target;
}

export function isPagesMode(argumentsList = process.argv.slice(2)) {
  const modeIndex = argumentsList.indexOf("--mode");
  return modeIndex !== -1 && argumentsList[modeIndex + 1] === "pages";
}

export function cacheControl(cache) {
  if (cache === "stable") return "public, max-age=300, must-revalidate";
  if (cache === "immutable") return "public, max-age=31536000, immutable";
  throw new Error(`Unsupported cache policy: ${cache}`);
}

export function releaseChannelForVersion(version) {
  const match = version.match(
    /^(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$/,
  );
  if (!match) throw new Error(`Unsupported release version: ${version}`);
  return match[1] ? "prerelease" : "stable";
}

export function expectedHeadersText() {
  return `${routes.assets
    .map((asset) =>
      [
        asset.route,
        `  Content-Type: ${asset.contentType}`,
        `  Cache-Control: ${cacheControl(asset.cache)}`,
      ].join("\n"),
    )
    .join("\n\n")}\n`;
}
