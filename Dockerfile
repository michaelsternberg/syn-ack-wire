# The SYN-ACK WIRE server as a container: what `fly deploy` builds (fly.toml), or any container host. Two programs
# (deploy/start.sh): the game servers in server/servers.fly.toml, which need only Python's standard library, and
# tnfsd, which serves the client disk (client/build/synack.atr, as built when you deploy) to the FujiNet lobby.

# tnfsd from upstream at a fixed release, with deploy/tnfsd-bind-address.patch (fly.io's UDP needs it).
FROM python:3.13-slim AS tnfsd
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates gcc git libc6-dev make xa65 xxd
# v26.1015.1
ARG TNFSD_COMMIT=4aab9100ac8af4d7a3154a4b6d7e0a008d1fe865
WORKDIR /src
RUN git init -q && git fetch -q --depth 1 https://github.com/FujiNetWIFI/tnfsd.git $TNFSD_COMMIT \
    && git checkout -q FETCH_HEAD
COPY deploy/tnfsd-bind-address.patch /tmp/
RUN git apply /tmp/tnfsd-bind-address.patch && make -C src OS=LINUX all

FROM python:3.13-slim
ENV PYTHONUNBUFFERED=1 PYTHONDONTWRITEBYTECODE=1
RUN useradd --uid 10001 --no-create-home --shell /usr/sbin/nologin synack
COPY --from=tnfsd /src/bin/tnfsd /usr/local/bin/tnfsd
COPY client/build/synack.atr /srv/tnfs/
WORKDIR /app
COPY theme/theme.toml theme/
COPY server/ server/
COPY deploy/start.sh .
WORKDIR /app/server
USER synack
EXPOSE 6502 6503 6504 6505 8080 16384/udp
CMD ["/app/start.sh"]
