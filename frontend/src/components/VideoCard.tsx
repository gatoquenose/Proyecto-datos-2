import type { components } from "../types/generated/metadata";
import { Link } from "react-router-dom";

type Video = components["schemas"]["Video"];

const formatDuration = (seconds: number): string => `${Math.floor(seconds / 60)}:${String(seconds % 60).padStart(2, "0")}`;
const formatDate = (value: string): string => new Intl.DateTimeFormat("es-MX", { dateStyle: "medium" }).format(new Date(value));

export function VideoCard({ video, showOwnerDetails }: { video: Video; showOwnerDetails: boolean }): JSX.Element {
  return <article className="card video-card">
    <div className="thumbnail" role="img" aria-label={`Miniatura de ${video.title}`}>▶</div>
    <div className="video-card-body">
      <h2><Link to={`/videos/${video.id}`}>{video.title}</Link></h2>
      <p>{video.ownerUsername}</p>
      <p>{formatDate(video.uploadedAt)} · {formatDuration(video.durationSeconds)}</p>
      {showOwnerDetails && <p className="video-status"><span>{video.visibility === "public" ? "Público" : "Privado"}</span> · <span>{video.status === "ready" ? "Listo" : video.status === "processing" ? "Procesando" : "Falló"}</span></p>}
    </div>
  </article>;
}
