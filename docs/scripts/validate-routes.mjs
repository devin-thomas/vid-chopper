import { readFile, rm, writeFile } from "node:fs/promises";

const coreUrl = new URL("./validate-routes-core.mjs", import.meta.url);
const generatedUrl = new URL("./.validate-routes-current.mjs", import.meta.url);
const oldVersionMarker = 'const stableSkillVersion = "1.0.0";';
const currentVersionMarker = 'const stableSkillVersion = "1.2.0";';

let source = await readFile(coreUrl, "utf8");
const firstMarker = source.indexOf(oldVersionMarker);
if (firstMarker === -1 || source.indexOf(oldVersionMarker, firstMarker + 1) !== -1) {
  throw new Error(
    "Route validator core no longer contains exactly one stable-skill version marker.",
  );
}
source = source.replace(oldVersionMarker, currentVersionMarker);

await writeFile(generatedUrl, source, "utf8");
try {
  await import(`${generatedUrl.href}?stable-skill=1.2.0`);
} finally {
  await rm(generatedUrl, { force: true });
}
