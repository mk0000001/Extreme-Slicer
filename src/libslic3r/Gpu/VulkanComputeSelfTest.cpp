#include "VulkanSlicer.hpp"
#include "GpuBatchSafety.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "usage: slicer-gpu-selftest <shader-directory> <cpu|cuda|vulkan|auto>\n";
        return 64;
    }
    const std::string mode(argv[2]);
    if (mode != "cpu" && mode != "cuda" && mode != "vulkan" && mode != "auto")
        return 64;
    using namespace Slic3r::Gpu;
    VulkanSlicerBackend::set_shader_directory(argv[1]);
    VulkanSlicerBackend::set_compute_enabled(mode == "vulkan" || mode == "auto");
    VulkanSlicerBackend::set_cuda_enabled(mode == "cuda" || mode == "auto");
    VulkanSlicerBackend::set_backend_preference(mode == "vulkan" ? ComputeBackendPreference::Vulkan :
        mode == "cuda" ? ComputeBackendPreference::Cuda : mode == "auto" ? ComputeBackendPreference::Auto : ComputeBackendPreference::Cpu);
    VulkanSlicerBackend::begin_slicing_session();
    std::vector<VulkanVerticalIntersectionRequest> requests;
    for (uint64_t index = 0; index < 1024; ++index) {
        const bool reverse = (index & 1) != 0;
        requests.push_back({{{reverse ? 1000 : 0, int64_t(index) - 512},
                             {reverse ? 0 : 1000, 700 - int64_t(index)}}, 375, index});
    }
    const auto batch = VulkanSlicerBackend::dispatch_vertical_intersections(requests);
    if (mode == "cpu") {
        if (batch.dispatched || !batch.intersections.empty())
            return 3;
        std::cout << "CPU-only mode preserved without GPU initialization.\n";
        return 0;
    }
    if (!batch.dispatched) {
        std::cerr << "Requested GPU backend unavailable: " << batch.diagnostic << '\n'
                  << VulkanSlicerBackend::runtime_diagnostic_report() << '\n';
        return 77;
    }
    if (!validate_vertical_batch(requests, batch))
        return 3;
    VulkanSlicerBackend::note_vertical_intersection_usage(requests.size(), requests.size(), false, "Selftest complete batch accepted.");
    std::cout << "Exact vertical-intersection kernel passed for " << requests.size() << " records.\n";
    // CUDA implements vertical intersections, not the Vulkan-only tree stage.
    if (mode == "vulkan") {
        std::vector<VulkanTreeContourRequest> branches;
        std::vector<Segment> boxes;
        for (uint64_t i = 0; i < 64; ++i)
            branches.push_back({{{10, int64_t(i)}, {20, int64_t(i)}}, i});
        boxes.push_back({{0, 0}, {100, 50}}); // contains the first51 branches
        for (size_t i = 1; i < 256; ++i)
            boxes.push_back({{1000 + int64_t(i), 1000}, {2000 + int64_t(i), 2000}});
        const auto tree = VulkanSlicerBackend::dispatch_tree_contour_candidates(branches, boxes);
        if (!tree.dispatched || tree.may_intersect.size() != branches.size())
            return 4;
        for (size_t i = 0; i < branches.size(); ++i)
            if ((tree.may_intersect[i] != 0) != may_intersect_filled_boxes(branches[i].segment, boxes))
                return 4;
        std::cout << "Vulkan whole-contour-box tree kernel passed containment and disjoint cases.\n";
    }
    std::cout << VulkanSlicerBackend::runtime_diagnostic_report() << '\n';
    return 0;
}
