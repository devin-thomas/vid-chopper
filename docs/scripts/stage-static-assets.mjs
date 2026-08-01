import { execFile } from "node:child_process";
import { createHash } from "node:crypto";
import { constants } from "node:fs";
import {
  copyFile,
  lstat,
  mkdir,
  readFile,
  realpath,
  rm,
  writeFile,
} from "node:fs/promises";
import path from "node:path";
import { promisify } from "node:util";
import {
  approvedSourceRoot,
  assetPath,
  assertInside,
  assetsIgnoreText,
  expectedHeadersText,
  releaseChannelForVersion,
  repositoryRoot,
  routes,
  stagedPublicDirectory,
} from "./site-contract.mjs";

const execFileAsync = promisify(execFile);

async function assertNoSymlinkComponents(source) {
  const relative = path.relative(repositoryRoot, source);
  let current = repositoryRoot;
  for (const segment of relative.split(path.sep)) {
    current = path.join(current, segment);
    if ((await lstat(current)).isSymbolicLink()) {
      throw new Error(
        `Static asset source path must not contain symlinks: ${source}`,
      );
    }
  }
}

async function validateSource(asset, realRepositoryRoot) {
  const source = path.resolve(repositoryRoot, asset.source);
  assertInside(repositoryRoot, source, `Static asset source ${asset.source}`);
  await assertNoSymlinkComponents(source);
  const metadata = await lstat(source);
  if (metadata.isSymbolicLink() || !metadata.isFile()) {
    throw new Error(
      `Static asset source must be a regular non-symlink file: ${asset.source}`,
    );
  }

  const [resolvedSource, resolvedApprovedRoot] = await Promise.all([
    realpath(source),
    realpath(approvedSourceRoot(asset.source)),
  ]);
  assertInside(
    realRepositoryRoot,
    resolvedApprovedRoot,
    `Approved source root for ${asset.source}`,
  );
  assertInside(
    realRepositoryRoot,
    resolvedSource,
    `Real static asset source ${asset.source}`,
  );
  assertInside(
    resolvedApprovedRoot,
    resolvedSource,
    `Static asset approved root ${asset.source}`,
  );

  try {
    await execFileAsync(
      "git",
      ["-C", repositoryRoot, "ls-files", "--error-unmatch", "--", asset.source],
      { windowsHide: true },
    );
  } catch {
    throw new Error(
      `Static asset source must be tracked by Git: ${asset.source}`,
    );
  }
  const requiresPortableTextBytes = routes.assets.some(
    (candidate) =>
      candidate.source === asset.source &&
      candidate.cache === "immutable" &&
      path.extname(candidate.source).toLowerCase() !== ".zip",
  );
  if (requiresPortableTextBytes) {
    const { stdout } = await execFileAsync(
      "git",
      [
        "-C",
        repositoryRoot,
        "check-attr",
        "text",
        "eol",
        "--",
        asset.source,
      ],
      { windowsHide: true },
    );
    if (!stdout.includes(": text: set") || !stdout.includes(": eol: lf")) {
      throw new Error(
        `Immutable text source must be pinned to LF in .gitattributes: ${asset.source}`,
      );
    }
  }
  const bytes = await readFile(resolvedSource);
  const sha256 = createHash("sha256").update(bytes).digest("hex");
  return { asset, source: resolvedSource, sha256 };
}

