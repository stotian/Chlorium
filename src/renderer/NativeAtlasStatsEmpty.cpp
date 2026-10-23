#if defined(CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS) || defined(CHLORIUM_EXPERIMENTAL_MATRIX_REUSE)
#include "NativeAtlasUploads.hpp"
// CPU-only experiments do not install an atlas backend or claim GPU checks.
namespace chlorium::native {
AtlasUploadStats getAtlasUploadStats() { return {}; }
void purgeAtlasUploadCache() {}
}
#endif
