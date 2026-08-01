# Cloudflare Production

This runbook publishes the canonical static artifact to the existing Cloudflare Worker and custom
domain. It does not authorize an agent to change DNS, certificates, billing, or production traffic
without the explicit VID-55 human gate.

## Pre-cutover Snapshot

Read-only inspection on 2026-08-01 confirmed:

- Worker: `vidchopper`
- Custom domain: `vidchopper.app`
- Active deployment: `5c335ae3-017e-43e3-8458-bac2f60f499f`
- Active version: `ea79b2ce-dfc3-4f6a-8c07-e5190a423e77`
- Previous deployment: `9ffc61e6-5d13-4163-99db-43213fdf913c`
- Previous version: `82bd296d-816d-4fd0-8768-f38ec29009c6`
- Apex A records: `172.67.168.126`, `104.21.38.213`
- Apex AAAA records: `2606:4700:3034::ac43:a87e`, `2606:4700:3036::6815:26d5`
- TLS and the Cloudflare-managed custom-domain certificate are active.
- Legacy root: 1,059 bytes, SHA-256
  `e7e5552ff43cc1acbd8a93a43c7352885b7cba0f0e66b2190cde27d81c7428ba`
- Legacy JavaScript: `/assets/index-CIphFV9M.js`, SHA-256
  `1716cf5c8881896851fc4d278f1ac310fdc48ea65bc868ded284b0b32854b131`
- Legacy CSS: `/assets/index-CKWnIMA1.css`, SHA-256
  `1d18ed52bb8a13d1b547886ff4fbc96d00f4e25929fb8506dff95321ff5638aa`

The active version is an emergency recovery target only. It serves the legacy `v0.2.0-alpha`
single-page shell and fails the VID-55 machine-resource and strict-404 contract. There was no
previous VID-55-accepted deployment before this cutover.

## GitHub Environment

Use the `cloudflare-environment` GitHub environment for production workflow runs:

- Environment secret `CLOUDFLARE_ACCOUNT_ID`: the existing Uppercut Labs account ID.
- Environment secret `CLOUDFLARE_API_TOKEN`: a dedicated least-privilege token.
- Restrict deployments to `main`.

Follow Cloudflare's documented `Edit Cloudflare Workers` token template and scope its account and
zone resources to the intended account and `vidchopper.app`. Do not hand-trim the template before
the first accepted deployment: Wrangler needs the template's Worker script/route access and its
supporting read permissions. Do not copy the broader local Wrangler OAuth credential into GitHub.
Never commit either value.

## Local Preflight

Run from a clean worktree at the exact candidate commit:

```powershell
pwsh -NoProfile -File tools/verify.ps1 -CiLane Docs
Push-Location docs
npm run build
npm run cloudflare:dry-run
Pop-Location
git diff --check origin/main...HEAD
```

The dry run must show only the files produced under `docs/dist`. The build contract rejects
source maps, repository source/configuration, credential-adjacent files, symlinks, excessive file
sizes, missing static routes, and an unpinned or unsafe Wrangler configuration.

## Production Deployment

Only after the explicit production approval:

1. Merge the focused, green VID-55 pull request to `main`.
2. Open the manual `Cloudflare Production` workflow on GitHub.
3. Select `main`, enter `deploy vidchopper.app`, and dispatch it.
4. Let the workflow first prove that both environment secrets can read the expected Worker
   deployment/version contracts, then rebuild, test, dry-run, deploy, capture the new identities,
   and run the cache-busted live route validator.
5. Record the accepted deployment ID, version ID, source commit, Actions run, and live evidence in
   VID-55. That deployment becomes the last known good production release.

The read-only credential preflight catches an invalid token, wrong account scope, or changed
Wrangler response contract immediately after the lockfile install and before tests, builds, or
deployment. The install step never receives production secrets, and the preflight uses only the
lockfile-installed Wrangler binary. Cloudflare does not expose the stored token's complete
permission list to this workflow, so the first upload remains the decisive proof that the token
retains the template's Worker write permissions. The deployment action is pinned to an audited
commit rather than a mutable version tag because it receives both production secrets.

The custom domain already exists, so the expected operation updates the Worker version and its
static assets rather than creating a new hostname. Stop if Wrangler proposes a different hostname,
account, Worker name, route, or binding.

Cloudflare documents static-asset requests as free and unlimited and asset storage as having no
additional charge. Confirm the account's existing Workers plan before dispatch because the account
plan itself may still be paid:

- https://developers.cloudflare.com/workers/static-assets/billing-and-limitations/
- https://developers.cloudflare.com/workers/platform/pricing/

## Live Acceptance

The workflow runs:

```powershell
node scripts/validate-routes.mjs --origin https://vidchopper.app
```

Remote mode cache-busts requests and verifies root/docs routes, the first-party skill, schema,
samples, release manifests, fingerprinted assets, canonical redirects, GET/HEAD parity, exact
machine-resource bytes, MIME and cache policy, and every forbidden or intentionally missing route.
The workflow is not accepted if that command fails.

Also confirm public DNS, TLS, the custom-domain attachment, and the new 100% Worker deployment with
read-only checks before marking VID-55 or the dependent website issues Done.

## Rollback

Cloudflare version rollback changes Worker traffic but does not revert DNS, the custom-domain
attachment, zone settings, or certificates.

For an accepted target, create a clean worktree at the exact source commit recorded beside that
Worker version in VID-55. Build that historical candidate before changing traffic so the byte-level
remote validator compares production with the artifact that actually produced the target:

```powershell
Push-Location docs
npm ci
npm run build
npx wrangler deployments list --name vidchopper --json
npx wrangler rollback <version-id> --name vidchopper --message "VID-55 rollback" --yes
node scripts/validate-routes.mjs --origin https://vidchopper.app
Pop-Location
```

Use the most recent deployment recorded as live-accepted in VID-55. If the first cutover must be
reversed before an accepted replacement exists, version
`ea79b2ce-dfc3-4f6a-8c07-e5190a423e77` restores the pre-cutover legacy site, but it does not
satisfy VID-55 and must be reported as degraded recovery. That legacy version has no trustworthy
source-commit annotation, so the VID-55 validator is expected to fail after this emergency rollback;
confirm only that the recorded legacy root and hashed assets respond, then keep the incident open.

Do not delete or recreate the custom domain, DNS record, or certificates as part of a version
rollback. Any resource-level recovery requires a new explicit admin gate and a fresh read-only
snapshot first.
