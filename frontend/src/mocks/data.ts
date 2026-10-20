import type { components } from "../types/generated/metadata";
import type { components as UserComponents } from "../types/generated/users";

export const mockUser: UserComponents["schemas"]["Me"] = {
  id: "550e8400-e29b-41d4-a716-446655440000",
  username: "ana.tec",
  email: "ana@example.test",
  quotaBytes: 5368709120,
  usedBytes: 104857600,
};

export const mockPassword = "CorrectHorseBatteryStaple!";

export const mockVideos: components["schemas"]["Video"][] = Array.from({ length: 30 }, (_, index) => ({
  id: `550e8400-e29b-41d4-a716-44665544${String(index).padStart(4, "0")}`,
  ownerId: index % 3 === 0 ? mockUser.id : `650e8400-e29b-41d4-a716-44665544${String(index).padStart(4, "0")}`,
  ownerUsername: index % 3 === 0 ? mockUser.username : `autor${(index % 7) + 1}`,
  title: `Clase de estructuras ${String(index + 1).padStart(2, "0")}`,
  description: "Video de ejemplo para desarrollo.",
  author: `Autor ${(index % 7) + 1}`,
  topic: index % 2 === 0 ? "Algoritmos" : "Programación",
  tags: ["tecflix", index % 2 === 0 ? "árboles" : "grafos"],
  durationSeconds: 120 + index * 13,
  uploadedAt: new Date(Date.UTC(2026, 9, 19 - index)).toISOString(),
  visibility: index % 4 === 0 ? "private" : "public",
  sizeBytes: 10000000 + index * 1000,
  status: index % 5 === 0 ? "processing" : index % 7 === 0 ? "failed" : "ready",
  createdAt: new Date(Date.UTC(2026, 9, 18 - index)).toISOString(),
  updatedAt: new Date(Date.UTC(2026, 9, 19 - index)).toISOString(),
}));
