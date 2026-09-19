import AppKit
import MetalKit
import Foundation
import GameController

let TIC80_FULLWIDTH = 256
let TIC80_FULLHEIGHT = 144
let TIC80_WIDTH = 240
let TIC80_HEIGHT = 136
let TIC80_MARGIN_TOP = (TIC80_FULLHEIGHT - TIC80_HEIGHT) / 2
let TIC80_MARGIN_LEFT = (TIC80_FULLWIDTH - TIC80_WIDTH) / 2

// Global keyboard and mouse state
private var keyboardState = [Bool](repeating: false, count: Int(tic_keys_count_val()))
private var keyboardPressed = [Bool](repeating: false, count: Int(tic_keys_count_val()))
private var globalKeyboardText: Character? = nil

private var mouseX: Int32 = -1
private var mouseY: Int32 = -1
private var mouseLeft: Bool = false
private var mouseMiddle: Bool = false
private var mouseRight: Bool = false
private var mouseLeftPressed: Bool = false
private var mouseMiddlePressed: Bool = false
private var mouseRightPressed: Bool = false
private var mouseScrollX: Int32 = 0
private var mouseScrollY: Int32 = 0
private var mouseXDelta: Int32 = 0
private var mouseYDelta: Int32 = 0

// OpaquePointer to Studio
private var globalStudio: OpaquePointer? = nil
private var intendedFullscreen = false

private func resetKeyboardState() {
    for i in 0..<keyboardState.count {
        keyboardState[i] = false
        keyboardPressed[i] = false
    }
    globalKeyboardText = nil
}

// Global synchronization lock between audio and tick threads
public let audioLock = NSLock()

public final class TICMetalView: MTKView {
    public var renderer: MetalRenderer?
    
    public override var acceptsFirstResponder: Bool { true }
    public override var canBecomeKeyView: Bool { true }
    public override func acceptsFirstMouse(for event: NSEvent?) -> Bool { return true }
    
    public override func keyDown(with event: NSEvent) {
        handleKeyEvent(event, down: true)
    }
    
    public override func keyUp(with event: NSEvent) {
        handleKeyEvent(event, down: false)
    }
    
    public override func flagsChanged(with event: NSEvent) {
        let flags = event.modifierFlags
        
        let shiftDown = flags.contains(.shift)
        let ctrlDown = flags.contains(.control) || flags.contains(.command)
        let altDown = flags.contains(.option)
        let capsDown = flags.contains(.capsLock)
        
        keyboardState[Int(tic_key_shift.rawValue)] = shiftDown
        keyboardState[Int(tic_key_ctrl.rawValue)] = ctrlDown
        keyboardState[Int(tic_key_alt.rawValue)] = altDown
        keyboardState[Int(tic_key_capslock.rawValue)] = capsDown
    }
    
    private func handleKeyEvent(_ event: NSEvent, down: Bool) {
        let keyCode = event.keyCode
        
        // Escape check for menu toggle
        if keyCode == 53 && down {
            // Forward escape to Studio
            // In C studio: studio_alive or escape handling is done in update
        }
        
        let ticKey = tic_map_mac_key(keyCode)
        if ticKey != 0 {
            let keyIdx = Int(ticKey)
            if keyIdx >= 0 && keyIdx < keyboardState.count {
                if down && !keyboardState[keyIdx] {
                    keyboardPressed[keyIdx] = true
                }
                keyboardState[keyIdx] = down
            }
        }
        
        // Handle text input
        if down, let chars = event.characters, let first = chars.first {
            // Only capture text input if it is printable (not esc, not cmd shortcuts, not function keys)
            let val = first.unicodeScalars.first?.value ?? 0
            if val >= 32 && val != 127 && val < 0xF700 {
                globalKeyboardText = first
            }
        }
    }
    

    // Mouse Event Handlers
    private func updateMouseState(with event: NSEvent) {
        let location = convert(event.locationInWindow, from: nil)
        let viewSize = bounds.size
        
        if location.x < 0 || location.y < 0 || location.x > viewSize.width || location.y > viewSize.height {
            mouseX = -1
            mouseY = -1
            mouseLeft = false
            mouseMiddle = false
            mouseRight = false
            return
        }
        
        let drawableSize = self.drawableSize
        
        // Map to backing pixel coords
        let backingLoc = convertToBacking(location)
        
        if let renderer = renderer {
            if let mapped = renderer.mapMouseToFullCoords(pixelLocation: backingLoc, viewWidth: Double(drawableSize.width), viewHeight: Double(drawableSize.height)) {
                mouseX = Int32(mapped.x)
                mouseY = Int32(mapped.y)
            }
        }
        
        switch event.type {
        case .leftMouseDown, .leftMouseDragged:
            mouseLeft = true
            mouseLeftPressed = true
        case .leftMouseUp:
            mouseLeft = false
        case .rightMouseDown, .rightMouseDragged:
            mouseRight = true
            mouseRightPressed = true
        case .rightMouseUp:
            mouseRight = false
        case .otherMouseDown, .otherMouseDragged:
            mouseMiddle = true
            mouseMiddlePressed = true
        case .otherMouseUp:
            mouseMiddle = false
        default:
            break
        }
        
        mouseXDelta += Int32(event.deltaX)
        mouseYDelta += Int32(event.deltaY)
    }
    
