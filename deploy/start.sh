#!/bin/sh
# The container's two programs (Dockerfile). tnfsd serves the client disks, read-only, for the FujiNet lobby to boot
# (TNFS on UDP 16384), listening on fly-global-services, the one address fly.io delivers UDP to; set TNFS_BIND=0.0.0.0
# to run the image anywhere else. The game servers stay in the foreground and get the SIGTERM; tnfsd ends with them.
tnfsd -r -b "${TNFS_BIND:-fly-global-services}" /srv/tnfs &
exec python3 serve.py --config servers.fly.toml
