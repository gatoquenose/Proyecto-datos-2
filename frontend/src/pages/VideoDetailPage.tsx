import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { Link, useNavigate, useParams } from "react-router-dom";
import { useEffect, useState } from "react";
import { metadataClient } from "../api/metadataClient";
import { streamingClient } from "../api/streamingClient";
import { useAuth } from "../auth/AuthContext";
import { ConfirmDialog } from "../components/ConfirmDialog";
import { DashPlayer } from "../components/DashPlayer";
import { ErrorState } from "../components/ErrorState";
import { LoadingState } from "../components/LoadingState";
import { ApiError } from "../types/api";

export function VideoDetailPage(): JSX.Element {
  const { id = "" } = useParams();
  const { user } = useAuth();
  const navigate = useNavigate();
  const queryClient = useQueryClient();
  const [showDelete, setShowDelete] = useState(false);
  const video = useQuery({ queryKey: ["video", id], queryFn: () => metadataClient.getVideo(id), enabled: Boolean(id) });
  const processing = useQuery({
    queryKey: ["processing", id],
    queryFn: () => streamingClient.getProcessing(id),
    enabled: Boolean(id) && video.data?.status === "processing",
    retry: false,
    refetchInterval: (query) => {
      const status = query.state.data?.status;
      return status === "ready" || status === "failed" || query.state.error instanceof ApiError && [401, 403, 404].includes(query.state.error.status) ? false : 2000;
    },
  });
  const visibility = useMutation({
    mutationFn: (next: "public" | "private") => metadataClient.updateVideoVisibility(id, next),
    onSuccess: (next) => queryClient.setQueryData(["video", id], next),
  });
  const remove = useMutation({
    mutationFn: () => metadataClient.deleteVideo(id),
    onSuccess: async () => { await queryClient.invalidateQueries({ queryKey: ["videos"] }); navigate("/videos/mine"); },
  });
  useEffect(() => {
    if (processing.data?.status === "ready") {
      void queryClient.invalidateQueries({ queryKey: ["video", id] });
      void queryClient.invalidateQueries({ queryKey: ["videos"] });
    }
  }, [id, processing.data?.status, queryClient]);
  if (video.isPending) return <LoadingState />;
  if (video.isError) return <ErrorState error={video.error} />;
  const isOwner = video.data.ownerId === user?.id;
  if (video.data.status === "processing") return <section><h1>{video.data.title}</h1><p>Procesando video…</p>{processing.data?.status === "failed" && <ErrorState error={new Error(processing.data.error ?? "El procesamiento falló.")} />}</section>;
  if (video.data.status === "failed") return <section><h1>{video.data.title}</h1><p className="error-box" role="alert">El procesamiento del video falló.</p></section>;
  return <section className="video-detail"><div className="page-heading"><div><h1>{video.data.title}</h1><p>{video.data.description}</p></div>{isOwner && <div className="owner-actions"><Link className="button" to={`/videos/${id}/edit`}>Editar</Link><button type="button" onClick={() => visibility.mutate(video.data.visibility === "public" ? "private" : "public")}>Cambiar a {video.data.visibility === "public" ? "privado" : "público"}</button><button type="button" className="danger" onClick={() => setShowDelete(true)}>Eliminar</button></div>}</div>
    <DashPlayer manifestUrl={streamingClient.getManifestUrl(id)} />
    {showDelete && <ConfirmDialog title="Confirmar eliminación" confirmLabel="Eliminar video" onCancel={() => setShowDelete(false)} onConfirm={() => remove.mutate()}><p>Esta acción no se puede deshacer.</p></ConfirmDialog>}
  </section>;
}
