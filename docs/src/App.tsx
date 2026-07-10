import { Navigate, Route, Routes } from "react-router-dom";
import { Shell } from "./components/shell";
import { DocsPage } from "./pages/docs-page";
import { HomePage } from "./pages/home-page";
import { ReleasePage } from "./pages/release-page";

export default function App() {
  return (
    <Routes>
      <Route element={<Shell />}>
        <Route index element={<HomePage />} />
        <Route path="/features" element={<Navigate to="/?section=features" replace />} />
        <Route path="/download" element={<Navigate to="/releases" replace />} />
        <Route path="/releases" element={<ReleasePage />} />
        <Route path="/docs" element={<DocsPage />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Route>
    </Routes>
  );
}
