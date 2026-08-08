#include <android/log.h>
#include <dlfcn.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <atomic>
#include <cstdlib>
#include <mutex>

#define LOG_TAG "FPSBooster"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using EGLSwapBuffersFn = EGLBoolean (*)(EGLDisplay, EGLSurface);

static EGLSwapBuffersFn real_eglSwapBuffers = nullptr;
static std::once_flag init_once;
static std::atomic<bool> initialized{false};

namespace swappy {
using FnVoidBool = void (*)(bool);
using FnVoidInt64 = void (*)(int64_t);
using FnVoidWindow = void (*)(ANativeWindow*);
using FnBool = bool (*)(void);

static FnVoidBool enableFramePacing = nullptr;
static FnVoidBool enableBlockingWait = nullptr;
static FnVoidBool setAutoSwapInterval = nullptr;
static FnVoidBool setUseAffinity = nullptr;
static FnVoidBool setBufferStuffingFixWait = nullptr;
static FnVoidInt64 setMaxAutoSwapIntervalNS = nullptr;
static FnVoidInt64 setSwapIntervalNS = nullptr;
static FnVoidWindow setWindow = nullptr;
static FnBool isEnabled = nullptr;
static FnVoidBool resetFramePacing = nullptr;
static FnVoidBool enableStats = nullptr;
static FnVoidBool clearStats = nullptr;

static void* resolve(const char* name) {
    void* sym = dlsym(RTLD_DEFAULT, name);
    if (!sym) ALOGW("Missing symbol: %s", name);
    return sym;
}

static void init() {
    enableFramePacing = reinterpret_cast<FnVoidBool>(resolve("SwappyGL_enableFramePacing"));
    enableBlockingWait = reinterpret_cast<FnVoidBool>(resolve("SwappyGL_enableBlockingWait"));
    setAutoSwapInterval = reinterpret_cast<FnVoidBool>(resolve("SwappyGL_setAutoSwapInterval"));
    setUseAffinity = reinterpret_cast<FnVoidBool>(resolve("SwappyGL_setUseAffinity"));
    setBufferStuffingFixWait = reinterpret_cast<FnVoidBool>(resolve("SwappyGL_setBufferStuffingFixWait"));
    setMaxAutoSwapIntervalNS = reinterpret_cast<FnVoidInt64>(resolve("SwappyGL_setMaxAutoSwapIntervalNS"));
    setSwapIntervalNS = reinterpret_cast<FnVoidInt64>(resolve("SwappyGL_setSwapIntervalNS"));
    setWindow = reinterpret_cast<FnVoidWindow>(resolve("SwappyGL_setWindow"));
    isEnabled = reinterpret_cast<FnBool>(resolve("SwappyGL_isEnabled"));
    resetFramePacing = reinterpret_cast<FnVoidBool>(resolve("SwappyGL_resetFramePacing"));
    enableStats = reinterpret_cast<FnVoidBool>(resolve("SwappyGL_enableStats"));
    clearStats = reinterpret_cast<FnVoidBool>(resolve("SwappyGL_clearStats"));
}

static void configure() {
    if (enableFramePacing) enableFramePacing(true);
    if (enableBlockingWait) enableBlockingWait(false);
    if (setAutoSwapInterval) setAutoSwapInterval(true);
    if (setUseAffinity) setUseAffinity(true);
    if (setBufferStuffingFixWait) setBufferStuffingFixWait(true);
    if (enableStats) enableStats(false);
    if (clearStats) clearStats(true);

    // Default to one refresh interval if the app does not already manage timing well.
    // You can override this with FPS_BOOSTER_SWAP_NS.
    const char* swapNsEnv = std::getenv("FPS_BOOSTER_SWAP_NS");
    if (swapNsEnv && *swapNsEnv) {
        long long ns = atoll(swapNsEnv);
        if (setSwapIntervalNS) setSwapIntervalNS(ns);
    }
    const char* maxSwapNsEnv = std::getenv("FPS_BOOSTER_MAX_SWAP_NS");
    if (maxSwapNsEnv && *maxSwapNsEnv) {
        long long ns = atoll(maxSwapNsEnv);
        if (setMaxAutoSwapIntervalNS) setMaxAutoSwapIntervalNS(ns);
    }
}

} // namespace swappy

static void init_once_fn() {
    ALOGI("Initializing FPS booster scaffold");
    swappy::init();
    swappy::configure();

    const char* force30 = std::getenv("FPS_BOOSTER_FORCE_30");
    if (force30 && *force30) {
        ALOGI("FPS_BOOSTER_FORCE_30=1; set your loader to target 33,333,333 ns if desired");
    }

    initialized.store(true, std::memory_order_release);
}

static EGLBoolean hooked_eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    if (!initialized.load(std::memory_order_acquire)) {
        std::call_once(init_once, init_once_fn);
    }

    // The goal is frame pacing stability, not bypassing the game's renderer.
    // Keep the call chain intact.
    return real_eglSwapBuffers ? real_eglSwapBuffers(display, surface) : EGL_FALSE;
}

__attribute__((constructor))
static void on_load() {
    std::call_once(init_once, []() {
        void* handle = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
        if (!handle) {
            ALOGW("dlopen(libEGL.so) failed: %s", dlerror());
        }
        real_eglSwapBuffers = reinterpret_cast<EGLSwapBuffersFn>(dlsym(RTLD_NEXT, "eglSwapBuffers"));
        if (!real_eglSwapBuffers && handle) {
            real_eglSwapBuffers = reinterpret_cast<EGLSwapBuffersFn>(dlsym(handle, "eglSwapBuffers"));
        }
        if (!real_eglSwapBuffers) {
            ALOGW("Failed to resolve eglSwapBuffers; hook will be inactive");
        } else {
            ALOGI("eglSwapBuffers hooked");
        }
    });
}

extern "C" EGLBoolean eglSwapBuffers(EGLDisplay display, EGLSurface surface) {
    if (!initialized.load(std::memory_order_acquire)) {
        std::call_once(init_once, init_once_fn);
    }
    return hooked_eglSwapBuffers(display, surface);
}
