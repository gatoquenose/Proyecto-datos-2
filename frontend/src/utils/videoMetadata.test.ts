import { describe, expect, it } from "vitest";
import { validateVideoMetadata, VIDEO_METADATA_LIMITS } from "./videoMetadata";

const validMetadata = {
  title: "Video",
  description: "",
  author: "",
  topic: "",
  tags: [],
};

describe("validateVideoMetadata", () => {
  it("expone los límites del contrato de metadata", () => {
    expect(VIDEO_METADATA_LIMITS).toEqual({
      title: { minLength: 1, maxLength: 200 },
      description: { maxLength: 5000 },
      author: { maxLength: 200 },
      topic: { maxLength: 100 },
      tags: { maxItems: 30, maxLength: 50 },
    });
  });

  it("acepta metadata dentro de los límites", () => {
    expect(validateVideoMetadata(validMetadata)).toEqual({});
    expect(validateVideoMetadata({
      title: "T".repeat(200),
      description: "D".repeat(5000),
      author: "A".repeat(200),
      topic: "T".repeat(100),
      tags: Array.from({ length: 30 }, () => "x".repeat(50)),
    })).toEqual({});
  });

  it("valida límites y requisitos campo por campo", () => {
    expect(validateVideoMetadata({
      title: " ",
      description: "D".repeat(5001),
      author: "A".repeat(201),
      topic: "T".repeat(101),
      tags: [...Array.from({ length: 31 }, () => "etiqueta"), "x".repeat(51)],
    })).toEqual({
      title: "El título es obligatorio y debe tener entre 1 y 200 caracteres.",
      description: "La descripción no puede superar 5000 caracteres.",
      author: "El autor no puede superar 200 caracteres.",
      topic: "El tema no puede superar 100 caracteres.",
      tags: "Las etiquetas deben ser hasta 30 y tener máximo 50 caracteres.",
    });
  });
});
