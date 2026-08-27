#include "ui_sdl_android.h"

#include <jni.h>
#include <SDL2/SDL_opengles2.h>
#include <SDL2/SDL_system.h>
#include <libavcodec/jni.h>

#include "platform.h"

static jclass video_class;
static jobject video_surface;
static jmethodID create_surface_method;
static jmethodID update_texture_method;
static jmethodID destroy_surface_method;
static jmethodID set_brightness_method;

static int handle_jni_exception(JNIEnv *env, const char *operation)
{
    if (!(*env)->ExceptionCheck(env)) {
        return 0;
    }

    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    vpilog("Android video surface: %s failed\n", operation);
    return -1;
}

static int init_jni(JNIEnv *env)
{
    if (video_class) {
        return 0;
    }

    jobject activity = (jobject) SDL_AndroidGetActivity();
    if (!activity) {
        vpilog("SDL_AndroidGetActivity returned NULL\n");
        return -1;
    }

    jclass local_class = (*env)->GetObjectClass(env, activity);
    (*env)->DeleteLocalRef(env, activity);
    if (!local_class || handle_jni_exception(env, "looking up activity class") < 0) {
        return -1;
    }

    video_class = (jclass) (*env)->NewGlobalRef(env, local_class);
    (*env)->DeleteLocalRef(env, local_class);
    if (!video_class) {
        vpilog("Android video surface: could not retain activity class\n");
        return -1;
    }

    create_surface_method = (*env)->GetStaticMethodID(env, video_class, "createVideoSurface", "(III)Landroid/view/Surface;");
    update_texture_method = (*env)->GetStaticMethodID(env, video_class, "updateVideoSurfaceTexture", "()[F");
    destroy_surface_method = (*env)->GetStaticMethodID(env, video_class, "destroyVideoSurface", "()V");
    set_brightness_method = (*env)->GetStaticMethodID(env, video_class, "setScreenBrightness", "(F)Z");
    if (!create_surface_method || !update_texture_method || !destroy_surface_method || !set_brightness_method ||
        handle_jni_exception(env, "looking up video surface methods") < 0) {
        (*env)->DeleteGlobalRef(env, video_class);
        video_class = NULL;
        return -1;
    }

    JavaVM *vm = NULL;
    if ((*env)->GetJavaVM(env, &vm) != JNI_OK ||
        av_jni_set_java_vm(vm, NULL) < 0) {
        vpilog("Android video surface: could not register the Java VM with FFmpeg\n");
        (*env)->DeleteGlobalRef(env, video_class);
        video_class = NULL;
        create_surface_method = NULL;
        update_texture_method = NULL;
        destroy_surface_method = NULL;
        set_brightness_method = NULL;
        return -1;
    }

    return 0;
}

int vui_sdl_android_create_video_texture(SDL_Renderer *renderer, int width, int height, SDL_Texture **texture)
{
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
    SDL_RendererInfo info;
    GLint texture_name = 0;
    SDL_Texture *external_texture = NULL;

    if (!env || !renderer || !texture || init_jni(env) < 0) {
        goto die;
    }

    if (SDL_GetRendererInfo(renderer, &info) < 0) {
        vpilog("Failed to determine SDL Android renderer info\n");
        goto die;
    }

    if (SDL_strcmp(info.name, "opengles2") != 0) {
        vpilog("Android video decoding requires opengles2 (currently using %s)\n", info.name);
        goto die;
    }

    external_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_EXTERNAL_OES, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!external_texture) {
        vpilog("Failed to create Android external texture: %s\n", SDL_GetError());
        goto die;
    }

    SDL_RenderFlush(renderer);
    if (SDL_GL_BindTexture(external_texture, NULL, NULL) < 0) {
        vpilog("Failed to bind Android external texture: %s\n", SDL_GetError());
        goto die_and_destroy_texture;
    }

    glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &texture_name);
    SDL_GL_UnbindTexture(external_texture);
    if (!texture_name) {
        vpilog("Failed to retrieve GL object from Android external texture\n");
        goto die_and_destroy_texture;
    }

    jobject local_surface = (*env)->CallStaticObjectMethod(env, video_class, create_surface_method, (jint) texture_name, (jint) width, (jint) height);
    if (handle_jni_exception(env, "creating SurfaceTexture") < 0 || !local_surface) {
        goto die_and_destroy_texture;
    }

    video_surface = (*env)->NewGlobalRef(env, local_surface);
    (*env)->DeleteLocalRef(env, local_surface);
    if (!video_surface) {
        (*env)->CallStaticVoidMethod(env, video_class, destroy_surface_method);
        handle_jni_exception(env, "destroying unretained SurfaceTexture");
        goto die_and_destroy_texture;
    }

    *texture = external_texture;
    vpilog("Android hardware video decode surface initialized\n");
    return 0;

die_and_destroy_texture:
    SDL_DestroyTexture(external_texture);

die:
    return -1;
}

void *vui_sdl_android_get_video_surface(void)
{
    return video_surface;
}

int vui_sdl_android_set_brightness(float brightness)
{
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
    if (!env || init_jni(env) < 0) {
        return -1;
    }

    jboolean dispatched = (*env)->CallStaticBooleanMethod(env, video_class, set_brightness_method,
                                                          (jfloat) brightness);
    if (handle_jni_exception(env, "setting screen brightness") < 0) {
        return -1;
    }
    return dispatched ? 0 : -1;
}

int vui_sdl_android_update_video_texture(float transform[16])
{
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
    if (!env || !video_class || !video_surface) {
        return 0;
    }

    jfloatArray matrix = (jfloatArray) (*env)->CallStaticObjectMethod(env, video_class, update_texture_method);
    if (handle_jni_exception(env, "updating SurfaceTexture") < 0 || !matrix) {
        return 0;
    }

    if ((*env)->GetArrayLength(env, matrix) != 16) {
        (*env)->DeleteLocalRef(env, matrix);
        vpilog("Android video surface returned an invalid transform matrix\n");
        return 0;
    }

    (*env)->GetFloatArrayRegion(env, matrix, 0, 16, transform);
    (*env)->DeleteLocalRef(env, matrix);
    if (handle_jni_exception(env, "reading SurfaceTexture transform") < 0) {
        return 0;
    }
    return 1;
}

void vui_sdl_android_destroy_video_texture(void)
{
    JNIEnv *env = (JNIEnv *) SDL_AndroidGetJNIEnv();
    if (!env) {
        return;
    }

    if (video_class && destroy_surface_method) {
        (*env)->CallStaticVoidMethod(env, video_class, destroy_surface_method);
        handle_jni_exception(env, "destroying SurfaceTexture");
    }
    if (video_surface) {
        (*env)->DeleteGlobalRef(env, video_surface);
        video_surface = NULL;
    }
    if (video_class) {
        (*env)->DeleteGlobalRef(env, video_class);
        video_class = NULL;
    }
    create_surface_method = NULL;
    update_texture_method = NULL;
    destroy_surface_method = NULL;
    set_brightness_method = NULL;
}
