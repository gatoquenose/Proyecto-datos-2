# Decisiones aceptadas

- Upload verifies metadata ownership, published/status eligibility, and that no
  other active upload exists before allocation.
- Quota reservations are persistent records with expiry timestamps and a cleanup
  worker; reservations are released on failure/expiry.
- Deleting a published video releases its reserved quota.
- Usernames are immutable after account creation.
- The frontend fetches protected thumbnails with `Authorization` and uses Blob
  URLs. There is no refresh endpoint: expired tokens or any 401 clear the
  session and return to login while preserving the intended route.
- MongoDB indexes cover `title`, `tags`, and `topic`, use case-insensitive
  collation, and every index query is validated with `explain`.
- User endpoints return only privacy-scoped data and never expose credentials,
  private metadata, or internal identifiers unnecessarily.

## Frontend F1

- `openapi-typescript` is a dev dependency and `gen:api` generates TypeScript
  types directly from the three versioned OpenAPI contracts. This prevents
  hand-written client types from drifting from the source of truth.
- The JWT is kept in memory and mirrored in `sessionStorage`. This survives a
  tab reload but is cleared when the tab session ends; it remains exposed to
  JavaScript on the origin, so an XSS vulnerability could read it. There is no
  refresh endpoint; after reload the client validates it with `GET /users/me`.
- Docker Compose publishes Users, Metadata, and Streaming on host ports 8081,
  8082, and 8083 respectively. The frontend exposes matching `VITE_*_API_URL`
  variables, and real APIs must configure CORS for the frontend origin in
  phases C, D, and E.
- `react-router-dom`, TanStack Query, MSW, Shaka Player, `@noble/hashes`,
  Vitest, Testing Library, and jsdom support routing, server-state caching,
  contract mocks, DASH playback, SHA-256 fallback, and required tests.
