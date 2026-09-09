#include "ui_sdl_videotoolbox.h"

#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <simd/simd.h>

#include <libavutil/pixfmt.h>
#include <string.h>

#include "platform.h"

typedef struct {
    vector_float4 red;
    vector_float4 green;
    vector_float4 blue;
} vui_yuv_conversion_t;

@interface VUIVideoToolboxRenderer : NSObject
@property(nonatomic, strong) id<MTLDevice> device;
@property(nonatomic, strong) id<MTLRenderPipelineState> rgbaPipeline;
@property(nonatomic, strong) id<MTLRenderPipelineState> bgraPipeline;
@property(nonatomic, strong) id<MTLSamplerState> sampler;
@property(nonatomic) CVMetalTextureCacheRef textureCache;
@end

static NSString *const vuiVideoShaderSource =
    @"#include <metal_stdlib>\n"
     "using namespace metal;\n"
     "struct RasterizerData {\n"
     "    float4 position [[position]];\n"
     "    float2 texcoord;\n"
     "};\n"
     "struct YUVConversion {\n"
     "    float4 red;\n"
     "    float4 green;\n"
     "    float4 blue;\n"
     "};\n"
     "vertex RasterizerData vanilla_video_vertex(uint vertexID [[vertex_id]]) {\n"
     "    constexpr float2 positions[] = {\n"
     "        float2(-1.0, 1.0), float2(1.0, 1.0),\n"
     "        float2(-1.0, -1.0), float2(1.0, -1.0)\n"
     "    };\n"
     "    constexpr float2 texcoords[] = {\n"
     "        float2(0.0, 0.0), float2(1.0, 0.0),\n"
     "        float2(0.0, 1.0), float2(1.0, 1.0)\n"
     "    };\n"
     "    RasterizerData output;\n"
     "    output.position = float4(positions[vertexID], 0.0, 1.0);\n"
     "    output.texcoord = texcoords[vertexID];\n"
     "    return output;\n"
     "}\n"
     "fragment float4 vanilla_video_fragment(\n"
     "    RasterizerData input [[stage_in]],\n"
     "    texture2d<float> luma [[texture(0)]],\n"
     "    texture2d<float> chroma [[texture(1)]],\n"
     "    sampler videoSampler [[sampler(0)]],\n"
     "    constant YUVConversion &conversion [[buffer(0)]]) {\n"
     "    const float y = luma.sample(videoSampler, input.texcoord).r;\n"
     "    const float2 cbcr = chroma.sample(videoSampler, input.texcoord).rg;\n"
     "    const float4 sample = float4(y, cbcr, 1.0);\n"
     "    return float4(dot(conversion.red, sample),\n"
     "                  dot(conversion.green, sample),\n"
     "                  dot(conversion.blue, sample), 1.0);\n"
     "}\n";

static id<MTLRenderPipelineState> vui_create_pipeline(id<MTLDevice> device, id<MTLLibrary> library, MTLPixelFormat pixelFormat)
{
    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.vertexFunction = [library newFunctionWithName:@"vanilla_video_vertex"];
    descriptor.fragmentFunction = [library newFunctionWithName:@"vanilla_video_fragment"];
    descriptor.colorAttachments[0].pixelFormat = pixelFormat;

    NSError *error = nil;
    id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!pipeline) {
        vpilog("Failed to create VideoToolbox Metal pipeline: %s\n", error.localizedDescription.UTF8String);
    }
    return pipeline;
}

@implementation VUIVideoToolboxRenderer

