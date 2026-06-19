# CI scripts

## Windows code signing

Strata's Windows release artifacts (`*-win64.exe` installer and the PE files inside
`*-win64.zip`) are Authenticode-signed with a code-signing certificate during release builds.
Without a valid signature, Microsoft Defender SmartScreen blocks the installer as an
"unrecognized app".

### Required GitHub secrets

Configure these under **Settings → Secrets and variables → Actions → Secrets**:

| Secret | Value |
| --- | --- |
| `WINDOWS_CERT_PFX_BASE64` | Base64 encoding of your `.pfx` / `.p12` certificate file |
| `WINDOWS_CERT_PASSWORD` | The PFX export password |

Produce the base64 blob from the certificate file:

```bash
# Linux / macOS
base64 -w0 cert.pfx          # (macOS: `base64 -i cert.pfx | tr -d '\n'`)
```

```powershell
# Windows PowerShell
[Convert]::ToBase64String([IO.File]::ReadAllBytes("cert.pfx"))
```

### How it works

- The release workflows ([`windows-qt6.yml`](../../.github/workflows/windows-qt6.yml) on
  `strata-v*` tags, and the manual [`windows-release-manual.yml`](../../.github/workflows/windows-release-manual.yml))
  enable signing only when both secrets are present (`WINDOWS_CODESIGN_CONFIGURED`). A Strata
  release with the secrets missing **fails** instead of shipping unsigned.
- [`prepare-windows-signing.ps1`](prepare-windows-signing.ps1) resolves `signtool.exe` (Windows
  SDK Build Tools) and materializes the PFX from `WINDOWS_CERT_PFX_BASE64` to a temp file,
  exporting `STRATA_SIGNTOOL_PATH` and `STRATA_WINDOWS_CERT_PATH`. The password is passed
  separately as `STRATA_WINDOWS_CERT_PASSWORD` (never written to `$GITHUB_ENV`).
- The build is configured with `-DSTRATA_WINDOWS_CODE_SIGN=ON`; CPack pre/post-build hooks call
  [`sign-windows-artifacts.ps1`](sign-windows-artifacts.ps1), which signs the staging tree and
  the final installer/ZIP with `signtool sign /f <pfx> /p <password> /fd SHA256 /tr <timestamp> /td SHA256`.

### Re-signing an already-built artifact (no rebuild)

To unblock an existing release without a multi-hour rebuild, sign and verify the published
`.exe` locally on a Windows box with the Windows SDK:

```powershell
signtool sign /v /fd SHA256 /f cert.pfx /p <password> `
  /tr http://timestamp.digicert.com /td SHA256 /d "Strata" "Strata-4.1.0-win64.exe"
signtool verify /pa /all "Strata-4.1.0-win64.exe"
```

then replace the GitHub release asset.

> ⚠️ Signing removes the "unknown publisher" block and shows the verified publisher. With a
> standard **OV** certificate, SmartScreen reputation still warms up over downloads/time before
> the prompt disappears entirely; only **EV** certificates get instant SmartScreen trust.
