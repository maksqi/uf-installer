#pragma once
#include <cstdint>
#include <vector>

namespace uf::ui {

// Rasterizes an SVG stored in the exe resources to `width` pixels wide (height keeps the aspect ratio).
// Output: straight-alpha RGBA rows, top-down. Returns false if the resource is missing or not valid SVG.
bool RasterizeSvgResource(int resourceId, int width, int* height, std::vector<std::uint8_t>& rgba);

}  // namespace uf::ui
