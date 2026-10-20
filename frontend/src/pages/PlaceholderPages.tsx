import { Link, useParams } from "react-router-dom";

export function VideoDetailPage(): JSX.Element { return <section><h1>Video</h1><p>Reproductor y detalles del video.</p><Link to={`/videos/${useParams().id ?? ""}/edit`}>Editar metadata</Link></section>; }
export function UploadPage(): JSX.Element { return <section><h1>Subir video</h1><p>El asistente de subida estará disponible en F5.</p></section>; }
export function ProfilePage(): JSX.Element { return <section><h1>Mi perfil</h1><p>Perfil, cuota y configuración de cuenta.</p></section>; }
export function NotFoundPage(): JSX.Element { return <section><h1>Página no encontrada</h1><Link to="/videos">Volver a videos</Link></section>; }
