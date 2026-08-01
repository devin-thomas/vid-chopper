import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { createServer } from "node:http";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { promisify } from "node:util";
import {
  assetPath,
  assetsIgnoreText,
  cacheControl,
  distDirectory,
  expectedHeadersText,
  htmlPath,
  isPagesMode,
  releaseChannelForVersion,
  repositoryRoot,
  routes,
} from "./site-contract.mjs";

const execFileAsync = promisify(execFile);

const argumentsList = process.argv.slice(2);
const pagesMode = isPagesMode(argumentsList);
let remoteOrigin = null;

for (let index = 0; index < argumentsList.length; index += 1) {
  const argument = argumentsList[index];
  if (argument === "--mode") {
    if (argumentsList[index + 1] !== "pages") {
      throw new Error("--mode supports only the pages value.");
    }
    index += 1;
    continue;
  }
  if (argument === "--origin") {
    if (remoteOrigin !== null || index + 1 >= argumentsList.length) {
      throw new Error("--origin requires exactly one HTTPS origin.");
    }
    const candidate = new URL(argumentsList[index + 1]);
    if (
      candidate.protocol !== "https:" ||
      candidate.username !== "" ||
      candidate.password !== "" ||
      candidate.pathname !== "/" ||
      candidate.search !== "" ||
      candidate.hash !== ""
    ) {
      throw new Error(
        "--origin must be an HTTPS origin without credentials, path, query, or hash.",
      );
    }
    remoteOrigin = candidate.origin;
    index += 1;
    continue;
  }
  throw new Error(`Unsupported route validator argument: ${argument}`);
}

const remoteMode = remoteOrigin !== null;
const basePath = pagesMode ? routes.pagesBasePath : "/";
const routeAssets = new Map(routes.assets.map((asset) => [asset.route, asset]));
const observedAssets = new Map();
const requestNonce = Date.now().toString(36);

const mimeTypes = new Map([
  [".css", "text/css; charset=utf-8"],
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".json", "application/json; charset=utf-8"],
  [".png", "image/png"],
  [".svg", "image/svg+xml"],
  [".yaml", "application/yaml; charset=utf-8"],
]);

function fail(message) {
  throw new Error(message);
}

function assert(condition, message) {
  if (!condition) fail(message);
}

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex");
}

function normalizedContentType(value) {
  return value
    ?.split(";")
    .map((part) => part.trim().toLowerCase())
    .join("; ");
}

function requestPath(route) {
  if (!pagesMode) return route;
  const base = basePath.replace(/\/$/, "");
  return route === "/" ? `${base}/` : `${base}${route}`;
}

function requestUrl(origin, route, cacheBust = true) {
  const url = new URL(requestPath(route), origin);
  if (remoteMode && cacheBust) {
    url.searchParams.set("vidchopper_verify", requestNonce);
  }
  return url;
}

function stripBase(pathname) {
  if (!pagesMode) return pathname;
  const base = basePath.replace(/\/$/, "");
  if (pathname === base || pathname === `${base}/`) return "/";
  if (!pathname.startsWith(`${base}/`)) return null;
  return pathname.slice(base.length) || "/";
}

function parseHeaders(text) {
  const parsed = new Map();
  let currentRoute = null;
  for (const line of text.split(/\r?\n/)) {
    if (line.length === 0) {
      currentRoute = null;
      continue;
    }
    if (!line.startsWith("  ")) {
      currentRoute = line;
      if (parsed.has(currentRoute)) {
        fail(`Duplicate _headers route: ${currentRoute}`);
      }
      parsed.set(currentRoute, new Map());
      continue;
    }
    if (currentRoute === null) fail("_headers contains an orphan header line.");
    const separator = line.indexOf(":");
    if (separator < 3) fail(`Malformed _headers line: ${line}`);
    const name = line.slice(2, separator).trim().toLowerCase();
    const value = line.slice(separator + 1).trim();
    const headers = parsed.get(currentRoute);
    if (headers.has(name)) {
      fail(`Duplicate ${name} header for ${currentRoute}`);
    }
    headers.set(name, value);
  }
  return parsed;
}

const [builtHeadersText, builtAssetsIgnoreText] = await Promise.all([
  readFile(path.join(distDirectory, "_headers"), "utf8"),
  readFile(path.join(distDirectory, ".assetsignore"), "utf8"),
]);
assert(
  builtHeadersText === expectedHeadersText(),
  "Built _headers bytes do not match the route contract.",
);
assert(
  builtAssetsIgnoreText === assetsIgnoreText,
  "Built .assetsignore bytes do not match the staging contract.",
);
const builtHeaders = parseHeaders(builtHeadersText);
assert(
  builtHeaders.size === routes.assets.length,
  "Built _headers route count does not match active assets.",
);

