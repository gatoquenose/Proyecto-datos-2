import { describe, expect, it } from "vitest";
import { sha256Hex } from "./checksum";

describe("sha256Hex", () => {
  it("produce hexadecimal minúsculo", async () => {
    await expect(sha256Hex(new TextEncoder().encode("abc"))).resolves.toBe("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  });
});
