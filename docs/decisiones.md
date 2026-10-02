# Decisiones aceptadas

- Upload verifies metadata ownership, published/status eligibility, and that no
  other active upload exists before allocation.
- Quota reservations are persistent records with expiry timestamps and a cleanup
  worker; reservations are released on failure/expiry.
- Deleting a published video releases its reserved quota.
- Usernames are immutable after account creation.
- The frontend fetches protected thumbnails with `Authorization` and uses Blob
  URLs; expired tokens trigger renewal before retrying.
- MongoDB indexes cover `title`, `tags`, and `topic`, use case-insensitive
  collation, and every index query is validated with `explain`.
- User endpoints return only privacy-scoped data and never expose credentials,
  private metadata, or internal identifiers unnecessarily.
