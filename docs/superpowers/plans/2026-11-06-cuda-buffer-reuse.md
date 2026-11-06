# CUDA dispatch buffer reuse

Execute inline under the existing approved slicer debugging/refactoring plan. This is separate from the scheduled application-startup work.

## Evidence and scope

Canonical QIDI29a uses the same CudaSlicer.cu blob as shared core1d7b313. Its dispatch allocates two device buffers and frees both for every batch. The frozen default-affinity experiment completed36 slices:12CPU controls,6strict GPU matches and18different outputs. Three CPU groups also have three distinct repeated CPU toolpaths, so those groups do not support a speedup claim or establish a GPU-only cause. The matching Classic/Grid group's median process wall times are CPU2996.91ms,CUDA3162.62ms,Vulkan3299.73ms. Its native infill-stage medians are316ms,416ms,609ms respectively; these source-defined milliseconds overlap other stage fields and must not be summed.

Repeated allocation is an observed code path and a performance hypothesis, not an established share of the measured overhead. Vulkan already retains mapped buffers. Do not change Vulkan, kernels, numeric domains, every-record validation, topology, eligibility thresholds, CPU threading, initialization flags or warmup behavior in this change.

## Design

Extract the CUDA allocation boundary into a small host-testable production helper. Characterize existing successful and failed allocation/cleanup first. Add an actual allocation-count RED test spanning two completed dispatches, then retain bounded capacity between completed batches. Reuse an allocation when it covers the requested bytes; grow geometrically within the existing256K-record limit. Request and result storage remain backend-owned, protected by the existing CUDA mutex and associated with device0. Do not add a worker thread, queue, public setting, device-wide reset or a new kernel.

No output is returned until both buffers exist, all copies/kernel/synchronization/readback succeeded and the unchanged outer exact gate validates every result. Failed growth/copy/dispatch follows the existing CPU fallback/quarantine path. Cleanup must attempt every owned allocation once, clear ownership before calling a potentially failing free, never double-free and permanently prevent further CUDA allocations in that process after ambiguous cleanup/device-selection failure. This limits uncertain retained resources. A static destructor must not call CUDA; use the existing explicit resource-release boundary while the runtime is usable. Never reset another CUDA user's context.

At256K records, current request/result layouts use12MiB+8MiB. Growth is bounded by those limits; document transient old/new ownership and measured capacities. Typical small workloads may retain only the qualification batch's capacity. Any release error concerns resource cleanup; already accepted data was fully validated independently. Preserve the distinction in diagnostics.

## Tasks

1. Verify clean isolated baseline and existing CPU contract tests. Add a production allocation-boundary extraction and characterization tests for first allocation, second-allocation failure, operation failure, cleanup failure and release ownership. Add the cache-count test, observe the specific RED, then implement retention/growth. Cover smaller/larger/max/invalid requests, stale storage, partial failure, repeated release and sticky cleanup failure. No wall-time assertions and no fake-GPU performance claims.
2. Integrate the helper into actual CudaSlicer.cu and the existing explicit release path. Keep CUDA qualification and the kernel unchanged. Add tests to standalone and normal libslic3r CMake lists. Run existing Windows/Linux contracts, actual NVCC compilation and one independent scope review with one Important-fix pass.
3. Build the shared source-bound CUDA12.9/sm61 package; verify downloaded source/runtime/shader/license/checksum structure and execute real hardware qualification, fallback and bounded repeated-dispatch controls serially. Retain cold first-use and subsequent calls separately; first-use savings, allocation-count savings and whole-slicer speed are distinct claims.
4. If accepted, port only the reviewed exact shared delta to isolated Bambu/QIDI/Orca product branches, preserving independent CPU/SMB fixes and pure/upstream versus fork-CI separation. New canonical app builds and source-bound native matrices/performance/GUI checks are required. Keep all earlier results and do not relabel them to the optimized source. Re-run the same frozen performance selection on the new source; baseline failures and slower results stay visible.

## Execution constraints

Original255inputs, primary/supplemental matrices, original controllers/capture host/comparators and all historical attempts remain byte-preserved. Current native launches are paused for the separately reviewed process-identity coordinator fix. No process/service termination, security change or stale-PID exception is authorized by this plan. Continue the existing one-time12:05KST startup reservation independently when its time arrives.