async function validateReleaseContract(validatedSources) {
  const requireRelease = (condition, message) => {
    if (!condition)
      throw new Error(`Release metadata contract drift: ${message}`);
  };
  const packageMetadata = JSON.parse(
    await readFile(path.join(repositoryRoot, "docs", "package.json"), "utf8"),
  );
  const releaseSource = `packaging/releases/${packageMetadata.version}.json`;
  const stableRoute = "/releases/current.json";
  const versionedRoute = `/releases/v${packageMetadata.version}/manifest.json`;
  const stableSchemaRoute = "/schemas/chapter-config/schema.json";
  const stable = routes.assets.find((asset) => asset.route === stableRoute);
  const versioned = routes.assets.find(
    (asset) => asset.route === versionedRoute,
  );
  const stableSchema = routes.assets.find(
    (asset) => asset.route === stableSchemaRoute,
  );
  if (stable?.source !== releaseSource || versioned?.source !== releaseSource) {
    throw new Error(
      `Release routes must both map ${releaseSource}: ${stableRoute}, ${versionedRoute}`,
    );
  }

  const sourceByName = new Map(
    validatedSources.map(({ asset, source }) => [asset.source, source]),
  );
  const validatedReleaseSource = sourceByName.get(releaseSource);
  const validatedSchemaSource = sourceByName.get(stableSchema?.source);
  if (!validatedReleaseSource || !validatedSchemaSource) {
    throw new Error("Release and schema sources must pass source validation.");
  }

  const [releaseMetadata, schema, cmake] = await Promise.all([
    readFile(validatedReleaseSource, "utf8").then(JSON.parse),
    readFile(validatedSchemaSource, "utf8").then(JSON.parse),
    readFile(path.join(repositoryRoot, "CMakeLists.txt"), "utf8"),
  ]);
  const displayVersion = cmake.match(
    /VIDCHOPPER_DISPLAY_VERSION\s+"([^"]+)"/,
  )?.[1];
  const version = packageMetadata.version;
  const expectedChannel = releaseChannelForVersion(version);
  const expectedTag = `v${version}`;
  const expectedReleaseUrl = `https://github.com/devin-thomas/vid-chopper/releases/tag/${expectedTag}`;
  const expectedAssetName = `VidChopper-${version}-windows-x64.zip`;
  const expectedAssetUrl = `https://github.com/devin-thomas/vid-chopper/releases/download/${expectedTag}/${expectedAssetName}`;
  const expectedChecksumName = `${expectedAssetName}.sha256`;
  const expectedChecksumUrl = `${expectedAssetUrl}.sha256`;
  const releaseAsset = releaseMetadata.assets?.[0];
  const checksumAsset = releaseMetadata.assets?.[1];
  const schemaVersion = schema?.properties?.version?.const;
  const publishedAt = new Date(releaseMetadata.publishedAt);

  requireRelease(
    releaseMetadata.schemaVersion === 1,
    "schemaVersion must be 1",
  );
  requireRelease(
    releaseMetadata.version === version,
    `version must be ${version}`,
  );
  requireRelease(
    releaseMetadata.tag === expectedTag,
    `tag must be ${expectedTag}`,
  );
  requireRelease(
    releaseMetadata.channel === expectedChannel,
    `channel must be ${expectedChannel}`,
  );
  requireRelease(
    !Number.isNaN(publishedAt.valueOf()) &&
      publishedAt.toISOString().replace(".000Z", "Z") ===
        releaseMetadata.publishedAt,
    "publishedAt must be canonical RFC 3339 UTC",
  );
  requireRelease(
    /^[a-f0-9]{40}$/.test(releaseMetadata.sourceCommit ?? ""),
    "sourceCommit must be a full lowercase Git SHA",
  );
  requireRelease(
    releaseMetadata.releaseUrl === expectedReleaseUrl,
    "releaseUrl drifted",
  );
  requireRelease(
    releaseMetadata.docsUrl === "https://vidchopper.app/docs",
    "docsUrl drifted",
  );
  requireRelease(releaseMetadata.cliVersion === version, "cliVersion drifted");
  requireRelease(
    Number.isSafeInteger(schemaVersion) && schemaVersion > 0,
    "published schema must declare a positive integer version const",
  );
  requireRelease(
    releaseMetadata.chapterFileSchemaVersion === schemaVersion,
    "chapterFileSchemaVersion must match the published schema",
  );
  requireRelease(
    releaseMetadata.exportManifestSchemaVersion === 1,
    "exportManifestSchemaVersion must be 1",
  );
  requireRelease(
    Array.isArray(releaseMetadata.assets) &&
      releaseMetadata.assets.length === 2,
    "the package and checksum release assets are required",
  );
  requireRelease(
    releaseAsset?.name === expectedAssetName,
    "asset name drifted",
  );
  requireRelease(releaseAsset?.url === expectedAssetUrl, "asset URL drifted");
  requireRelease(
    Number.isSafeInteger(releaseAsset?.size) && releaseAsset.size > 0,
    "asset size must be a positive safe integer",
  );
  requireRelease(
    /^[a-f0-9]{64}$/.test(releaseAsset?.sha256 ?? ""),
    "asset SHA-256 must be 64 lowercase hexadecimal characters",
  );
  requireRelease(
    checksumAsset?.name === expectedChecksumName,
    "checksum asset name drifted",
  );
  requireRelease(
    checksumAsset?.url === expectedChecksumUrl,
    "checksum asset URL drifted",
  );
  requireRelease(
    Number.isSafeInteger(checksumAsset?.size) && checksumAsset.size > 0,
    "checksum asset size must be a positive safe integer",
  );
  requireRelease(
    /^[a-f0-9]{64}$/.test(checksumAsset?.sha256 ?? ""),
    "checksum asset SHA-256 must be 64 lowercase hexadecimal characters",
  );
  requireRelease(displayVersion === version, "CMake display version drifted");
}

assertInside(
  path.dirname(stagedPublicDirectory),
  stagedPublicDirectory,
  "Staging directory",
);
if (path.basename(stagedPublicDirectory) !== ".staged-public") {
  throw new Error(
    `Refusing to clean unexpected staging directory: ${stagedPublicDirectory}`,
  );
}

// Validate every input before cleaning the previous staging tree.
const realRepositoryRoot = await realpath(repositoryRoot);
const uniqueAssets = [
  ...new Map(routes.assets.map((asset) => [asset.source, asset])).values(),
];
const validatedSources = await Promise.all(
  uniqueAssets.map((asset) => validateSource(asset, realRepositoryRoot)),
);
await validateReleaseContract(validatedSources);
const sourceByRoute = new Map(
  validatedSources.map(({ asset, source, sha256 }) => [
    asset.source,
    { source, sha256 },
  ]),
);
for (const asset of routes.assets) {
  const validated = sourceByRoute.get(asset.source);
  if (!validated) {
    throw new Error(`Static asset source was not validated: ${asset.source}`);
  }
  if (asset.cache === "immutable" && validated.sha256 !== asset.sha256) {
    throw new Error(
      `Immutable asset digest drift for ${asset.route}: expected ${asset.sha256}, got ${validated.sha256}`,
    );
  }
}

await rm(stagedPublicDirectory, { recursive: true, force: true });
await mkdir(stagedPublicDirectory, { recursive: true });
await Promise.all(
  routes.assets.map(async (asset) => {
    const destination = assetPath(stagedPublicDirectory, asset.route);
    await mkdir(path.dirname(destination), { recursive: true });
    await copyFile(
      sourceByRoute.get(asset.source).source,
      destination,
      constants.COPYFILE_EXCL,
    );
  }),
);
await Promise.all([
  writeFile(
    path.join(stagedPublicDirectory, "_headers"),
    expectedHeadersText(),
    "utf8",
  ),
  writeFile(
    path.join(stagedPublicDirectory, ".assetsignore"),
    assetsIgnoreText,
    "utf8",
  ),
]);

console.log(
  `Staged ${routes.assets.length} static assets from tracked canonical sources.`,
);
