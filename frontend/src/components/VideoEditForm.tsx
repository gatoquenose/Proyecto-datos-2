import { useEffect, useRef, useState, type FormEvent } from "react";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { Link, useNavigate } from "react-router-dom";
import { metadataClient } from "../api/metadataClient";
import type { components } from "../types/generated/metadata";
import { ApiError, friendlyErrorMessage } from "../types/api";
import {
  validateVideoMetadata,
  type EditableVideoMetadata,
  type VideoMetadataField,
} from "../utils/videoMetadata";

type Video = components["schemas"]["Video"];
type VideoUpdate = components["schemas"]["UpdateVideoRequest"];
type Visibility = components["schemas"]["Visibility"];
type EditableField = Exclude<VideoMetadataField, "tags"> | "tags" | "uploadedAt";

interface VideoEditFormProps {
  videoId: string;
  ownerId: string;
  currentUserId: string;
  onSuccess: () => void;
  onError: (message: string) => void;
}

interface FormData {
  title: string;
  description: string;
  author: string;
  topic: string;
  tags: string;
  uploadedAt: string;
}

const EMPTY_FORM: FormData = {
  title: "",
  description: "",
  author: "",
  topic: "",
  tags: "",
  uploadedAt: "",
};

const STATUS_LABELS: Record<Video["status"], string> = {
  processing: "Procesando",
  ready: "Listo",
  failed: "Falló",
};

const pad = (value: number): string => String(value).padStart(2, "0");

const toLocalDateTimeInput = (isoValue: string): string => {
  const date = new Date(isoValue);
  if (!Number.isFinite(date.getTime())) return "";
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
};

const formatDateForUser = (isoValue: string): string => {
  const date = new Date(isoValue);
  return Number.isFinite(date.getTime())
    ? new Intl.DateTimeFormat("es-CR", { dateStyle: "medium", timeStyle: "short" }).format(date)
    : "Fecha no disponible";
};

const formatDuration = (seconds: number): string => {
  const totalSeconds = Math.max(0, Math.floor(seconds));
  return `${Math.floor(totalSeconds / 60)}:${pad(totalSeconds % 60)}`;
};

const toFormData = (video: Video): FormData => ({
  title: video.title,
  description: video.description,
  author: video.author,
  topic: video.topic,
  tags: video.tags.join(", "),
  uploadedAt: toLocalDateTimeInput(video.uploadedAt),
});

const getApiMessage = (error: unknown): string =>
  error instanceof ApiError ? error.body.message || friendlyErrorMessage(error.body.code) : "No se pudo guardar el video.";

