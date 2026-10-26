// Read as data by the packager; never load a DLL to determine its build type.
extern "C" {
#ifdef CHLORIUM_TRANSFER_HYBRID_AUDIT_V9
__declspec(dllexport) extern const char chloriumTransferPolicyIdentity[] = "CHLORIUM_TRANSFER_HYBRID_AUDIT_V9";
#endif
#ifdef CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9
__declspec(dllexport) extern const char chloriumPersistentTransferIdentity[] = "CHLORIUM_NATIVE_PERSISTENT_TRANSFER_V9";
#endif
#ifdef CHLORIUM_NATIVE_GPU_EXCHANGE_V8
__declspec(dllexport) extern const char chloriumNativeExchangeIdentity[] = "CHLORIUM_NATIVE_GPU_EXCHANGE_V8";
#endif
#ifdef CHLORIUM_NATIVE_DRAW_BRIDGE_V7
__declspec(dllexport) extern const char chloriumNativeBridgeIdentity[] = "CHLORIUM_NATIVE_DRAW_BRIDGE_V7";
#endif
#ifdef CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6
__declspec(dllexport) extern const char chloriumAdaptiveUploadIdentity[] = "CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS
__declspec(dllexport) extern const char chloriumSnapshotIdentity[] = "CHLORIUM_EXPERIMENTAL_NATIVE_SNAPSHOTS_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_QUADS
#ifdef CHLORIUM_NATIVE_VERTEX_PARITY
__declspec(dllexport) extern const char chloriumNativeQuadIdentity[] = "CHLORIUM_NATIVE_VERTEX_BACKEND_V5";
#else
__declspec(dllexport) extern const char chloriumNativeQuadIdentity[] = "CHLORIUM_NATIVE_QUAD_BACKEND_V4";
#endif
#endif
__declspec(dllexport) extern const char chloriumBuildIdentity[] =
#ifdef CHLORIUM_RUNTIME_AUDIT
    "CHLORIUM_RUNTIME_AUDIT_BINARY_V1";
#else
    "CHLORIUM_NORMAL_BINARY_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING
__declspec(dllexport) extern const char chloriumStreamingIdentity[] = "CHLORIUM_EXPERIMENTAL_NATIVE_STREAMING_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_AFFINE_POINT
__declspec(dllexport) extern const char chloriumAffineIdentity[] = "CHLORIUM_EXPERIMENTAL_AFFINE_POINT_V1";
__declspec(dllexport) extern const char chloriumAffineRevision[] = "CHLORIUM_AFFINE_POINT_REVISION_V2";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_AREA_MATH
__declspec(dllexport) extern const char chloriumAreaMathIdentity[] = "CHLORIUM_EXPERIMENTAL_AREA_MATH_V1";
__declspec(dllexport) extern const char chloriumAreaMathRevision[] = "CHLORIUM_AREA_MATH_TARGETED_CALLS_V3";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_MATH_PIPELINE
__declspec(dllexport) extern const char chloriumMathPipelineIdentity[] = "CHLORIUM_EXPERIMENTAL_MATH_PIPELINE_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_HEAP_RELEASE
__declspec(dllexport) extern const char chloriumHeapReleaseIdentity[] = "CHLORIUM_EXPERIMENTAL_HEAP_RELEASE_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS
__declspec(dllexport) extern const char chloriumAreaIdentity[] = "CHLORIUM_EXPERIMENTAL_AREA_DIVISIONS_V1";
#endif
#ifdef CHLORIUM_EXPERIMENTAL_MATRIX_REUSE
__declspec(dllexport) extern const char chloriumMatrixIdentity[] = "CHLORIUM_EXPERIMENTAL_MATRIX_REUSE_V1";
__declspec(dllexport) extern const char chloriumMatrixRevision[] = "CHLORIUM_MATRIX_PERSISTENT_REUSE_V3";
#endif
}
