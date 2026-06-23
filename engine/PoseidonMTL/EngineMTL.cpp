#include <PoseidonMTL/EngineMTL.hpp>

#include <SDL3/SDL.h>

#include <Poseidon/Core/Application.hpp>
#include <Poseidon/Core/Config/EngineConfig.hpp>
#include <Poseidon/Graphics/Shared/WindowPlacement.hpp>
#include <Poseidon/Graphics/Dummy/TextBankDummy.hpp>
#include <Poseidon/Foundation/Logging/Logging.hpp>

namespace Poseidon
{

EngineMTL::EngineMTL(int width, int height, bool windowed, int bpp)
{
    _w = width;
    _h = height;
    _windowed = windowed;
    _windowedRestoreW = width;
    _windowedRestoreH = height;
    _pixelSize = bpp;
    _refreshRate = 60;

    LOG_INFO(Graphics, "MTL: Initializing engine — bootstrap {}x{} {}bpp {}", _w, _h, _pixelSize,
             _windowed ? "windowed" : "fullscreen");

    CreateWindowAndDevice();

    _textBank = new TextBankDummy();
}

void EngineMTL::CreateWindowAndDevice()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LOG_ERROR(Graphics, "MTL: SDL_Init failed: {}", SDL_GetError());
        return;
    }

    // Resolve final placement the same way GL33 does — keeps the
    // "borderless covers monitor" / windowed-vs-fullscreen rules in one
    // shared resolver instead of duplicating them per backend.
    auto& engineCfg = GApp->GetConfig().GetEngineConfig();
    DisplayPlacementInput displayCfg;
    displayCfg.displayMode = engineCfg.displayMode;
    if (_windowed && displayCfg.displayMode != "windowed")
        displayCfg.displayMode = "windowed";
    if (!_windowed && displayCfg.displayMode == "windowed")
        displayCfg.displayMode = "borderless";
    displayCfg.width = _w;
    displayCfg.height = _h;

    int desktopW = 0, desktopH = 0, desktopRefresh = 0;
    if (const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay()))
    {
        desktopW = dm->w;
        desktopH = dm->h;
        desktopRefresh = (int)(dm->refresh_rate + 0.5f);
    }
    const WindowPlacement placement = ResolveWindowPlacement(displayCfg, desktopW, desktopH, desktopRefresh);
    _windowMode = placement.mode;

    Uint32 flags = SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    switch (placement.mode)
    {
        case WindowMode::Fullscreen:
        case WindowMode::Borderless:
            flags |= SDL_WINDOW_BORDERLESS;
            break;
        case WindowMode::Windowed:
            flags |= SDL_WINDOW_RESIZABLE;
            break;
    }

    _sdlWindow = SDL_CreateWindow("Poseidon [Metal]", placement.width, placement.height, flags);
    if (!_sdlWindow)
    {
        LOG_ERROR(Graphics, "MTL: SDL_CreateWindow failed: {}", SDL_GetError());
        return;
    }

    if (placement.mode == WindowMode::Borderless)
    {
        SDL_SetWindowFullscreenMode(_sdlWindow, nullptr);
        if (!SDL_SetWindowFullscreen(_sdlWindow, true))
            LOG_WARN(Graphics, "MTL: SDL_SetWindowFullscreen(true) failed for borderless startup: {}", SDL_GetError());
    }
    else if (placement.posX != WindowPlacement::kCentered)
    {
        SDL_SetWindowPosition(_sdlWindow, placement.posX, placement.posY);
    }

    if (placement.refreshHz > 0)
        _refreshRate = placement.refreshHz;

    if (!_bootstrap.AttachToWindow(_sdlWindow))
    {
        LOG_ERROR(Graphics, "MTL: EngineMTLBootstrap::AttachToWindow failed");
        return;
    }

    int cw = 0, ch = 0;
    SDL_GetWindowSizeInPixels(_sdlWindow, &cw, &ch);
    _w = cw;
    _h = ch;

    LOG_INFO(Graphics, "MTL: Metal device — {}", _bootstrap.GetRendererName().c_str());
    LOG_INFO(Graphics, "MTL: surface resolved to {}x{} {}", _w, _h, _windowed ? "windowed" : "fullscreen");

    // Hook SDL events to the engine — same helper GL33 uses, input
    // forwarding/focus/Alt+Enter logic is identical across backends.
    _eventWindow.Attach(_sdlWindow, _w, _h);

    LoadConfig();
}

