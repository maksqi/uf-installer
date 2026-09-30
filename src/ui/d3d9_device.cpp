#include "ui/d3d9_device.h"

#include <wincodec.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>

#include <vector>

#include "core/log.h"

namespace uf::ui {

namespace {

template <typename T>
void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

}  // namespace

bool D3D9Device::Create(HWND hwnd, UINT width, UINT height) {
    pp_ = {};
    pp_.Windowed = TRUE;
    pp_.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp_.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp_.BackBufferWidth = width;
    pp_.BackBufferHeight = height;
    pp_.hDeviceWindow = hwnd;
    pp_.EnableAutoDepthStencil = FALSE;
    pp_.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    const DWORD vertexFlags[] = {D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DCREATE_SOFTWARE_VERTEXPROCESSING};
    using CreateEx = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
    auto createEx = reinterpret_cast<CreateEx>(GetProcAddress(GetModuleHandleW(L"d3d9.dll"), "Direct3DCreate9Ex"));
    if (createEx && SUCCEEDED(createEx(D3D_SDK_VERSION, &d3dEx_))) {
        for (DWORD vp : vertexFlags) {
            HRESULT hr = d3dEx_->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, vp, &pp_, nullptr, &deviceEx_);
            if (SUCCEEDED(hr)) {
                device_ = deviceEx_;
                device_->AddRef();
                log::Info("Direct3D 9Ex device created ({})", vp == D3DCREATE_HARDWARE_VERTEXPROCESSING ? "HW VP" : "SW VP");
                return true;
            }
            log::Warn("CreateDeviceEx failed: 0x{:08X}", static_cast<unsigned>(hr));
        }
        SafeRelease(d3dEx_);
    }
    d3d_ = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d_) return false;
    for (DWORD vp : vertexFlags) {
        HRESULT hr = d3d_->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, vp, &pp_, &device_);
        if (SUCCEEDED(hr)) {
            log::Info("Direct3D 9 device created");
            return true;
        }
        log::Warn("CreateDevice failed: 0x{:08X}", static_cast<unsigned>(hr));
    }
    return false;
}

void D3D9Device::Destroy() {
    SafeRelease(deviceEx_);
    SafeRelease(device_);
    SafeRelease(d3dEx_);
    SafeRelease(d3d_);
}

void D3D9Device::Reset() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = deviceEx_ ? deviceEx_->ResetEx(&pp_, nullptr) : device_->Reset(&pp_);
    if (FAILED(hr)) log::Warn("Device reset failed: 0x{:08X}", static_cast<unsigned>(hr));
    ImGui_ImplDX9_CreateDeviceObjects();
}

void D3D9Device::Resize(UINT width, UINT height) {
    if (width == 0 || height == 0 || (width == pp_.BackBufferWidth && height == pp_.BackBufferHeight)) return;
    pp_.BackBufferWidth = width;
    pp_.BackBufferHeight = height;
    Reset();
}

bool D3D9Device::Ready() {
    if (!lost_) return true;
    HRESULT hr = deviceEx_ ? deviceEx_->CheckDeviceState(pp_.hDeviceWindow) : device_->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST) return false;
    Reset();
    lost_ = false;
    return true;
}

void D3D9Device::Render(ImDrawData* drawData, D3DCOLOR clear) {
    device_->SetRenderState(D3DRS_ZENABLE, FALSE);
    device_->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    device_->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    device_->Clear(0, nullptr, D3DCLEAR_TARGET, clear, 1.0f, 0);
    if (SUCCEEDED(device_->BeginScene())) {
        ImGui_ImplDX9_RenderDrawData(drawData);
        device_->EndScene();
    }
}

void D3D9Device::Present() {
    HRESULT hr = deviceEx_ ? deviceEx_->PresentEx(nullptr, nullptr, nullptr, nullptr, 0) : device_->Present(nullptr, nullptr, nullptr, nullptr);
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICEHUNG || hr == D3DERR_DEVICEREMOVED) lost_ = true;
}

bool D3D9Device::SaveBackbufferPng(const fs::path& file) {
    IDirect3DSurface9* back = nullptr;
    IDirect3DSurface9* sys = nullptr;
    bool ok = false;
    std::vector<std::uint8_t> pixels;
    D3DSURFACE_DESC desc{};
    if (SUCCEEDED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back)) && SUCCEEDED(back->GetDesc(&desc)) &&
        SUCCEEDED(device_->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) &&
        SUCCEEDED(device_->GetRenderTargetData(back, sys))) {
        D3DLOCKED_RECT lr{};
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            pixels.resize(static_cast<std::size_t>(desc.Width) * desc.Height * 4);
            for (UINT y = 0; y < desc.Height; ++y) {
                const auto* src = static_cast<const std::uint8_t*>(lr.pBits) + static_cast<std::size_t>(y) * lr.Pitch;
                std::uint8_t* dst = pixels.data() + static_cast<std::size_t>(y) * desc.Width * 4;
                memcpy(dst, src, static_cast<std::size_t>(desc.Width) * 4);
                for (UINT x = 0; x < desc.Width; ++x) dst[x * 4 + 3] = 255;
            }
            sys->UnlockRect();
            ok = true;
        }
    }
    SafeRelease(sys);
    SafeRelease(back);
    if (!ok) return false;

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
         SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) &&
         SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
         SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) && SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
         SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(desc.Width, desc.Height)) &&
         SUCCEEDED(frame->SetPixelFormat(&format)) && format == GUID_WICPixelFormat32bppBGRA &&
         SUCCEEDED(frame->WritePixels(desc.Height, desc.Width * 4, static_cast<UINT>(pixels.size()), pixels.data())) &&
         SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    SafeRelease(frame);
    SafeRelease(encoder);
    SafeRelease(stream);
    SafeRelease(factory);
    log::Info("Screenshot {} -> {}", ok ? "saved" : "FAILED", PathUtf8(file));
    return ok;
}

}  // namespace uf::ui
