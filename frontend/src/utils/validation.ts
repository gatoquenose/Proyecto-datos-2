export interface FieldErrors {
  username?: string;
  email?: string;
  password?: string;
  confirmPassword?: string;
}

export const validateUsername = (value: string): string | undefined => {
  if (!value) return "El usuario es obligatorio.";
  if (value.length < 3 || value.length > 64) return "Usa entre 3 y 64 caracteres.";
  if (!/^[A-Za-z0-9_.-]+$/.test(value)) return "Usa letras, números, punto, guion o guion bajo.";
  return undefined;
};

export const validateEmail = (value: string): string | undefined => {
  if (!value) return "El correo es obligatorio.";
  if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(value)) return "Escribe un correo válido.";
  return undefined;
};

export const validatePassword = (value: string): string | undefined => {
  if (!value) return "La contraseña es obligatoria.";
  if (value.length < 8 || value.length > 128) return "Usa entre 8 y 128 caracteres.";
  return undefined;
};
