FROM debian:bookworm-slim AS deps

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

ENV TECFLIX_CMAKE_FLAGS="-G Ninja -DCMAKE_BUILD_TYPE=Release -DVCPKG_TARGET_TRIPLET=x64-linux -DVCPKG_INSTALLED_DIR=/opt/vcpkg_installed -DVCPKG_MANIFEST_INSTALL=OFF"

FROM deps AS build-users-api
COPY CMakeLists.txt ./
COPY cmake/ ./cmake/
COPY services/common/ ./services/common/
COPY services/users-api/ ./services/users-api/
RUN cmake -S . -B build ${TECFLIX_CMAKE_FLAGS} \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM deps AS build-metadata-api
COPY CMakeLists.txt ./
COPY cmake/ ./cmake/
COPY services/common/ ./services/common/
COPY services/metadata-api/ ./services/metadata-api/
RUN cmake -S . -B build ${TECFLIX_CMAKE_FLAGS} \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM deps AS build-streaming-api
COPY CMakeLists.txt ./
COPY cmake/ ./cmake/
COPY services/common/ ./services/common/
COPY services/streaming-api/ ./services/streaming-api/
RUN cmake -S . -B build ${TECFLIX_CMAKE_FLAGS} \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM deps AS builder
COPY . .
RUN cmake -S . -B build ${TECFLIX_CMAKE_FLAGS} \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM debian:bookworm-slim AS runtime
RUN useradd --system --uid 10001 --no-create-home tecflix
USER tecflix
EXPOSE 8080

FROM runtime AS users-api
COPY --from=build-users-api /src/build/services/users-api/users-api /usr/local/bin/users-api
ENTRYPOINT ["/usr/local/bin/users-api"]

FROM runtime AS metadata-api
COPY --from=build-metadata-api /src/build/services/metadata-api/metadata-api /usr/local/bin/metadata-api
ENTRYPOINT ["/usr/local/bin/metadata-api"]

FROM runtime AS streaming-api
COPY --from=build-streaming-api /src/build/services/streaming-api/streaming-api /usr/local/bin/streaming-api
ENTRYPOINT ["/usr/local/bin/streaming-api"]
