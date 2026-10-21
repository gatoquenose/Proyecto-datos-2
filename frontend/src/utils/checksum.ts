import { sha256 } from "@noble/hashes/sha2.js";

export const DEFAULT_CHUNK_SIZE_BYTES = 8 * 1024 * 1024;

const toHex = (bytes: Uint8Array): string =>
  Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");

const readBlob = async (blob: Blob): Promise<Uint8Array> => {
  if (typeof blob.arrayBuffer === "function") {
    return new Uint8Array(await blob.arrayBuffer());
  }
  return new Promise<Uint8Array>((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => {
      if (!(reader.result instanceof ArrayBuffer)) {
        reject(new Error("No se pudo leer el fragmento."));
        return;
      }
      resolve(new Uint8Array(reader.result));
    };
    reader.onerror = () => reject(reader.error ?? new Error("No se pudo leer el fragmento."));
    reader.readAsArrayBuffer(blob);
  });
};

export const sha256Hex = async (input: Blob | ArrayBuffer | Uint8Array): Promise<string> => {
  const bytes = input instanceof Blob
    ? await readBlob(input)
    : input instanceof ArrayBuffer
      ? new Uint8Array(input)
      : input;
  return toHex(sha256(bytes));
};

export const hashChunk = async (chunk: Blob): Promise<string> =>
  sha256Hex(chunk);

export interface IncrementalFileHash {
  fileChecksum: string;
  chunkChecksums: string[];
}

export const hashFileIncrementally = async (
  file: Blob,
  chunkSize = DEFAULT_CHUNK_SIZE_BYTES,
): Promise<IncrementalFileHash> => {
  if (file.size === 0) throw new Error("El archivo no puede estar vacío.");
  if (!Number.isInteger(chunkSize) || chunkSize < 1) {
    throw new Error("El tamaño del fragmento debe ser positivo.");
  }

  const fileHasher = sha256.create();
  const chunkChecksums: string[] = [];
  for (let start = 0; start < file.size; start += chunkSize) {
    const chunk = file.slice(start, Math.min(start + chunkSize, file.size));
    const bytes = await readBlob(chunk);
    fileHasher.update(bytes);
    chunkChecksums.push(toHex(sha256(bytes)));
  }
  return { fileChecksum: toHex(fileHasher.digest()), chunkChecksums };
};
