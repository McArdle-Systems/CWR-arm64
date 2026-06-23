#pragma once

#include <string>

struct SDL_Window;

namespace Poseidon
{

// Native Metal device/layer/queue wrapper (macOS / Apple Silicon). Used two
// ways:
//  - Milestone 0 (MetalSmokeTest): Init() creates its own SDL_WINDOW_METAL
//    window and owns it end to end.
//  - EngineMTL (the real Poseidon::Engine backend): AttachToWindow() sets up
//    the layer/device/queue against a window EngineMTL already created
//    itself (via the shared WindowPlacement resolver, same as GL33).
//
// Deliberately the *only* place in PoseidonMTL that includes metal-cpp's
// Foundation/Metal/QuartzCore headers. Poseidon's core headers do
// `using Poseidon::Object;` at global scope (Types.hpp) and `typedef int
// BOOL` (Memtype.h), both of which collide with metal-cpp's NS::Object
// template machinery and the real Objective-C BOOL if the two ever land in
// the same translation unit. Keeping this class's header metal-cpp-free
// (PIMPL'd) lets EngineMTL.cpp include Poseidon headers freely and talk to
// Metal only through this opaque interface.
class EngineMTLBootstrap
{
  public:
    EngineMTLBootstrap();
    ~EngineMTLBootstrap();

    EngineMTLBootstrap(const EngineMTLBootstrap&) = delete;
    EngineMTLBootstrap& operator=(const EngineMTLBootstrap&) = delete;

    // Creates the SDL_WINDOW_METAL window, attaches a CAMetalLayer, picks the
    // system default MTLDevice, and creates the command queue. Returns false
    // (with a message on stderr) on any failure.
    bool Init(const char* title, int width, int height);

    // Attaches to a window the caller already created (any SDL window flags
    // -- EngineMTL creates it with SDL_WINDOW_METAL itself). Sets up the
    // CAMetalLayer/device/queue the same way Init() does, minus the window
    // creation. Returns false on failure.
    bool AttachToWindow(SDL_Window* window);

    // Clears the next drawable to the given color (0..1 range) and presents
    // it. No render pass content beyond the clear — no shaders/meshes yet.
    // `clear` = false uses a Load action instead of Clear (matches Engine's
    // Clear(bool clearZ, bool clear, ...) `clear` flag).
    void RenderClearAndPresent(float r, float g, float b, float a, bool clear = true);

    void OnWindowResized(int width, int height);

    void Shutdown();

    SDL_Window* Window() const { return _window; }

    // Metal device name (e.g. "Apple M2 Pro"), empty if not yet initialized.
    std::string GetRendererName() const;

  private:
    bool SetupDevice(); // shared by Init() and AttachToWindow()

    struct Impl;
    Impl* _impl = nullptr;
    SDL_Window* _window = nullptr;
    bool _ownsWindow = false;
};

} // namespace Poseidon
