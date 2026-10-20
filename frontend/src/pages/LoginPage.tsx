import { FormEvent, useState } from "react";
import { Link, useLocation, useNavigate, useSearchParams } from "react-router-dom";
import { usersClient } from "../api/usersClient";
import { ApiError, friendlyErrorMessage } from "../types/api";
import { useAuth } from "../auth/AuthContext";

export function LoginPage(): JSX.Element {
  const { login } = useAuth();
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const location = useLocation();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const submit = async (event: FormEvent): Promise<void> => {
    event.preventDefault();
    try {
      const response = await usersClient.login({ username, password });
      login(response.accessToken, response.user);
      navigate(params.get("returnTo") ?? "/videos", { replace: true });
    } catch (caught) {
      setError(caught instanceof ApiError ? friendlyErrorMessage(caught.body.code) : "No se pudo iniciar sesión.");
    }
  };
  return <AuthForm title="Iniciar sesión" error={error} onSubmit={submit} footer={<Link to={`/register${location.search}`}>Crear cuenta</Link>}>
    <label>Usuario<input value={username} onChange={(e) => setUsername(e.target.value)} required /></label>
    <label>Contraseña<input type="password" value={password} onChange={(e) => setPassword(e.target.value)} required /></label>
  </AuthForm>;
}

export function RegisterPage(): JSX.Element {
  const navigate = useNavigate();
  const [form, setForm] = useState({ username: "", email: "", password: "", confirm: "" });
  const [error, setError] = useState("");
  const submit = async (event: FormEvent): Promise<void> => {
    event.preventDefault();
    if (form.password !== form.confirm) { setError("Las contraseñas no coinciden."); return; }
    try {
      await usersClient.createUser({ username: form.username, email: form.email, password: form.password });
      navigate("/login");
    } catch (caught) {
      setError(caught instanceof ApiError ? friendlyErrorMessage(caught.body.code) : "No se pudo registrar la cuenta.");
    }
  };
  return <AuthForm title="Crear cuenta" error={error} onSubmit={submit} footer={<Link to="/login">Ya tengo cuenta</Link>}>
    <label>Usuario<input value={form.username} onChange={(e) => setForm({ ...form, username: e.target.value })} minLength={3} required /></label>
    <label>Correo<input type="email" value={form.email} onChange={(e) => setForm({ ...form, email: e.target.value })} required /></label>
    <label>Contraseña<input type="password" value={form.password} onChange={(e) => setForm({ ...form, password: e.target.value })} minLength={8} required /></label>
    <label>Confirmar contraseña<input type="password" value={form.confirm} onChange={(e) => setForm({ ...form, confirm: e.target.value })} required /></label>
  </AuthForm>;
}

function AuthForm({ title, error, onSubmit, footer, children }: { title: string; error: string; onSubmit: (event: FormEvent) => Promise<void>; footer: JSX.Element; children: JSX.Element[] }): JSX.Element {
  return <main className="auth-page"><form className="card auth-card" onSubmit={(event) => void onSubmit(event)}>
    <h1>{title}</h1>{error && <p className="form-error" role="alert">{error}</p>}{children}<button className="button primary" type="submit">Continuar</button><p>{footer}</p>
  </form></main>;
}
