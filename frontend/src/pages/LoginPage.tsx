import { FormEvent, useState } from "react";
import { Link, useLocation, useNavigate, useSearchParams } from "react-router-dom";
import { usersClient } from "../api/usersClient";
import { ApiError, friendlyErrorMessage } from "../types/api";
import { useAuth } from "../auth/AuthContext";
import { type FieldErrors, validateEmail, validatePassword, validateUsername } from "../utils/validation";

export function LoginPage(): JSX.Element {
  const { login, sessionExpired } = useAuth();
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const location = useLocation();
  const [form, setForm] = useState({ username: "", password: "" });
  const [errors, setErrors] = useState<FieldErrors>({});
  const [formError, setFormError] = useState("");
  const expiryMessage = sessionExpired ? "Tu sesión expiró." : (location.state as { message?: string } | null)?.message;
  const submit = async (event: FormEvent): Promise<void> => {
    event.preventDefault();
    const nextErrors: FieldErrors = {};
    const usernameError = validateUsername(form.username);
    const passwordError = validatePassword(form.password);
    if (usernameError) nextErrors.username = usernameError;
    if (passwordError) nextErrors.password = passwordError;
    setErrors(nextErrors);
    setFormError("");
    if (Object.keys(nextErrors).length) return;
    try {
      const response = await usersClient.login(form);
      login(response.accessToken, response.user);
      navigate(params.get("returnTo") ?? "/videos", { replace: true, state: null });
    } catch (caught) {
      setFormError(caught instanceof ApiError ? friendlyErrorMessage(caught.body.code) : "No se pudo iniciar sesión.");
    }
  };
  return <AuthForm title="Iniciar sesión" error={formError || expiryMessage} onSubmit={submit} footer={<Link to={`/register${location.search}`}>Crear cuenta</Link>}>
    <Field label="Usuario" name="login-username" value={form.username} onChange={(value) => setForm({ ...form, username: value })} error={errors.username} autoComplete="username" />
    <Field label="Contraseña" name="login-password" type="password" value={form.password} onChange={(value) => setForm({ ...form, password: value })} error={errors.password} autoComplete="current-password" />
  </AuthForm>;
}

export function RegisterPage(): JSX.Element {
  const navigate = useNavigate();
  const [form, setForm] = useState({ username: "", email: "", password: "", confirmPassword: "" });
  const [errors, setErrors] = useState<FieldErrors>({});
  const [formError, setFormError] = useState("");
  const submit = async (event: FormEvent): Promise<void> => {
    event.preventDefault();
    const nextErrors: FieldErrors = {};
    const usernameError = validateUsername(form.username);
    const emailError = validateEmail(form.email);
    const passwordError = validatePassword(form.password);
    if (usernameError) nextErrors.username = usernameError;
    if (emailError) nextErrors.email = emailError;
    if (passwordError) nextErrors.password = passwordError;
    if (form.password !== form.confirmPassword) nextErrors.confirmPassword = "Las contraseñas no coinciden.";
    setErrors(nextErrors);
    setFormError("");
    if (Object.keys(nextErrors).length) return;
    try {
      await usersClient.createUser({ username: form.username, email: form.email, password: form.password });
      navigate("/login", { state: { message: "Cuenta creada. Ya puedes iniciar sesión." } });
    } catch (caught) {
      if (caught instanceof ApiError) {
        if (caught.body.code === "USERNAME_EXISTS") setErrors({ username: friendlyErrorMessage(caught.body.code) });
        else if (caught.body.code === "EMAIL_EXISTS") setErrors({ email: friendlyErrorMessage(caught.body.code) });
        else setFormError(friendlyErrorMessage(caught.body.code));
      } else setFormError("No se pudo registrar la cuenta.");
    }
  };
  return <AuthForm title="Crear cuenta" error={formError} onSubmit={submit} footer={<Link to="/login">Ya tengo cuenta</Link>}>
    <Field label="Usuario" name="register-username" value={form.username} onChange={(value) => setForm({ ...form, username: value })} error={errors.username} autoComplete="username" />
    <Field label="Correo" name="register-email" type="email" value={form.email} onChange={(value) => setForm({ ...form, email: value })} error={errors.email} autoComplete="email" />
    <Field label="Contraseña" name="register-password" type="password" value={form.password} onChange={(value) => setForm({ ...form, password: value })} error={errors.password} autoComplete="new-password" hint="Entre 8 y 128 caracteres." />
    <Field label="Confirmar contraseña" name="register-confirm-password" type="password" value={form.confirmPassword} onChange={(value) => setForm({ ...form, confirmPassword: value })} error={errors.confirmPassword} autoComplete="new-password" />
  </AuthForm>;
}

function Field({ label, name, type = "text", value, onChange, error, autoComplete, hint }: { label: string; name: string; type?: string; value: string; onChange: (value: string) => void; error?: string; autoComplete: string; hint?: string }): JSX.Element {
  const errorId = `${name}-error`;
  return <div className="field"><label htmlFor={name}>{label}</label><input id={name} name={name} type={type} value={value} onChange={(event) => onChange(event.target.value)} autoComplete={autoComplete} aria-invalid={Boolean(error)} aria-describedby={error ? errorId : hint ? `${name}-hint` : undefined} />{hint && <small id={`${name}-hint`}>{hint}</small>}{error && <small id={errorId} className="field-error">{error}</small>}</div>;
}

function AuthForm({ title, error, onSubmit, footer, children }: { title: string; error?: string; onSubmit: (event: FormEvent) => Promise<void>; footer: JSX.Element; children: JSX.Element[] }): JSX.Element {
  return <main className="auth-page"><form className="card auth-card" onSubmit={(event) => void onSubmit(event)} noValidate>
    <h1>{title}</h1>{error && <p className="form-error" role="alert" aria-live="assertive">{error}</p>}{children}<button className="button primary" type="submit">Continuar</button><p>{footer}</p>
  </form></main>;
}