function contentType(route, file) {
  return (
    builtHeaders.get(route)?.get("content-type") ??
    mimeTypes.get(path.extname(file)) ??
    "application/octet-stream"
  );
}

async function resolveRequest(route) {
  const asset = routeAssets.get(route);
  if (asset) {
    const file = assetPath(distDirectory, route);
    return {
      file,
      status: 200,
      contentType: contentType(route, file),
      cacheControl: builtHeaders.get(route)?.get("cache-control"),
    };
  }

  if (routes.htmlRoutes.includes(route)) {
    return {
      file: htmlPath(distDirectory, route),
      status: 200,
      contentType: "text/html; charset=utf-8",
    };
  }

  const exactFile = assetPath(distDirectory, route);
  try {
    if ((await stat(exactFile)).isFile()) {
      return {
        file: exactFile,
        status: 200,
        contentType: contentType(route, exactFile),
      };
    }
  } catch (error) {
    if (
      !(error instanceof Error) ||
      !("code" in error) ||
      error.code !== "ENOENT"
    ) {
      throw error;
    }
  }

  return {
    file: path.join(distDirectory, "404.html"),
    status: 404,
    contentType: "text/html; charset=utf-8",
  };
}

let server = null;
let origin = remoteOrigin;

if (!remoteMode) {
  server = createServer(async (request, response) => {
    try {
      if (request.method !== "GET" && request.method !== "HEAD") {
        response.writeHead(405).end();
        return;
      }

      const pathname = decodeURIComponent(
        new URL(request.url ?? "/", "http://artifact.local").pathname,
      );
      const route = stripBase(pathname);
      if (route === null || route.includes("..")) {
        response.writeHead(404).end();
        return;
      }

      const resolved = await resolveRequest(route);
      const body = await readFile(resolved.file);
      const headers = {
        "Content-Type": resolved.contentType,
        "Content-Length": String(body.length),
        ...(resolved.cacheControl
          ? { "Cache-Control": resolved.cacheControl }
          : {}),
      };
      response.writeHead(resolved.status, headers);
      response.end(request.method === "HEAD" ? undefined : body);
    } catch (error) {
      response.writeHead(500, { "Content-Type": "text/plain; charset=utf-8" });
      response.end(error instanceof Error ? error.message : String(error));
    }
  });

  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const address = server.address();
  if (address === null || typeof address === "string") {
    fail("Artifact server did not expose a TCP port.");
  }
  origin = `http://127.0.0.1:${address.port}`;
}

async function fetchRoute(route, expectedStatus, expectedType) {
  const url = requestUrl(origin, route);
  const getResponse = await fetch(url, { redirect: "manual" });
  assert(
    getResponse.status === expectedStatus,
    `GET ${route}: expected ${expectedStatus}, got ${getResponse.status}`,
  );
  assert(
    normalizedContentType(getResponse.headers.get("content-type")) ===
      normalizedContentType(expectedType),
    `GET ${route}: unexpected content type ${getResponse.headers.get("content-type")}`,
  );
  const body = new Uint8Array(await getResponse.arrayBuffer());

  const headResponse = await fetch(url, {
    method: "HEAD",
    redirect: "manual",
  });
  assert(
    headResponse.status === expectedStatus,
    `HEAD ${route}: expected ${expectedStatus}, got ${headResponse.status}`,
  );
  assert(
    normalizedContentType(headResponse.headers.get("content-type")) ===
      normalizedContentType(expectedType),
    `HEAD ${route}: unexpected content type ${headResponse.headers.get("content-type")}`,
  );
  assert(
    (await headResponse.arrayBuffer()).byteLength === 0,
    `HEAD ${route}: response included a body`,
  );
  if (!remoteMode && expectedStatus === 200) {
    assert(
      headResponse.headers.get("content-length") === String(body.length),
      `HEAD ${route}: length drift`,
    );
  }
  return { body, headers: getResponse.headers };
}

async function verifyCanonicalRedirect(from, to) {
  const sourceUrl = requestUrl(origin, from);
  const response = await fetch(sourceUrl, { redirect: "manual" });
  assert(
    [301, 302, 307, 308].includes(response.status),
    `GET ${from}: expected redirect, got ${response.status}`,
  );
  const location = response.headers.get("location");
  assert(location !== null, `GET ${from}: redirect is missing Location`);
  const destination = new URL(location, sourceUrl);
  assert(
    destination.origin === new URL(origin).origin,
    `GET ${from}: redirect crossed the canonical origin`,
  );
  assert(
    destination.pathname === requestPath(to),
    `GET ${from}: expected redirect to ${requestPath(to)}, got ${destination.pathname}`,
  );
}

