import type { components, paths } from "../types/generated/users";
import { request } from "./httpClient";

type CreateUserRequest = components["schemas"]["CreateUserRequest"];
type LoginRequest = components["schemas"]["LoginRequest"];
type UpdateUserRequest = components["schemas"]["UpdateUserRequest"];
type LoginResponse = components["schemas"]["LoginResponse"];
type Me = components["schemas"]["Me"];
type Quota = components["schemas"]["Quota"];

const baseUrl = import.meta.env.VITE_USERS_API_URL ?? "http://localhost:8081/api/v1";

export const usersClient = {
  createUser: (body: CreateUserRequest) =>
    request<paths["/users"]["post"]["responses"]["201"]["content"]["application/json"]>(`${baseUrl}/users`, { method: "POST", body: JSON.stringify(body), handleUnauthorized: false }),
  login: (body: LoginRequest) =>
    request<LoginResponse>(`${baseUrl}/auth/login`, { method: "POST", body: JSON.stringify(body), handleUnauthorized: false }),
  getCurrentUser: () => request<Me>(`${baseUrl}/users/me`),
  getMyQuota: () => request<Quota>(`${baseUrl}/users/me/quota`),
  updateUser: (id: string, body: UpdateUserRequest) =>
    request<Me>(`${baseUrl}/users/${id}`, { method: "PATCH", body: JSON.stringify(body) }),
  deleteUser: (id: string) => request<void>(`${baseUrl}/users/${id}`, { method: "DELETE" }),
};
