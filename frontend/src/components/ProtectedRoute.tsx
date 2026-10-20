import { Navigate, Outlet, useLocation } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";

export function ProtectedRoute(): JSX.Element {
  const { token, ready } = useAuth();
  const location = useLocation();
  if (!ready) return <p className="status">Cargando sesión…</p>;
  if (!token) return <Navigate to={`/login?returnTo=${encodeURIComponent(location.pathname)}`} replace />;
  return <Outlet />;
}
