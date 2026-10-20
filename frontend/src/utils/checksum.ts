import { sha256 } from "@noble/hashes/sha2.js";

const toHex = (bytes: Uint8Array): string =>
  Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");

export const sha256Hex = async (input: Blob | ArrayBuffer | Uint8Array): Promise<string> => {
  const bytes = input instanceof Blob
    ? new Uint8Array(await input.arrayBuffer())
    : input instanceof ArrayBuffer
      ? new Uint8Array(input)
      : input;
  if (globalThis.crypto?.subtle) {
    const digest = await globalThis.crypto.subtle.digest("SHA-256", bytes.slice().buffer);
    return toHex(new Uint8Array(digest));
  }
  return toHex(sha256(bytes));
};
