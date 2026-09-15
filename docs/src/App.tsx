import { Shell } from "./components/shell";
import { docsGuideposts } from "./content/site";
import { DocsPage } from "./pages/docs-page";
import { HomePage } from "./pages/home-page";
import { NotFoundPage } from "./pages/not-found-page";
import { ReleasePage } from "./pages/release-page";
import { useCanonicalLocation, useSiteLocation } from "./router";

export default function App() {
  const location = useSiteLocation();
  const { pathname } = location;
  useCanonicalLocation(location);
  const docsRoute =
    pathname === "/docs" ||
    docsGuideposts.some((section) => section.path === pathname);
  const page =
    pathname === "/" ? (
      <HomePage />
    ) : pathname === "/releases" ? (
      <ReleasePage />
    ) : docsRoute ? (
      <DocsPage />
    ) : (
      <NotFoundPage />
    );
  return <Shell>{page}</Shell>;
}