function assertReleaseMetadata(release, packageVersion, schemaVersion) {
  const expectedTag = `v${packageVersion}`;
  const expectedChannel = releaseChannelForVersion(packageVersion);
  const expectedReleaseUrl = `https://github.com/devin-thomas/vid-chopper/releases/tag/${expectedTag}`;
  const expectedAssetName = `VidChopper-${packageVersion}-windows-x64.zip`;
  const expectedAssetUrl = `https://github.com/devin-thomas/vid-chopper/releases/download/${expectedTag}/${expectedAssetName}`;
  const expectedChecksumName = `${expectedAssetName}.sha256`;
  const expectedChecksumUrl = `${expectedAssetUrl}.sha256`;
  const releaseAsset = release.assets?.[0];
  const checksumAsset = release.assets?.[1];
  const publishedAt = new Date(release.publishedAt);

  assert(release.schemaVersion === 1, "Release schemaVersion drifted.");
  assert(release.version === packageVersion, "Release version drifted.");
  assert(release.tag === expectedTag, "Release tag drifted.");
  assert(
    release.channel === expectedChannel,
    `Release channel must be ${expectedChannel}.`,
  );
  assert(
    !Number.isNaN(publishedAt.valueOf()) &&
      publishedAt.toISOString().replace(".000Z", "Z") === release.publishedAt,
    "Release publishedAt must be canonical RFC 3339 UTC.",
  );
  assert(
    /^[a-f0-9]{40}$/.test(release.sourceCommit ?? ""),
    "Release sourceCommit must be a full lowercase Git SHA.",
  );
  assert(release.releaseUrl === expectedReleaseUrl, "Release URL drifted.");
  assert(
    release.docsUrl === "https://vidchopper.app/docs",
    "Release docs URL drifted.",
  );
  assert(release.cliVersion === packageVersion, "CLI version drifted.");
  assert(
    release.chapterFileSchemaVersion === schemaVersion,
    "ChapterFile schema version disagrees with the published schema.",
  );
  assert(
    release.exportManifestSchemaVersion === 1,
    "Export manifest schema version drifted.",
  );
  assert(
    Array.isArray(release.assets) && release.assets.length === 2,
    "Release must declare the published package and checksum assets.",
  );
  assert(
    releaseAsset?.name === expectedAssetName,
    "Release asset name drifted.",
  );
  assert(releaseAsset?.url === expectedAssetUrl, "Release asset URL drifted.");
  assert(
    Number.isSafeInteger(releaseAsset?.size) && releaseAsset.size > 0,
    "Release asset size must be a positive safe integer.",
  );
  assert(
    /^[a-f0-9]{64}$/.test(releaseAsset?.sha256 ?? ""),
    "Release asset SHA-256 must be 64 lowercase hexadecimal characters.",
  );
  assert(
    checksumAsset?.name === expectedChecksumName,
    "Release checksum asset name drifted.",
  );
  assert(
    checksumAsset?.url === expectedChecksumUrl,
    "Release checksum asset URL drifted.",
  );
  assert(
    Number.isSafeInteger(checksumAsset?.size) && checksumAsset.size > 0,
    "Release checksum size must be a positive safe integer.",
  );
  assert(
    /^[a-f0-9]{64}$/.test(checksumAsset?.sha256 ?? ""),
    "Release checksum SHA-256 must be 64 lowercase hexadecimal characters.",
  );
  return { expectedAssetUrl, expectedTag };
}

function tokenizePowerShell(line) {
  const tokens = [];
  for (const match of line.matchAll(/"([^"]*)"|'([^']*)'|(\S+)/g)) {
    tokens.push(match[1] ?? match[2] ?? match[3]);
  }
  return tokens;
}

