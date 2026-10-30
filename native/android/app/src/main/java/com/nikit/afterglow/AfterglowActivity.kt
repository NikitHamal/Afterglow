package com.nikit.afterglow

import android.app.NativeActivity
import android.os.Bundle
import android.view.View
import android.view.WindowManager

/**
 * Thin NativeActivity host: hands the app's writable dirs to the engine, then lets raylib run
 * (raylib provides android_main, which calls the engine's main()).
 */
class AfterglowActivity : NativeActivity() {
    companion object {
        init {
            System.loadLibrary("afterglow")
        }

        @JvmStatic
        external fun nativeSetFilesDir(filesDir: String, cacheDir: String)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        nativeSetFilesDir(filesDir.absolutePath, cacheDir.absolutePath)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        super.onCreate(savedInstanceState)
        hideSystemUi()
    }

    override fun onBackPressed() {
        // keep the night alive: going back leaves the app in the background, the autosave protects it
        moveTaskToBack(true)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemUi()
    }

    private fun hideSystemUi() {
        window.decorView.systemUiVisibility =
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
                View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
    }
}
