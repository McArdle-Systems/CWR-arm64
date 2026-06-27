#pragma once

#include <Poseidon/Graphics/Textures/TextureBank.hpp>
#include <PoseidonMTL/TextureMTL.hpp>
#include <Poseidon/Foundation/Containers/Array.hpp>

namespace Poseidon
{

class EngineMTLBootstrap;

// Real Metal-backed texture bank. Owns the budget/LRU tracking for every
// texture's on-demand "big" surface (see TextureMTL.hpp's class doc comment
// for the full small/big design and why the LRU is a single list instead of
// GL33's five) -- this is the Milestone-2 piece of the GL33-parity texture-
// streaming port. GPU-surface pooling (GL33's `_freeTextures`) is the one
// remaining gap, Milestone 3, stretch/lower-priority.
class TextBankMTL : public AbstractTextBank
{
  public:
    explicit TextBankMTL(EngineMTLBootstrap* bootstrap) : _bootstrap(bootstrap) {}
    ~TextBankMTL() override;

    int Find(RStringB name) const;

    Ref<Texture> Load(RStringB name) override;
    Ref<Texture> LoadInterpolated(RStringB n1, RStringB n2, float factor) override;
    MipInfo UseMipmap(Texture* texture, int level, int levelTop) override;
    void InitDetailTextures();
    TextureMTL* GetDetailTexture();
    TextureMTL* GetGrassTexture();
    TextureMTL* GetSpecularTexture();
    TextureMTL* GetWaterBumpMap();

    // Font-atlas pages etc. -- AbstractTextBank's default returns nullptr,
    // which silently dropped every FreeType glyph-atlas upload under this
    // backend (see FontDrawFreeType.cpp's SyncAtlasTextures): text never
    // got a real GPU texture, so nothing rendered.
    Texture* CreateDynamic(int w, int h, const void* rgba, uint32_t size, bool mipmap = false) override;
    void UpdateDynamic(Texture* texture, const void* rgba, uint32_t size) override;

    void Compact() override {}
    void Preload() override {}
    void FlushTextures() override {}
    void FlushBank(QFBank* /*bank*/) override {}
    // Wipes every texture at once (level unload etc.) -- the budget total
    // and LRU list would otherwise go stale (individual TextureMTL
    // destruction doesn't notify the bank, since that would need a back-
    // pointer on every texture for a case that's otherwise vanishingly
    // rare -- textures live for the whole session normally). This is the
    // one realistic bulk-destroy path, so reset the bookkeeping here
    // explicitly instead.
    void ReleaseAllTextures() override
    {
        _texture.Clear();
        _bigSurfaceLRU.Clear();
        _totalBigSurfaceBytes = 0;
    }
    void FinishFrame() override;

    int NTextures() const override { return _texture.Size(); }
    Texture* GetTexture(int i) const override { return _texture[i]; }

    // Evicts least-recently-used big surfaces (CLList::Last() first) until
    // there's room for `neededBytes` more, or nothing's left to evict.
    // Called by TextureMTL::EnsureBigSurface before it allocates a new/
    // bigger big surface.
    void ReserveMemory(int64_t neededBytes);

    // Bookkeeping hooks TextureMTL::EnsureBigSurface/EvictBigSurface call
    // directly -- kept as plain methods rather than folding into
    // ReserveMemory/EnsureBigSurface themselves so the byte-accounting and
    // LRU-touching responsibilities stay explicit at each call site (see
    // EnsureBigSurface's doc comment).
    void AdjustTotalBigSurfaceBytes(int64_t delta) { _totalBigSurfaceBytes += delta; }
    void TouchLRU(TextureMTL* texture) { texture->CacheUse(_bigSurfaceLRU); }

  private:
    // Lazily reads EngineMTLBootstrap::RecommendedMaxWorkingSetSize() on
    // first use (the Metal device isn't necessarily ready at TextBankMTL
    // construction time) -- see EngineMTLBootstrap.hpp's doc comment for
    // why this needs no GL33-style 256MB fallback.
    void EnsureBudgetInitialized();

    EngineMTLBootstrap* _bootstrap;

    // Declared *before* _texture/_detail/etc. deliberately -- C++ destroys
    // members in reverse declaration order, and ~TextureMTL() unlinks
    // itself from this list (via _cache->Delete()). _bigSurfaceLRU must
    // still be a live CLList when that runs, or Delete() asserts on a link
    // whose root was already torn down. Keep this above every
    // TextureMTL-owning member, not just LLinkArray<TextureMTL> _texture.
    CLList<HMipCacheMTL> _bigSurfaceLRU;
    int64_t _totalBigSurfaceBytes = 0;
    int64_t _maxTextureMemory = -1; // -1 = not yet initialized, see EnsureBudgetInitialized

    LLinkArray<TextureMTL> _texture;
    Ref<TextureMTL> _detail;
    Ref<TextureMTL> _specular;
    Ref<TextureMTL> _grass;
    Ref<TextureMTL> _waterBump;
};

} // namespace Poseidon
