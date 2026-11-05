# Windows CUDA / wrapper OpenGL causal control

Diagnostic only, not a release or an established fix. The workflow builds this helper without executing it. The parent task owns all native experiments and verifies the artifact before execution in an isolated copy of the verified app root.

The production wrapper comes from `de4488ad1806d72462c58ebf5f56fcdc8bea5174`, original Git blob `14295b27c38870e6f4ed41b4e7433d2784845424`. The previous warmup helper is preserved as the baseline from `193610fc5acc0c99cba575e5ff143cbb0b631e64`, Git blob `f0b503ee32eb7e2979b1aecf76343a3c9ee629fe`. Existing production and prior diagnostic files remain unchanged.

Relative to that baseline, the only source change surrounds the original wrapper OpenGL-version probe with an optional branch. Exact `SLIC3R_DIAG_SKIP_OPENGL=1` bypasses that probe and leaves `load_mesa=false`, so this wrapper does not require or load its software Mesa fallback in that mode, including when `--sw-renderer` was supplied. With any other value, the original probe/fallback expression runs. The selected mode is traced and flushed to stderr. This controls the wrapper probe only; it makes no claim about later OpenGL use inside the app DLL.

The existing warmup block is unchanged. Exact `SLIC3R_DIAG_CUDA_WARMUP=1` dynamically loads absolute app-root `cudart64_12.dll` after resolving `orcaslicer_main`, calls `cudaSetDevice(0)` on the entry thread, traces and flushes the result, then calls the original entry point. Other values add no CUDA API calls. Argument processing, Python DLL-directory setup, app DLL search and original entry are preserved.

Use the same binary for all four cells, with affinity established before startup, separate fresh profiles/contexts, the same app copy and arguments, and no simultaneous native GPU experiments:

| Wrapper GL probe | CUDA warmup | SLIC3R_DIAG_SKIP_OPENGL | SLIC3R_DIAG_CUDA_WARMUP |
| --- | --- | --- | --- |
| Original | No | 0 | 0 |
| Original | Yes | 0 | 1 |
| Bypassed | No | 1 | 0 |
| Bypassed | Yes | 1 | 1 |

Microsoft documents that `wglDeleteContext` first makes a context non-current when it is current on the calling thread; deleting another thread's current context is an error. Thus a missing explicit unbind is not established as a defect. This diagnostic changes no WGL deletion behavior. Primary source: https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wgldeletecontext . The related `wglMakeCurrent` documentation is https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglmakecurrent .

Build: Windows2022, MSVC19.44/v143 x64 Release, CMake3.31.6, original GUI/no-console wrapper defines, Windows SDK OpenGL/shell/user/GDI libraries and verified Boost1.84 headers. No CUDA SDK or app rebuild is involved. The package includes EXE/PDB, both pristine baselines, new source, a probe-only patch, full original-to-helper patch, CMake/workflow/compiler evidence, source URLs, provenance, AGPL and Boost licenses, and checksums. No driver, registry, security or machine settings are changed.
