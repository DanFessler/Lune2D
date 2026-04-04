import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import { installEngineScriptBridgeShim } from "./luaEditorBridge";
import "./index.css";

installEngineScriptBridgeShim();

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
