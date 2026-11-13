/* Shared between start.c and net_nio.c. */
#ifndef NL_H
#define NL_H
extern char nl_server_url[80];          /* "N:TCP://host:port/" from --server or SYNACK_URL, else empty: dial the built-in URL */
#define NL_DEFAULT_PTY "/tmp/fujinet-pty"   /* where client/README.md's fujinet.yaml puts the PTY */
#endif
