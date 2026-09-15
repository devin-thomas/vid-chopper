# Cloudflare Production

`vidchopper.app` is served by the direct-upload Cloudflare Pages project `vidchopper` in the Uppercut
Labs account. The project also answers at `vidchopper.pages.dev`. There is no Worker: the earlier
`vidchopper` Worker has been deleted, and nothing in this repository may recreate it.

## How the Site Updates

Merge to `main`. When the push touches the site inputs listed in `.github/workflows/cloudflare.yml`,
the `Cloudflare Pages` workflow:

1. installs the pinned dependencies,
2. checks that the environment secrets can read the Pages project,
3. validates agent skill artifacts and runs frontend tests,
4. builds and audits the canonical artifact (`npm run build`),
5. uploads `docs/dist` with `wrangler pages deploy dist --project-name vidchopper --branch main`,
6. runs the cache-busted live validator against `https://vidchopper.app`.

To republish without a code change, open **Actions → Cloudflare Pages → Run workflow** on `main`.

## GitHub Environment

The workflow uses the `cloudflare-environment` GitHub environment, restricted to `main`:

- Environment secret `CLOUDFLARE_ACCOUNT_ID`: the Uppercut Labs account ID.
- Environment secret `CLOUDFLARE_API_TOKEN`: a dedicated API token with the
  **Account → Cloudflare Pages → Edit** permission, scoped to the Uppercut Labs account.

Do not copy a local Wrangler OAuth credential into GitHub. Never commit either value.

## Manual Deployment

Use the workflow whenever possible. From a workstation, deploy only the canonical build:

```powershell
npx wrangler login
$env:CLOUDFLARE_ACCOUNT_ID = "<Uppercut Labs account ID>"
Push-Location docs
npm ci
npm run deploy:cloudflare
node scripts/validate-routes.mjs --origin https://vidchopper.app
Pop-Location
```

`CLOUDFLARE_ACCOUNT_ID` is required locally when the Wrangler login can see more than one account.

Never upload the `npm run build:pages` output to Cloudflare. That artifact is rooted at
`/vid-chopper/` for the GitHub Pages mirror, so every script and stylesheet 404s on
`vidchopper.app` and the site renders blank. A manual upload in September 2026 did exactly that.

## Previews

Any other branch name creates a preview without touching production:

```powershell
Push-Location docs
npm run build
npx wrangler pages deploy dist --project-name vidchopper --branch my-change
node scripts/validate-routes.mjs --origin https://my-change.vidchopper.pages.dev
Pop-Location
```

## How Routing Works on Pages

- Every HTML route is emitted as a flat file (`docs.html`, `docs/cli.html`). Pages serves it at the
  slash-free URL and redirects `/docs/` and `/docs.html` there. A `docs/index.html` would instead
  redirect `/docs` to `/docs/`, breaking the canonical URLs.
- `_redirects` sends legacy `/<route>/index.html` links to `/<route>`.
- `_headers` sets the MIME and cache policy for every HTML route, machine resource, and fingerprinted
  bundle. Pages parses both files and never serves them.
- The root `404.html` disables Pages' single-page fallback, so unknown machine resources return a real
  `404`.

The build emits all of this. `npm run build` fails if the artifact drifts from `docs/routes.json`.

## Rollback

Rolling back changes which deployment serves production. It never touches DNS, the custom domain, or
certificates.

- Fastest: Cloudflare dashboard → **Workers & Pages → vidchopper → Deployments**. Open the last good
  production deployment and choose **Rollback to this deployment**.
- Durable: revert the bad commit on `main`; the workflow rebuilds and redeploys.

After a dashboard rollback, check out the source commit shown on that deployment and run
`npm run build`. Then run `node scripts/validate-routes.mjs --origin https://vidchopper.app`: the
validator compares production bytes with the local build.

Stop if Wrangler proposes a different account or project name. Changes to DNS, the custom domain
attachment, or certificates need an explicit admin decision and a fresh read-only snapshot first.
