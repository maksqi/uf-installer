#include "ui/svg.h"

#include <cmath>
#include <string>

#pragma warning(push, 0)
#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS  // flag-icons uses names like "gold"
#include <nanosvg/nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg/nanosvgrast.h>
#pragma warning(pop)

#include "core/payload.h"

namespace uf::ui {

bool RasterizeSvgResource(int resourceId, int width, int* height, std::vector<std::uint8_t>& rgba) {
    auto bytes = ResourceBytes(resourceId);
    if (bytes.empty() || width <= 0) return false;
    std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());  // nsvgParse writes into its input
    NSVGimage* image = nsvgParse(text.data(), "px", 96.f);
    if (!image || image->width <= 0 || image->height <= 0) {
        if (image) nsvgDelete(image);
        return false;
    }
    float scale = static_cast<float>(width) / image->width;
    int h = static_cast<int>(std::lround(image->height * scale));
    bool ok = false;
    if (NSVGrasterizer* r = nsvgCreateRasterizer()) {
        rgba.assign(static_cast<std::size_t>(width) * h * 4, 0);
        nsvgRasterize(r, image, 0, 0, scale, rgba.data(), width, h, width * 4);
        nsvgDeleteRasterizer(r);
        *height = h;
        ok = true;
    }
    nsvgDelete(image);
    return ok;
}

}  // namespace uf::ui
