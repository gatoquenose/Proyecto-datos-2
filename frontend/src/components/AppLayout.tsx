import { Link, NavLink, Outlet } from "react-router-dom";
import { useAuth } from "../auth/AuthContext";

export function AppLayout(): JSX.Element {
  const { user, logout } = useAuth();
  return (
    <div className="app-shell">
      <header className="topbar">
        <Link className="brand" to="/videos">Tecflix</Link>
        <nav aria-label="Navegación principal">
          <NavLink to="/videos">Videos</NavLink>
          <NavLink to="/videos/mine">Mis videos</NavLink>
          <NavLink className="button primary" to="/videos/new">Subir video</NavLink>
        </nav>
        <div className="user-menu">
          <Link to="/profile">{user?.username ?? "Mi perfil"}</Link>
          <button type="button" onClick={logout}>Cerrar sesión</button>
        </div>
      </header>
      <main className="content"><Outlet /></main>
    </div>
  );
}
