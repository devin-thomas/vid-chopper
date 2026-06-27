import { Route, Routes } from "react-router-dom";
import { Shell } from "./components/shell";
import { DocsPage } from "./pages/docs-page";
import { FeaturesPage } from "./pages/features-page";
import { HomePage } from "./pages/home-page";
import { ReleasePage } from "./pages/release-page";

export default function App() {
  return (
    <Routes>
      <Route element={<Shell />}>
        <Route index element={<HomePage />} />
        <Route path="/features" element={<FeaturesPage />} />
        <Route path="/download" element={<ReleasePage />} />
        <Route path="/docs" element={<DocsPage />} />
      </Route>
    </Routes>
  );
}