- (instancetype)initWithSDLRenderer:(SDL_Renderer *)renderer
{
    self = [super init];
    if (!self) {
        return nil;
    }

    SDL_RendererInfo rendererInfo;
    if (SDL_GetRendererInfo(renderer, &rendererInfo) < 0 || !rendererInfo.name || strcmp(rendererInfo.name, "metal") != 0) {
        vpilog("VideoToolbox requires SDL's Metal renderer\n");
        return nil;
    }

    CAMetalLayer *layer = (__bridge CAMetalLayer *) SDL_RenderGetMetalLayer(renderer);
    self.device = layer.device;
    if (!self.device) {
        vpilog("SDL's Metal renderer did not provide a Metal device\n");
        return nil;
    }

    CVReturn result = CVMetalTextureCacheCreate(kCFAllocatorDefault, NULL, self.device, NULL, &_textureCache);
    if (result != kCVReturnSuccess) {
        vpilog("Failed to create CoreVideo Metal texture cache: %d\n", result);
        return nil;
    }

    NSError *error = nil;
    id<MTLLibrary> library = [self.device newLibraryWithSource:vuiVideoShaderSource options:nil error:&error];
    if (!library) {
        vpilog("Failed to compile VideoToolbox Metal shader: %s\n", error.localizedDescription.UTF8String);
        return nil;
    }

    self.rgbaPipeline = vui_create_pipeline(self.device, library, MTLPixelFormatRGBA8Unorm);
    self.bgraPipeline = vui_create_pipeline(self.device, library, MTLPixelFormatBGRA8Unorm);
    if (!self.rgbaPipeline && !self.bgraPipeline) {
        return nil;
    }

    MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterLinear;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    self.sampler = [self.device newSamplerStateWithDescriptor:samplerDescriptor];
    if (!self.sampler) {
        vpilog("Failed to create VideoToolbox Metal sampler\n");
        return nil;
    }

    return self;
}

- (void)dealloc
{
    if (_textureCache) {
        CFRelease(_textureCache);
    }
}

@end

static vui_yuv_conversion_t vui_yuv_conversion(const AVFrame *frame, OSType pixelFormat)
{
    const int fullRange = pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange || frame->color_range == AVCOL_RANGE_JPEG;
    const int bt709 = frame->colorspace == AVCOL_SPC_BT709;
    const float kr = bt709 ? 0.2126f : 0.2990f;
    const float kb = bt709 ? 0.0722f : 0.1140f;
    const float kg = 1.0f - kr - kb;
    const float yOffset = fullRange ? 0.0f : 16.0f / 255.0f;
    const float chromaOffset = 128.0f / 255.0f;
    const float yScale = fullRange ? 1.0f : 255.0f / 219.0f;
    const float chromaScale = fullRange ? 1.0f : 255.0f / 224.0f;

    const float redCr = 2.0f * (1.0f - kr) * chromaScale;
    const float blueCb = 2.0f * (1.0f - kb) * chromaScale;
    const float greenCb = -2.0f * kb * (1.0f - kb) / kg * chromaScale;
    const float greenCr = -2.0f * kr * (1.0f - kr) / kg * chromaScale;

    vui_yuv_conversion_t conversion = {
        .red = { yScale, 0.0f, redCr, -yScale * yOffset - redCr * chromaOffset },
        .green = { yScale, greenCb, greenCr, -yScale * yOffset - (greenCb + greenCr) * chromaOffset },
        .blue = { yScale, blueCb, 0.0f, -yScale * yOffset - blueCb * chromaOffset }
    };
    return conversion;
}

vui_sdl_videotoolbox_context_t *vui_sdl_videotoolbox_create(SDL_Renderer *renderer)
{
    VUIVideoToolboxRenderer *context = [[VUIVideoToolboxRenderer alloc] initWithSDLRenderer:renderer];
    return (__bridge_retained vui_sdl_videotoolbox_context_t *) context;
}

void vui_sdl_videotoolbox_destroy(vui_sdl_videotoolbox_context_t **context)
{
    if (!context || !*context) {
        return;
    }
    CFBridgingRelease(*context);
    *context = NULL;
}

