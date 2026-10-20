import { useQuery } from "@tanstack/react-query";
import { Link, useSearchParams } from "react-router-dom";
import { metadataClient } from "../api/metadataClient";
import { ErrorState } from "../components/ErrorState";
import { LoadingState } from "../components/LoadingState";

export function VideoListPage(): JSX.Element {
  const [params, setParams] = useSearchParams();
  const owner = window.location.pathname.endsWith("/mine") ? "me" as const : undefined;
  const query = useQuery({ queryKey: ["videos", params.toString(), owner], queryFn: () => metadataClient.listVideos({ owner, sortBy: (params.get("sortBy") as "title" | "author" | "uploadedAt" | null) ?? "uploadedAt", order: (params.get("order") as "asc" | "desc" | null) ?? "desc", q: params.get("q") ?? undefined, pageSize: 12, cursor: params.get("cursor") ?? undefined }) });
  if (query.isPending) return <LoadingState />;
  if (query.isError) return <ErrorState error={query.error} retry={() => void query.refetch()} />;
  return <section><div className="page-heading"><div><h1>{owner ? "Mis videos" : "Videos"}</h1><p>Explora el catálogo de Tecflix.</p></div><input aria-label="Buscar videos" placeholder="Buscar…" value={params.get("q") ?? ""} onChange={(e) => { const next = new URLSearchParams(params); next.set("q", e.target.value); next.delete("cursor"); setParams(next); }} /></div>
    <div className="controls"><label>Ordenar<select value={params.get("sortBy") ?? "uploadedAt"} onChange={(e) => { const next = new URLSearchParams(params); next.set("sortBy", e.target.value); next.delete("cursor"); setParams(next); }}><option value="uploadedAt">Fecha de carga</option><option value="title">Título</option><option value="author">Autor</option></select></label><select aria-label="Dirección" value={params.get("order") ?? "desc"} onChange={(e) => { const next = new URLSearchParams(params); next.set("order", e.target.value); next.delete("cursor"); setParams(next); }}><option value="desc">Descendente</option><option value="asc">Ascendente</option></select></div>
    {query.data.items.length === 0 ? <p className="status">No hay videos para mostrar.</p> : <div className="video-grid">{query.data.items.map((video) => <article className="card video-card" key={video.id}><div className="thumbnail" aria-label={`Miniatura de ${video.title}`}>▶</div><h2>{video.title}</h2><p>{video.ownerUsername} · {video.status}</p><Link to={`/videos/${video.id}`}>Ver video</Link></article>)}</div>}
    {query.data.nextCursor && <button className="button" type="button" onClick={() => { const next = new URLSearchParams(params); next.set("cursor", query.data.nextCursor ?? ""); setParams(next); }}>Cargar más</button>}
  </section>;
}