EngineMTL::~EngineMTL()
{
    LOG_INFO(Graphics, "MTL: Destroying engine");
    SaveConfig();

    delete _textBank;
    _textBank = nullptr;

    _bootstrap.Shutdown(); // releases device/queue/layer; AttachToWindow means it does NOT own _sdlWindow

    if (_sdlWindow)
    {
        SDL_DestroyWindow(_sdlWindow);
        _sdlWindow = nullptr;
    }
}

void EngineMTL::Clear(bool /*clearZ*/, bool clear, PackedColor color)
{
    // Piece 1 has no draw calls between Clear() and NextFrame() yet, so
    // clearing and presenting in one shot is equivalent to the "proper"
    // split (begin pass here, end+present in NextFrame) and avoids holding
    // an encoder open across member state for nothing. Revisit once real
    // draws land (Piece 2).
    _bootstrap.RenderClearAndPresent(color.R8() / 255.0f, color.G8() / 255.0f, color.B8() / 255.0f,
                                     color.A8() / 255.0f, clear);
}

bool EngineMTL::SwitchRes(int w, int h, int bpp)
{
    _pixelSize = bpp;
    if (_sdlWindow && _windowed)
        SDL_SetWindowSize(_sdlWindow, w, h);

    int cw = 0, ch = 0;
    if (_sdlWindow)
        SDL_GetWindowSizeInPixels(_sdlWindow, &cw, &ch);
    _w = cw > 0 ? cw : w;
    _h = ch > 0 ? ch : h;
    _bootstrap.OnWindowResized(_w, _h);
    return true;
}

bool EngineMTL::SwitchRefreshRate(int refresh)
{
    if (refresh == 0)
        return false;
    _refreshRate = refresh;
    return true;
}

bool EngineMTL::SetWindowMode(WindowMode mode)
{
    if (!_sdlWindow)
        return false;

    if (_windowed && mode != WindowMode::Windowed)
        SDL_GetWindowSize(_sdlWindow, &_windowedRestoreW, &_windowedRestoreH);

    _windowMode = mode;
    SDL_SetWindowFullscreen(_sdlWindow, mode != WindowMode::Windowed);
    SDL_SetWindowBordered(_sdlWindow, mode == WindowMode::Windowed);
    _windowed = (mode == WindowMode::Windowed);

    if (mode == WindowMode::Windowed && _windowedRestoreW > 0)
        SDL_SetWindowSize(_sdlWindow, _windowedRestoreW, _windowedRestoreH);

    int cw = 0, ch = 0;
    SDL_GetWindowSizeInPixels(_sdlWindow, &cw, &ch);
    _w = cw;
    _h = ch;
    _bootstrap.OnWindowResized(_w, _h);

    OnFullscreenChanged(_windowed);
    return true;
}

RString EngineMTL::GetDebugName() const
{
    return "Metal";
}

RString EngineMTL::GetRendererName() const
{
    std::string name = _bootstrap.GetRendererName();
    return name.empty() ? "Metal" : name.c_str();
}

void EngineMTL::ListResolutions(FindArray<ResolutionInfo>& ret)
{
    ret.Clear();
    if (_windowed)
        return;

    SDL_DisplayID display = _sdlWindow ? SDL_GetDisplayForWindow(_sdlWindow) : SDL_GetPrimaryDisplay();
    if (!display)
        return;

    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    if (!modes)
        return;

    for (int i = 0; i < count; i++)
    {
        ResolutionInfo info;
        info.w = modes[i]->w;
        info.h = modes[i]->h;
        info.bpp = SDL_BITSPERPIXEL(modes[i]->format);
        ret.AddUnique(info);
    }
    SDL_free(modes);
}

void EngineMTL::ListRefreshRates(FindArray<int>& ret)
{
    ret.Clear();
    if (_windowed)
    {
        ret.Add(0);
        return;
    }

    SDL_DisplayID display = _sdlWindow ? SDL_GetDisplayForWindow(_sdlWindow) : SDL_GetPrimaryDisplay();
    if (!display)
        return;

    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    if (!modes)
        return;

    for (int i = 0; i < count; i++)
    {
        if (modes[i]->w == Width() && modes[i]->h == Height())
            ret.AddUnique(static_cast<int>(modes[i]->refresh_rate));
    }
    SDL_free(modes);
}

AbstractTextBank* EngineMTL::TextBank()
{
    return _textBank;
}

bool EngineMTL::IsResizable() const
{
    return _sdlWindow && (SDL_GetWindowFlags(_sdlWindow) & SDL_WINDOW_RESIZABLE) != 0;
}

int EngineMTL::AFrameTime() const
{
    return 0; // matches GL33::FrameTime() (also stubbed to 0)
}

} // namespace Poseidon