function validateCliCommand(line, flagArity) {
  const tokens = tokenizePowerShell(line);
  assert(
    tokens[0] === "&",
    `CLI example must use PowerShell call syntax: ${line}`,
  );
  assert(
    /^C:\\.*\\VidChopperCLI\.exe$/i.test(tokens[1] ?? ""),
    `CLI example must use an explicit absolute executable path: ${line}`,
  );
  const argumentsOnly = tokens.slice(2);
  const standaloneFlags = argumentsOnly.filter(
    (argument) => argument === "--version" || argument === "--help",
  );
  if (standaloneFlags.length > 0) {
    assert(
      argumentsOnly.length === 1 && standaloneFlags.length === 1,
      `${standaloneFlags[0]} must be a standalone invocation: ${line}`,
    );
    return standaloneFlags[0] === "--version" ? "version" : "help";
  }

  const usesChop = argumentsOnly[0] === "chop";
  let index = usesChop ? 1 : 0;
  const positionals = [];
  let usesEmbedded = false;
  let sawOption = false;
  const seenFlags = new Set();
  while (index < argumentsOnly.length) {
    const token = argumentsOnly[index];
    if (!token.startsWith("--")) {
      assert(
        !sawOption,
        `CLI example places a positional after options: ${line}`,
      );
      positionals.push(token);
      index += 1;
      continue;
    }
    sawOption = true;
    assert(flagArity.has(token), `CLI example uses an unknown flag ${token}`);
    assert(!seenFlags.has(token), `CLI example repeats ${token}: ${line}`);
    seenFlags.add(token);
    const arity = flagArity.get(token);
    if (token === "--embedded") usesEmbedded = true;
    assert(
      index + arity < argumentsOnly.length,
      `CLI example omits a value for ${token}: ${line}`,
    );
    if (arity === 1) {
      const value = argumentsOnly[index + 1];
      assert(
        !value.startsWith("--"),
        `CLI example omits a value for ${token}: ${line}`,
      );
      if (token === "--aggregate-json" || token === "--aggregate-csv") {
        assert(
          /^C:\\/i.test(value),
          `${token} must use an explicit Windows output path: ${line}`,
        );
      }
    }
    index += arity + 1;
  }

  assert(
    !(usesChop && usesEmbedded),
    `The released chop form requires a ChapterFile, not --embedded: ${line}`,
  );

  assert(
    positionals.length === (usesEmbedded ? 1 : 2),
    `CLI example has an invalid positional/source shape: ${line}`,
  );
  assert(
    /^C:\\/i.test(positionals[0]),
    `CLI input must use an explicit Windows path: ${line}`,
  );
  if (!usesEmbedded) {
    assert(
      /^C:\\.*\.(json|ya?ml)$/i.test(positionals[1]),
      `CLI ChapterFile must use an explicit JSON/YAML path: ${line}`,
    );
  }
  if (usesChop) return "chop-config";
  return usesEmbedded ? "direct-embedded" : "direct-config";
}

