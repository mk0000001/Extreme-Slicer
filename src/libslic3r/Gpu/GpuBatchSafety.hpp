#pragma once

#include "GpuExactGeometry.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <unordered_set>

namespace Slic3r::Gpu {

constexpr size_t maximum_intersection_requests = 256 * 1024;
constexpr size_t maximum_tree_requests = 128 * 1024;
constexpr size_t maximum_tree_pairs = 16 * 1024 * 1024;

class BackendSafetyState {
public:
    bool quarantined() const { return m_quarantined.load(std::memory_order_acquire); }
    void quarantine() { m_quarantined.store(true, std::memory_order_release); }
private:
    std::atomic_bool m_quarantined { false };
};

inline BackendSafetyState& cuda_safety_state()
{
    static BackendSafetyState state;
    return state;
}

inline BackendSafetyState& vulkan_safety_state()
{
    static BackendSafetyState state;
    return state;
}

inline bool checked_vertical_reference(const VerticalIntersectionRequest& request,
                                       VerticalIntersection& result)
{
    result = {};
    const auto& a = request.segment.a;
    const auto& b = request.segment.b;
    const bool forward = b.x > a.x;
    const WideCoord denominator = forward ? WideCoord(b.x) - a.x : WideCoord(a.x) - b.x;
    const WideCoord t = forward ? WideCoord(request.scan_x) - a.x : WideCoord(a.x) - request.scan_x;
    const WideCoord dy = WideCoord(b.y) - a.y;
    const WideCoord product = t * dy;
    const WideCoord origin_product = WideCoord(a.y) * denominator;
    const WideCoord numerator = product + origin_product;
    const auto fits = [](const WideCoord& value) {
        return value >= std::numeric_limits<Coord>::min() && value <= std::numeric_limits<Coord>::max();
    };
    // Match every actual device operation, not merely the final mathematical
    // value: cancelling products can overflow before their sum becomes small.
    // FillRectilinear stores its positive rational denominator in uint32_t.
    if (denominator <= 1 || denominator > std::numeric_limits<uint32_t>::max() ||
        t <= 0 || t >= denominator || !fits(dy) || !fits(product) ||
        !fits(origin_product) || !fits(numerator))
        return false;
    result = {numerator.convert_to<Coord>(), denominator.convert_to<Coord>(), request.stable_id, true};
    return true;
}

template<class Requests>
bool unique_stable_ids(const Requests& requests)
{
    std::unordered_set<uint64_t> ids;
    ids.reserve(requests.size());
    for (const auto& request : requests)
        if (!ids.insert(request.stable_id).second)
            return false;
    return true;
}

inline bool preflight_vertical_batch(const std::vector<VerticalIntersectionRequest>& requests)
{
    if (requests.empty() || requests.size() > maximum_intersection_requests || !unique_stable_ids(requests))
        return false;
    VerticalIntersection result;
    return std::all_of(requests.begin(), requests.end(), [&](const auto& request) {
        return checked_vertical_reference(request, result);
    });
}

inline bool validate_vertical_batch(const std::vector<VerticalIntersectionRequest>& requests,
                                    const VerticalIntersectionBatch& batch)
{
    if (!batch.dispatched || batch.intersections.size() != requests.size())
        return false;
    for (size_t index = 0; index < requests.size(); ++index) {
        VerticalIntersection expected;
        const auto& actual = batch.intersections[index];
        if (!checked_vertical_reference(requests[index], expected) || !actual.valid ||
            actual.stable_id != expected.stable_id || actual.numerator != expected.numerator ||
            actual.denominator != expected.denominator)
            return false;
    }
    return true;
}

template<class Dispatch>
VerticalIntersectionBatch dispatch_checked_vertical_batch(
    const std::vector<VerticalIntersectionRequest>& requests, BackendSafetyState& state,
    bool enabled, Dispatch&& dispatch)
{
    const auto start = std::chrono::steady_clock::now();
    VerticalIntersectionBatch batch;
    const auto finish = [&] {
        batch.host_elapsed_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        return batch;
    };
    if (!enabled || state.quarantined()) {
        batch.diagnostic = "GPU backend disabled or quarantined; retain CPU geometry.";
        return finish();
    }
    try {
        if (!preflight_vertical_batch(requests)) {
            batch.diagnostic = "GPU numeric domain or bounded batch contract rejected; retain CPU geometry.";
            return finish();
        }
        batch = dispatch();
        if (validate_vertical_batch(requests, batch) && !state.quarantined())
            return finish();
        state.quarantine();
        batch.diagnostic = "GPU dispatch or complete exact validation failed; backend quarantined. " + batch.diagnostic;
    } catch (const std::exception& error) {
        state.quarantine();
        batch.diagnostic = std::string("GPU dispatch failed; backend quarantined: ") + error.what();
    } catch (...) {
        state.quarantine();
        batch.diagnostic = "GPU dispatch failed; backend quarantined.";
    }
    batch.dispatched = false;
    batch.intersections.clear();
    return finish();
}

inline bool boxes_overlap(const Segment& first, const Segment& second)
{
    return std::min(first.a.x, first.b.x) <= std::max(second.a.x, second.b.x) &&
           std::min(second.a.x, second.b.x) <= std::max(first.a.x, first.b.x) &&
           std::min(first.a.y, first.b.y) <= std::max(second.a.y, second.b.y) &&
           std::min(second.a.y, second.b.y) <= std::max(first.a.y, first.b.y);
}

// Boxes enclose whole contours, including holes conservatively. Edge boxes
// cannot reject a line inside a filled polygon. Touching remains a candidate.
inline bool may_intersect_filled_boxes(const Segment& branch, const std::vector<Segment>& contour_boxes)
{
    return std::any_of(contour_boxes.begin(), contour_boxes.end(),
                       [&](const auto& box) { return boxes_overlap(branch, box); });
}

template<class Dispatch>
TreeContourBatch dispatch_checked_tree_batch(
    const std::vector<TreeContourRequest>& requests, const std::vector<Segment>& contour_boxes,
    BackendSafetyState& state, bool enabled, Dispatch&& dispatch)
{
    TreeContourBatch batch;
    if (!enabled || state.quarantined())
        return batch;
    try {
        if (requests.empty() || contour_boxes.empty() || requests.size() > maximum_tree_requests ||
            contour_boxes.size() > maximum_tree_requests ||
            requests.size() > maximum_tree_pairs / contour_boxes.size() || !unique_stable_ids(requests))
            return batch;
        batch = dispatch();
        bool valid = batch.dispatched && batch.may_intersect.size() == requests.size();
        for (size_t i = 0; valid && i < requests.size(); ++i)
            valid = batch.may_intersect[i] <= 1 &&
                (batch.may_intersect[i] != 0 || !may_intersect_filled_boxes(requests[i].segment, contour_boxes));
        if (valid && !state.quarantined())
            return batch;
    } catch (...) {
        // A failed broad phase must never manufacture a negative cache entry.
    }
    state.quarantine();
    batch.dispatched = false;
    batch.may_intersect.clear();
    batch.diagnostic = "GPU tree dispatch or conservative validation failed; backend quarantined.";
    return batch;
}

} // namespace Slic3r::Gpu
