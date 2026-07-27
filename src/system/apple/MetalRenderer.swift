import Metal
import MetalKit
import Foundation

private struct TICVertex {
    var position: SIMD2<Float>
    var texCoord: SIMD2<Float>
}

public final class MetalRenderer: NSObject, MTKViewDelegate {
    public let device: MTLDevice
    public let commandQueue: MTLCommandQueue
    private var pipelineState: MTLRenderPipelineState?
    private var screenTexture: MTLTexture?
    
    private let studio: OpaquePointer
    public var onUpdate: (() -> Void)?
    
    private struct ViewportRect: Equatable {
        var x: Double
        var y: Double
        var w: Double
        var h: Double
    }
    
    // Cache for frame optimization to reduce CPU/GPU usage
    private var hasPrevState = false
    private var prevScreenBuffer = [UInt32](repeating: 0, count: Int(TIC80_FULLWIDTH) * Int(TIC80_FULLHEIGHT))
    private var prevViewportRect: ViewportRect?
    
    public init?(metalView: MTKView, studio: OpaquePointer) {
        guard let defaultDevice = MTLCreateSystemDefaultDevice() else { return nil }
        self.device = defaultDevice
        guard let queue = defaultDevice.makeCommandQueue() else { return nil }
        self.commandQueue = queue
        
        self.studio = studio
        
        metalView.device = defaultDevice
        metalView.clearColor = MTLClearColor(red: 0.0, green: 0.0, blue: 0.0, alpha: 1.0)
        metalView.colorPixelFormat = .bgra8Unorm
        metalView.isPaused = false
        metalView.enableSetNeedsDisplay = false
        metalView.preferredFramesPerSecond = 60
        
        super.init()
        
        metalView.delegate = self
        setupTexture()
        setupPipeline(metalView: metalView)
    }
    
