# syntax=docker/dockerfile:1

ARG ALPINE_VERSION=3.22

FROM alpine:${ALPINE_VERSION} AS builder

RUN apk add --no-cache \
        build-base \
        cmake \
        git \
        linux-headers \
        ninja \
        sdl2-dev \
        zlib-dev \
        zlib-static

WORKDIR /src
COPY . .

# f2_server does not use SDL at runtime. Building it as a static musl executable
# lets the final image contain the server and nothing else.
RUN cmake -S . -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DCMAKE_C_FLAGS=-g \
        -DCMAKE_CXX_FLAGS=-g \
        -DCMAKE_EXE_LINKER_FLAGS=-static \
        -DFALLOUT_VENDORED=OFF \
        -DZLIB_LIBRARY=/usr/lib/libz.a \
    && cmake --build /build --target f2_server --parallel \
    && cp /build/f2_server /build/f2_server-linux-debug \
    && strip /build/f2_server

# The unstripped twin of the shipped binary, published with each release so a
# "f2_server: CRASH" line from the container can be turned into a call chain:
#   addr2line -f -C -e f2_server-linux-debug <ip - base> ...
FROM scratch AS symbols
COPY --from=builder /build/f2_server-linux-debug /

FROM scratch AS fo2-dedicated-server

LABEL org.opencontainers.image.title="Fallout 2 SuperCoop server" \
      org.opencontainers.image.description="Headless Fallout 2 co-op dedicated server with Restoration Project support. Mount your installed SuperCoop folder at /game." \
      org.opencontainers.image.source="https://github.com/bclark9153/fallout2-supercoop" \
      org.opencontainers.image.licenses="LicenseRef-Sustainable-Use"

# Defaults for a plain `docker run` / an Unraid template: game port 9300 (what
# the Windows join launcher assumes), real-time pace, autosave every 5 minutes,
# keep running with nobody connected, and on (re)start continue the newest save
# or begin a new game when there is none. Override any of them with -e.
ENV F2_SERVER_NET=9300 \
    F2_SERVER_KEEPALIVE=1 \
    F2_SERVER_PACE_MS=100 \
    F2_AUTOSAVE_SECS=300 \
    F2_SERVER_LOAD=newest

COPY --from=builder --chmod=0555 /build/f2_server /f2_server

WORKDIR /game
USER 1000:1000

EXPOSE 9300/tcp 9301/tcp

ENTRYPOINT ["/f2_server"]
