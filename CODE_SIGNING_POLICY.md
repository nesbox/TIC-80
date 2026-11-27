# Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io), certificate
by [SignPath Foundation](https://signpath.org).

## Roles

- Authors: Vadim Grigoruk (@nesbox), the repository maintainer.
- Reviewers: Vadim Grigoruk (@nesbox).
- Approvers: Vadim Grigoruk (@nesbox). The signing policy in SignPath defines
  the approval flow for each request.

## Trusted build source

The signed Windows executable is built by GitHub Actions from this repository
at a release tag (`v*`), in `.github/workflows/build.yml`, job `windows`. The
workflow uploads the unsigned `tic80.exe` as an artifact and submits it to
SignPath; no binary built elsewhere is signed. The export stub the job uploads
separately stays unsigned, because appending a cart to a signed binary
invalidates the signature.

## Approval flow

A release signing request is handled by the signing policy configured in
SignPath. A rejected request, a timeout, or an exe without a signer fails the
release job; an unsigned executable is not published.

## Revocation

If the signing identity or a maintainer account is compromised, the
certificate is revoked through SignPath Foundation and the affected release is
withdrawn from GitHub Releases and itch.io.
