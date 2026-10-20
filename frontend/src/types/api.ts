export interface ApiErrorBody {
  code: string;
  message: string;
  details: Record<string, unknown> | null;
  traceId: string;
}

export class ApiError extends Error {
  readonly status: number;
  readonly body: ApiErrorBody;

  constructor(status: number, body: ApiErrorBody) {
    super(body.message);
    this.name = "ApiError";
    this.status = status;
    this.body = body;
  }
}

export const friendlyErrorMessage = (code: string): string => {
  const messages: Record<string, string> = {
    VALIDATION_ERROR: "Revisa los datos enviados.",
    INVALID_CURSOR: "La página solicitada ya no está disponible.",
    MISSING_TOKEN: "Tu sesión es necesaria.",
    INVALID_TOKEN: "Tu sesión ya no es válida.",
    TOKEN_EXPIRED: "Tu sesión expiró.",
    FORBIDDEN: "No tienes permisos para esta acción.",
    QUOTA_EXCEEDED: "No tienes suficiente cuota disponible.",
    NOT_FOUND: "No encontramos el recurso solicitado.",
    CONFLICT: "La operación entra en conflicto con el estado actual.",
    USERNAME_EXISTS: "Ese nombre de usuario ya existe.",
    EMAIL_EXISTS: "Ese correo ya está registrado.",
    INVALID_STATE: "La operación no es válida en este estado.",
    PAYLOAD_TOO_LARGE: "El archivo es demasiado grande.",
    UNSUPPORTED_MEDIA_TYPE: "El tipo de archivo no está soportado.",
    UNPROCESSABLE_ENTITY: "No se puede procesar la información.",
    DEPENDENCY_ERROR: "Un servicio dependiente no está disponible.",
    SERVICE_UNAVAILABLE: "El servicio no está listo.",
  };
  return messages[code] ?? "Ocurrió un error inesperado.";
};