int vui_sdl_videotoolbox_render(vui_sdl_videotoolbox_context_t *opaque, SDL_Renderer *renderer, const AVFrame *frame)
{
    if (!opaque || !renderer || !frame || frame->format != AV_PIX_FMT_VIDEOTOOLBOX || !frame->data[3]) {
        return 0;
    }

    VUIVideoToolboxRenderer *context = (__bridge VUIVideoToolboxRenderer *) opaque;
    CVPixelBufferRef pixelBuffer = (CVPixelBufferRef) frame->data[3];
    OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (CVPixelBufferGetPlaneCount(pixelBuffer) != 2 || (pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange && pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)) {
        vpilog("VideoToolbox returned unsupported pixel format: 0x%08x\n", pixelFormat);
        return 0;
    }

    SDL_Texture *target = SDL_GetRenderTarget(renderer);
    Uint32 targetFormat = 0;
    int targetWidth = 0;
    int targetHeight = 0;
    if (!target || SDL_QueryTexture(target, &targetFormat, NULL, &targetWidth, &targetHeight) < 0) {
        vpilog("VideoToolbox rendering requires an SDL texture target\n");
        return 0;
    }

    id<MTLRenderPipelineState> pipeline = nil;
    if (targetFormat == SDL_PIXELFORMAT_RGBA32) {
        pipeline = context.rgbaPipeline;
    } else if (targetFormat == SDL_PIXELFORMAT_BGRA32) {
        pipeline = context.bgraPipeline;
    }
    if (!pipeline) {
        vpilog("VideoToolbox cannot render to SDL pixel format 0x%08x\n", targetFormat);
        return 0;
    }

    CVMetalTextureRef lumaRef = NULL;
    CVMetalTextureRef chromaRef = NULL;
    CVReturn result = CVMetalTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault,
        context.textureCache,
        pixelBuffer,
        NULL,
        MTLPixelFormatR8Unorm,
        CVPixelBufferGetWidthOfPlane(pixelBuffer, 0),
        CVPixelBufferGetHeightOfPlane(pixelBuffer, 0),
        0,
        &lumaRef
    );
    if (result == kCVReturnSuccess) {
        result = CVMetalTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            context.textureCache,
            pixelBuffer,
            NULL,
            MTLPixelFormatRG8Unorm,
            CVPixelBufferGetWidthOfPlane(pixelBuffer, 1),
            CVPixelBufferGetHeightOfPlane(pixelBuffer, 1),
            1,
            &chromaRef
        );
    }

    id<MTLTexture> luma = lumaRef ? CVMetalTextureGetTexture(lumaRef) : nil;
    id<MTLTexture> chroma = chromaRef ? CVMetalTextureGetTexture(chromaRef) : nil;
    if (result != kCVReturnSuccess || !luma || !chroma) {
        vpilog("Failed to expose VideoToolbox frame as Metal textures: %d\n", result);
        if (chromaRef) CFRelease(chromaRef);
        if (lumaRef) CFRelease(lumaRef);
        return 0;
    }

    if (SDL_RenderFlush(renderer) < 0) {
        vpilog("Failed to flush SDL before VideoToolbox render: %s\n", SDL_GetError());
        CFRelease(chromaRef);
        CFRelease(lumaRef);
        return 0;
    }

    id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>) SDL_RenderGetMetalCommandEncoder(renderer);
    if (!encoder) {
        vpilog("SDL did not provide a Metal command encoder\n");
        CFRelease(chromaRef);
        CFRelease(lumaRef);
        return 0;
    }

    MTLViewport viewport = { 0.0, 0.0, targetWidth, targetHeight, 0.0, 1.0 };
    MTLScissorRect scissor = { 0, 0, targetWidth, targetHeight };
    vui_yuv_conversion_t conversion = vui_yuv_conversion(frame, pixelFormat);

    [encoder setViewport:viewport];
    [encoder setScissorRect:scissor];
    [encoder setRenderPipelineState:pipeline];
    [encoder setFragmentTexture:luma atIndex:0];
    [encoder setFragmentTexture:chroma atIndex:1];
    [encoder setFragmentSamplerState:context.sampler atIndex:0];
    [encoder setFragmentBytes:&conversion length:sizeof(conversion) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

    CFRelease(chromaRef);
    CFRelease(lumaRef);
    return 1;
}
