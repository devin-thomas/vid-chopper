import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";
import { AppErrorBoundary } from "./components/app-error-boundary";
import { markBootFailed, markBootReady } from "./lib/boot";
import "./styles.css";

const container = document.getElementById("root");

if (container === null) {
  markBootFailed("The application root element is missing.");
} else {
  try {
    ReactDOM.createRoot(container).render(
      <React.StrictMode>
        <AppErrorBoundary>
          <App />
        </AppErrorBoundary>
      </React.StrictMode>,
    );
    markBootReady();
  } catch (error) {
    markBootFailed(
      error instanceof Error
        ? error.message
        : "The application failed to start.",
    );
  }
}
