// Private, collision-preserving benchmark. Never included in normal builds.
#include <Geode/Geode.hpp>
#include <Geode/modify/CCDisplayLinkDirector.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/GManager.hpp>
#include <Geode/modify/CCTextureAtlas.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/binding/LevelDownloadDelegate.hpp>
#ifdef CHLORIUM_EXPERIMENTAL_STATIC_RENDERER
#include "renderer/Renderer.hpp"
#endif
#include "renderer/NativeAtlasUploads.hpp"
#include "renderer/NativeTransformStats.hpp"
#include "renderer/NativeMatrixStats.hpp"
#include "renderer/NativeSnapshotStats.hpp"
#include "renderer/NativeAffinePoint.hpp"
#include "BProfiler.hpp"
#include "FrameWatch.hpp"
#include "MeasurementHookPolicy.hpp"
#include "../research/R4-57/ContinuousStats.hpp"
#ifdef CHLORIUM_DIRECT_VERTEX_AUDIT_R65
#include "../research/R4-65/DirectStats.hpp"
extern "C" __declspec(dllexport) const char chloriumDirectAuditIdentity[]="CHLORIUM_DIRECT_VERTEX_AUDIT_R65";
#endif
extern "C" __declspec(dllexport) const char chloriumContinuousAuditIdentity[]="CHLORIUM_RESIDENT_CONTINUOUS_AUDIT_R57";
#ifdef CHLORIUM_GROUP_TRACE_AUDIT_R63
extern "C" __declspec(dllexport) const char chloriumGroupTraceAuditIdentity[]="CHLORIUM_GROUP_TRACE_AUDIT_R63";
#endif
#ifdef CHLORIUM_EXTENDED_RESIDENT_STATS_R59
extern "C" __declspec(dllexport) const char chloriumResidentTelemetryIdentity[]="CHLORIUM_RESIDENT_TELEMETRY_R59";
#endif
#ifdef CHLORIUM_RENDERER_TIMING_AUDIT_R60
extern "C" __declspec(dllexport) const char chloriumRendererTimingAuditIdentity[]="CHLORIUM_RENDERER_TIMING_AUDIT_R60";
#endif
#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
#include "../research/R4-32/NativePassCapture.hpp"
#endif
#ifdef CHLORIUM_OPACITY_PREFIX_SHADOW
#include "../research/R4-30/NativeOpacityShadow.hpp"
#endif
#ifdef CHLORIUM_CANONICAL_AUDIT_V10
#include "CanonicalCoreProof.hpp"
#include <xmmintrin.h>
extern "C" __declspec(dllexport) const char chloriumCanonicalAuditIdentity[]="CHLORIUM_CANONICAL_CORE_AUDIT_V10";
extern "C" __declspec(dllexport) const char chloriumOpacityHashAuditIdentity[]="CHLORIUM_OPACITY_HASH_AUDIT_R36";
#ifdef CHLORIUM_CANONICAL_AUDIT_PE_R2
extern "C" __declspec(dllexport) const char chloriumCanonicalAuditBoundsIdentity[]="CHLORIUM_CANONICAL_AUDIT_BOUNDED_PE_R2";
#endif
#endif
#ifdef CHLORIUM_EXTERNAL_SAMPLING_AUDIT
#include <cstdlib>
extern "C" __declspec(dllexport) const char chloriumExternalSamplingIdentity[]="CHLORIUM_EXTERNAL_SAMPLING_AUDIT_V2";
extern "C" __declspec(dllexport) const unsigned chloriumExternalSamplingSessionMode=
    std::getenv("CHLORIUM_EXTERNAL_SAMPLING")?0x53414d50u:0u;
#endif
#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_MOVES
#include "renderer/NativeMovementLoop.hpp"
#endif
#include <windows.h>
#include <psapi.h>
#include <charconv>
#include <limits>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <numeric>
#include <cmath>
#include <map>
#include <sstream>

using namespace geode::prelude;
#ifdef CHLORIUM_AREA_WORK_CENSUS
extern "C" void ChloriumFlushAreaWorkCensus();
#endif
using Clock = std::chrono::steady_clock;
extern "C" __declspec(dllexport) const char chloriumAutoCollisionIdentity[] =
    "CHLORIUM_AUTO_COLLISIONS_ALLOWLIST_V4";
extern "C" __declspec(dllexport) const char chloriumUserModProfileIdentity[] =
    "CHLORIUM_AUTO_USER_MOD_PROFILE_V1";
extern "C" __declspec(dllexport) const char chloriumVisualReadbackIdentity[] =
    "CHLORIUM_VISUAL_READBACK_V2";
extern "C" __declspec(dllexport) const char chloriumVisualProbeIdentity[] =
    "CHLORIUM_VISUAL_PROBES_V1";
extern "C" __declspec(dllexport) const char chloriumTargetedMathTelemetryIdentity[] =
    "CHLORIUM_TARGETED_MATH_TELEMETRY_V1";
extern "C" __declspec(dllexport) const char chloriumMathPipelineAuditIdentity[] =
    "CHLORIUM_MATH_PIPELINE_AUDIT_V1";
extern "C" __declspec(dllexport) const char chloriumSnapshotAuditIdentity[] =
    "CHLORIUM_NATIVE_SNAPSHOT_AUDIT_V1";
extern "C" __declspec(dllexport) const char chloriumPresentedCaptureIdentity[] =
    "CHLORIUM_PRESENTED_CAPTURE_ONLY_V3";
extern "C" __declspec(dllexport) const char chloriumReadbackStateIdentity[] =
    "CHLORIUM_READBACK_STATE_GUARDS_V5";
extern "C" __declspec(dllexport) const char chloriumSceneGuardIdentity[] =
    "CHLORIUM_AUDIT_SCENE_TRANSITION_GUARD_R3";
extern "C" __declspec(dllexport) const char chloriumPacingIdentity[] =
    "CHLORIUM_AUDIT_120FPS_PACING_R4";
extern "C" __declspec(dllexport) const char chloriumSourceLevelIdentity[] =
    "CHLORIUM_RESEARCH_SOURCE_LEVEL_ID_R45";
bool chloriumAuditUseNative() { return std::getenv("CHLORIUM_AUDIT_NATIVE") != nullptr; }
#ifdef CHLORIUM_MEASUREMENT_ONLY
extern "C" __declspec(dllexport) extern const char chloriumPassiveTransformTimingIdentity[] =
    "CHLORIUM_PASSIVE_TRANSFORM_TIMER_V1";