export function VideoEditForm({
  videoId,
  ownerId,
  currentUserId,
  onSuccess,
  onError,
}: VideoEditFormProps): JSX.Element {
  const navigate = useNavigate();
  const queryClient = useQueryClient();
  const [formData, setFormData] = useState<FormData>(EMPTY_FORM);
  const [visibility, setVisibility] = useState<Visibility | "">("");
  const [errors, setErrors] = useState<Partial<Record<EditableField, string>>>({});
  const [submitError, setSubmitError] = useState("");
  const [successMessage, setSuccessMessage] = useState("");
  const [isSubmitting, setIsSubmitting] = useState(false);
  const initializedForVideo = useRef("");

  const videoQuery = useQuery({
    queryKey: ["video", videoId],
    queryFn: () => metadataClient.getVideo(videoId),
    enabled: Boolean(videoId),
  });
  const updateMetadata = useMutation({
    mutationFn: (body: VideoUpdate) => metadataClient.updateVideo(videoId, body),
  });
  const updateVisibility = useMutation({
    mutationFn: (nextVisibility: Visibility) => metadataClient.updateVideoVisibility(videoId, nextVisibility),
  });

  useEffect(() => {
    const video = videoQuery.data;
    if (!video || initializedForVideo.current === videoId) return;
    initializedForVideo.current = videoId;
    setFormData(toFormData(video));
    setVisibility(video.visibility);
  }, [videoId, videoQuery.data]);

  useEffect(() => {
    const error = videoQuery.error;
    if (!(error instanceof ApiError) || ![403, 404].includes(error.status)) return;
    const message = error.status === 403
      ? "No tienes permisos para editar este video."
      : "No encontramos el video solicitado.";
    onError(message);
    navigate("/videos", { replace: true, state: { message } });
  }, [navigate, onError, videoQuery.error]);

  const setField = (field: keyof FormData, value: string): void => {
    setFormData((previous) => ({ ...previous, [field]: value }));
    setErrors((previous) => ({ ...previous, [field]: "" }));
    setSubmitError("");
    setSuccessMessage("");
  };

  const reportFailure = (error: unknown, partialSaveMessage?: string): void => {
    if (error instanceof ApiError && error.status === 401) return;
    if (error instanceof ApiError && [403, 404].includes(error.status)) {
      const message = error.status === 403
        ? "No tienes permisos para editar este video."
        : "No encontramos el video solicitado.";
      onError(message);
      navigate("/videos", { replace: true, state: { message } });
      return;
    }
    const message = partialSaveMessage
      ? `${partialSaveMessage} ${getApiMessage(error)}`
      : getApiMessage(error);
    setSubmitError(message);
    onError(message);
  };

  const handleSubmit = async (event: FormEvent<HTMLFormElement>): Promise<void> => {
    event.preventDefault();
    const tags = formData.tags.split(",").map((tag) => tag.trim()).filter(Boolean);
    const metadata: EditableVideoMetadata = {
      title: formData.title,
      description: formData.description,
      author: formData.author,
      topic: formData.topic,
      tags,
    };
    const nextErrors: Partial<Record<EditableField, string>> = { ...validateVideoMetadata(metadata) };
    const date = new Date(formData.uploadedAt);
    if (!formData.uploadedAt || !Number.isFinite(date.getTime())) {
      nextErrors.uploadedAt = "Ingresa una fecha y hora válidas.";
    }
    setErrors(nextErrors);
    setSubmitError("");
    setSuccessMessage("");
    if (Object.keys(nextErrors).length > 0) {
      setSubmitError("Corrige los errores del formulario.");
      const firstInvalid = (Object.keys(nextErrors) as EditableField[])[0];
      const fieldId = firstInvalid === "uploadedAt" ? "edit-video-uploaded-at" : `edit-video-${firstInvalid}`;
      window.setTimeout(() => document.getElementById(fieldId)?.focus(), 0);
      return;
    }

    const video = videoQuery.data;
    if (!video) return;
    const updatedAt = date.toISOString();
    const metadataPatch: VideoUpdate = {};
    if (formData.title !== video.title) metadataPatch.title = formData.title;
    if (formData.description !== video.description) metadataPatch.description = formData.description;
    if (formData.author !== video.author) metadataPatch.author = formData.author;
    if (formData.topic !== video.topic) metadataPatch.topic = formData.topic;
    if (tags.length !== video.tags.length || tags.some((tag, index) => tag !== video.tags[index])) {
      metadataPatch.tags = tags;
    }
    if (date.getTime() !== new Date(video.uploadedAt).getTime()) metadataPatch.uploadedAt = updatedAt;
    const metadataChanged = Object.keys(metadataPatch).length > 0;
    const visibilityChanged = visibility !== "" && visibility !== video.visibility;
    if (!metadataChanged && !visibilityChanged) {
      setSuccessMessage("No hay cambios para guardar.");
      return;
    }

    setIsSubmitting(true);
    let metadataSaved = false;
    try {
      if (metadataChanged) {
        await updateMetadata.mutateAsync(metadataPatch);
        metadataSaved = true;
        setSuccessMessage("Metadatos actualizados.");
      }
      if (visibilityChanged && visibility) {
        try {
          await updateVisibility.mutateAsync(visibility);
          setSuccessMessage(metadataSaved
            ? "Metadatos y visibilidad actualizados."
            : "Visibilidad actualizada.");
        } catch (error) {
          reportFailure(
            error,
            metadataSaved ? "Los metadatos se guardaron, pero no se pudo actualizar la visibilidad." : undefined,
          );
          return;
        }
      }

      onSuccess();
    } catch (error) {
      reportFailure(error);
    } finally {
      await Promise.all([
        queryClient.invalidateQueries({ queryKey: ["video", videoId] }),
        queryClient.invalidateQueries({ queryKey: ["videos"] }),
      ]);
      setIsSubmitting(false);
    }
  };

  if (videoQuery.isPending) {
    return <div className="card status" aria-label="Cargando video"><span className="skeleton">Cargando video…</span></div>;
  }
  if (videoQuery.isError) {
    if (videoQuery.error instanceof ApiError && videoQuery.error.status === 401) return <p className="status">Tu sesión expiró.</p>;
    if (videoQuery.error instanceof ApiError && [403, 404].includes(videoQuery.error.status)) return <p className="status">Volviendo a la lista de videos…</p>;
    return <div className="error-box" role="alert">{getApiMessage(videoQuery.error)}</div>;
  }

  const video = videoQuery.data;
  if (!video) return <p className="status">No encontramos el video solicitado.</p>;
  if (ownerId !== currentUserId || video.ownerId !== currentUserId) {
    return <section className="error-box">
      <h1>Sin permisos</h1>
      <p>No tienes permisos para editar este video.</p>
      <Link className="button secondary" to="/videos">Volver a los videos</Link>
    </section>;
  }

  const fieldError = (field: EditableField): string | undefined => errors[field];
  const fieldProps = (field: EditableField) => ({
    "aria-invalid": Boolean(fieldError(field)),
    "aria-describedby": fieldError(field) ? `video-edit-${field}-error` : undefined,
  });
  const sizeInMegabytes = (video.sizeBytes / 1_000_000).toFixed(2);

  return <form className="card upload-form" noValidate onSubmit={(event) => void handleSubmit(event)}>
    <h1>Editar video</h1>
    {submitError && <p className="form-error" role="alert">{submitError}</p>}
    {successMessage && <p className="success-box" role="status">{successMessage}</p>}

    <label htmlFor="edit-video-title">Título<input id="edit-video-title" type="text" maxLength={200} value={formData.title} {...fieldProps("title")} onChange={(event) => setField("title", event.target.value)} /></label>
    {errors.title && <small id="video-edit-title-error" className="field-error">{errors.title}</small>}

    <label htmlFor="edit-video-description">Descripción<textarea id="edit-video-description" maxLength={5000} value={formData.description} {...fieldProps("description")} onChange={(event) => setField("description", event.target.value)} /></label>
    {errors.description && <small id="video-edit-description-error" className="field-error">{errors.description}</small>}

    <label htmlFor="edit-video-author">Autor<input id="edit-video-author" type="text" maxLength={200} value={formData.author} {...fieldProps("author")} onChange={(event) => setField("author", event.target.value)} /></label>
    {errors.author && <small id="video-edit-author-error" className="field-error">{errors.author}</small>}

    <label htmlFor="edit-video-topic">Tema<input id="edit-video-topic" type="text" maxLength={100} value={formData.topic} {...fieldProps("topic")} onChange={(event) => setField("topic", event.target.value)} /></label>
    {errors.topic && <small id="video-edit-topic-error" className="field-error">{errors.topic}</small>}

    <label htmlFor="edit-video-tags">Etiquetas<input id="edit-video-tags" type="text" value={formData.tags} {...fieldProps("tags")} onChange={(event) => setField("tags", event.target.value)} aria-describedby={fieldError("tags") ? "edit-video-tags-help video-edit-tags-error" : "edit-video-tags-help"} /></label>
    <small id="edit-video-tags-help">Separa las etiquetas con comas; máximo 30 etiquetas de 50 caracteres.</small>
    {errors.tags && <small id="video-edit-tags-error" className="field-error">{errors.tags}</small>}

    <label htmlFor="edit-video-uploaded-at">Fecha y hora de carga<input id="edit-video-uploaded-at" type="datetime-local" step="1" value={formData.uploadedAt} {...fieldProps("uploadedAt")} onChange={(event) => setField("uploadedAt", event.target.value)} /></label>
    <small>Fecha actual: {formatDateForUser(video.uploadedAt)} (es-CR)</small>
    {errors.uploadedAt && <small id="video-edit-uploadedAt-error" className="field-error">{errors.uploadedAt}</small>}

    <fieldset>
      <legend>Visibilidad</legend>
      <label htmlFor="edit-video-visibility">Acceso al video</label>
      <select id="edit-video-visibility" value={visibility} onChange={(event) => setVisibility(event.target.value as Visibility)}>
        <option value="public">Público</option>
        <option value="private">Privado</option>
      </select>
      <small>Público: cualquier usuario autenticado lo ve. Privado: solo tú lo ves.</small>
    </fieldset>

    <fieldset>
      <legend>Datos de solo lectura</legend>
      <p>Duración: {formatDuration(video.durationSeconds)}</p>
      <p>Tamaño: {sizeInMegabytes} MB</p>
      <p>Estado: {STATUS_LABELS[video.status]}</p>
    </fieldset>

    <div className="upload-actions">
      <Link className="button secondary" to={`/videos/${videoId}`}>Volver</Link>
      <button className="button primary" type="submit" disabled={isSubmitting}>
        {isSubmitting ? "Guardando…" : "Guardar"}
      </button>
    </div>
  </form>;
}
