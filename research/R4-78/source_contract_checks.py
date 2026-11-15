from pathlib import Path

root = Path(__file__).resolve().parents[2]
renderer = (root / "research/R4-57/ContinuousRenderer.cpp").read_text(encoding="utf-8")
builder = (root / "research/R4-04/build_quad_candidate.py").read_text(encoding="utf-8")
cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")

assert '"--live-root-r78": "native-live-root-r78"' in builder
assert 'selected in ("--live-root-r78", "--live-root-measure-r78")' in builder
assert 'flags += ["/DCHLORIUM_LIVE_ROOT_DRAW_R78=1"]' in builder
assert 'sources += [root/"research/R4-72/RotationTrace.cpp",root/"research/R4-72/PointGateway.asm"]' in builder
assert 'sources += [root/"research/R4-78/LiveDraw.cpp"]' in builder
assert 'if(drawn && m_pQuads && m_pBuffersVBO[0] && !first && count==m_uTotalQuads' in renderer
assert 'rotationtrace::livePrepare(this,currentBatch)' in renderer

gpu = renderer.index('if(result==chlorium::research::liveroot::Status::gpu)')
fallback = renderer.index('if(result==chlorium::research::liveroot::Status::native)')
gpu_body = renderer[gpu:fallback]
assert 'NativeVertexLease lease(m_pBuffersVBO[0],m_bDirty,m_pBuffersVBO[0]);drawNative();' in gpu_body
assert gpu_body.count('drawNative();') == 1
assert gpu_body.count('liveroot::submitted();') == 1
assert gpu_body.index('drawNative();') < gpu_body.index('liveroot::submitted();')
fallback_body = renderer[fallback:renderer.index('#endif', fallback)]
assert fallback_body.count('drawNative();') == 1
assert 'liveroot::submitted();' not in fallback_body
assert fallback_body.index('markNativeVboDirtyForFallback(m_bDirty)') < fallback_body.index('drawNative();')
assert 'option(CHLORIUM_EXPERIMENTAL_STATIC_RENDERER "Build the incomplete Bismuth GPU replacement (development only)" OFF)' in cmake
assert 'option(CHLORIUM_EXPERIMENTAL_NATIVE_QUADS "Canonical native renderer and exact Area quotient reuse" ON)' in cmake
assert 'option(CHLORIUM_NATIVE_VERTEX_PARITY "Preserve native shader, uniforms and index storage" ON)' in cmake
assert 'set(SOURCES src/renderer/NativeAreaDivisions.cpp' in cmake
assert 'src/renderer/hooks.cpp' not in cmake[cmake.index('if(CHLORIUM_EXPERIMENTAL_NATIVE_QUADS)'):cmake.index('add_library(Chlorium')]
print("PASS R78 draw call graph, full-atlas gate, post-draw accounting, failure restoration and unchanged canonical default")
