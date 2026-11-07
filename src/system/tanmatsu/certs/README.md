# Certificates

`tic80_root.pem` is **GTS Root R4**, the trust anchor for tic80.com:

    tic80.com  <-  GTS Root R4 / WE1  <-  GTS Root R4

Checked with `openssl s_client -connect tic80.com:443 -CAfile tic80_root.pem`,
which verifies with return code 0. The certificate is valid until June 2036.

It is pinned rather than relying on a full certificate bundle because this port
talks to exactly one host, and a bundle of two hundred authorities costs flash
for no benefit here.

A dev snapshot talks to dev.tic80.com instead (see the Network section of the
port's README). It serves the same `*.tic80.com` certificate, so this root
covers it too.

If tic80.com ever moves to a different authority this file has to be replaced,
and the symptom will be `mbedtls_ssl_handshake returned -0x3000` in the log.
