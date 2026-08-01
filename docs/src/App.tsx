import { Shell } from "./components/shell";
import { docsGuideposts } from "./content/site";
import { DocsPage } from "./pages/docs-page";
import { HomePage } from "./pages/home-page";
import { NotFoundPage } from "./pages/not-found-page";
import { ReleasePage } from "./pages/release-page";
import { useSiteLocation } from "./router";

export default function App() {
  const { pathname } = useSiteLocation();
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
