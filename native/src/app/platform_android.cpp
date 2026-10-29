// Android platform services: the files dir handed over by the activity, and the touch gestures the mouse-driven
// engine needs (a two-finger pinch stands in for the wheel; the left mouse press comes from the raylib patch).
#if defined(__ANDROID__)
#include "app/platform.h"
#include "core/paths.h"
#include "raylib.h"
#include <jni.h>
#include <cmath>

extern "C" JNIEXPORT void JNICALL
Java_com_nikit_afterglow_AfterglowActivity_nativeSetFilesDir(JNIEnv* env, jclass, jstring files) {
    if (!files) return;
    const char* f = env->GetStringUTFChars(files, nullptr);
    if (f) {
        ag::paths::setUserDirOverride(f);
        env->ReleaseStringUTFChars(files, f);
    }
}

namespace ag::platform {

float wheel() {
    static double lastTime = -1.0;
    static float cached = 0.f;
    static float lastDist = -1.f;
    double now = GetTime();
    if (now == lastTime) return cached;   // one value per frame, however many callers ask
    lastTime = now;
    cached = 0.f;
    int n = GetTouchPointCount();
    if (n >= 2) {
        Vector2 a = GetTouchPosition(0), b = GetTouchPosition(1);
        float d = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
        if (lastDist > 0.f) cached = (d - lastDist) * 0.02f;   // fingers apart = wheel forward = zoom in
        lastDist = d;
    } else lastDist = -1.f;
    return cached;
}

}  // namespace ag::platform
#endif
