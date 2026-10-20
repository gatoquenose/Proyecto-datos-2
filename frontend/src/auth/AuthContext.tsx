import { createContext, useContext, useEffect, useMemo, useState, useSyncExternalStore, type ReactNode } from "react";
import { useNavigate } from "react-router-dom";
import { setUnauthorizedHandler } from "../api/httpClient";
import { usersClient } from "../api/usersClient";
import { authStore } from "./authStore";

interface AuthContextValue {
  token: string | null;
  user: ReturnType<typeof authStore.getUser>;
  ready: boolean;
  login: (token: string, user: NonNullable<ReturnType<typeof authStore.getUser>>) => void;
  logout: () => void;
}

const AuthContext = createContext<AuthContextValue | null>(null);

export function AuthProvider({ children }: { children: ReactNode }): JSX.Element {
  const snapshot = useSyncExternalStore(authStore.subscribe, () => `${authStore.getToken()}:${authStore.getUser()?.id ?? ""}`);
  const navigate = useNavigate();
  const [ready, setReady] = useState(!authStore.getToken());
  useEffect(() => {
    const restore = async (): Promise<void> => {
      if (!authStore.getToken()) {
        setReady(true);
        return;
      }
      try {
        authStore.setUser(await usersClient.getCurrentUser());
      } catch {
        authStore.clear();
        navigate("/login", { replace: true, state: { message: "Tu sesión ya no es válida." } });
      } finally {
        setReady(true);
      }
    };
    void restore();
  }, []);
  useEffect(() => {
    setUnauthorizedHandler((path) => {
      authStore.clear();
      navigate(`/login?returnTo=${encodeURIComponent(path)}`, { replace: true, state: { message: "Tu sesión expiró." } });
    });
    return () => setUnauthorizedHandler(null);
  }, [navigate]);
  const value = useMemo(() => ({
    token: authStore.getToken(),
    user: authStore.getUser(),
    ready,
    login: (token: string, user: NonNullable<ReturnType<typeof authStore.getUser>>) => authStore.setSession(token, user),
    logout: () => { authStore.clear(); navigate("/login"); },
  }), [ready, snapshot, navigate]);
  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export const useAuth = (): AuthContextValue => {
  const value = useContext(AuthContext);
  if (!value) throw new Error("useAuth debe usarse dentro de AuthProvider");
  return value;
};
