#pragma once

#include <Poseidon/Graphics/Core/Engine.hpp>
#include <PoseidonGL33/SDLEventWindow.hpp>
#include <PoseidonMTL/EngineMTLBootstrap.hpp>

namespace Poseidon
{

class TextBankDummy;

// First real Metal Engine backend: implements the full IGraphicsEngine /
// Engine contract so it can register with GraphicsEngineFactory and run
// through GameApplication's normal lifecycle. Draw*/Mesh* calls are no-op
// stubs for now (mirrors GraphicsEngineDummy) -- nothing exercises them
// before world/landscape content loads. TextBank() returns a TextBankDummy
// for the same reason: textures are tracked but not yet GPU-uploaded.
//
// All actual Metal calls go through EngineMTLBootstrap (AttachToWindow /
// RenderClearAndPresent / etc.) rather than metal-cpp types directly --
// metal-cpp's Foundation headers can't be included in the same translation
// unit as Poseidon's core headers (see EngineMTLBootstrap.hpp for why), and
// this file needs Engine.hpp.
class EngineMTL : public Engine
{
  public:
    EngineMTL(int width, int height, bool windowed, int bpp);
    ~EngineMTL() override;

    void Clear(bool clearZ = true, bool clear = true, PackedColor color = PackedColor(0)) override;
    void Pause() override {}
    void Restore() override {}
    void FogColorChanged(ColorVal /*fogColor*/) override {}

    bool SwitchRes(int w, int h, int bpp) override;
    bool SwitchRefreshRate(int refresh) override;
    bool SetWindowMode(WindowMode mode) override;

    void HandleEvents() override { _eventWindow.HandleEvents(); }
    bool IsOpen() const override { return _eventWindow.IsOpen(); }
    void SetMouseGrab(bool grab) override { _eventWindow.SetMouseGrab(grab); }
    bool IsMouseGrabbed() const override { return _eventWindow.IsMouseGrabbed(); }

    RString GetDebugName() const override;
    RString GetRendererName() const override;

    void ListResolutions(FindArray<ResolutionInfo>& ret) override;
    void ListRefreshRates(FindArray<int>& ret) override;

    void SetGamma(float g) override { _gamma = g; }
    float GetGamma() const override { return _gamma; }

    void PrepareTriangle(const MipInfo& /*mip*/, int /*specFlags*/) override {}
    void DrawPolygon(const VertexIndex* /*i*/, int /*n*/) override {}
    void DrawSection(const FaceArray& /*face*/, Offset /*beg*/, Offset /*end*/) override {}
    void DrawDecal(Vector3Par /*pos*/, float /*rhw*/, float /*sizeX*/, float /*sizeY*/, PackedColor /*col*/,
                   const MipInfo& /*mip*/, int /*specFlags*/) override
    {
    }

    void Draw2D(const Draw2DPars& /*pars*/, const Rect2DAbs& /*rect*/,
                const Rect2DAbs& /*clip*/ = Rect2DClipAbs) override
    {
    }
    void DrawPoly(const MipInfo& /*mip*/, const Vertex2DAbs* /*vertices*/, int /*nVertices*/,
                  const Rect2DAbs& /*clip*/ = Rect2DClipAbs, int /*specFlags*/ = DefSpecFlags2D) override
    {
    }
    void DrawPoly(const MipInfo& /*mip*/, const Vertex2DPixel* /*vertices*/, int /*nVertices*/,
                  const Rect2DPixel& /*clip*/ = Rect2DClipPixel, int /*specFlags*/ = DefSpecFlags2D) override
    {
    }
    void DrawLine(const Line2DAbs& /*rect*/, PackedColor /*c0*/, PackedColor /*c1*/,
                  const Rect2DAbs& /*clip*/ = Rect2DClipAbs) override
    {
    }
    void DrawLine(int /*beg*/, int /*end*/) override {}

    void PrepareMesh(const render::LegacySpec& /*spec*/) override {}
    void BeginMesh(TLVertexTable& /*mesh*/, const render::LegacySpec& /*spec*/) override {}
    void EndMesh(TLVertexTable& /*mesh*/) override {}

    AbstractTextBank* TextBank() override;
    void TextureDestroyed(Texture* /*tex*/) override {}

    float ZShadowEpsilon() const override { return 0.01f; }
    float ZRoadEpsilon() const override { return 0.005f; }
    float ObjMipmapCoef() const override { return 1.5f; }
    void GetZCoefs(float& zAdd, float& zMult) override
    {
        zAdd = 0;
        zMult = 1;
    }
    int GetBias() override { return _bias; }
    void SetBias(int value) override { _bias = value; }
    bool CanZBias() const override { return false; }
    bool ZBiasExclusion() const override { return true; }

    int Width() const override { return _w; }
    int Height() const override { return _h; }
    int PixelSize() const override { return _pixelSize; }
    int RefreshRate() const override { return _refreshRate; }
    bool CanBeWindowed() const override { return true; }
    bool IsWindowed() const override { return _windowed; }
    bool IsResizable() const override;

    int AFrameTime() const override;

  private:
    int _w = 0, _h = 0; // backbuffer dimensions (pixels)
    int _pixelSize;
    int _refreshRate;
    bool _windowed;
    int _bias = 0;
    float _gamma = 1.0f;
    WindowMode _windowMode = WindowMode::Borderless;
    int _windowedRestoreW = 0, _windowedRestoreH = 0;

    SDL_Window* _sdlWindow = nullptr;
    SDLEventWindow _eventWindow;
    EngineMTLBootstrap _bootstrap;

    TextBankDummy* _textBank = nullptr;

    void CreateWindowAndDevice();
};

} // namespace Poseidon
