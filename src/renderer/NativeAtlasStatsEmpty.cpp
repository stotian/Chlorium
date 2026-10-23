#if defined(CHLORIUM_NATIVE_GPU_TRANSFERS_ONLY) || defined(CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS) || defined(CHLORIUM_EXPERIMENTAL_MATRIX_REUSE)
#include "NativeAtlasUploads.hpp"
// Native GPU transfers do not need a shadow cache or atlas lifecycle hooks.
namespace chlorium::native {
AtlasUploadStats getAtlasUploadStats() { return {}; }
void purgeAtlasUploadCache() {}
}
extern "C" __declspec(dllexport) bool ChloriumReadAtlasUploadStats(
    chlorium::native::AtlasUploadStats* output, std::size_t size) {
    if (!output || size != sizeof(*output)) return false;
    *output = {}; return true;
}
#ifdef CHLORIUM_NATIVE_GPU_TRANSFERS_ONLY
extern "C" __declspec(dllexport) extern const char chloriumNativeGpuTransfersIdentity[] =
    "CHLORIUM_NATIVE_GPU_TRANSFERS_ONLY_V1";
#endif
#endif
