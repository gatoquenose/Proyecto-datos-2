import { Link } from "react-router-dom";

export function UploadPage(): JSX.Element { return <section><h1>Subir video</h1><p>El asistente de subida estará disponible en F5.</p></section>; }
export function NotFoundPage(): JSX.Element { return <section><h1>Página no encontrada</h1><Link to="/videos">Volver a videos</Link></section>; }