#endif
namespace {
constexpr double targetFps = 120.0;
constexpr std::array authorizedLevels{133260800, 129598490, 88952972, 139648964};
int allowedLevel = authorizedLevels[0];
bool selectAuthorizedLevel(const char* text) {
    if (!text) return true;
    int value = 0;
    const auto end = text + std::strlen(text);
    const auto parsed = std::from_chars(text, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        std::find(authorizedLevels.begin(), authorizedLevels.end(), value) == authorizedLevels.end()) return false;
    allowedLevel = value; return true;
}
bool ready = false, initialized = false, loading = false, running = false;
bool downloaded = false, downloadStarted = false, completed = false;
bool capturedThisFrame = false;
bool verifiedThisFrame = false;
bool mathVerifiedThisFrame = false;
bool deathRecorded = false;
bool setupFinished = false;
bool timingStarted = false, finishing = false, frameHadContext = false;
bool previousFrameEligible = false, foregroundBeforeFrame = false;
int menuFrames = 0, draws = 0, queryIndex = 0, activeQuery = -1, progressSecond = -1;
int deaths = 0, rendererInactiveFrames = 0, issuedQuery = -1;
bool continuousRendererSeen=false;
std::uint64_t continuousCommitted=0;
std::uint64_t continuousAtlasCalls=0;
double duration = 360, initialGameTime = 0;
double targetFrom = 0, targetTo = 100;
bool targeted = false, targetEntered = false, targetCompleted = false;
bool inTarget(PlayLayer* play) {
    if (!targeted) return true;
    if (!play) return false;
    const double percent = play->getCurrentPercent();
    return percent >= targetFrom && percent < targetTo;
}
double previousHarnessMs = 0;
uint64_t frameSequence = 0;
unsigned maximumAtlasRangeEnd = 0, maximumAtlasQuads = 0;
uint64_t atlasIndexOverflowDraws = 0;
double firstAtlasIndexOverflowPercent = -1;
Clock::time_point started, lastEnd, nextControlCheck;
std::ofstream report, frameTrace, gpuTrace;
Ref<CCLabelBMFont> fpsCounter;
Ref<CCLabelBMFont> gpuStatusCounter;
double counterMilliseconds = 0;
unsigned counterFrames = 0;
chlorium::test::FrameWatch frameWatch;
fs::path directory, stopRequestPath;
Ref<GJGameLevel> sourceLevel;
std::vector<double> cpu, intervals, gpu;
struct Segment {
    std::vector<double> cpu, intervals, gpu;
    size_t overBudget = 0;
} segments[10];
struct Window : Segment {
    size_t observed = 0, excluded = 0, unfocused = 0, paused = 0, dead = 0, noDraw = 0, noContext = 0;
    size_t captures = 0, boundaries = 0, gpuPending = 0;
    double percentFirst = -1, percentLast = 0, percentMin = 100, percentMax = 0;
    double gameTimeFirst = 0, gameTimeLast = 0;
    bool flushed = false, gpuDirty = false;
};
std::map<int, Window> windows;
struct Query {
    GLuint id[2]{};
    bool pending = false, measure = false;
    int segment = -1, second = -1;
    uint64_t frame = 0;
    double elapsed = 0, percent = 0;
} queries[32];
struct FrameSample {
    uint64_t frame;
    double elapsed, gameTime, percent, cpuMs, frameMs, rawFrameMs;
    double updateMs, visibilityMs, sceneMs;
    int segment, draws, foreground, context, paused, dead1, dead2, included;
    std::string reason;
    std::array<double, 8> stages{};
};
struct GpuSample { uint64_t frame; double elapsed, percent, ms; int second; };
std::vector<FrameSample> pendingFrames;
std::vector<GpuSample> pendingGpu;
bool gapActive = false;
double gapStart = 0, gapLast = 0;
size_t gapFrames = 0;
std::string gapReason;

void line(const std::string& text) { if (report) { report << text << '\n'; report.flush(); } }
bool matrixSnapshot(chlorium::matrix::Snapshot& result);
#ifdef CHLORIUM_EXTENDED_RESIDENT_STATS_R59
void recordResidentExperiments(HMODULE module,float percent){
    using Read=bool(*)(void*,std::size_t);
#ifdef CHLORIUM_RENDERER_TIMING_AUDIT_R60
    struct Timing {std::uint64_t version,count,frequency;std::array<std::uint64_t,13> ticks,calls;} timing{};
    const auto readTiming=reinterpret_cast<Read>(GetProcAddress(module,"ChloriumReadRendererTimingR60"));
    if(readTiming && readTiming(&timing,sizeof(timing)) && timing.version==60 && timing.count==13 && timing.frequency){
        constexpr const char* names[]{"batch_outside_atlas","atlas","collect","indices","prepare","program","sources","upload","materialize","backup","draw","native","batch_total"};
        auto row=matjson::Value::object();row["event"]="resident_timing_r60";row["percent"]=percent;row["frequency"]=timing.frequency;
        row["performance_comparison"]=false;auto ticks=matjson::Value::object(),calls=matjson::Value::object();
        for(unsigned n=0;n<13;++n){ticks[names[n]]=timing.ticks[n];calls[names[n]]=timing.calls[n];}
        row["ticks"]=std::move(ticks);row["calls"]=std::move(calls);line(row.dump());
    }
#endif
    struct Fast {std::uint64_t version,indexReads,indexBytes,indexReuses,nativePointBatches,suppressedNativeDraws,checkpointsSaved,preSubmitDeclines;} fast{};
    const auto readFast=reinterpret_cast<Read>(GetProcAddress(module,"ChloriumReadFastStatsR58"));
    if(readFast && readFast(&fast,sizeof(fast)) && fast.version==58){
        auto row=matjson::Value::object();row["event"]="resident_fast_r58";row["percent"]=percent;
        row["index_reads"]=fast.indexReads;row["index_bytes"]=fast.indexBytes;row["index_reuses"]=fast.indexReuses;
        row["native_point_batches"]=fast.nativePointBatches;row["suppressed_native_draws"]=fast.suppressedNativeDraws;
        row["checkpoints_saved"]=fast.checkpointsSaved;row["pre_submit_declines"]=fast.preSubmitDeclines;line(row.dump());
    }
    struct Upload {std::uint64_t version,skippedCalls,skippedBytes,declined;} upload{};
    const auto readUpload=reinterpret_cast<Read>(GetProcAddress(module,"ChloriumReadUploadDeferralStatsR59"));
    if(readUpload && readUpload(&upload,sizeof(upload)) && upload.version==59){
        auto row=matjson::Value::object();row["event"]="resident_upload_r59";row["percent"]=percent;
        row["deferred_calls"]=upload.skippedCalls;row["deferred_bytes"]=upload.skippedBytes;row["declined"]=upload.declined;line(row.dump());
    }
}
#endif
#ifdef CHLORIUM_CANONICAL_AUDIT_V10
bool canonicalCoreActive() {
    static const bool engine=[] {
        MODULEINFO info{};const auto game=GetModuleHandleW(nullptr);
        return GetModuleInformation(GetCurrentProcess(),game,&info,sizeof(info))&&
            chlorium::test::canonicalGameImage({static_cast<const unsigned char*>(info.lpBaseOfDll),info.SizeOfImage});
    }();
    if(!engine)return false;
    const bool native=chloriumAuditUseNative();
    const auto module=GetModuleHandleW(L"local.chlorium.dll");
    bool optimizedArea=!native;
    if(native) {if(module)return false;}
    else {
        if(!module)return false;
        const auto continuous=GetProcAddress(module,"chloriumContinuousIdentity");
        if(continuous){
            using Stats=chlorium::research::continuous::Stats;
            const auto read=reinterpret_cast<bool(*)(Stats*,std::size_t)>(GetProcAddress(module,"ChloriumReadContinuousStatsR57"));Stats value;
            if(std::strcmp(reinterpret_cast<const char*>(continuous),"CHLORIUM_RESIDENT_CONTINUOUS_R57") || !read ||
               !read(&value,sizeof(value)) || value.version!=57 || !value.enabled || value.fatal)return false;
            continuousRendererSeen=true;continuousCommitted=value.committed;continuousAtlasCalls=value.atlasCalls;
            optimizedArea=false;
        }else{
        const auto core=GetProcAddress(module,"chloriumCanonicalIdentity");
        const auto area=GetProcAddress(module,"chloriumCanonicalAreaIdentity");
        if(!core||!area||std::strcmp(reinterpret_cast<const char*>(core),"CHLORIUM_NATIVE_CANONICAL_BACKEND_V10")||
           std::strcmp(reinterpret_cast<const char*>(area),"CHLORIUM_CANONICAL_AREA_QUOTIENTS_V10"))return false;
        static const bool requireHash=[] {const auto value=std::getenv("CHLORIUM_REQUIRE_OPACITY_HASH_R36");return value&&std::string_view(value)=="1";}();
        if(requireHash){const auto active=reinterpret_cast<bool(*)()>(GetProcAddress(module,"ChloriumOpacityHashActiveR36"));if(!active||!active())return false;}
        }
    }
    const auto game=reinterpret_cast<const unsigned char*>(GetModuleHandleW(nullptr));
    return (_mm_getcsr()&0x1f80u)==0x1f80u&&chlorium::test::canonicalAreaBody(
        {game+chlorium::area::functionRva,chlorium::area::functionBytes},optimizedArea);
}
#endif
bool recordNativeSnapshotStats(float percent) {
#ifdef CHLORIUM_CANONICAL_AUDIT_V10
    const bool active=canonicalCoreActive();
    if(const auto module=GetModuleHandleW(L"local.chlorium.dll")){
        using Stats=chlorium::research::continuous::Stats;
        const auto read=reinterpret_cast<bool(*)(Stats*,std::size_t)>(GetProcAddress(module,"ChloriumReadContinuousStatsR57"));Stats value;
        if(read && read(&value,sizeof(value))){
#ifdef CHLORIUM_DIRECT_VERTEX_AUDIT_R65
            using Direct=chlorium::research::directvertices::Stats;Direct direct;
            const auto readDirect=reinterpret_cast<bool(*)(Direct*,std::size_t)>(GetProcAddress(module,"ChloriumReadDirectVertexStatsR65"));
            if(readDirect && readDirect(&direct,sizeof(direct))){
                auto row=matjson::Value::object();row["event"]="direct_vertex_pages_r65";row["percent"]=percent;
                row["version"]=direct.version;row["uploads"]=direct.uploads;row["bytes"]=direct.bytes;row["busy"]=direct.busy;
                row["fences"]=direct.fences;row["allocated"]=direct.allocated;row["fence_failures"]=direct.fenceFailures;line(row.dump());
            }
#endif
#ifdef CHLORIUM_EXTENDED_RESIDENT_STATS_R59
            recordResidentExperiments(module,percent);
#endif
            auto row=matjson::Value::object();row["event"]="resident_continuous_r57";row["percent"]=percent;
            row["active"]=active;row["frames"]=value.frames;row["atlas_calls"]=value.atlasCalls;
            row["gpu_submitted"]=value.submitted;row["gpu_committed"]=value.committed;row["native_fallbacks"]=value.nativeFallbacks;
            row["considered_quads"]=value.consideredQuads;row["committed_quads"]=value.committedQuads;
            row["reconstructed_quads"]=value.reconstructed;row["rollbacks"]=value.rollbacks;row["fatal"]=value.fatal;
            row["native_preparation_preserved"]=true;auto declines=matjson::Value::array();for(auto n:value.declines)declines.push(n);row["declines"]=std::move(declines);
            line(row.dump());return active && rendererInactiveFrames==0 && value.committed>0;
        }
    }
    line(fmt::format("{{\"event\":\"canonical_core_v10\",\"percent\":{},\"native\":{},\"active\":{},\"inactive_frames\":{}}}",
        percent,chloriumAuditUseNative(),active,rendererInactiveFrames));
    return active&&rendererInactiveFrames==0;
#endif
    using Stats = chlorium::snapshot::Stats;
    using Read = bool (*)(Stats*, std::size_t);
    static const auto read = [] {
        auto module = GetModuleHandleW(L"local.chlorium.dll");
        return module ? reinterpret_cast<Read>(GetProcAddress(module, "ChloriumReadNativeSnapshotStats")) : nullptr;
    }();
    Stats stats;
    const bool available = read && read(&stats, sizeof(stats)) && stats.version == 1;
    using Upload=chlorium::snapshot::UploadStatsV6;
    using ReadUpload=bool(*)(Upload*,std::size_t);
    static const auto readUpload=[] {
        auto module=GetModuleHandleW(L"local.chlorium.dll");
        return module?reinterpret_cast<ReadUpload>(GetProcAddress(module,"ChloriumReadAdaptiveUploadStatsV6")):nullptr;
    }();
    Upload upload;
    if(readUpload&&readUpload(&upload,sizeof(upload))&&upload.version==6)
        line(fmt::format("{{\"event\":\"adaptive_upload_v6\",\"percent\":{},\"direct\":{},\"seed\":{},\"sparse\":{},\"compared_records\":{},\"copied_records\":{},\"uploaded_records\":{}}}",
            percent,upload.direct,upload.seed,upload.sparse,upload.compared,upload.copied,upload.uploaded));
    using Transfer=chlorium::snapshot::TransferStatsV9;
    using ReadTransfer=bool(*)(Transfer*,std::size_t);
    static const auto readTransfer=[] {
        auto module=GetModuleHandleW(L"local.chlorium.dll");
        return module?reinterpret_cast<ReadTransfer>(GetProcAddress(module,"ChloriumReadTransferStatsV9")):nullptr;
    }();
    Transfer transfer;
    const bool transferAvailable=readTransfer&&readTransfer(&transfer,sizeof(transfer))&&transfer.version==9;
    if(transferAvailable)
        line(fmt::format("{{\"event\":\"persistent_transfer_v9\",\"percent\":{},\"uploads\":{},\"bytes\":{},\"native_fallbacks\":{},\"clean_draws\":{},\"busy_fallbacks\":{},\"fences\":{}}}",
            percent,transfer.uploads,transfer.bytes,transfer.nativeFallbacks,transfer.cleanDraws,transfer.busyFallbacks,transfer.fences));
    if (available)
        line(fmt::format("{{\"event\":\"native_snapshot_stats\",\"percent\":{},\"active\":{},\"frames\":{},\"sprites\":{},\"uploaded_bytes\":{},\"fallbacks\":{},\"capture_ms\":{}}}",
            percent, stats.active != 0, stats.frames, stats.sprites, stats.uploadedBytes, stats.fallbacks, stats.captureMilliseconds));
    bool hybrid=false;
#ifdef CHLORIUM_TRANSFER_HYBRID_AUDIT_V9
    hybrid=true;
#endif
    return !std::getenv("CHLORIUM_REQUIRE_NATIVE_SNAPSHOTS") ||
        chlorium::snapshot::validSubmissionStats(stats,available,transferAvailable?&transfer:nullptr,hybrid);
}
bool recordMathPatchState(float percent) {
    using namespace chlorium::native;
    static const auto requirement = std::getenv("CHLORIUM_REQUIRE_TARGETED_MATH_V3");
    static const bool required = requirement != nullptr;
    static const bool requireMatrix = required && std::strcmp(requirement, "pipeline") == 0;
    static const auto read = [] {
        auto module = GetModuleHandleW(L"local.chlorium.dll");
        return module ? reinterpret_cast<ReadMathPatchStateV1>(
            GetProcAddress(module, "ChloriumReadMathPatchStateV1")) : nullptr;
    }();
    MathPatchStateV1 state;
    if (!read || !read(&state, sizeof(state)) || state.version != 1) return !required;
    line(fmt::format("{{\"event\":\"native_math_patch_state\",\"percent\":{},\"revision\":{},\"requested_sites\":{},\"active_sites\":{},\"cache_enabled\":{},\"per_call_counters\":{},\"verification_stride\":{},\"ownership_lost\":{}}}",
        percent, state.revision, state.requestedSites, state.activeSites, state.cacheEnabled != 0,
        state.perCallCounters != 0, state.verificationStride, state.ownershipLost != 0));
    if (required && (state.revision != 3 || state.requestedSites != 4 ||
        state.activeSites != 4 || !state.cacheEnabled || state.ownershipLost)) return false;
    chlorium::matrix::Snapshot matrix;
    return !requireMatrix || (matrixSnapshot(matrix) && matrix.enabled && matrix.rotations.installedSites == 2);
}
void recordAffineStats(float percent) {
    using Stats = chlorium::affine::Stats;
    using Read = bool (*)(Stats*, std::size_t);
    static const auto read = [] {
        auto module = GetModuleHandleW(L"local.chlorium.dll");
        return module ? reinterpret_cast<Read>(GetProcAddress(module, "ChloriumReadAffineStats")) : nullptr;
    }();
    Stats stats;
    if (read && read(&stats, sizeof(stats)))
        line(fmt::format("{{\"event\":\"affine_point_stats\",\"percent\":{},\"calls\":{},\"compatibility_fallbacks\":{},\"installed\":{}}}",
            percent, stats.calls, stats.fallbacks, stats.installed));
}
bool matrixSnapshot(chlorium::matrix::Snapshot& result) {
    static const auto read = [] {
        auto module = GetModuleHandleW(L"local.chlorium.dll");
        if (!module) module = GetModuleHandleW(L"local.chlorium.audit.dll");
        return module ? reinterpret_cast<chlorium::matrix::ReadSnapshot>(
            GetProcAddress(module, "ChloriumReadMatrixSnapshotV1")) : nullptr;
    }();
    return read && read(&result, sizeof(result)) && result.version == 1;
}
chlorium::native::TransformMathStats transformMathStats() {
    static const auto read = [] {
        auto module = GetModuleHandleW(L"local.chlorium.dll");
        if (!module) module = GetModuleHandleW(L"local.chlorium.audit.dll");
        return module ? reinterpret_cast<chlorium::native::ReadTransformMathStats>(
            GetProcAddress(module, "ChloriumReadTransformMathStats")) : nullptr;
    }();
    chlorium::native::TransformMathStats result;
    if (read) read(&result, sizeof(result));
    return result;
}
chlorium::native::AtlasUploadStats atlasUploadStats() {
    using Read = bool (*)(chlorium::native::AtlasUploadStats*, std::size_t);
    static const auto read = [] {
        auto module = GetModuleHandleW(L"local.chlorium.dll");
        if (!module) module = GetModuleHandleW(L"local.chlorium.audit.dll");
        return module ? reinterpret_cast<Read>(GetProcAddress(module, "ChloriumReadAtlasUploadStats")) : nullptr;
    }();
    chlorium::native::AtlasUploadStats result;
    if (read && read(&result, sizeof(result))) return result;
    return chlorium::native::getAtlasUploadStats();
}
void stop(bool success, const char* reason) {
    line(fmt::format("{{\"event\":\"finished\",\"success\":{},\"reason\":{}}}", success, matjson::Value(reason).dump()));
    report.close();
    frameTrace.close(); gpuTrace.close();
    TerminateProcess(GetCurrentProcess(), success ? 0 : 2);
}
double elapsed() { return std::chrono::duration<double>(Clock::now() - started).count(); }
struct Distribution {
    double average = 0, stddev = 0, worst = 0, p50 = 0, p95 = 0, p99 = 0;
};
Distribution distribution(const std::vector<double>& values) {
    Distribution result;
    if (values.empty()) return result;
    result.average = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    for (const auto value : values) result.stddev += (value - result.average) * (value - result.average);
    result.stddev = std::sqrt(result.stddev / values.size());
    auto sorted = values;
    std::sort(sorted.begin(), sorted.end());
    auto percentile = [&](double p) { return sorted[static_cast<size_t>(p * (sorted.size() - 1))]; };
    result.p50 = percentile(.5); result.p95 = percentile(.95); result.p99 = percentile(.99);
    result.worst = sorted.back();
    return result;
}
int segmentFor(PlayLayer* play) {
    return std::clamp(static_cast<int>(play->getCurrentPercent() / 10), 0, 9);
}
void metrics(const char* event, int segment, const std::vector<double>& cpus,
             const std::vector<double>& frames, const std::vector<double>& gpus, const std::string& details = "") {
    const auto c = distribution(cpus), f = distribution(frames), g = distribution(gpus);
    const auto slow = std::count_if(frames.begin(), frames.end(), [](double ms) { return ms > 1000.0 / 60; });
    const auto overTarget = std::count_if(frames.begin(), frames.end(), [](double ms) { return ms > 1000.0 / targetFps; });
    line(fmt::format("{{\"event\":\"{}\",\"segment\":{},\"frames\":{},\"frame_samples\":{},\"gpu_samples\":{},\"fps_average\":{},"
        "\"cpu_ms_mean\":{},\"cpu_ms_p50\":{},\"cpu_ms_p95\":{},\"cpu_ms_p99\":{},\"cpu_ms_stddev\":{},\"cpu_ms_worst\":{},"
        "\"frame_ms_mean\":{},\"frame_ms_p50\":{},\"frame_ms_p95\":{},\"frame_ms_p99\":{},\"frame_ms_stddev\":{},\"frame_ms_worst\":{},"
        "\"gpu_ms_mean\":{},\"gpu_ms_p50\":{},\"gpu_ms_p95\":{},\"gpu_ms_p99\":{},\"gpu_ms_stddev\":{},\"gpu_ms_worst\":{},"
        "\"frames_over_60fps_budget\":{},\"target_fps\":{},\"frames_over_target_budget\":{}{} }}",
        event, segment, cpus.size(), frames.size(), gpus.size(), f.average > 0 ? 1000 / f.average : 0,
        c.average, c.p50, c.p95, c.p99, c.stddev, c.worst, f.average, f.p50, f.p95, f.p99, f.stddev, f.worst,
        g.average, g.p50, g.p95, g.p99, g.stddev, g.worst, slow, targetFps, overTarget, details));
}
bool foregroundIsGame() {
    DWORD process = 0;
    const auto window = GetForegroundWindow();
    if (!window || IsIconic(window)) return false;
    GetWindowThreadProcessId(window, &process);
    return process == GetCurrentProcessId();
}
bool playerTwoDead(PlayLayer* play) {
    return play->m_level && play->m_level->m_twoPlayerMode && play->m_player2 && play->m_player2->m_isDead;
}
void closeCoverageGap(double end) {
    if (!gapActive) return;
    line(fmt::format("{{\"event\":\"coverage_gap\",\"start_elapsed\":{},\"end_elapsed\":{},\"duration\":{},\"frames\":{},\"reason\":{}}}",
        gapStart, end, std::max(0.0, end - gapStart), gapFrames, matjson::Value(gapReason).dump()));
    gapActive = false;
}
void collectQuery(Query& query) {
    if (!query.pending || !glGetQueryObjectiv || !glGetQueryObjecti64v) return;
    GLint available = 0;
    glGetQueryObjectiv(query.id[1], GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available) return;
    GLint64 a = 0, b = 0;
    glGetQueryObjecti64v(query.id[0], GL_QUERY_RESULT, &a);
    glGetQueryObjecti64v(query.id[1], GL_QUERY_RESULT, &b);
    if (query.measure) {
        auto& window = windows[query.second];
        if (window.gpuPending) --window.gpuPending;
        window.gpuDirty = true;
        if (b >= a) {
            const double ms = (b - a) / 1e6;
            gpu.push_back(ms); segments[query.segment].gpu.push_back(ms); window.gpu.push_back(ms);
            pendingGpu.push_back({query.frame, query.elapsed, query.percent, ms, query.second});
        }
    }
    query.pending = false;
}
void flushTelemetry(bool final) {
    if (!timingStarted) return;
    const int currentSecond = static_cast<int>(elapsed());
    for (auto& [second, window] : windows) {
        if (window.flushed && !window.gpuDirty) continue;
        if (!final && second >= currentSecond) continue;
        const auto details = fmt::format(",\"window_start_sec\":{},\"window_end_sec\":{},\"elapsed\":{},\"partial\":{},\"native\":{},"
            "\"percent_first\":{},\"percent_last\":{},\"percent_min\":{},\"percent_max\":{},\"game_time_first\":{},\"game_time_last\":{},"
            "\"observed_frames\":{},\"excluded_frames\":{},\"unfocused_frames\":{},\"paused_frames\":{},\"dead_frames\":{},"
            "\"no_draw_frames\":{},\"no_context_frames\":{},\"capture_frames\":{},\"boundary_frames\":{},\"gpu_pending\":{}",
            second, second + 1, elapsed(), second >= currentSecond, chloriumAuditUseNative(),
            window.percentFirst, window.percentLast, window.percentMin, window.percentMax, window.gameTimeFirst, window.gameTimeLast,
            window.observed, window.excluded, window.unfocused, window.paused, window.dead, window.noDraw, window.noContext,
            window.captures, window.boundaries, window.gpuPending);
        metrics(window.flushed ? "window_gpu_update" : "window_metrics", -1, window.cpu, window.intervals, window.gpu, details);
        window.flushed = true; window.gpuDirty = false;
    }
    std::string batch;
    batch.reserve(pendingFrames.size() * 160);
    for (const auto& frame : pendingFrames)
        batch += fmt::format("{},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{},{},{},{},{},{},{},{},{},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f},{:.6f}\n",
            frame.frame, frame.elapsed, frame.gameTime, frame.percent, frame.cpuMs, frame.frameMs, frame.rawFrameMs,
            frame.updateMs, frame.visibilityMs, frame.sceneMs,
            frame.segment, frame.draws, frame.foreground, frame.context, frame.paused, frame.dead1, frame.dead2, frame.included, frame.reason,
            frame.stages[0], frame.stages[1], frame.stages[2], frame.stages[3],
            frame.stages[4], frame.stages[5], frame.stages[6], frame.stages[7]);
    frameTrace.write(batch.data(), batch.size()); frameTrace.flush(); pendingFrames.clear();
    batch.clear();
    for (const auto& sample : pendingGpu)
        batch += fmt::format("{},{:.6f},{:.6f},{},{:.6f}\n", sample.frame, sample.elapsed, sample.percent, sample.second, sample.ms);
    gpuTrace.write(batch.data(), batch.size()); gpuTrace.flush(); pendingGpu.clear();
}
bool recordFrame(PlayLayer* play, Clock::time_point before, Clock::time_point after) {
    const double time = elapsed();
    const double work = std::chrono::duration<double,std::milli>(after - before).count();
    const double rawInterval = std::chrono::duration<double,std::milli>(after - lastEnd).count();
    if (fpsCounter && rawInterval > 0) {
        counterMilliseconds += rawInterval; ++counterFrames;
        if (counterMilliseconds >= 250) {
            const double fps = counterFrames * 1000.0 / counterMilliseconds;
            fpsCounter->setString(fmt::format("FPS: {:.0f}", fps).c_str());
            fpsCounter->setColor(fps >= targetFps ? ccColor3B{150,255,150} :
                fps >= targetFps / 2 ? ccColor3B{255,215,100} : ccColor3B{255,120,120});
            if(gpuStatusCounter){
                static std::uint64_t priorCalls=0,priorCommitted=0;
                const auto calls=continuousAtlasCalls-priorCalls,committed=continuousCommitted-priorCommitted;
                const double share=calls?100.0*double(committed)/double(calls):0;
                gpuStatusCounter->setString(calls?fmt::format("{} : {:.0f}% LOTS",committed?"GPU ACTIF":"REPLI NATIF",share).c_str():"GPU : EN ATTENTE");
                gpuStatusCounter->setColor(committed?ccColor3B{150,255,150}:ccColor3B{255,190,100});
                priorCalls=continuousAtlasCalls;priorCommitted=continuousCommitted;
            }
            counterMilliseconds = 0; counterFrames = 0;
        }
    }
    // Reach the section through normal gameplay so prior trigger state is
    // preserved. Outside it, skip telemetry, queries and synchronous captures.
    if (!inTarget(play)) { previousFrameEligible = false; return false; }
    if (targeted && !targetEntered) {
        targetEntered = true;
        line(fmt::format("{{\"event\":\"target_enter\",\"elapsed\":{},\"percent\":{},\"from\":{},\"to\":{}}}",
            time, play->getCurrentPercent(), targetFrom, targetTo));
    }
    const double interval = std::max(0.0, rawInterval - previousHarnessMs);
    const bool foreground = foregroundBeforeFrame && foregroundIsGame();
    const bool dead1 = play->m_player1 && play->m_player1->m_isDead, dead2 = playerTwoDead(play);
    const bool paused = play->m_isPaused;
    std::string reason;
    auto exclude = [&](bool condition, const char* text) {
        if (condition) { if (!reason.empty()) reason += '|'; reason += text; }
    };
    exclude(time < 2, "warmup"); exclude(!foreground, "unfocused"); exclude(paused, "paused");
    exclude(dead1 || dead2, "dead"); exclude(draws == 0, "no_draw"); exclude(!frameHadContext, "no_context");
    exclude(capturedThisFrame, "capture");
    exclude(verifiedThisFrame, "gpu_verification");
    exclude(mathVerifiedThisFrame, "exact_math_verification");
    exclude(frameWatch.enabled(), "visual_diagnostic");
#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
    exclude(true, "native_pass_capture_diagnostic");
#endif
#ifdef CHLORIUM_EXTERNAL_SAMPLING_AUDIT
    exclude(chloriumExternalSamplingSessionMode==0x53414d50u,"external_sampling");
#endif
#ifdef CHLORIUM_OPACITY_PREFIX_SHADOW
    exclude(true, "opacity_shadow_diagnostic");
#endif
    const bool baseEligible = reason.empty();
    const bool boundary = baseEligible && !previousFrameEligible;
    exclude(boundary, "coverage_boundary");
    const bool included = reason.empty();
    previousFrameEligible = baseEligible;
    const int segment = segmentFor(play), second = static_cast<int>(time);
    auto& window = windows[second];
    const double percent = play->getCurrentPercent(), gameTime = play->m_gameState.m_totalTime;
    if (!window.observed) { window.percentFirst = percent; window.gameTimeFirst = gameTime; }
    window.percentLast = percent; window.gameTimeLast = gameTime;
    window.percentMin = std::min(window.percentMin, percent); window.percentMax = std::max(window.percentMax, percent);
    ++window.observed; window.excluded += !included; window.unfocused += !foreground; window.paused += paused;
    window.dead += dead1 || dead2; window.noDraw += draws == 0; window.noContext += !frameHadContext;
    window.captures += capturedThisFrame; window.boundaries += boundary;
    if (included) {
        cpu.push_back(work); intervals.push_back(rawInterval);
        segments[segment].cpu.push_back(work); segments[segment].intervals.push_back(rawInterval);
        window.cpu.push_back(work); window.intervals.push_back(rawInterval);
        closeCoverageGap(time);
    } else {
        if (gapActive && reason != gapReason) closeCoverageGap(time);
        if (!gapActive) { gapActive = true; gapStart = time; gapFrames = 0; gapReason = reason; }
        gapLast = time; ++gapFrames;
    }
    if (issuedQuery >= 0) {
        auto& query = queries[issuedQuery]; query.measure = included;
        query.second = second; query.elapsed = time; query.percent = percent; query.segment = segment;
        if (included) ++window.gpuPending;
    }
    pendingFrames.push_back({frameSequence, time, gameTime, percent, work, interval, rawInterval,
        BProfiler::currentMilliseconds("GJBaseGameLayer::update"),
        BProfiler::currentMilliseconds("PlayLayer::updateVisibility"),
        BProfiler::currentMilliseconds("CCDirector::drawScene"),
        segment, draws, foreground, frameHadContext, paused, dead1, dead2, included, std::move(reason),
        {BProfiler::currentMilliseconds("commands"), BProfiler::currentMilliseconds("move"),
         BProfiler::currentMilliseconds("rotation"), BProfiler::currentMilliseconds("transform"),
         BProfiler::currentMilliseconds("area"), BProfiler::currentMilliseconds("follow"),
         BProfiler::currentMilliseconds("collisions"), BProfiler::currentMilliseconds("particles")}});
    return included;
}
void finish(bool fullLevel, const char* reason) {
#ifdef CHLORIUM_AREA_WORK_CENSUS
    ChloriumFlushAreaWorkCensus();
#endif
    if (finishing) return;
    finishing = true;
    frameWatch.finish();
    if (wglGetCurrentContext()) for (auto& query : queries) collectQuery(query);
    closeCoverageGap(elapsed()); flushTelemetry(true);
    metrics("metrics", -1, cpu, intervals, gpu);
    for (int i = 0; i < 10; ++i) metrics("segment_metrics", i, segments[i].cpu, segments[i].intervals, segments[i].gpu);
    size_t observed = 0, excluded = 0, pending = 0;
    for (const auto& [second, window] : windows) { observed += window.observed; excluded += window.excluded; pending += window.gpuPending; }
    line(fmt::format("{{\"event\":\"coverage\",\"completed\":{},\"targeted\":{},\"target_completed\":{},\"target_from\":{},\"target_to\":{},\"deaths\":{},\"observed_frames\":{},\"excluded_frames\":{},\"gpu_pending\":{},\"renderer_inactive_frames\":{},\"cpu_profile\":{}}}",
        fullLevel, targeted, targetCompleted, targetFrom, targetTo, deaths, observed, excluded, pending, rendererInactiveFrames, matjson::Value(BProfiler::toString()).dump()));
    const bool graphicsVerified = atlasUploadStats().verificationFailures == 0;
    const bool mathVerified = transformMathStats().failures == 0;
    const bool targetedMathActive = recordMathPatchState(PlayLayer::get() ? PlayLayer::get()->getCurrentPercent() : -1.f);
    recordAffineStats(PlayLayer::get() ? PlayLayer::get()->getCurrentPercent() : -1.f);
    const bool snapshotsActive = recordNativeSnapshotStats(PlayLayer::get() ? PlayLayer::get()->getCurrentPercent() : -1.f);
    const bool visualVerified = frameWatch.coverageComplete();
    bool passCaptureVerified = true;
#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
    passCaptureVerified = chlorium::research::pass::finish();
#endif
    stop((fullLevel || targetCompleted) && deaths == 0 && graphicsVerified && mathVerified && visualVerified && targetedMathActive && snapshotsActive && passCaptureVerified,
        deaths != 0 ? "Run contains deaths; paired performance/visual validation rejected" :
        !passCaptureVerified ? "Native pass capture is incomplete; no replacement coverage proof is valid" :
        !snapshotsActive ? "Native snapshot renderer did not stay active; no GPU batching performance verdict is valid" :
        !targetedMathActive ? "Required targeted math V3 is unavailable, inactive or changed owner; no candidate performance verdict is valid" :
        !visualVerified ? "Visual readback failed or coverage is incomplete; no black-screen verdict is valid" :
        !mathVerified ? "Exact native transform math verification failed" :
        graphicsVerified ? reason : "GPU byte verification failed; native upload repaired the frame");
}
class Download final : public LevelDownloadDelegate {
public:
    void levelDownloadFinished(GJGameLevel* level) override {
        if (!level || level->m_levelID.value() != allowedLevel || level->m_levelString.empty()) {
            stop(false, "Unexpected or empty level download"); return;
        }
        sourceLevel = level;
        fs::create_directories(directory / "references");
        std::ofstream data(directory / "references" / fmt::format("level-{}.level", allowedLevel), std::ios::binary);
        data.write(level->m_levelString.data(), level->m_levelString.size()); data.close();
        std::ofstream metadata(directory / "references" / fmt::format("level-{}.json", allowedLevel));
        metadata << fmt::format("{{\"id\":{},\"name\":{},\"song_id\":{},\"audio_track\":{},\"data_bytes\":{}}}",
            allowedLevel, matjson::Value(std::string(level->m_levelName.c_str())).dump(), level->m_songID,
            level->m_audioTrack, level->m_levelString.size()); metadata.close();
        downloaded = true;
        line(fmt::format("{{\"event\":\"downloaded\",\"level\":{},\"name\":{},\"bytes\":{}}}",
            allowedLevel, matjson::Value(std::string(level->m_levelName.c_str())).dump(), level->m_levelString.size()));
        if (std::getenv("CHLORIUM_AUDIT_FETCH_ID")) stop(true, "Authorized auto level downloaded");
    }
    void levelDownloadFailed(int response) override {
        line(fmt::format("{{\"event\":\"download_failed\",\"response\":{}}}", response));
        stop(false, "Auto level download failed");
    }
} download;

class BeginNode : public CCNode {
    void draw() override {
        if (!inTarget(PlayLayer::get())) { activeQuery = -1; return; }
        frameHadContext = wglGetCurrentContext() != nullptr;
        auto& query = queries[queryIndex]; activeQuery = -1;
        if (!frameHadContext || !glQueryCounter || !glGetQueryObjectiv || !glGetQueryObjecti64v) return;
        if (query.pending) {
            collectQuery(query);
            if (query.pending) return;
        }
        if (!query.id[0]) glGenQueries(2, query.id);
        query.measure = false; query.frame = frameSequence;
        glQueryCounter(query.id[0], GL_TIMESTAMP); activeQuery = queryIndex;
    }
};
class EndNode : public CCNode {
    void draw() override {
        ++draws;
        frameHadContext = wglGetCurrentContext() != nullptr;
        if (activeQuery >= 0) {
            auto& query = queries[activeQuery]; glQueryCounter(query.id[1], GL_TIMESTAMP); query.pending = true;
            issuedQuery = activeQuery;
            queryIndex = (queryIndex + 1) % 32; activeQuery = -1;
        }
        // Do not read a framebuffer from inside the object-layer traversal.
        // Native effects can render into an intermediate/multisampled target;
        // the former unvalidated reference read could generate GL errors and
        // poison the first presented-frame read. FrameWatch is the sole image
        // collector, after final composition, with validated PBO/FBO state.

    }
};
void tick() {
    if (!initialized) {
        initialized = true;
        const auto path = std::getenv("CHLORIUM_AUDIT_OUTPUT");
        if (!path) return;
        report.open(path, std::ios::trunc); directory = fs::path(path).parent_path();
        frameWatch.open(std::string(path), [](const std::string& event) { line(event); });
        stopRequestPath = std::string(path) + ".stop-request";
        frameTrace.open(std::string(path) + ".frames.csv", std::ios::trunc);
        gpuTrace.open(std::string(path) + ".gpu.csv", std::ios::trunc);
        frameTrace << "frame,elapsed,game_time,percent,cpu_ms,frame_ms,raw_frame_ms,update_ms,visibility_ms,scene_ms,segment,draws,foreground,context,paused,dead1,dead2,included,reason,commands_ms,move_ms,rotation_ms,transform_ms,area_ms,follow_ms,collisions_ms,particles_ms\n";
        gpuTrace << "frame,elapsed,percent,second,gpu_ms\n";
        if (!selectAuthorizedLevel(std::getenv("CHLORIUM_BENCHMARK_LEVEL"))) {
            stop(false, "Level not in the user-authorized allowlist"); return;
        }
        if (auto requested = std::getenv("CHLORIUM_AUDIT_FETCH_ID"); requested && std::string(requested) != std::to_string(allowedLevel)) {
            stop(false, "Conflicting selected and fetch level"); return;
        }
        if (auto seconds = std::getenv("CHLORIUM_BENCHMARK_SECONDS")) duration = std::clamp(std::atof(seconds), 30.0, 900.0);
        auto parsePercent = [](const char* name, double& value) {
            const auto text = std::getenv(name);
            if (!text) return true;
            char* end = nullptr; value = std::strtod(text, &end);
            return end != text && !*end && std::isfinite(value);
        };
        if (!parsePercent("CHLORIUM_TARGET_FROM", targetFrom) || !parsePercent("CHLORIUM_TARGET_TO", targetTo) ||
            targetFrom < 0 || targetTo > 100 || targetFrom >= targetTo) {
            stop(false, "Invalid targeted percentage range"); return;
        }
        targeted = targetFrom > 0 || targetTo < 100;
        line(fmt::format("{{\"event\":\"target_window\",\"enabled\":{},\"from\":{},\"to\":{},\"native_gameplay_before_window\":true}}",
            targeted, targetFrom, targetTo));
        line(fmt::format("{{\"event\":\"started\",\"level\":{},\"native\":{},\"seconds_limit\":{},\"collisions\":\"native\",\"noclip\":false}}",
            allowedLevel, chloriumAuditUseNative(), duration));

    }
    if (!report) return;
    if (!running && ready && ++menuFrames > 60) {
        static bool variantChecked = false;
        if (!variantChecked) {
            variantChecked = true;
            matjson::Value modInventory = matjson::Value::array();
            for (auto mod : Loader::get()->getAllMods()) {
                matjson::Value item = matjson::Value::object();
                item["id"] = mod->getID(); item["version"] = fmt::format("{}", mod->getVersion());
                item["loaded"] = mod->isLoaded();
                modInventory.push(std::move(item));
                for (auto hook : mod->getHooks()) {
                    const auto& name = hook->getDisplayName();
                    if (name.find("CCTextureAtlas::") == std::string::npos &&
                        !chlorium::test::nativeProducerObserverName(name) &&
                        name.find("processTransformActions") == std::string::npos &&
                        name.find("updateVisibility") == std::string::npos &&
                        name.find("ShaderLayer::") == std::string::npos &&
                        name.find("handleButton") == std::string::npos) continue;
                    line(fmt::format("{{\"event\":\"compatibility_hook\",\"mod\":{},\"name\":{},\"enabled\":{}}}",
                        matjson::Value(mod->getID()).dump(), matjson::Value(name).dump(), hook->isEnabled()));
                }
            }
            line(fmt::format("{{\"event\":\"mod_inventory\",\"mods\":{}}}", modInventory.dump(matjson::NO_INDENTATION)));
            const bool userMods = std::getenv("CHLORIUM_EXPECTED_USER_MODS") != nullptr;
            if (userMods) {
                std::stringstream expected(std::getenv("CHLORIUM_EXPECTED_USER_MODS"));
                std::string id;
                while (std::getline(expected, id, ',')) {
                    const auto mod = Loader::get()->getLoadedMod(id);
                    if (!mod || !mod->isLoaded()) {
                        line(fmt::format("{{\"event\":\"missing_user_mod\",\"id\":{}}}", matjson::Value(id).dump()));
                        stop(false, "An enabled user mod did not load; full-stack comparison is unavailable"); return;
                    }
                }
            }
            line(fmt::format("{{\"event\":\"mod_profile\",\"name\":\"{}\",\"expected_mods_verified\":true}}",
                userMods ? "user-mods" : "isolated"));
#ifdef CHLORIUM_MEASUREMENT_ONLY
        auto production = Loader::get()->getLoadedMod("local.chlorium");
        const bool productionEnabled = production && production->getSettingValue<bool>("enabled");
        line(fmt::format("{{\"event\":\"comparison_variant\",\"measurement_only\":true,\"production_loaded\":{},\"production_enabled\":{},\"production_version\":{}}}",
            production != nullptr, productionEnabled,
            matjson::Value(production ? fmt::format("{}", production->getVersion()) : "absent").dump()));
        if ((production != nullptr) != !chloriumAuditUseNative() || (production && !productionEnabled)) {
            stop(false, "Production mod presence/setting does not match requested comparison"); return;
        }
        const auto atlasDraw = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(L"libcocos2d.dll"),
            "?drawNumberOfQuads@CCTextureAtlas@cocos2d@@QEAAXII@Z"));
        constexpr bool captureProfile =
#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
            true;
#else
            false;
#endif
        const auto producerFlag=std::getenv("CHLORIUM_NATIVE_PRODUCER_CAPTURE");const bool producerCapture=producerFlag&&std::string_view(producerFlag)=="1";
        for (auto hook : Mod::get()->getHooks()) {
            if(hook->isEnabled() && chlorium::test::nativeProducerObserverName(hook->getDisplayName()) &&
               !chlorium::test::nativePassObserverAllowed(captureProfile,chloriumAuditUseNative(),production!=nullptr,hook->getDisplayName(),producerCapture)){
                stop(false,"Producer hooks became active outside their isolated diagnostic");return;
            }
            if (hook->isEnabled() && hook->getAddress() == atlasDraw) {
                if (chlorium::test::nativePassObserverAllowed(captureProfile, chloriumAuditUseNative(),
                        production != nullptr, hook->getDisplayName(), producerCapture)) {
                    line("{\"event\":\"native_pass_observer_allowed\",\"performance_comparison\":false}");
                    continue;
                }
                stop(false, "Measurement module must not hook atlas draws or disable production optimization"); return;
            }
        }
#endif
        }
        if (std::getenv("CHLORIUM_ATLAS_STORAGE_REGRESSION")) {
            auto module = GetModuleHandleW(L"libcocos2d.dll");
            auto remap = reinterpret_cast<void(*)(CCTextureAtlas*)>(GetProcAddress(module,
                "?mapBuffers@CCTextureAtlas@cocos2d@@AEAAXXZ"));
            if (!wglGetCurrentContext() || !glGetBufferSubData ||
                reinterpret_cast<uintptr_t>(remap) != reinterpret_cast<uintptr_t>(module) + 0xa69c0) {
                stop(false, "Atlas storage regression unavailable on this engine"); return;
            }
            const unsigned char white[]{255,255,255,255};
            auto texture = new CCTexture2D;
            if (!texture->initWithData(white, kCCTexture2DPixelFormat_RGBA8888, 1, 1, {1,1})) {
                texture->release(); stop(false, "Regression texture creation failed"); return;
            }
            auto atlas = CCTextureAtlas::createWithTexture(texture, 64); texture->release();
            if (!atlas) { stop(false, "Regression atlas creation failed"); return; }
            std::memset(atlas->m_pQuads, 0, 64 * sizeof(ccV3F_C4B_T2F_Quad));
            auto bytes = reinterpret_cast<unsigned char*>(atlas->m_pQuads);
            bytes[12] = 77; atlas->m_uTotalQuads = 64; atlas->m_bDirty = true;
            atlas->drawNumberOfQuads(64, 0); // Degenerate vertices: no visible pixels.
            auto readColor = [&] {
                GLint previous; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous);
                glBindBuffer(GL_ARRAY_BUFFER, atlas->m_pBuffersVBO[0]);
                unsigned char value = 0; glGetBufferSubData(GL_ARRAY_BUFFER, 12, 1, &value);
                glBindBuffer(GL_ARRAY_BUFFER, previous); return value;
            };
            const auto first = readColor();
            bytes[12] = 102; remap(atlas); const auto remapped = readColor();
            bytes[12] = 77; atlas->m_bDirty = true; atlas->drawNumberOfQuads(64, 0);
            const auto restored = readColor();
            line(fmt::format("{{\"event\":\"atlas_storage_regression\",\"initial_gpu_byte\":{},\"remapped_gpu_byte\":{},\"final_gpu_byte\":{},\"expected_final\":77}}",
                first, remapped, restored));
            stop(first == 77 && remapped == 102 && restored == 77,
                "GPU storage replacement must invalidate a shadow even when buffer IDs and capacity stay equal"); return;
        }
        if (!downloaded) {
            if (downloadStarted) return;
            downloadStarted = true;
            auto manager = GameLevelManager::get();
            auto saved = manager->getSavedLevel(allowedLevel);
            if (saved && !saved->m_levelString.empty()) download.levelDownloadFinished(saved);
            else {
                manager->m_levelDownloadDelegate = &download;
                manager->downloadLevel(allowedLevel, false, 0);
                line(fmt::format("{{\"event\":\"downloading\",\"level\":{}}}", allowedLevel));
            }
            return;
        }
        if (loading) return;
        loading = true;
        using SwapInterval = BOOL(WINAPI*)(int);
        using GetSwapInterval = int(WINAPI*)();
        auto resolveWgl = [](const char* name) {
            const auto proc = wglGetProcAddress(name);
            const auto address = reinterpret_cast<std::uintptr_t>(proc);
            return address > 3 && address != UINTPTR_MAX ? proc : nullptr;
        };
        const auto swap = reinterpret_cast<SwapInterval>(resolveWgl("wglSwapIntervalEXT"));
        const auto getSwap = reinterpret_cast<GetSwapInterval>(resolveWgl("wglGetSwapIntervalEXT"));
        if (!wglGetCurrentContext() || !swap || !getSwap || !swap(0) || getSwap() != 0) {
            stop(false, "Cannot verify disabled VSync for the isolated 120 FPS campaign"); return;
        }
        auto settings = GameManager::get();
        settings->m_customFPSTarget = static_cast<float>(targetFps);
        settings->setGameVariable("0116", true);
        settings->updateCustomFPS();
        const auto interval = CCDirector::get()->getAnimationInterval();
        const bool pacingVerified = std::isfinite(interval) && std::abs(interval - 1.0 / targetFps) < 1e-6;
        line(fmt::format("{{\"event\":\"frame_pacing\",\"target_fps\":{},\"interval_seconds\":{},\"swap_interval\":{},\"verified\":{}}}",
            targetFps, interval, getSwap(), pacingVerified));
        if (!pacingVerified) { stop(false, "Native 120 FPS limiter was not applied"); return; }
        auto level = GJGameLevel::create();
        level->copyLevelInfo(sourceLevel);
        // Preserve gameplay compatibility switches as well as level data.
        // A default-created level can otherwise take the legacy physics path.
        const auto preserveSourceId=std::getenv("CHLORIUM_RESEARCH_SOURCE_LEVEL_ID");
        const bool sourceIdentity=preserveSourceId&&std::string_view(preserveSourceId)=="1";
        level->m_levelID = sourceIdentity?static_cast<int>(sourceLevel->m_levelID):0;
        level->m_dontSave = true;
        line(fmt::format("{{\"event\":\"level_identity\",\"preserve_source_id\":{},\"source_id\":{},\"effective_id\":{},\"dont_save\":true}}",sourceIdentity,static_cast<int>(sourceLevel->m_levelID),static_cast<int>(level->m_levelID)));
        level->m_levelName = sourceLevel->m_levelName;
        level->m_levelString = sourceLevel->m_levelString;
        level->m_songID = sourceLevel->m_songID;
        level->m_audioTrack = sourceLevel->m_audioTrack;
        level->m_gameVersion = sourceLevel->m_gameVersion;
        level->m_levelVersion = sourceLevel->m_levelVersion;
        level->m_levelLength = sourceLevel->m_levelLength;
        level->m_levelType = sourceLevel->m_levelType;
        level->m_twoPlayerMode = sourceLevel->m_twoPlayerMode;
        level->m_autoLevel = sourceLevel->m_autoLevel;
        level->m_platformerSeed = sourceLevel->m_platformerSeed;
        level->m_songIDs = sourceLevel->m_songIDs;
        level->m_sfxIDs = sourceLevel->m_sfxIDs;
        level->m_lowDetailModeToggled = false;
        level->m_disableShakeToggled = true;
        line(fmt::format("{{\"event\":\"compatibility\",\"game_version\":{},\"level_version\":{},\"two_player\":{},\"auto\":{},\"max_deaths\":2}}",
            level->m_gameVersion, level->m_levelVersion, level->m_twoPlayerMode, level->m_autoLevel));
        line(fmt::format("{{\"event\":\"loading\",\"level\":{}}}", allowedLevel));
        auto scene = PlayLayer::scene(level, false, false);
        if (!scene) { stop(false, "Auto scene creation failed"); return; }
        CCDirector::get()->replaceScene(scene); CCDirector::get()->setNextDeltaTimeZero(true);
        running = true; started = Clock::now(); lastEnd = started;
        BProfiler::setEnabled(true); BProfiler::useAverages(true);
        line("{\"event\":\"loaded\"}");
        return;
    }
    if (!running) return;
    auto play = PlayLayer::get();
    if (!play || !play->m_objectLayer || !play->m_player1) { finish(false, "Auto level left; partial data saved"); return; }
    if (!setupFinished) return;
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene || !play->getParent()) {
        if (timingStarted || elapsed() >= 60)
            finish(false, "Native scene cleared or PlayLayer detached; no complete gameplay test");
        return;
    }
    if (!scene->getChildByTag(0x43484c42)) {
        line(fmt::format("{{\"event\":\"test_quality\",\"low_detail_requested\":false,\"disable_shake_requested\":true,\"low_detail_effective\":{},\"skip_camera_shake_effective\":{}}}",
            play->m_lowDetailMode, play->m_skipCameraShake));
        if (play->m_lowDetailMode || !play->m_skipCameraShake) {
            finish(false, "Full detail / no shake test settings were not applied"); return;
        }
        // Observe the final scene, after the level's shader/post-effect pass.
        auto begin = new BeginNode; begin->init(); begin->autorelease(); scene->addChild(begin, INT_MIN, 0x43484c42);
        auto end = new EndNode; end->init(); end->autorelease(); scene->addChild(end, INT_MAX);
        fpsCounter = CCLabelBMFont::create("FPS: --", "bigFont.fnt");
        fpsCounter->setScale(.35f); fpsCounter->setAnchorPoint({1,0});
        fpsCounter->setPosition({CCDirector::get()->getWinSize().width - 8, 8});
        scene->addChild(fpsCounter, INT_MAX);
        const auto hashFlag=std::getenv("CHLORIUM_REQUIRE_OPACITY_HASH_R36");
        const auto residentFlag=std::getenv("CHLORIUM_RESIDENT_NATIVE_REPLAY");
        const bool residentDiagnostic=chloriumAuditUseNative()&&residentFlag&&std::string_view(residentFlag)=="1";
        const auto continuousModule=GetModuleHandleW(L"local.chlorium.dll");
        const bool continuous=continuousModule && GetProcAddress(continuousModule,"chloriumContinuousIdentity");
        const auto label=continuous?
#ifdef CHLORIUM_DIRECT_VERTEX_AUDIT_R65
            (GetProcAddress(continuousModule,"chloriumDirectVertexPagesIdentity")?"CHLORIUM R65 / DIRECT":"CHLORIUM R61 / CONTROLE R65"):
#else
#ifdef CHLORIUM_EXTENDED_RESIDENT_STATS_R59
            (GetProcAddress(continuousModule,"chloriumGroupTraceIdentity")?"CHLORIUM R61 + TRACE R63":GetProcAddress(continuousModule,"chloriumNativeStreamDirectIdentity")?"CHLORIUM R61 / DIAGNOSTIC":GetProcAddress(continuousModule,"chloriumRendererTimingIdentity")?"CHLORIUM R60 / DIAGNOSTIC":GetProcAddress(continuousModule,"chloriumUploadDeferralIdentity")?"AVEC CHLORIUM GPU R59":"AVEC CHLORIUM GPU R57"):
#else
            "AVEC CHLORIUM GPU CONTINU":
#endif
#endif
            residentDiagnostic?"DIAG CHLORIUM / NATIF + GPU":chloriumAuditUseNative()?"SANS CHLORIUM":hashFlag&&std::string_view(hashFlag)=="1"?"AVEC CHLORIUM R36":"AVEC CHLORIUM REF";
        auto modeLabel = CCLabelBMFont::create(label, "bigFont.fnt");
        modeLabel->setScale(.3f); modeLabel->setAnchorPoint({1,0});
        modeLabel->setPosition({CCDirector::get()->getWinSize().width - 8, 29});
        modeLabel->setColor(chloriumAuditUseNative() ? ccColor3B{180,215,255} : ccColor3B{160,255,190});
        scene->addChild(modeLabel, INT_MAX);
        auto qualityLabel = CCLabelBMFont::create("LDM OFF / SHAKE OFF", "bigFont.fnt");
        qualityLabel->setScale(.22f); qualityLabel->setAnchorPoint({1,0});
        qualityLabel->setPosition({CCDirector::get()->getWinSize().width - 8, 46});
        scene->addChild(qualityLabel, INT_MAX);
        if(continuous){
            gpuStatusCounter=CCLabelBMFont::create("GPU : INITIALISATION", "bigFont.fnt");
            gpuStatusCounter->setScale(.22f);gpuStatusCounter->setAnchorPoint({1,0});
            gpuStatusCounter->setPosition({CCDirector::get()->getWinSize().width-8,64});
            scene->addChild(gpuStatusCounter,INT_MAX);
        }
        if (std::getenv("CHLORIUM_EXPECTED_USER_MODS")) {
            auto profileLabel = CCLabelBMFont::create("MES MODS CHARGES", "bigFont.fnt");
            profileLabel->setScale(.22f); profileLabel->setAnchorPoint({1,0});
            profileLabel->setPosition({CCDirector::get()->getWinSize().width - 8, 60});
            scene->addChild(profileLabel, INT_MAX);
        }
        line(fmt::format("{{\"event\":\"test_overlay\",\"label\":\"{}\",\"fps\":true}}",
            label));
        initialGameTime = play->m_gameState.m_totalTime;
        started = Clock::now(); lastEnd = started;
        timingStarted = true; nextControlCheck = started;
        maximumAtlasRangeEnd = maximumAtlasQuads = 0; atlasIndexOverflowDraws = 0;
        firstAtlasIndexOverflowPercent = -1;
        line(fmt::format("{{\"event\":\"level_objects\",\"level\":{},\"objects\":{}}}",
            allowedLevel, play->m_objects ? play->m_objects->count() : 0));
        GLint viewport[4]{}; glGetIntegerv(GL_VIEWPORT, viewport);
        line(fmt::format("{{\"event\":\"graphics_context\",\"width\":{},\"height\":{},\"vendor\":{},\"renderer\":{},\"version\":{}}}",
            viewport[2], viewport[3], matjson::Value(reinterpret_cast<const char*>(glGetString(GL_VENDOR))).dump(),
            matjson::Value(reinterpret_cast<const char*>(glGetString(GL_RENDERER))).dump(),
            matjson::Value(reinterpret_cast<const char*>(glGetString(GL_VERSION))).dump()));
        return;
    }
    // Do not call drawScene manually: Cocos may advance the scheduler there.
    // The engine owns both simulation timing and presentation in this test.
    if (inTarget(play)) glFlush();
    if (play->m_player1->m_isDead || playerTwoDead(play)) {
        if (!deathRecorded) {
            deathRecorded = true; ++deaths;
            line(fmt::format("{{\"event\":\"death\",\"attempt_deaths\":{},\"elapsed\":{},\"game_time\":{},\"percent\":{},\"player_x\":{},\"player_y\":{}}}",
                deaths, elapsed(), play->m_gameState.m_totalTime, play->getCurrentPercent(),
                play->m_player1->getPositionX(), play->m_player1->getPositionY()));
            if (deaths >= 2) { finish(false, "Two deaths with normal collisions; full level not reached"); return; }
        }
    } else deathRecorded = false;
    bool gpuActive = false, cpuOptimizations = false;
