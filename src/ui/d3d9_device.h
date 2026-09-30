#pragma once
#include <windows.h>
#include <d3d9.h>

#include "core/paths.h"

struct ImDrawData;

namespace uf::ui {

// Direct3D 9Ex device (plain D3D9 fallback) for the ImGui DX9 backend.
// D3D9 is available on every PC that can run GTA SA, and 9Ex never loses the device on Vista+.
class D3D9Device {
public:
    bool Create(HWND hwnd, UINT width, UINT height);
    void Destroy();
    void Resize(UINT width, UINT height);
    // Returns false while the device is lost (plain D3D9 only) - skip the frame then.
    bool Ready();
    void Render(ImDrawData* drawData, D3DCOLOR clear);
    bool SaveBackbufferPng(const fs::path& file);
    void Present();

    IDirect3DDevice9* device() const { return device_; }
    bool isEx() const { return deviceEx_ != nullptr; }

private:
    void Reset();

    IDirect3D9* d3d_ = nullptr;
    IDirect3D9Ex* d3dEx_ = nullptr;
    IDirect3DDevice9* device_ = nullptr;
    IDirect3DDevice9Ex* deviceEx_ = nullptr;
    D3DPRESENT_PARAMETERS pp_{};
    bool lost_ = false;
};

}  // namespace uf::ui