    public override func mouseDown(with event: NSEvent) { updateMouseState(with: event) }
    public override func mouseUp(with event: NSEvent) { updateMouseState(with: event) }
    public override func rightMouseDown(with event: NSEvent) { updateMouseState(with: event) }
    public override func rightMouseUp(with event: NSEvent) { updateMouseState(with: event) }
    public override func otherMouseDown(with event: NSEvent) { updateMouseState(with: event) }
    public override func otherMouseUp(with event: NSEvent) { updateMouseState(with: event) }
    public override func mouseMoved(with event: NSEvent) { updateMouseState(with: event) }
    public override func mouseDragged(with event: NSEvent) { updateMouseState(with: event) }
    public override func rightMouseDragged(with event: NSEvent) { updateMouseState(with: event) }
    public override func otherMouseDragged(with event: NSEvent) { updateMouseState(with: event) }
    
    public override func scrollWheel(with event: NSEvent) {
        mouseScrollX = Int32(event.scrollingDeltaX)
        mouseScrollY = Int32(event.scrollingDeltaY)
        updateMouseState(with: event)
    }
    
    
    // Hide mouse cursor when inside the view
    private let invisibleCursor: NSCursor = {
        let image = NSImage(size: NSSize(width: 1, height: 1))
        return NSCursor(image: image, hotSpot: NSPoint.zero)
    }()
    
    public override func resetCursorRects() {
        super.resetCursorRects()
        addCursorRect(bounds, cursor: invisibleCursor)
    }
    
    private var trackingArea: NSTrackingArea?
    public override func updateTrackingAreas() {
        if let existing = trackingArea {
            removeTrackingArea(existing)
        }
        let options: NSTrackingArea.Options = [.activeInKeyWindow, .mouseMoved, .mouseEnteredAndExited, .inVisibleRect]
        let newTrackingArea = NSTrackingArea(rect: bounds, options: options, owner: self, userInfo: nil)
        addTrackingArea(newTrackingArea)
        self.trackingArea = newTrackingArea
        super.updateTrackingAreas()
    }
}

// Main entry point for top-level code in main.swift
private var globalDelegate: AppDelegate? = nil

func startApp() {
    let app = NSApplication.shared
    app.setActivationPolicy(.regular)
    let delegate = AppDelegate()
    globalDelegate = delegate
    app.delegate = delegate
    app.run()
}

startApp()