#ifdef CHLORIUM_CANONICAL_AUDIT_V10
    if(!canonicalCoreActive())++rendererInactiveFrames;
    static std::uint64_t previousContinuousCommitted=0;
    if(continuousRendererSeen)gpuActive=continuousCommitted>previousContinuousCommitted;
    previousContinuousCommitted=continuousCommitted;
#endif
#ifdef CHLORIUM_EXPERIMENTAL_STATIC_RENDERER
    auto ren = Renderer::get();
    gpuActive = ren && ren->isEnabled(); cpuOptimizations = ren && ren->useOptimizations();
    if (!chloriumAuditUseNative() && (!ren || !ren->isEnabled())) ++rendererInactiveFrames;
#endif
    const double time = elapsed();
    const int second = static_cast<int>(time);
    if (second != progressSecond) {
        progressSecond = second;
        flushTelemetry(false);
        chlorium::matrix::Snapshot matrix;
        if (matrixSnapshot(matrix)) {
            const auto& m = matrix.matrices;
            const auto& r = matrix.rotations;
            const auto& p = matrix.persistence;
            line(fmt::format("{{\"event\":\"matrix_reuse\",\"available\":true,\"elapsed\":{},\"percent\":{},\"enabled\":{},\"installed_sites\":{},\"phases\":{},\"eligible\":{},\"matrix_hits\":{},\"matrix_misses\":{},\"unsupported\":{},\"transform_phases\":{},\"area_phases\":{},\"rotation_hits\":{},\"rotation_misses\":{},\"zero_rotations\":{},\"native_calls\":{},\"cooldown_windows\":{},\"recent_matrix_hits\":{},\"recent_rotation_hits\":{},\"invalidations\":{},\"skipped_transform_phases\":{},\"skipped_area_phases\":{}}}",
                time, play->getCurrentPercent(), matrix.enabled != 0, r.installedSites,
                m.phases, m.eligible, m.hits, m.misses, m.unsupported, r.transformPhases, r.areaPhases,
                r.rotationHits, r.rotationMisses, r.zeroRotations, r.nativeCalls, r.lowReusePhases,
                p.recentMatrixHits, p.recentRotationHits, p.invalidations, p.skippedTransformPhases, p.skippedAreaPhases));
        } else {
            line(fmt::format("{{\"event\":\"matrix_reuse\",\"available\":false,\"elapsed\":{},\"percent\":{}}}",
                time, play->getCurrentPercent()));
        }
        const auto math = transformMathStats();
        if (!recordMathPatchState(play->getCurrentPercent())) {
            finish(false, "Required targeted math V3 stopped being active"); return;
        }
        recordAffineStats(play->getCurrentPercent());
        recordNativeSnapshotStats(play->getCurrentPercent());
        line(fmt::format("{{\"event\":\"native_transform_math\",\"elapsed\":{},\"percent\":{},\"installed\":{},\"phases\":{},\"calls\":{},\"hits\":{},\"misses\":{},\"bypassed\":{},\"low_reuse_phases\":{},\"verified\":{},\"failures\":{}}}",
            time, play->getCurrentPercent(), math.installed, math.phases, math.calls, math.hits,
            math.misses, math.bypassed, math.lowReusePhases, math.verified, math.failures));
#ifdef CHLORIUM_EXPERIMENTAL_NATIVE_MOVES
        const auto movement = chlorium::native::getNativeMovementStats();
        line(fmt::format("{{\"event\":\"native_movement\",\"elapsed\":{},\"fast_calls\":{},\"object_visits\":{},\"avoided_section_calls\":{},\"fallback_calls\":{}}}",
            time, movement.fastCalls, movement.objectVisits, movement.avoidedSectionCalls, movement.fallbackCalls));
#endif
        line(fmt::format("{{\"event\":\"atlas_index_limits\",\"elapsed\":{},\"max_quads\":{},\"max_range_end\":{},\"overflow_draws\":{},\"first_overflow_percent\":{}}}",
            time, maximumAtlasQuads, maximumAtlasRangeEnd, atlasIndexOverflowDraws, firstAtlasIndexOverflowPercent));
        PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb = sizeof(memory);
        const bool processMemoryValid = GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory));
        MEMORYSTATUSEX systemMemory{}; systemMemory.dwLength = sizeof(systemMemory);
        const bool systemMemoryValid = GlobalMemoryStatusEx(&systemMemory);
        FILETIME created{}, exited{}, kernel{}, user{};
        const bool cpuTimeValid = GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
        auto ticks = [](FILETIME time) { return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime; };
        line(fmt::format("{{\"event\":\"resources\",\"elapsed\":{},\"percent\":{},\"process_memory_valid\":{},\"system_memory_valid\":{},\"cpu_time_valid\":{},\"private_bytes\":{},\"working_set_bytes\":{},\"page_faults\":{},\"available_physical_bytes\":{},\"cpu_user_ms\":{},\"cpu_kernel_ms\":{}}}",
            time, play->getCurrentPercent(), processMemoryValid, systemMemoryValid, cpuTimeValid,
            memory.PrivateUsage, memory.WorkingSetSize, memory.PageFaultCount, systemMemory.ullAvailPhys,
            ticks(user) / 10000.0, ticks(kernel) / 10000.0));
        const auto atlas = atlasUploadStats();
        line(fmt::format("{{\"event\":\"atlas_cost_limits\",\"elapsed\":{},\"percent\":{},\"oversized_fallbacks\":{},\"budget_fallbacks\":{},\"cached_bytes\":{}}}",
            time, play->getCurrentPercent(), atlas.oversizedFallbacks, atlas.budgetFallbacks, atlas.cachedBytes));
        line(fmt::format("{{\"event\":\"atlas_adaptive\",\"cooldown_draws\":{}}}", atlas.cooldownDraws));
        line(fmt::format("{{\"event\":\"atlas_identity_changes\",\"quad_pointer\":{},\"context\":{},\"vertex_buffer\":{},\"index_buffer\":{},\"capacity\":{}}}",
            atlas.identityChanges[0], atlas.identityChanges[1], atlas.identityChanges[2], atlas.identityChanges[3], atlas.identityChanges[4]));
        line(fmt::format("{{\"event\":\"atlas_cache_debug\",\"post_native_draws\":{},\"captures\":{},\"allocation_failures\":{},\"invalidations\":[{},{},{},{},{},{},{},{}]}}",
            atlas.postNativeDraws, atlas.cacheCaptures, atlas.cacheAllocationFailures,
            atlas.invalidations[0], atlas.invalidations[1], atlas.invalidations[2], atlas.invalidations[3],
            atlas.invalidations[4], atlas.invalidations[5], atlas.invalidations[6], atlas.invalidations[7]));
        line(fmt::format("{{\"event\":\"atlas_uploads\",\"elapsed\":{},\"draws\":{},\"native_uploads\":{},\"partial_uploads\":{},\"unchanged_uploads\":{},\"bytes_uploaded\":{},\"bytes_avoided\":{},\"cached_bytes\":{},\"partial_range_draws\":{},\"verified_uploads\":{},\"verification_failures\":{},\"eligible_uploads\":{},\"cache_rejected_memory\":{},\"cache_rejected_buffer\":{}}}",
            time, atlas.draws, atlas.nativeUploads, atlas.partialUploads, atlas.unchangedUploads,
            atlas.bytesUploaded, atlas.bytesAvoided, atlas.cachedBytes, atlas.partialRangeDraws,
            atlas.verifiedUploads, atlas.verificationFailures, atlas.eligibleUploads,
            atlas.cacheRejectedMemory, atlas.cacheRejectedBuffer));
        line(fmt::format("{{\"event\":\"progress\",\"elapsed\":{},\"game_time\":{},\"percent\":{},\"player_x\":{},\"player_y\":{},\"gpu_active\":{},\"cpu_optimizations\":{}}}",
            time, play->m_gameState.m_totalTime, play->getCurrentPercent(), play->m_player1->getPositionX(), play->m_player1->getPositionY(),
            gpuActive, cpuOptimizations));
    }
    if (Clock::now() >= nextControlCheck) {
        nextControlCheck = Clock::now() + std::chrono::seconds(1);
        std::error_code error;
        if (fs::exists(stopRequestPath, error)) { finish(false, "Stopped by user; partial data saved"); return; }
    }
    if (targeted && targetEntered && play->getCurrentPercent() >= targetTo &&
        !play->m_player1->m_isDead && !playerTwoDead(play)) {
        targetCompleted = true;
        finish(false, "Requested percentage section reached; full level completion is not claimed");
    }
    else if (completed || play->m_hasCompletedLevel) finish(true, "Auto level reached its normal completion");
    else if (time >= duration) finish(false, "Time limit reached before normal level completion");
}
}
class $modify(AutoBenchmarkMenu, MenuLayer) {
    bool init() { if (!MenuLayer::init()) return false; ready = true; return true; }
};
class $modify(AutoBenchmarkPresentation, CCEGLView) {
    void swapBuffers() {
        // The default back buffer is the actual image about to be presented.
        // Scene traversal can still be inside an intermediate shader target.
        if (running && setupFinished && PlayLayer::get() && targeted) {
            const auto percent=PlayLayer::get()->getCurrentPercent();
            if(percent>=targetFrom-.5 && percent<targetFrom) frameWatch.observeBeforeTarget(frameSequence,percent);
        }
        if (running && timingStarted && PlayLayer::get() && inTarget(PlayLayer::get())) {
            const auto capturesBefore = frameWatch.captureCount();
            frameWatch.draw(frameSequence, PlayLayer::get()->getCurrentPercent(),
                PlayLayer::get()->m_gameState.m_totalTime, elapsed());
            capturedThisFrame = capturedThisFrame || frameWatch.captureCount() != capturesBefore;
        }
        CCEGLView::swapBuffers();
    }
};
#ifndef CHLORIUM_MEASUREMENT_ONLY
class $modify(AutoBenchmarkAtlasLimits, CCTextureAtlas) {
    void drawNumberOfQuads(unsigned n, unsigned start) {
        if (running && timingStarted && n) {
            maximumAtlasQuads = std::max(maximumAtlasQuads, n);
            if (start <= std::numeric_limits<unsigned>::max() - n) {
                maximumAtlasRangeEnd = std::max(maximumAtlasRangeEnd, start + n);
                if (start + n > 16384) {
                    ++atlasIndexOverflowDraws;
                    if (firstAtlasIndexOverflowPercent < 0 && PlayLayer::get())
                        firstAtlasIndexOverflowPercent = PlayLayer::get()->getCurrentPercent();
                }
            }
        }
        CCTextureAtlas::drawNumberOfQuads(n, start);
    }
};
#endif
class $modify(AutoBenchmarkSceneProfiler, CCDirector) {
    void drawScene() {
        auto timer = BProfiler::start("CCDirector::drawScene");
        CCDirector::drawScene();
        timer.end();
    }
};
// Measurement only: preserve arguments, return values and every native call.
// These coarse stages identify expensive updates without per-object hooks.
class $modify(AutoBenchmarkUpdateProfiler, GJBaseGameLayer) {
#ifndef CHLORIUM_EXPERIMENTAL_STATIC_RENDERER
    void update(float dt) {
        auto timer = BProfiler::start("GJBaseGameLayer::update");
        GJBaseGameLayer::update(dt); timer.end();
    }
#endif
    void processCommands(float dt, bool half, bool last) {
        auto timer = BProfiler::start("commands");
        GJBaseGameLayer::processCommands(dt, half, last); timer.end();
    }
    void processMoveActions() {
        auto timer = BProfiler::start("move");
        GJBaseGameLayer::processMoveActions(); timer.end();
    }
    void processRotationActions() {
        auto timer = BProfiler::start("rotation");
        GJBaseGameLayer::processRotationActions(); timer.end();
    }
    void processTransformActions(bool visible) {
        auto timer = BProfiler::start("transform");
        GJBaseGameLayer::processTransformActions(visible); timer.end();
    }
    void processAreaActions(float dt, bool visible) {
        auto timer = BProfiler::start("area");
        GJBaseGameLayer::processAreaActions(dt, visible); timer.end();
    }
    void processFollowActions() {
        auto timer = BProfiler::start("follow");
        GJBaseGameLayer::processFollowActions(); timer.end();
    }
    int checkCollisions(PlayerObject* player, float dt, bool ignoreDamage) {
        auto timer = BProfiler::start("collisions");
        const auto result = GJBaseGameLayer::checkCollisions(player, dt, ignoreDamage); timer.end();
        return result;
    }
    void updateParticles(float dt) {
        auto timer = BProfiler::start("particles");
        GJBaseGameLayer::updateParticles(dt); timer.end();
    }
};
class $modify(AutoBenchmarkDirector, CCDisplayLinkDirector) {
    void mainLoop() {
        static const bool verifyUploads = std::getenv("CHLORIUM_VERIFY_NATIVE_UPLOADS") != nullptr;
        static const bool verifyMath = std::getenv("CHLORIUM_TRANSFORM_VERIFY") != nullptr;
        const auto verifiedBefore = verifyUploads ? atlasUploadStats().verifiedUploads : 0;
        const auto mathBefore = verifyMath ? transformMathStats().verified : 0;
        const auto before = Clock::now(); draws = 0; capturedThisFrame = false; verifiedThisFrame = false;
        mathVerifiedThisFrame = false;
        frameHadContext = false; issuedQuery = -1; ++frameSequence;
        foregroundBeforeFrame = foregroundIsGame();
        if (running) BProfiler::beginFrameSampling(!targeted || inTarget(PlayLayer::get()));
#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
        {
            chlorium::research::pass::PreserveFP preserve;
            auto play = PlayLayer::get();
            const bool eligible = running && timingStarted && play && !play->m_isPaused &&
                play->m_player1 && !play->m_player1->m_isDead && !playerTwoDead(play) && inTarget(play);
            chlorium::research::pass::beginFrame(eligible, frameSequence, eligible ? play->getCurrentPercent() : -1);
        }
#endif
        CCDisplayLinkDirector::mainLoop();
#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
        chlorium::research::pass::endFrame();
#endif
        const auto after = Clock::now();
        verifiedThisFrame = verifyUploads && atlasUploadStats().verifiedUploads != verifiedBefore;
        mathVerifiedThisFrame = verifyMath && transformMathStats().verified != mathBefore;
        bool includeProfileFrame = false;
        if (running && timingStarted && PlayLayer::get())
            includeProfileFrame = recordFrame(PlayLayer::get(), before, after);
        // Accumulate before tick(), which may finish and terminate this process.
        // Use exactly the same eligibility decision as the per-frame CSV.
        if (running) BProfiler::frameEnd(includeProfileFrame);
        lastEnd = after;
        tick();
        previousHarnessMs = std::chrono::duration<double,std::milli>(Clock::now() - after).count();
    }
};
class $modify(AutoBenchmarkPlay, PlayLayer) {
#ifndef CHLORIUM_EXPERIMENTAL_STATIC_RENDERER
    void updateVisibility(float dt) {
        auto timer = BProfiler::start("PlayLayer::updateVisibility");
#ifdef CHLORIUM_OPACITY_PREFIX_SHADOW
        double shadowPercent = -1;
        bool shadowEligible = false;
        if (running && setupFinished && !finishing && chlorium::research::opacity::nativeOpacityShadowPending() &&
            (_mm_getcsr() & 0x1f80u) == 0x1f80u) {
            chlorium::research::opacity::SavedOpacityFPState preserve;
            shadowPercent = getCurrentPercent();
            shadowEligible = !targeted || (shadowPercent >= targetFrom && shadowPercent < targetTo);
        }
        chlorium::research::opacity::NativeOpacityShadowScope shadow(shadowEligible, frameSequence, shadowPercent);
#endif
        PlayLayer::updateVisibility(dt); timer.end();
#ifdef CHLORIUM_OPACITY_PREFIX_SHADOW
        shadow.finish();
#endif
    }
#endif
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        setupFinished = true;
#ifdef CHLORIUM_NATIVE_PASS_CAPTURE
        chlorium::research::pass::prepareLevel(this);
#endif
    }
    void levelComplete() {
        PlayLayer::levelComplete();
        if (running) completed = true;
    }
};
// No collision/damage/death hook and no input automation. The auto level owns
// all movement. Only persistent save writes are suppressed for this private run.
class $modify(AutoBenchmarkSaveGuard, GManager) {
    void save() { if (!std::getenv("CHLORIUM_AUDIT_OUTPUT")) GManager::save(); }
};
