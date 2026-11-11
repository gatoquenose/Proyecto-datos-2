import { describe, expect, it } from "vitest";
import { parsePositiveIntegerEnv } from "./parsePositiveIntegerEnv";

describe("parsePositiveIntegerEnv", () => {
  it.each([
    "",
    "texto",
    "0",
    "-1",
    "1.5",
    "Infinity",
    "NaN",
    "9007199254740992",
    "0x10",
    "1e3",
    "+5",
    "5.0",
  ])("usa el valor por defecto para %j", (value) => {
    expect(parsePositiveIntegerEnv(value, 42)).toBe(42);
  });

  it("acepta el valor válido mínimo", () => {
    expect(parsePositiveIntegerEnv("1", 42)).toBe(1);
  });

  it("recorta espacios y acepta el valor entero positivo", () => {
    expect(parsePositiveIntegerEnv(" 8 ", 42)).toBe(8);
  });

  it("usa el valor por defecto si la variable no está definida", () => {
    expect(parsePositiveIntegerEnv(undefined, 42)).toBe(42);
  });
});
