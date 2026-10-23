// Measurement module: deliberately contains no production atlas hooks.
#include "renderer/NativeAtlasUploads.hpp"
extern "C" __declspec(dllexport) const char chloriumMeasurementIdentity[] =
    "CHLORIUM_MEASUREMENT_ONLY_V1";
namespace chlorium::native {
AtlasUploadStats getAtlasUploadStats() { return {}; }
void purgeAtlasUploadCache() {}
}
