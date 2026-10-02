# Arquitectura

Phase A uses a monorepo with independently deployable C++20 HTTP services:
`users-api`, `metadata-api`, and `streaming-api`. Shared transport/bootstrap
code lives in `services/common`. The React frontend is a separate Vite app.
Persistence adapters, authentication, queues, and object storage are future
phases. `/health` reports process health and `/ready` reports readiness.

Existing `PáginaInicio.cpp` and `PáginaInicio.h` were discarded; no prototype
entry point is retained.
