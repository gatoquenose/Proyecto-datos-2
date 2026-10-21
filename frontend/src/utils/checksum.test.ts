import { createHash } from "node:crypto";
import { describe, expect, it } from "vitest";
import { hashChunk, hashFileIncrementally, sha256Hex } from "./checksum";

describe("sha256Hex", () => {
  it("coincide con el vector SHA-256 de abc", async () => {
    await expect(sha256Hex(new TextEncoder().encode("abc"))).resolves.toBe("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  });

  it("coincide con el vector SHA-256 de la cadena vacía", async () => {
    await expect(sha256Hex(new Uint8Array())).resolves.toBe("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  });

  it("usa un hash incremental por fragmentos y conserva el último fragmento menor", async () => {
    const chunkSize = 8;
    const bytes = new Uint8Array(20);
    bytes.forEach((_, index) => { bytes[index] = index; });
    const file = new Blob([bytes]);
    const result = await hashFileIncrementally(file, chunkSize);
    const expectedFile = createHash("sha256").update(Buffer.from(bytes)).digest("hex");
    const expectedChunks = [bytes.slice(0, 8), bytes.slice(8, 16), bytes.slice(16)];

    expect(result.fileChecksum).toBe(expectedFile);
    expect(result.chunkChecksums).toEqual(expectedChunks.map((chunk) =>
      createHash("sha256").update(Buffer.from(chunk)).digest("hex")));
    expect(file.slice(16).size).toBe(4);
  });

  it("calcula el checksum independiente de un fragmento contra node:crypto", async () => {
    const source = new TextEncoder().encode("fragmento independiente");
    const chunk = new Blob([source]);
    const bytes = Buffer.from(source);
    await expect(hashChunk(chunk)).resolves.toBe(createHash("sha256").update(bytes).digest("hex"));
  });

  it("rechaza archivos vacíos para la subida", async () => {
    await expect(hashFileIncrementally(new Blob())).rejects.toThrow("no puede estar vacío");
  });
});
