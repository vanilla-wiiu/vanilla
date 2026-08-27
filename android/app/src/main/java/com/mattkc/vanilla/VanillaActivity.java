package com.mattkc.vanilla;

import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.WindowManager;

import java.util.concurrent.atomic.AtomicBoolean;

import org.libsdl.app.SDLActivity;

public class VanillaActivity extends SDLActivity {
	private static SurfaceTexture videoSurfaceTexture;
	private static Surface videoSurface;
	private static final AtomicBoolean videoFrameAvailable = new AtomicBoolean(false);
	private static final float[] videoTransform = new float[16];

	@Override
	protected String[] getLibraries() {
		return new String[] {
			"SDL2",
			"SDL2_image",
			"SDL2_ttf",
			"xml2",
			"vanilla"
		};
	}

	public static synchronized Surface createVideoSurface(int textureName, int width, int height) {
		destroyVideoSurface();

		videoSurfaceTexture = new SurfaceTexture(textureName);
		videoSurfaceTexture.setDefaultBufferSize(width, height);
		videoSurfaceTexture.setOnFrameAvailableListener(surfaceTexture -> videoFrameAvailable.set(true));
		videoSurface = new Surface(videoSurfaceTexture);
		return videoSurface;
	}

    // Called from SDL render thread where owning GLES context is current
	public static synchronized float[] updateVideoSurfaceTexture() {
		if (videoSurfaceTexture == null || !videoFrameAvailable.getAndSet(false)) {
			return null;
		}

		videoSurfaceTexture.updateTexImage();
		videoSurfaceTexture.getTransformMatrix(videoTransform);
		return videoTransform;
	}

	public static synchronized void destroyVideoSurface() {
		videoFrameAvailable.set(false);
		if (videoSurface != null) {
			videoSurface.release();
			videoSurface = null;
		}
		if (videoSurfaceTexture != null) {
			videoSurfaceTexture.release();
			videoSurfaceTexture = null;
		}
	}

	public static boolean setScreenBrightness(float brightness) {
		final VanillaActivity activity = (VanillaActivity) mSingleton;
		if (activity == null) {
			return false;
		}

		activity.runOnUiThread(() -> {
			WindowManager.LayoutParams attributes = activity.getWindow().getAttributes();
			attributes.screenBrightness = Math.max(0.0f, Math.min(1.0f, brightness));
			activity.getWindow().setAttributes(attributes);
		});
		return true;
	}
}
