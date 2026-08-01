import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import {
  distDirectory,
  htmlPath,
  isPagesMode,
  routes,
} from "./site-contract.mjs";

const pagesMode = isPagesMode();
const sourceHtml = await readFile(
  path.join(distDirectory, "index.html"),
  "utf8",
);

function canonicalRoute(route) {
  if (route === "/features") return "/?section=features";
  if (route === "/download") return "/releases";
  return route;
}

function withMetadata(html, route, title) {
  const canonicalUrl = new URL(canonicalRoute(route), routes.canonicalOrigin)
    .href;
  return html
    .replace(/<title>[^<]*<\/title>/, `<title>${title}</title>`)
    .replace(
      /<link\s+rel="canonical"\s+href="[^"]*"\s*\/?>/,
      `<link rel="canonical" href="${canonicalUrl}" />`,
    )
    .replace(
      /<meta\s+property="og:title"\s+content="[^"]*"\s*\/?>/,
      `<meta property="og:title" content="${title}" />`,
    )
    .replace(
      /<meta\s+property="og:url"\s+content="[^"]*"\s*\/?>/,
      `<meta property="og:url" content="${canonicalUrl}" />`,
    );
}

for (const route of routes.htmlRoutes) {
  const destination = htmlPath(distDirectory, route);
  await mkdir(path.dirname(destination), { recursive: true });
  await writeFile(
    destination,
    withMetadata(sourceHtml, route, routes.htmlTitles[route]),
    "utf8",
  );
}

await writeFile(
  path.join(distDirectory, "404.html"),
  withMetadata(sourceHtml, "/not-found", "Page not found | VidChopper"),
  "utf8",
);

console.log(
  `Finalized ${routes.htmlRoutes.length} HTML routes for ${pagesMode ? "GitHub Pages compatibility" : "the canonical site"}.`,
);
