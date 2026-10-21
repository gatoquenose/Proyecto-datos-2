import { describe, expect, it } from "vitest";
import { authStore } from "../auth/authStore";
import { mockUser } from "./data";

describe("DASH mock manifest", () => {
  it("serves BaseURL before the first AdaptationSet", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const response = await fetch("http://localhost:8083/api/v1/videos/mock-video/manifest.mpd", {
      headers: { Authorization: "Bearer mock-jwt" },
    });
    const manifest = new DOMParser().parseFromString(await response.text(), "application/xml");
    const parserError = manifest.querySelector("parsererror");
    const baseUrl = manifest.getElementsByTagNameNS("urn:mpeg:dash:schema:mpd:2011", "BaseURL")[0];
    const firstAdaptationSet = manifest.getElementsByTagNameNS("urn:mpeg:dash:schema:mpd:2011", "AdaptationSet")[0];

    expect(response.ok).toBe(true);
    expect(parserError).toBeNull();
    expect(baseUrl).toBeDefined();
    expect(firstAdaptationSet).toBeDefined();
    expect(baseUrl?.textContent).toBe("dash/");
    expect(baseUrl?.compareDocumentPosition(firstAdaptationSet) & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
  });
});
