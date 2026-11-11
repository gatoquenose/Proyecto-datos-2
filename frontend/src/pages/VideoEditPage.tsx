import { useCallback, useState } from "react";
import { useQuery } from "@tanstack/react-query";
import { Navigate, useNavigate, useParams } from "react-router-dom";
import { metadataClient } from "../api/metadataClient";
import { useAuth } from "../auth/AuthContext";
import { ErrorState } from "../components/ErrorState";
import { VideoEditForm } from "../components/VideoEditForm";
import { ApiError } from "../types/api";
import { LoadingState } from "../components/LoadingState";

export function VideoEditPage(): JSX.Element {
  const { id = "" } = useParams();
  const { user } = useAuth();
  const navigate = useNavigate();
  const [pageError, setPageError] = useState("");
  const video = useQuery({
    queryKey: ["video", id],
    queryFn: () => metadataClient.getVideo(id),
    enabled: Boolean(id),
  });
  const handleSuccess = useCallback(() => navigate(`/videos/${id}`), [id, navigate]);
  const handleError = useCallback((message: string) => setPageError(message), []);

  if (video.isPending) return <LoadingState label="Cargando video…" />;
  if (video.isError) {
    if (video.error instanceof ApiError && [403, 404].includes(video.error.status)) {
      return <Navigate to="/videos" replace state={{ message: video.error.body.message }} />;
    }
    return <ErrorState error={video.error} />;
  }
  if (!user || video.data.ownerId !== user.id) return <Navigate to="/videos" replace />;

  return <section className="video-edit-page">
    {pageError && <p className="form-error" role="alert">{pageError}</p>}
    <VideoEditForm
      videoId={id}
      ownerId={video.data.ownerId}
      currentUserId={user.id}
      onSuccess={handleSuccess}
      onError={handleError}
    />
  </section>;
}
