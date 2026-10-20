import { ApiError, type ApiErrorBody } from "../types/api";

export type UnauthorizedHandler = (returnPath: string) => void;

let token: string | null = null;
let unauthorizedHandler: UnauthorizedHandler | null = null;

export const setHttpToken = (value: string | null): void => {
  token = value;
};

export const setUnauthorizedHandler = (handler: UnauthorizedHandler | null): void => {
  unauthorizedHandler = handler;
};

const parseError = async (response: Response): Promise<ApiError> => {
  let body: ApiErrorBody;
  try {
    body = (await response.json()) as ApiErrorBody;
  } catch {
    body = {
      code: "INTERNAL_ERROR",
      message: "La respuesta del servidor no es válida.",
      details: null,
      traceId: "unknown",
    };
  }
  return new ApiError(response.status, body);
};

export const request = async <T>(
  url: string,
  init: RequestInit = {},
  returnPath = window.location.pathname + window.location.search,
): Promise<T> => {
  const headers = new Headers(init.headers);
  if (init.body && !headers.has("Content-Type")) headers.set("Content-Type", "application/json");
  if (token) headers.set("Authorization", `Bearer ${token}`);
  const response = await fetch(url, { ...init, headers });
  if (!response.ok) {
    const error = await parseError(response);
    if (response.status === 401) unauthorizedHandler?.(returnPath);
    throw error;
  }
  if (response.status === 204) return undefined as T;
  return (await response.json()) as T;
};
