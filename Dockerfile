FROM debian:bookworm-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential cmake ninja-build git curl zip unzip tar pkg-config perl ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ARG VCPKG_COMMIT=0df93a05b9de080129e653ec4776d7e14a1b2e4f
ENV VCPKG_ROOT=/opt/vcpkg
ENV VCPKG_DISABLE_METRICS=1
RUN git init ${VCPKG_ROOT} \
    && git -C ${VCPKG_ROOT} remote add origin https://github.com/microsoft/vcpkg.git \
    && git -C ${VCPKG_ROOT} fetch --depth 1 origin ${VCPKG_COMMIT} \
    && git -C ${VCPKG_ROOT} checkout FETCH_HEAD \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /src
COPY vcpkg.json ./
RUN --mount=type=cache,target=/root/.cache/vcpkg \
    --mount=type=cache,target=/opt/vcpkg/downloads \
    VCPKG_MAX_CONCURRENCY=4 ${VCPKG_ROOT}/vcpkg install \
      --triplet x64-linux \
      --x-install-root=/opt/vcpkg_installed

COPY CMakeLists.txt ./
COPY services ./services
RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DVCPKG_TARGET_TRIPLET=x64-linux \
      -DVCPKG_INSTALLED_DIR=/opt/vcpkg_installed \
      -DVCPKG_MANIFEST_INSTALL=OFF \
    && cmake --build build

FROM debian:bookworm-slim AS runtime
RUN useradd --system --uid 10001 --no-create-home tecflix
USER tecflix
EXPOSE 8080

FROM runtime AS users-api
COPY --from=builder /src/build/users-api /usr/local/bin/users-api
ENTRYPOINT ["/usr/local/bin/users-api"]

FROM runtime AS metadata-api
COPY --from=builder /src/build/metadata-api /usr/local/bin/metadata-api
ENTRYPOINT ["/usr/local/bin/metadata-api"]

FROM runtime AS streaming-api
COPY --from=builder /src/build/streaming-api /usr/local/bin/streaming-api
ENTRYPOINT ["/usr/local/bin/streaming-api"]