    private func setupTexture() {
        let descriptor = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: .rgba8Unorm,
            width: Int(TIC80_FULLWIDTH),
            height: Int(TIC80_FULLHEIGHT),
            mipmapped: false
        )
        descriptor.usage = MTLTextureUsage.shaderRead
        self.screenTexture = device.makeTexture(descriptor: descriptor)
    }
    
    private func setupPipeline(metalView: MTKView) {
        let shaderSource = """
        #include <metal_stdlib>
        using namespace metal;

        struct Vertex {
            float2 position;
            float2 texCoord;
        };

        struct VertexOut {
            float4 position [[position]];
            float2 texCoord;
        };

        vertex VertexOut vertexShader(uint vertexID [[vertex_id]],
                                     constant Vertex* vertices [[buffer(0)]]) {
            VertexOut out;
            out.position = float4(vertices[vertexID].position, 0.0, 1.0);
            out.texCoord = vertices[vertexID].texCoord;
            return out;
        }

        fragment float4 fragmentShader(VertexOut in [[stage_in]],
                                       texture2d<float> screenTexture [[texture(0)]]) {
            constexpr sampler textureSampler(mag_filter::nearest, min_filter::nearest);
            return screenTexture.sample(textureSampler, in.texCoord);
        }
        """
        
        do {
            let library = try device.makeLibrary(source: shaderSource, options: nil)
            let vertexFunction = library.makeFunction(name: "vertexShader")
            let fragmentFunction = library.makeFunction(name: "fragmentShader")
            
            let pipelineDescriptor = MTLRenderPipelineDescriptor()
            pipelineDescriptor.vertexFunction = vertexFunction
            pipelineDescriptor.fragmentFunction = fragmentFunction
            pipelineDescriptor.colorAttachments[0].pixelFormat = metalView.colorPixelFormat
            
            self.pipelineState = try device.makeRenderPipelineState(descriptor: pipelineDescriptor)
        } catch {
            print("Failed to compile Metal shaders: \(error)")
        }
    }
    
    public func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {
    }
    
    private func calcTextureRect(viewWidth: Double, viewHeight: Double) -> ViewportRect {
        let config = studio_config(studio)
        let integerScale = config?.pointee.options.integerScale ?? false
        
        let sw = Int(viewWidth)
        let sh = Int(viewHeight)
        
        let width = Int(TIC80_FULLWIDTH)
        let height = Int(TIC80_FULLHEIGHT)
        
        var w: Int
        var h: Int
        
        if sw * height < sh * width {
            w = sw - (integerScale ? sw % width : 0)
            h = height * w / width
        } else {
            h = sh - (integerScale ? sh % height : 0)
            w = width * h / height
        }
        
        if w < width { w = width }
        if h < height { h = height }
        
        let x = (sw - w) / 2
        let y = (sh - h) / 2
        
        return ViewportRect(x: Double(x), y: Double(y), w: Double(w), h: Double(h))
    }
    
    public func mapMouseToFullCoords(pixelLocation: CGPoint, viewWidth: Double, viewHeight: Double) -> (x: Int, y: Int)? {
        let rect = calcTextureRect(viewWidth: viewWidth, viewHeight: viewHeight)
        guard rect.w > 0 && rect.h > 0 else { return nil }
        
        let vx = Double(pixelLocation.x) - rect.x
        let v_top_y = (viewHeight - rect.y) - Double(pixelLocation.y)
        
        let tx = Int(floor(vx * Double(TIC80_FULLWIDTH) / rect.w))
        let ty = Int(floor(v_top_y * Double(TIC80_FULLHEIGHT) / rect.h))
        
        return (tx, ty)
    }
    
    private func drawSlice(
        renderEncoder: MTLRenderCommandEncoder,
        srcX: Double, srcY: Double, srcW: Double, srcH: Double,
        dstX: Double, dstY: Double, dstW: Double, dstH: Double
    ) {
        guard dstW > 0 && dstH > 0 else { return }
        
        let viewport = MTLViewport(
            originX: dstX,
            originY: dstY,
            width: dstW,
            height: dstH,
            znear: 0.0,
            zfar: 1.0
        )
        renderEncoder.setViewport(viewport)
        
        let texW = Double(TIC80_FULLWIDTH)
        let texH = Double(TIC80_FULLHEIGHT)
        
        let u1 = srcX / texW
        let u2 = (srcX + srcW) / texW
        let v1 = srcY / texH
        let v2 = (srcY + srcH) / texH
        
        let vertices = [
            TICVertex(position: SIMD2<Float>(-1.0, -1.0), texCoord: SIMD2<Float>(Float(u1), Float(v2))),
            TICVertex(position: SIMD2<Float>( 1.0, -1.0), texCoord: SIMD2<Float>(Float(u2), Float(v2))),
            TICVertex(position: SIMD2<Float>(-1.0,  1.0), texCoord: SIMD2<Float>(Float(u1), Float(v1))),
            TICVertex(position: SIMD2<Float>( 1.0,  1.0), texCoord: SIMD2<Float>(Float(u2), Float(v1)))
        ]
        
        renderEncoder.setVertexBytes(vertices, length: MemoryLayout<TICVertex>.stride * 4, index: 0)
        renderEncoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
    }
    
    public func draw(in view: MTKView) {
        // 1. Tick game logic first to update the screen in C memory
        onUpdate?()
        
        let tic = studio_mem(studio)
        guard let screenPtr = tic?.pointee.product.screen else {
            return
        }
        
        let viewWidth = Double(view.drawableSize.width)
        let viewHeight = Double(view.drawableSize.height)
        let rect = calcTextureRect(viewWidth: viewWidth, viewHeight: viewHeight)
        
        let fullWidth = Int(TIC80_FULLWIDTH)
        let fullHeight = Int(TIC80_FULLHEIGHT)
        let bufferCount = fullWidth * fullHeight
        
        var shouldDraw = true
        if hasPrevState && rect == prevViewportRect {
            let matches = prevScreenBuffer.withUnsafeBufferPointer { prevBufPtr in
                memcmp(screenPtr, prevBufPtr.baseAddress!, bufferCount * MemoryLayout<UInt32>.stride) == 0
            }
            if matches {
                shouldDraw = false
            }
        }
        
        guard shouldDraw else {
            return
        }
        
        // Update cache
        _ = prevScreenBuffer.withUnsafeMutableBufferPointer { destPtr in
            memcpy(destPtr.baseAddress!, screenPtr, bufferCount * MemoryLayout<UInt32>.stride)
        }
        prevViewportRect = rect
        hasPrevState = true
        
        guard let pipelineState = pipelineState,
              let screenTex = screenTexture,
              let commandBuffer = commandQueue.makeCommandBuffer(),
              let drawable = view.currentDrawable,
              let descriptor = view.currentRenderPassDescriptor else {
            return
        }
        
        // Replace texture with new bytes
        let region = MTLRegionMake2D(0, 0, fullWidth, fullHeight)
        screenTex.replace(
            region: region,
            mipmapLevel: 0,
            withBytes: screenPtr,
            bytesPerRow: fullWidth * 4
        )
        
        // 2. Set clear color
        view.clearColor = MTLClearColor(red: 0.0, green: 0.0, blue: 0.0, alpha: 1.0)
        
        // 3. Render
        guard let renderEncoder = commandBuffer.makeRenderCommandEncoder(descriptor: descriptor) else { return }
        
        renderEncoder.setRenderPipelineState(pipelineState)
        renderEncoder.setFragmentTexture(screenTex, index: 0)
        
        let srcW_full = Double(TIC80_FULLWIDTH)
        let srcH_full = Double(TIC80_FULLHEIGHT)
        
        drawSlice(
            renderEncoder: renderEncoder,
            srcX: 0.0, srcY: 0.0, srcW: srcW_full, srcH: srcH_full,
            dstX: rect.x, dstY: rect.y, dstW: rect.w, dstH: rect.h
        )
        
        renderEncoder.endEncoding()
        commandBuffer.present(drawable)
        commandBuffer.commit()
    }
}
