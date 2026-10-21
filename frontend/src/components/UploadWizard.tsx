import { useEffect, useRef, useState } from "react";
import { useNavigate } from "react-router-dom";
import type { components } from "../types/generated/metadata";
import type { UploadClients } from "../hooks/useVideoUpload";
import { useVideoUpload } from "../hooks/useVideoUpload";
import { TagInput } from "./TagInput";

type CreateVideoRequest = components["schemas"]["CreateVideoRequest"];
type Visibility = components["schemas"]["Visibility"];

const DEFAULT_MAX_UPLOAD_BYTES = 2 * 1024 * 1024 * 1024;
const ACCEPTED_CONTENT_TYPES = ["video/mp4", "video/webm", "video/quicktime", "video/x-matroska"] as const;

interface ResumeRecord {
  uploadId: string;
  videoId: string;
  fileName: string;
  sizeBytes: number;
  contentType: string;
  chunkSize: number;
}

interface UploadWizardProps {
  clients: UploadClients;
  username: string;
  remainingBytes?: number;
}

const maxUploadBytes = (): number => {
  const configured = Number(import.meta.env.VITE_MAX_UPLOAD_BYTES);
  return Number.isFinite(configured) && configured > 0 ? configured : DEFAULT_MAX_UPLOAD_BYTES;
};

const getResumeRecord = (): ResumeRecord | null => {
  const raw = sessionStorage.getItem("tecflix.pending-upload");
  if (!raw) return null;
  try {
    const parsed: unknown = JSON.parse(raw);
    if (parsed && typeof parsed === "object" && "uploadId" in parsed && "videoId" in parsed && "fileName" in parsed && "sizeBytes" in parsed && "contentType" in parsed && "chunkSize" in parsed) {
      return parsed as ResumeRecord;
    }
  } catch {
    sessionStorage.removeItem("tecflix.pending-upload");
  }
  return null;
};

