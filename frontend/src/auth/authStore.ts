import type { components } from "../types/generated/users";
import { setHttpToken } from "../api/httpClient";

type Me = components["schemas"]["Me"];
const TOKEN_KEY = "tecflix.jwt";

let currentToken = sessionStorage.getItem(TOKEN_KEY);
let currentUser: Me | null = null;
const listeners = new Set<() => void>();

const notify = (): void => listeners.forEach((listener) => listener());

export const authStore = {
  getToken: (): string | null => currentToken,
  getUser: (): Me | null => currentUser,
  subscribe: (listener: () => void): (() => void) => {
    listeners.add(listener);
    return () => listeners.delete(listener);
  },
  setSession: (token: string, user: Me): void => {
    currentToken = token;
    currentUser = user;
    sessionStorage.setItem(TOKEN_KEY, token);
    setHttpToken(token);
    notify();
  },
  setUser: (user: Me): void => {
    currentUser = user;
    notify();
  },
  clear: (): void => {
    currentToken = null;
    currentUser = null;
    sessionStorage.removeItem(TOKEN_KEY);
    setHttpToken(null);
    notify();
  },
};

setHttpToken(currentToken);
