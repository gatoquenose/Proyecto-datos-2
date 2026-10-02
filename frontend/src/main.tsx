import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "./style.css";

function App() {
  return <main><h1>Tecflix</h1><p>Phase A frontend shell</p></main>;
}

createRoot(document.getElementById("root")!).render(<StrictMode><App /></StrictMode>);