final class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
    public static private(set) var shared: AppDelegate?
    
    var window: NSWindow?
    var metalView: TICMetalView?
    var renderer: MetalRenderer?
    var audioEngine: AudioEngine?
    var gamepadManager: GamepadManager?
    
    var studio: OpaquePointer?
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        AppDelegate.shared = self
        
        // 1. App Support Folder Setup
        let fileManager = FileManager.default
        let appSupportURL = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        let tic80FolderURL = appSupportURL.appendingPathComponent("com.nesbox.tic").appendingPathComponent("tic80")
        try? fileManager.createDirectory(at: tic80FolderURL, withIntermediateDirectories: true, attributes: nil)
        
        let appFolder = tic80FolderURL.path
        
        // 2. Create C Studio Instance
        // We use tic_layout(1) for QWERTY
        let studioPtr = studio_create(
            CommandLine.argc,
            CommandLine.unsafeArgv,
            Int32(TIC80_SAMPLERATE),
            TIC80_PIXEL_COLOR_RGBA8888,
            appFolder,
            4,
            tic_layout(1)
        )
        
        guard let studio = studioPtr else {
            print("Failed to initialize TIC-80 Studio.")
            NSApp.terminate(nil)
            return
        }
        
        self.studio = studio
        globalStudio = studio
        
        // 3. Create Window and Metal View
        let initialSize = CGSize(width: CGFloat(TIC80_FULLWIDTH) * 4.0, height: CGFloat(TIC80_FULLHEIGHT) * 4.0)
        let windowRect = NSRect(origin: .zero, size: initialSize)
        
        let window = NSWindow(
            contentRect: windowRect,
            styleMask: [.titled, .closable, .miniaturizable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "TIC-80 Native macOS"
        window.center()
        window.contentMinSize = CGSize(width: Int(TIC80_FULLWIDTH), height: Int(TIC80_FULLHEIGHT))
        window.acceptsMouseMovedEvents = true
        self.window = window
        window.delegate = self
        
        let mtkView = TICMetalView(frame: windowRect)
        mtkView.autoresizingMask = [NSView.AutoresizingMask.width, NSView.AutoresizingMask.height]
        self.metalView = mtkView
        window.contentView?.addSubview(mtkView)
        
        // 4. Initialize Metal Renderer
        guard let renderer = MetalRenderer(metalView: mtkView, studio: studio) else {
            print("Failed to initialize Metal Renderer.")
            NSApp.terminate(nil)
            return
        }
        self.renderer = renderer
        mtkView.renderer = renderer
        
        // 5. Initialize Gamepad and Audio Engine
        self.gamepadManager = GamepadManager()
        self.audioEngine = AudioEngine(studio: studio)
        self.audioEngine?.start()
        
        window.makeKeyAndOrderFront(self)
        window.makeFirstResponder(mtkView)
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
        
        // 6. Hook Main Frame Game Loop
        mtkView.renderer?.onUpdate = { [weak self] in
            self?.tickGame()
        }
    }
    
    private func tickGame() {
        guard let studio = studio else { return }
        
        if studio_alive(studio) {
            // C core requested exit
            NSApp.terminate(nil)
            return
        }
        
        // Read Gamepad state
        gamepadManager?.update()
        let gamepadInput = gamepadManager?.controllerState ?? 0
        
        // Populate inputs
        var input = tic80_input()
        input.gamepads.data = gamepadInput
        
        // Populate keyboard inputs
        var keyCount = 0
        withUnsafeMutablePointer(to: &input.keyboard.keys) { keysPtr in
            let rawKeys = UnsafeMutableRawPointer(keysPtr).assumingMemoryBound(to: UInt8.self)
            for i in 0..<keyboardState.count {
                if keyboardState[i] || keyboardPressed[i] {
                    if keyCount < Int(TIC80_KEY_BUFFER) {
                        rawKeys[keyCount] = UInt8(i)
                        keyCount += 1
                    }
                }
                keyboardPressed[i] = false
            }
            if let gamepadManager = gamepadManager, gamepadManager.menuPressed {
                if keyCount < Int(TIC80_KEY_BUFFER) {
                    rawKeys[keyCount] = UInt8(tic_key_escape.rawValue)
                    keyCount += 1
                }
            }
        }
        
        // Populate mouse inputs
        let tic = studio_mem(studio)
        let relativeMode = tic?.pointee.ram.pointee.input.mouse.relative != 0
        
        // Clear scroll wheel and relative movement state after capturing
        let scX = mouseScrollX
        let scY = mouseScrollY
        let rx = mouseXDelta
        let ry = mouseYDelta
        mouseScrollX = 0
        mouseScrollY = 0
        mouseXDelta = 0
        mouseYDelta = 0
        
        let left = mouseLeft || mouseLeftPressed
        let right = mouseRight || mouseRightPressed
        let middle = mouseMiddle || mouseMiddlePressed
        
        mouseLeftPressed = false
        mouseRightPressed = false
        mouseMiddlePressed = false
        
        tic_input_set_mouse(&input, mouseX, mouseY, rx, ry, left, middle, right, scX, scY, relativeMode)
        
        // Call C tick (synchronized with audio thread)
        audioLock.lock()
        studio_tick(studio, input)
        audioLock.unlock()
    }
    
    func applicationWillTerminate(_ notification: Notification) {
        audioEngine?.stop()
        if let studio = studio {
            studio_delete(studio)
        }
    }
    
    func windowDidResignKey(_ notification: Notification) {
        resetKeyboardState()
    }
    
    func windowWillEnterFullScreen(_ notification: Notification) {
        resetKeyboardState()
    }
    
    func windowWillExitFullScreen(_ notification: Notification) {
        resetKeyboardState()
    }
    
    func windowDidEnterFullScreen(_ notification: Notification) {
        intendedFullscreen = true
    }
    
    func windowDidExitFullScreen(_ notification: Notification) {
        intendedFullscreen = false
    }
    
    func windowWillClose(_ notification: Notification) {
        NSApp.terminate(nil)
    }
}

// MARK: - C System Interfaces Implementation (@_cdecl)

@_cdecl("tic_sys_clipboard_set")
public func tic_sys_clipboard_set(_ text: UnsafePointer<CChar>?) {
    guard let text = text else { return }
    let str = String(cString: text)
    let pasteboard = NSPasteboard.general
    pasteboard.clearContents()
    pasteboard.setString(str, forType: .string)
}

