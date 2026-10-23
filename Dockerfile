# The SYN-ACK WIRE server as a container: what `fly deploy` builds (fly.toml), or any container host.
# Python's standard library is all it needs. The servers it runs are in server/servers.fly.toml.
FROM python:3.13-slim
ENV PYTHONUNBUFFERED=1 PYTHONDONTWRITEBYTECODE=1
RUN useradd --uid 10001 --no-create-home --shell /usr/sbin/nologin synack
WORKDIR /app
COPY theme/theme.toml theme/
COPY server/ server/
WORKDIR /app/server
USER synack
EXPOSE 6502 6503 6504 6505
CMD ["python3", "serve.py", "--config", "servers.fly.toml"]
