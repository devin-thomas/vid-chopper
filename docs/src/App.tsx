import { Shell } from "./components/shell";
import { DocsPage } from "./pages/docs-page";
import { HomePage } from "./pages/home-page";
import { ReleasePage } from "./pages/release-page";
import { useHashLocation } from "./router";

export default function App() {
  const { pathname } = useHashLocation();
  const page = pathname === "/releases" ? <ReleasePage /> : pathname === "/docs" ? <DocsPage /> : <HomePage />;
  return <Shell>{page}</Shell>;
}
