import { createContext, useContext, useEffect, useMemo, useState, useSyncExternalStore, type ReactNode } from "react";
import { useNavigate } from "react-router-dom";
import { setUnauthorizedHandler } from "../api/httpClient";
import { usersClient } from "../api/usersClient";
import { authStore } from "./authStore";

interface AuthContextValue {
  token: string | null;
  user: ReturnType<typeof authStore.getUser>;
  ready: boolean;
  sessionExpired: boolean;
  login: (token: string, user: NonNullable<ReturnType<typeof authStore.getUser>>) => void;
  clearSessionExpired: () => void;
  logout: () => void;
}

const AuthContext = createContext<AuthContextValue | null>(null);

export function AuthProvider({ children }: { children: ReactNode }): JSX.Element {
  const snapshot = useSyncExternalStore(authStore.subscribe, () => `${authStore.getToken()}:${authStore.getUser()?.id ?? ""}`);
  const navigate = useNavigate();
  const [ready, setReady] = useState(!authStore.getToken());
  const [sessionExpired, setSessionExpired] = useState(false);
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
  }, [navigate]);
  useEffect(() => {
    setUnauthorizedHandler((path) => {
      authStore.clear();
      setSessionExpired(true);
      navigate(`/login?returnTo=${encodeURIComponent(path)}`, { replace: true });
    });
    return () => setUnauthorizedHandler(null);
  }, [navigate]);
  const value = useMemo(() => {
    void snapshot;
    return {
      token: authStore.getToken(),
      user: authStore.getUser(),
      ready,
      sessionExpired,
      login: (token: string, user: NonNullable<ReturnType<typeof authStore.getUser>>) => {
        authStore.setSession(token, user);
        setSessionExpired(false);
      },
      clearSessionExpired: () => setSessionExpired(false),
      logout: () => { authStore.clear(); setSessionExpired(false); navigate("/login"); },
    };
  }, [ready, sessionExpired, snapshot, navigate]);
  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export const useAuth = (): AuthContextValue => {
  const value = useContext(AuthContext);
  if (!value) throw new Error("useAuth debe usarse dentro de AuthProvider");
  return value;
};
