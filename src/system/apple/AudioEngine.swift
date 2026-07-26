import AVFoundation
import Foundation

public final class AudioEngine: NSObject {
    private let audioEngine = AVAudioEngine()
    private var sourceNode: AVAudioSourceNode?
    
    private let studio: OpaquePointer
    
    private var bufferRemaining: Int = 0
    private var bufferReadIdx: Int = 0
    
    public init(studio: OpaquePointer) {
        self.studio = studio
        super.init()
        setupAudioNode()
    }
    
    deinit {
        stop()
    }
    
    private func setupAudioNode() {
        let mainMixer = audioEngine.mainMixerNode
        let format = AVAudioFormat(standardFormatWithSampleRate: Double(TIC80_SAMPLERATE), channels: 2)!
        
        let sourceNode = AVAudioSourceNode { [weak self] _, _, frameCount, audioBufferList -> OSStatus in
            guard let self = self else { return noErr }
            
            let ablPointer = UnsafeMutableAudioBufferListPointer(audioBufferList)
            let frames = Int(frameCount)
            
            guard let leftChannel = ablPointer[0].mData?.assumingMemoryBound(to: Float.self) else { return noErr }
            let rightChannel = ablPointer.count > 1 ? ablPointer[1].mData?.assumingMemoryBound(to: Float.self) : nil
            
            let tic = studio_mem(self.studio)
            guard let tic = tic else { return noErr }
            
            var writeIdx = 0
            while writeIdx < frames {
                if self.bufferRemaining <= 0 {
                    audioLock.lock()
                    studio_sound(self.studio)
                    audioLock.unlock()
                    self.bufferRemaining = Int(tic.pointee.product.samples.count) / 2
                    self.bufferReadIdx = 0
                }
                
                let samplesToCopy = min(frames - writeIdx, self.bufferRemaining)
                if let samplesBuffer = tic.pointee.product.samples.buffer {
                    for i in 0..<samplesToCopy {
                        let sampleL = Float(samplesBuffer[(self.bufferReadIdx + i) * 2]) / 32768.0
                        let sampleR = Float(samplesBuffer[(self.bufferReadIdx + i) * 2 + 1]) / 32768.0
                        
                        leftChannel[writeIdx + i] = sampleL
                        rightChannel?[writeIdx + i] = sampleR
                    }
                } else {
                    for i in 0..<samplesToCopy {
                        leftChannel[writeIdx + i] = 0.0
                        rightChannel?[writeIdx + i] = 0.0
                    }
                }
                
                writeIdx += samplesToCopy
                self.bufferReadIdx += samplesToCopy
                self.bufferRemaining -= samplesToCopy
            }
            
            return noErr
        }
        
        self.sourceNode = sourceNode
        audioEngine.attach(sourceNode)
        audioEngine.connect(sourceNode, to: mainMixer, format: format)
    }
    
    public func start() {
        do {
            try audioEngine.start()
            print("[AudioEngine]: CoreAudio AVAudioEngine started successfully")
        } catch {
            print("[AudioEngine]: Failed to start AVAudioEngine: \(error)")
        }
    }
    
    public func stop() {
        audioEngine.stop()
    }
    
    public func setVolume(_ volume: Float) {
        audioEngine.mainMixerNode.outputVolume = volume
    }
}
