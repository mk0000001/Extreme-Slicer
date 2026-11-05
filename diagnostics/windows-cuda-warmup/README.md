# CUDA entry-thread diagnostic launcher

Diagnostic only. This helper is not a release or an established fix. It does not change the slicer DLL, GPU algorithms, profiles, drivers, or machine configuration.

The original wrapper is copied from `src/OrcaSlicer_app_msvc.cpp` at commit `de4488ad1806d72462c58ebf5f56fcdc8bea5174`, Git blob `14295b27c38870e6f4ed41b4e7433d2784845424`. The only source difference is one optional block immediately after successful `OrcaSlicer.dll` entry resolution and before `orcaslicer_main` is called. The original OpenGL probe, Python DLL directory setup, argument processing, DLL path, and app entry call remain present.

Both modes use the same helper binary. Only the exact environment value `SLIC3R_DIAG_CUDA_WARMUP=1` loads absolute app-root `cudart64_12.dll`, resolves `cudaSetDevice`, and calls `cudaSetDevice(0)` on the entry thread. Each stage and the returned CUDA status are written to stderr and flushed. The extra DLL reference remains loaded until process exit. An absent, zero, or different value adds no CUDA API calls. Failure to load or resolve is logged, then the original app entry still runs.

Before any execution, the task owner verifies the artifact and source checksums. The helper may then be copied only into an isolated copy of the verified app directory, beside its existing `OrcaSlicer.dll`, Python directory and CUDA runtime. Preserve the original packaged launcher. Run cold and warm modes with the same helper, the same process affinity, and separate fresh profiles/contexts. Only the parent task owns these native runs. No helper execution occurs in this build workflow.

The build uses Windows 2022, VS2022 v143/MSVC19.44 x64 Release, CMake3.31.6, the original GUI and no-console wrapper defines, Windows SDK OpenGL/shell/user/GDI libraries, and verified Boost1.84 headers. No CUDA SDK is required. The package provides the EXE/PDB, original and diagnostic wrapper sources, complete patch, CMake recipe, workflow, AGPL license, Boost license, CI commit/source URLs, compiler metadata and SHA256SUMS.

This bounded control tests whether entry-thread CUDA initialization changes the single-affinity hang. A successful warm run alone would not establish an application fix or a speedup.
