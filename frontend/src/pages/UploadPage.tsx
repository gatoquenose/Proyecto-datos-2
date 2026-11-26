import { useQuery } from "@tanstack/react-query";
import { metadataClient } from "../api/metadataClient";
import { streamingClient } from "../api/streamingClient";
import { usersClient } from "../api/usersClient";
import { useAuth } from "../auth/AuthContext";
import { ACCEPTED_CONTENT_TYPES, UploadWizard } from "../components/UploadWizard";
import type { UploadClients } from "../hooks/useVideoUpload";

const startUpload: UploadClients["startUpload"] = (videoId, body) => {
  if (!(ACCEPTED_CONTENT_TYPES as readonly string[]).includes(body.contentType)) {
    return Promise.reject(new Error("Tipo de video no permitido."));
  }
  return streamingClient.startUpload(videoId, {
    ...body,
    contentType: body.contentType as typeof ACCEPTED_CONTENT_TYPES[number],
  });
};

export function UploadPage(): JSX.Element {
  const { user } = useAuth();
  const quota = useQuery({ queryKey: ["quota"], queryFn: usersClient.getMyQuota, enabled: Boolean(user) });
  if (!user) return <p className="status">Cargando sesión…</p>;
  return <UploadWizard
    username={user.username}
    remainingBytes={quota.data?.remainingBytes}
    clients={{
      createVideo: metadataClient.createVideo,
      startUpload,
      uploadChunk: streamingClient.uploadChunk,
      getUpload: streamingClient.getUpload,
      completeUpload: streamingClient.completeUpload,
      getProcessing: streamingClient.getProcessing,
      cancelUpload: streamingClient.cancelUpload,
      deleteVideo: metadataClient.deleteVideo,
    }}
  />;
}
