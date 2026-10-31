# The browser build is a static site: compile the C engine to wasm with
# Emscripten, serve web/dist with nginx. The build stage runs on the builder's
# own architecture, since its output (html/js/wasm) is platform-neutral and
# needs no emulation for the arm64 image.
FROM --platform=$BUILDPLATFORM emscripten/emsdk:6.0.6 AS build
# .git is not in the build context, so the version string comes in as an arg.
ARG CT_VERSION=unknown
WORKDIR /app
COPY Makefile ./
COPY src ./src
COPY web ./web
RUN make web CT_VERSION="$CT_VERSION"

FROM nginx:1-alpine
COPY web/nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/web/dist /usr/share/nginx/html
EXPOSE 80
HEALTHCHECK --interval=30s --timeout=5s --start-period=5s \
  CMD wget -q --spider http://127.0.0.1/ || exit 1
