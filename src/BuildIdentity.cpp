// Read as data by the packager; never load a DLL to determine its build type.
extern "C" {
__declspec(dllexport) extern const char chloriumBuildIdentity[] =
#ifdef CHLORIUM_RUNTIME_AUDIT
    "CHLORIUM_RUNTIME_AUDIT_BINARY_V1";
#else
    "CHLORIUM_NORMAL_BINARY_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING
__declspec(dllexport) extern const char chloriumStreamingIdentity[] = "CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS
__declspec(dllexport) extern const char chloriumAreaIdentity[] = "CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_MATRIX_REUSE
__declspec(dllexport) extern const char chloriumMatrixIdentity[] = "CHLORIUM_EXPERIMENTAL_MATRIX_REUSE_V1";
__declspec(dllexport) extern const char chloriumMatrixRevision[] = "CHLORIUM_MATRIX_PERSISTENT_REUSE_V3";
#endif
}