@_cdecl("tic_sys_clipboard_get")
public func tic_sys_clipboard_get() -> UnsafeMutablePointer<CChar>? {
    let pasteboard = NSPasteboard.general
    if let str = pasteboard.string(forType: .string) {
        return strdup(str)
    }
    return nil
}

@_cdecl("tic_sys_clipboard_has")
public func tic_sys_clipboard_has() -> Bool {
    let pasteboard = NSPasteboard.general
    return pasteboard.types?.contains(.string) ?? false
}

@_cdecl("tic_sys_clipboard_free")
public func tic_sys_clipboard_free(_ text: UnsafePointer<CChar>?) {
    if let text = text {
        free(UnsafeMutableRawPointer(mutating: text))
    }
}

import Darwin

@_cdecl("tic_sys_counter_get")
public func tic_sys_counter_get() -> UInt64 {
    return mach_absolute_time()
}

@_cdecl("tic_sys_freq_get")
public func tic_sys_freq_get() -> UInt64 {
    var info = mach_timebase_info()
    mach_timebase_info(&info)
    if info.numer == 0 { return 1_000_000_000 }
    return UInt64(1_000_000_000) * UInt64(info.denom) / UInt64(info.numer)
}

@_cdecl("tic_sys_fullscreen_set")
public func tic_sys_fullscreen_set(_ value: Bool) {
    intendedFullscreen = value
    DispatchQueue.main.async {
        guard let window = AppDelegate.shared?.window else { return }
        let isFullscreen = window.styleMask.contains(.fullScreen)
        if value != isFullscreen {
            window.toggleFullScreen(nil)
        }
    }
}

@_cdecl("tic_sys_fullscreen_get")
public func tic_sys_fullscreen_get() -> Bool {
    return intendedFullscreen
}

@_cdecl("tic_sys_message")
public func tic_sys_message(_ title: UnsafePointer<CChar>?, _ message: UnsafePointer<CChar>?) {
    let t = title != nil ? String(cString: title!) : "Message"
    let m = message != nil ? String(cString: message!) : ""
    DispatchQueue.main.async {
        let alert = NSAlert()
        alert.messageText = t
        alert.informativeText = m
        alert.alertStyle = .informational
        alert.addButton(withTitle: "OK")
        alert.runModal()
    }
}

@_cdecl("tic_sys_title")
public func tic_sys_title(_ title: UnsafePointer<CChar>?) {
    guard let title = title else { return }
    let str = String(cString: title)
    DispatchQueue.main.async {
        AppDelegate.shared?.window?.title = str
    }
}

@_cdecl("tic_sys_open_path")
public func tic_sys_open_path(_ path: UnsafePointer<CChar>?) {
    guard let path = path else { return }
    let str = String(cString: path)
    let url = URL(fileURLWithPath: str)
    NSWorkspace.shared.open(url)
}

@_cdecl("tic_sys_open_url")
public func tic_sys_open_url(_ url: UnsafePointer<CChar>?) {
    guard let url = url else { return }
    let str = String(cString: url)
    if let nsUrl = URL(string: str) {
        NSWorkspace.shared.open(nsUrl)
    }
}

@_cdecl("tic_sys_preseed")
public func tic_sys_preseed() {
    tic_sys_srand(UInt32(time(nil)))
}

@_cdecl("tic_sys_keyboard_text")
public func tic_sys_keyboard_text(_ text: UnsafeMutablePointer<CChar>?) -> Bool {
    guard let text = text else { return false }
    if let char = globalKeyboardText {
        let str = String(char)
        if let firstByte = str.utf8.first {
            text.pointee = Int8(bitPattern: firstByte)
            globalKeyboardText = nil
            return true
        }
    }
    return false
}

@_cdecl("tic_sys_update_config")
public func tic_sys_update_config() {
}

@_cdecl("tic_sys_default_mapping")
public func tic_sys_default_mapping(_ mapping: UnsafeMutablePointer<tic_mapping>?) {
    guard let mapping = mapping else { return }
    mapping.pointee.data.0 = UInt8(tic_key_up.rawValue)
    mapping.pointee.data.1 = UInt8(tic_key_down.rawValue)
    mapping.pointee.data.2 = UInt8(tic_key_left.rawValue)
    mapping.pointee.data.3 = UInt8(tic_key_right.rawValue)
    mapping.pointee.data.4 = UInt8(tic_key_z.rawValue)
    mapping.pointee.data.5 = UInt8(tic_key_x.rawValue)
    mapping.pointee.data.6 = UInt8(tic_key_a.rawValue)
    mapping.pointee.data.7 = UInt8(tic_key_s.rawValue)
}
