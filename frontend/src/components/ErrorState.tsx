import { friendlyErrorMessage, type ApiError } from "../types/api";

export function ErrorState({ error, retry }: { error: unknown; retry?: () => void }): JSX.Element {
  const apiError = error as Partial<ApiError>;
  const body = apiError.body;
  return (
    <section className="error-box" role="alert" aria-live="assertive">
      <strong>{body ? friendlyErrorMessage(body.code) : "No se pudo completar la solicitud."}</strong>
      {body?.traceId && <small>Trace ID: {body.traceId}</small>}
      {retry && <button type="button" onClick={retry}>Reintentar</button>}
    </section>
  );
}