export function UploadWizard({ clients, username, remainingBytes }: UploadWizardProps): JSX.Element {
  const navigate = useNavigate();
  const [step, setStep] = useState<1 | 2>(1);
  const [title, setTitle] = useState("");
  const [description, setDescription] = useState("");
  const [author, setAuthor] = useState(username);
  const [topic, setTopic] = useState("");
  const [tags, setTags] = useState<string[]>([]);
  const [visibility, setVisibility] = useState<Visibility>("private");
  const [file, setFile] = useState<File | null>(null);
  const [formError, setFormError] = useState("");
  const [fileError, setFileError] = useState("");
  const [resume, setResume] = useState<ResumeRecord | null>(getResumeRecord);
  const headingRef = useRef<HTMLHeadingElement>(null);
  const metadata: CreateVideoRequest = { title, description, author, topic, tags, visibility, sizeBytes: file?.size ?? 0 };
  const { state, start, retryFailed, discardFailed, cancel } = useVideoUpload(clients);

  useEffect(() => {
    headingRef.current?.focus();
  }, [step]);

  useEffect(() => {
    if (state.phase === "published" && state.videoId) navigate(`/videos/${state.videoId}`);
  }, [navigate, state.phase, state.videoId]);

  const validateMetadata = (): boolean => {
    if (!title.trim() || title.length > 200) { setFormError("El título es obligatorio y debe tener entre 1 y 200 caracteres."); return false; }
    if (description.length > 5000) { setFormError("La descripción no puede superar 5000 caracteres."); return false; }
    if (author.length > 200) { setFormError("El autor no puede superar 200 caracteres."); return false; }
    if (topic.length > 100) { setFormError("El tema no puede superar 100 caracteres."); return false; }
    if (tags.length > 30 || tags.some((tag) => tag.length > 50)) { setFormError("Las etiquetas deben ser hasta 30 y tener máximo 50 caracteres."); return false; }
    setFormError("");
    return true;
  };

  const validateFile = (selected: File | null): boolean => {
    if (!selected) { setFileError("Selecciona un archivo de video."); return false; }
    if (!(ACCEPTED_CONTENT_TYPES as readonly string[]).includes(selected.type)) { setFileError("Tipo no permitido. Usa MP4, WebM, QuickTime o Matroska."); return false; }
    if (selected.size === 0) { setFileError("El archivo no puede estar vacío."); return false; }
    if (selected.size > maxUploadBytes()) { setFileError("El archivo supera el tamaño máximo configurado de 2 GiB."); return false; }
    setFileError("");
    return true;
  };

  const blockedByQuota = Boolean(file && remainingBytes !== undefined && file.size > remainingBytes);
  const selectFile = (selected: File | null): void => {
    setFile(selected);
    setResume(getResumeRecord());
    validateFile(selected);
  };
  const sameAsResume = Boolean(file && resume && file.name === resume.fileName && file.size === resume.sizeBytes && file.type === resume.contentType);
  const busy = ["creating", "starting", "uploading", "completing", "processing"].includes(state.phase);
  const retry = (): void => {
    if (file) void retryFailed(file, metadata).catch(() => undefined);
  };
  const submit = (): void => {
    if (!file || !validateFile(file) || blockedByQuota) return;
    void (sameAsResume && resume
      ? start(file, metadata, { uploadId: resume.uploadId, videoId: resume.videoId, chunkSize: resume.chunkSize })
      : start(file, metadata)).catch(() => undefined);
  };

  return <section className="upload-page">
    <h1 ref={headingRef} tabIndex={-1}>Subir video</h1>
    <p aria-live="polite">Paso {step} de 2: {step === 1 ? "metadatos" : "archivo"}</p>
    {step === 1 && <form className="card upload-form" onSubmit={(event) => { event.preventDefault(); if (validateMetadata()) setStep(2); }}>
      <h2>Metadatos</h2>
      {formError && <p className="form-error" role="alert">{formError}</p>}
      <label htmlFor="video-title">Título<input id="video-title" value={title} maxLength={200} onChange={(event) => setTitle(event.target.value)} required /></label>
      <label htmlFor="video-description">Descripción<textarea id="video-description" value={description} maxLength={5000} onChange={(event) => setDescription(event.target.value)} /></label>
      <label htmlFor="video-author">Autor<input id="video-author" value={author} maxLength={200} onChange={(event) => setAuthor(event.target.value)} /></label>
      <label htmlFor="video-topic">Tema<input id="video-topic" value={topic} maxLength={100} onChange={(event) => setTopic(event.target.value)} /></label>
      <TagInput value={tags} onChange={setTags} />
      <label htmlFor="video-visibility">Visibilidad<select id="video-visibility" value={visibility} onChange={(event) => setVisibility(event.target.value as Visibility)}><option value="private">Privado</option><option value="public">Público</option></select></label>
      <button className="button primary" type="submit">Continuar al archivo</button>
    </form>}
    {step === 2 && <div className="card upload-form">
      <h2>Archivo</h2>
      {fileError && <p className="form-error" role="alert">{fileError}</p>}
      <label htmlFor="video-file">Archivo de video<input id="video-file" type="file" accept={ACCEPTED_CONTENT_TYPES.join(",")} onChange={(event) => selectFile(event.target.files?.[0] ?? null)} /></label>
      {blockedByQuota && <div className="form-error" role="alert">
        <strong>El archivo supera el espacio disponible.</strong>
        <p>El servidor reserva espacio adicional para el procesamiento; si responde QUOTA_EXCEEDED, esa respuesta es la fuente de verdad.</p>
        {remainingBytes !== undefined && <small>Espacio restante: {remainingBytes} bytes.</small>}
      </div>}
      {sameAsResume && <p className="success-box" role="status">Existe una subida pendiente para este archivo. Puedes reanudarla.</p>}
      <div className="upload-actions">
        <button className="button" type="button" onClick={() => setStep(1)} disabled={busy}>Volver a metadatos</button>
        <button className="button primary" type="button" onClick={submit} disabled={!file || Boolean(fileError) || blockedByQuota || busy}>Subir video</button>
      </div>
      {state.phase !== "idle" && <div className="upload-status" aria-live="polite">
        <p>Estado: {state.phase}</p>
        {(state.phase === "uploading" || state.phase === "completing") && <div role="progressbar" aria-label="Progreso de subida" aria-valuemin={0} aria-valuemax={100} aria-valuenow={state.progress}>{state.progress}%</div>}
        {state.error && <p className="form-error" role="alert">{state.error}</p>}
        {busy && state.phase !== "processing" && <button className="button danger" type="button" onClick={() => void cancel()}>Cancelar</button>}
        {state.phase === "failed" && <><button className="button primary" type="button" onClick={retry} disabled={!file}>Reintentar</button><button className="button" type="button" onClick={() => void discardFailed()}>Descartar</button></>}
      </div>}
    </div>}
  </section>;
}

export { ACCEPTED_CONTENT_TYPES, DEFAULT_MAX_UPLOAD_BYTES };