try {
  for (const route of routes.htmlRoutes) {
    const { body } = await fetchRoute(route, 200, "text/html; charset=utf-8");
    const html = new TextDecoder().decode(body);
    assert(
      html.includes('<div id="root"></div>'),
      `${route}: missing React root`,
    );
    const canonicalRoute =
      route === "/features"
        ? "/?section=features"
        : route === "/download"
          ? "/releases"
          : route;
    const canonicalUrl = new URL(canonicalRoute, routes.canonicalOrigin).href;
    assert(
      html.includes(`rel="canonical" href="${canonicalUrl}"`),
      `${route}: canonical URL drift`,
    );
    const expectedTitle = routes.htmlTitles[route];
    assert(
      html.includes(`<title>${expectedTitle}</title>`),
      `${route}: document title drift`,
    );
    assert(
      html.includes(`property="og:title" content="${expectedTitle}"`),
      `${route}: Open Graph title drift`,
    );
  }

  for (const asset of routes.assets) {
    const { body, headers } = await fetchRoute(
      asset.route,
      200,
      asset.contentType,
    );
    const source = new Uint8Array(
      await readFile(path.resolve(repositoryRoot, asset.source)),
    );
    assert(
      Buffer.compare(body, source) === 0,
      `${asset.route}: published bytes differ from ${asset.source}`,
    );
    assert(
      headers.get("cache-control") === cacheControl(asset.cache),
      `${asset.route}: cache policy drift`,
    );
    if (asset.cache === "immutable") {
      assert(
        sha256(body) === asset.sha256,
        `${asset.route}: immutable digest drift`,
      );
    }
    observedAssets.set(asset.route, body);
  }

  const stableSchemaRoute = "/schemas/chapter-config/schema.json";
  const schema = JSON.parse(
    new TextDecoder().decode(observedAssets.get(stableSchemaRoute)),
  );
  const schemaVersion = schema?.properties?.version?.const;
  assert(
    Number.isSafeInteger(schemaVersion) && schemaVersion > 0,
    "Published schema must declare a positive integer version const.",
  );
  const versionedSchemaRoute =
    `/schemas/chapter-config/v${schemaVersion}/schema.json`;
  const stableSampleRoute = "/samples/chapter-config/chapter-config.json";
  const versionedSampleRoute =
    `/samples/chapter-config/v${schemaVersion}/chapter-config.json`;
  const stableYamlRoute = "/samples/chapter-config/chapter-config.yaml";
  const versionedYamlRoute =
    `/samples/chapter-config/v${schemaVersion}/chapter-config.yaml`;

  for (const [stable, versioned] of [
    [stableSchemaRoute, versionedSchemaRoute],
    [stableSampleRoute, versionedSampleRoute],
    [stableYamlRoute, versionedYamlRoute],
  ]) {
    assert(
      observedAssets.has(stable) && observedAssets.has(versioned),
      `${stable} and ${versioned} must both be published`,
    );
    assert(
      Buffer.compare(
        observedAssets.get(stable),
        observedAssets.get(versioned),
      ) === 0,
      `${stable} and ${versioned} must publish byte-identical aliases`,
    );
  }

  assert(
    schema.$id === new URL(versionedSchemaRoute, routes.canonicalOrigin).href,
    "Published ChapterFile schema has a stale $id.",
  );

  const sample = JSON.parse(
    new TextDecoder().decode(observedAssets.get(stableSampleRoute)),
  );
  assert(
    sample.version === schemaVersion,
    "Published sample ChapterFile version disagrees with the schema.",
  );
  assert(
    sample.$schema === schema.$id,
    "Published sample does not point at the canonical schema.",
  );

  const packageMetadata = JSON.parse(
    await readFile(path.join(repositoryRoot, "docs", "package.json"), "utf8"),
  );
  const versionedReleaseRoute = `/releases/v${packageMetadata.version}/manifest.json`;
  assert(
    observedAssets.has(versionedReleaseRoute),
    `Release route is missing for package version ${packageMetadata.version}.`,
  );
  const release = JSON.parse(
    new TextDecoder().decode(observedAssets.get(versionedReleaseRoute)),
  );
  assert(
    Buffer.compare(
      observedAssets.get("/releases/current.json"),
      observedAssets.get(versionedReleaseRoute),
    ) === 0,
    "/releases/current.json and the versioned manifest must be byte-identical aliases",
  );
  const { expectedAssetUrl, expectedTag } = assertReleaseMetadata(
    release,
    packageMetadata.version,
    schemaVersion,
  );

  assert(
    routes.delivery.htmlHandling === "drop-trailing-slash",
    "Canonical route style must keep /docs slash-free.",
  );
  assert(
    routes.delivery.notFoundHandling === "404-page",
    "Machine routes require strict 404-page delivery.",
  );

  const siteSource = await readFile(
    path.join(repositoryRoot, "docs", "src", "content", "site.ts"),
    "utf8",
  );
  const routerSources = await Promise.all(
    [
      "App.tsx",
      "components/shell.tsx",
      "pages/docs-page.tsx",
      "pages/home-page.tsx",
      "pages/release-page.tsx",
      "router.tsx",
    ].map((file) =>
      readFile(path.join(repositoryRoot, "docs", "src", file), "utf8"),
    ),
  );
  const sourceText = [siteSource, ...routerSources].join("\n");
  const onboardingSource = await readFile(
    path.join(repositoryRoot, "docs", "src", "components", "agent-onboarding.tsx"),
    "utf8",
  );
  const routerSource = routerSources.at(-1);
  assert(
    !sourceText.includes("HashLink") && !sourceText.includes("useHash"),
    "Hash-only router APIs remain in site source.",
  );
  assert(
    !sourceText.includes('to="#/'),
    "Canonical site links must not emit hash routes.",
  );
  const expectedLegacyRoutes = {
    "build-from-source": "/docs/getting-started",
    architecture: "/docs",
    "coding-style": "/docs",
    "test-strategy": "/docs/manifests",
    "release-workflows": "/releases",
    "agent-knowledge-base": "/docs/safety",
  };
  assert(
    JSON.stringify(routes.legacyDocsRoutes) ===
      JSON.stringify(expectedLegacyRoutes),
    "Retired documentation route mappings drifted.",
  );
  for (const safeguard of [
    'window.location.hash.startsWith("#/")',
    "stripPagesBase(window.location.pathname)",
    "routeContract.legacyDocsRoutes",
    "event.defaultPrevented",
    "event.button !== 0",
    "event.metaKey",
    "event.ctrlKey",
    "event.shiftKey",
    "event.altKey",
    "target !== undefined",
    "window.history.pushState",
  ]) {
    assert(
      routerSource.includes(safeguard),
      `Router navigation safeguard drifted: ${safeguard}`,
    );
  }
  for (const focusSafeguard of [
    'querySelector<HTMLElement>("[data-route-focus]")',
    "target.focus({ preventScroll: true })",
  ]) {
    assert(
      sourceText.includes(focusSafeguard),
      `Route focus/search safeguard drifted: ${focusSafeguard}`,
    );
  }
  for (const forbiddenDataSink of [
    "fetch(",
    "localStorage",
    "sessionStorage",
    "sendBeacon",
    "XMLHttpRequest",
    "URLSearchParams",
    "window.history",
  ]) {
    assert(
      !onboardingSource.includes(forbiddenDataSink),
      `Agent onboarding must not put prompt or media data in ${forbiddenDataSink}.`,
    );
  }

  const expectedSiteConstants = [
    'export const siteUrl = "https://vidchopper.app";',
    `export const releaseVersion = "${expectedTag}";`,
    `  "${expectedAssetUrl}";`,
    "export const docsUrl = `${siteUrl}/docs`;",
    "export const chapterFileSchemaUrl = `${siteUrl}/schemas/chapter-config/v1/schema.json`;",
    "export const chapterFileSampleUrl = `${siteUrl}/samples/chapter-config/v1/chapter-config.json`;",
    "export const agentSkillUrl = `${siteUrl}/agents/vidchopper-cli/SKILL.md`;",
    `export const releaseMetadataUrl = \`\${siteUrl}/releases/v${packageMetadata.version}/manifest.json\`;`,
  ];
  for (const expected of expectedSiteConstants) {
    assert(
      siteSource.includes(expected),
      `Site release/link constant drifted: ${expected}`,
    );
  }

  const hardCodedLinks = [
    ...sourceText.matchAll(/<SiteLink[^>]*\bto="([^"]+)"/g),
  ].map((match) => match[1]);
  for (const link of hardCodedLinks) {
    const pathname = new URL(link, routes.canonicalOrigin).pathname;
    assert(
      routes.htmlRoutes.includes(pathname),
      `Internal link ${link} is missing from the HTML route contract`,
    );
  }

  const declaredDocsEntries = [
    ...siteSource.matchAll(
      /path: "(\/docs\/[a-z0-9-]+)",\s+title: "([^"]+)"/g,
    ),
  ].map((match) => ({ path: match[1], title: match[2] }));
  const declaredDocsPaths = declaredDocsEntries.map((entry) => entry.path);
  const contractDocsPaths = routes.htmlRoutes.filter((route) =>
    route.startsWith("/docs/"),
  );
  assert(
    JSON.stringify([...declaredDocsPaths].sort()) ===
      JSON.stringify([...contractDocsPaths].sort()),
    "Docs content paths and physical HTML routes disagree.",
  );
  for (const entry of declaredDocsEntries) {
    assert(
      routes.htmlTitles[entry.path] === `${entry.title} | VidChopper Docs`,
      `${entry.path}: route title disagrees with docs content`,
    );
  }

  const cliArguments = await readFile(
    path.join(repositoryRoot, "src", "cli", "cli_arguments.cpp"),
    "utf8",
  );
  const expectedUsageLines = [
    "VidChopperCLI.exe <input-video> <chapters.json|chapters.yaml> [options]",
    "VidChopperCLI.exe <input-video> --embedded [options]",
    "VidChopperCLI.exe chop <input-video> <chapters.json|chapters.yaml> [options]",
  ];
  const usageExpression = cliArguments.match(
    /auto cli_usage\(\) -> std::string \{\s*return ([\s\S]*?);\s*\}/,
  )?.[1];
  assert(usageExpression, "Could not parse cli_usage().");
  const usageText = [...usageExpression.matchAll(/"((?:\\.|[^"\\])*)"/g)]
    .map((match) => JSON.parse(`"${match[1]}"`))
    .join("");
  const usageLines = usageText.split("\n").map((line) => line.trim());
  const releasedUsageLines = usageLines.filter((line) =>
    line.startsWith("VidChopperCLI.exe "),
  );
  assert(
    JSON.stringify(releasedUsageLines) === JSON.stringify(expectedUsageLines),
    "Released CLI invocation forms drifted from the documented contract.",
  );
  assert(
    cliArguments.includes(
      'constexpr auto command_chop = std::string_view {"chop"};',
    ),
    "Released chop subcommand drifted.",
  );

  const flagArity = new Map();
  for (const line of usageLines) {
    const match = line.match(
      /^(?:-h,\s+)?(--[a-z0-9-]+)(?:\s+(<[^>]+>))?\s+/,
    );
    if (match) flagArity.set(match[1], match[2] ? 1 : 0);
  }
  const releasedFlags = new Set(
    [...cliArguments.matchAll(/"(--[a-z0-9-]+)"/g)].map((match) => match[1]),
  );
  assert(
    JSON.stringify([...releasedFlags].sort()) ===
      JSON.stringify([...flagArity.keys()].sort()),
    `Frozen ${packageMetadata.version} flag contract drifted from cli_arguments.cpp.`,
  );

  const stableSkillRoute = "/agents/vidchopper-cli/SKILL.md";
  const stableSkillManifestRoute = "/agents/vidchopper-cli/manifest.json";
  const versionedSkillRoute =
    `/agents/vidchopper-cli/v${packageMetadata.version}/SKILL.md`;
  const versionedSkillManifestRoute =
    `/agents/vidchopper-cli/v${packageMetadata.version}/manifest.json`;
  const skillArchiveRoute =
    `/agents/vidchopper-cli/v${packageMetadata.version}/vidchopper-cli.zip`;
  const skillIndexRoute = "/.well-known/agent-skills/index.json";
  for (const route of [
    stableSkillRoute,
    stableSkillManifestRoute,
    versionedSkillRoute,
    versionedSkillManifestRoute,
    skillArchiveRoute,
    skillIndexRoute,
  ]) {
    assert(observedAssets.has(route), `Agent skill route is missing: ${route}`);
  }
  assert(
    Buffer.compare(
      observedAssets.get(stableSkillRoute),
      observedAssets.get(versionedSkillRoute),
    ) === 0,
    "Stable and versioned agent skill bytes must be identical.",
  );
  assert(
    Buffer.compare(
      observedAssets.get(stableSkillManifestRoute),
      observedAssets.get(versionedSkillManifestRoute),
    ) === 0,
    "Stable and versioned agent skill manifests must be byte-identical aliases.",
  );

  const skillBytes = observedAssets.get(versionedSkillRoute);
  const skillArchiveBytes = observedAssets.get(skillArchiveRoute);
  const skillManifest = JSON.parse(
    new TextDecoder().decode(observedAssets.get(versionedSkillManifestRoute)),
  );
  assert(
    skillManifest.skillContractVersion === 1 &&
      skillManifest.cliVersion === packageMetadata.version &&
      skillManifest.chapterFileSchemaVersion === schemaVersion &&
      skillManifest.exportManifestSchemaVersion === 1,
    "Agent skill compatibility tuple drifted.",
  );
  assert(
    skillManifest.skillSha256 === sha256(skillBytes),
    "Agent skill manifest SKILL.md digest drifted.",
  );
  assert(
    skillManifest.skillArchiveSha256 === sha256(skillArchiveBytes),
    "Agent skill manifest archive digest drifted.",
  );
  assert(
    skillManifest.chapterFileSchemaSha256 ===
      sha256(observedAssets.get(versionedSchemaRoute)),
    "Agent skill manifest ChapterFile schema digest drifted.",
  );
  assert(
    skillManifest.repositoryPath ===
      ".agents/skills/vidchopper-cli/SKILL.md" &&
      /^[a-f0-9]{40}$/.test(skillManifest.sourceCommit),
    "Agent skill source identity drifted.",
  );
  assert(
    skillManifest.stableUrl ===
      new URL(stableSkillRoute, routes.canonicalOrigin).href &&
      skillManifest.versionedUrl ===
        new URL(versionedSkillRoute, routes.canonicalOrigin).href &&
      skillManifest.archiveUrl ===
        new URL(skillArchiveRoute, routes.canonicalOrigin).href,
    "Agent skill manifest URLs drifted.",
  );
  assert(
    JSON.stringify([...skillManifest.cliFlags].sort()) ===
      JSON.stringify([...releasedFlags].sort()),
    "Agent skill manifest flags drifted from the released CLI.",
  );

  const { stdout: sourceSkillBytes } = await execFileAsync(
    "git",
    [
      "-C",
      repositoryRoot,
      "show",
      `${skillManifest.sourceCommit}:${skillManifest.repositoryPath}`,
    ],
    { encoding: "buffer", maxBuffer: 2 * 1024 * 1024, windowsHide: true },
  );
  assert(
    Buffer.compare(skillBytes, sourceSkillBytes) === 0,
    "Agent skill sourceCommit does not resolve to the published SKILL.md bytes.",
  );

  const skillIndex = JSON.parse(
    new TextDecoder().decode(observedAssets.get(skillIndexRoute)),
  );
  const skillIndexEntry = skillIndex.skills?.[0];
  assert(
    skillIndex.$schema ===
      "https://schemas.agentskills.io/discovery/0.2.0/schema.json" &&
      skillIndex.skills?.length === 1 &&
      skillIndexEntry?.name === "vidchopper-cli" &&
      skillIndexEntry?.type === "archive" &&
      skillIndexEntry?.url === skillArchiveRoute &&
      skillIndexEntry?.digest ===
        `sha256:${skillManifest.skillArchiveSha256}`,
    "Agent skill discovery index drifted.",
  );

  const skillText = new TextDecoder().decode(skillBytes);
  for (const required of [
    "name: vidchopper-cli",
    'vidchopper.skill-contract-version: "1"',
    "Planned chapters: N",
    "Existing output: yes",
    "Immediately before export",
  ]) {
    assert(
      skillText.includes(required),
      `Published agent skill safety contract drifted: ${required}`,
    );
  }

  const commandBlocks = [...siteSource.matchAll(/command: `([^`]*)`/gs)].map(
    (match) => match[1],
  );
  const cliLines = commandBlocks
    .flatMap((command) => command.split(/\r?\n/))
    .filter((line) => line.includes("VidChopperCLI.exe"));
  const documentedForms = new Set(
    cliLines.map((line) => validateCliCommand(line, flagArity)),
  );
  for (const expectedForm of [
    "version",
    "direct-config",
    "direct-embedded",
    "chop-config",
  ]) {
    assert(
      documentedForms.has(expectedForm),
      `Docs are missing the ${expectedForm} CLI invocation form.`,
    );
  }

  const tokenizedLines = cliLines.map(tokenizePowerShell);
  const hasDryRunExportPair = tokenizedLines.some((tokens) => {
    const dryRunIndex = tokens.indexOf("--dry-run");
    if (dryRunIndex === -1 || !tokens.includes("chop")) return false;
    const exportTokens = tokens.toSpliced(dryRunIndex, 1);
    return tokenizedLines.some(
      (candidate) =>
        candidate.length === exportTokens.length &&
        candidate.every((token, index) => token === exportTokens[index]),
    );
  });
  assert(
    hasDryRunExportPair,
    "Docs must show a chop dry-run and the identical approved export command.",
  );

  for (const deferred of routes.deferredAssets) {
    await fetchRoute(deferred.route, 404, "text/html; charset=utf-8");
  }
  for (const route of routes.notFoundProbes) {
    await fetchRoute(route, 404, "text/html; charset=utf-8");
  }

  const rootHtml = await readFile(
    path.join(distDirectory, "index.html"),
    "utf8",
  );
  assert(
    rootHtml.includes("<noscript>"),
    "Built root HTML is missing the no-JavaScript fallback.",
  );
  assert(
    rootHtml.indexOf("<noscript>") < rootHtml.indexOf('<div id="root"></div>'),
    "The no-JavaScript fallback must render before the empty React root.",
  );
  for (const fallbackContract of [
    "https://vidchopper.app/agents/vidchopper-cli/SKILL.md",
    "dry-run",
    "exporting",
    "overwriting",
    "uploading",
    "publishing",
    "deleting",
  ]) {
    assert(
      rootHtml.includes(fallbackContract),
      `No-JavaScript agent prompt drifted: ${fallbackContract}`,
    );
  }
  const assetReferences = [
    ...rootHtml.matchAll(/(?:href|src)="([^"]*\/assets\/[^"]+)"/g),
  ].map((match) => match[1]);
  assert(
    assetReferences.length > 0,
    "Built root HTML does not reference a hashed asset.",
  );
  const expectedAssetOrigin = new URL(origin).origin;
  const expectedAssetPrefix = requestPath("/assets/");
  for (const reference of assetReferences) {
    const assetUrl = new URL(reference, origin);
    assert(
      assetUrl.origin === expectedAssetOrigin &&
        assetUrl.username === "" &&
        assetUrl.password === "",
      `Built asset reference escaped the validation origin: ${reference}`,
    );
    assert(
      assetUrl.pathname.startsWith(expectedAssetPrefix) &&
        assetUrl.search === "" &&
        assetUrl.hash === "",
      `Built asset reference escaped ${expectedAssetPrefix}: ${reference}`,
    );
    const response = await fetch(assetUrl, {
      redirect: "manual",
    });
    assert(
      response.status === 200,
      `Built asset ${reference} returned ${response.status}`,
    );
  }

  if (remoteMode && !pagesMode) {
    await verifyCanonicalRedirect("/docs/", "/docs");
    await verifyCanonicalRedirect("/docs/index.html", "/docs");
    await verifyCanonicalRedirect("/index.html", "/");
  }

  console.log(
    `Validated ${remoteMode ? `remote origin ${origin}` : "built artifact"}: ` +
      `${routes.htmlRoutes.length} HTML routes, ${routes.assets.length} static assets, ` +
      `${routes.deferredAssets.length + routes.notFoundProbes.length} strict 404 routes, ` +
      "GET/HEAD, source bytes, immutable digests, MIME, cache, metadata, links, and CLI examples.",
  );
} finally {
  if (server !== null) {
    await new Promise((resolve, reject) => {
      server.close((error) => (error ? reject(error) : resolve()));
      server.closeAllConnections();
    });
  }
}
