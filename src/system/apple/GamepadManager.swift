import GameController
import Foundation

public final class GamepadManager: NSObject {
    public private(set) var controllerState: UInt32 = 0
    
    public override init() {
        super.init()
        NotificationCenter.default.addObserver(self, selector: #selector(controllerDidConnect), name: .GCControllerDidConnect, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(controllerDidDisconnect), name: .GCControllerDidDisconnect, object: nil)
    }
    
    @objc private func controllerDidConnect(_ notification: Notification) {
        if let controller = notification.object as? GCController {
            print("[GamepadManager]: Controller connected: \(controller.vendorName ?? "Unknown")")
        }
    }
    
    @objc private func controllerDidDisconnect(_ notification: Notification) {
        if let controller = notification.object as? GCController {
            print("[GamepadManager]: Controller disconnected: \(controller.vendorName ?? "Unknown")")
        }
    }
    
    public private(set) var menuPressed: Bool = false
    
    public func update() {
        var padState: UInt32 = 0
        var menuPressedState = false
        let controllers = GCController.controllers()
        for (index, controller) in controllers.prefix(4).enumerated() {
            var buttons: UInt32 = 0
            if let extendedGamepad = controller.extendedGamepad {
                if extendedGamepad.dpad.up.isPressed { buttons |= (1 << 0) }
                if extendedGamepad.dpad.down.isPressed { buttons |= (1 << 1) }
                if extendedGamepad.dpad.left.isPressed { buttons |= (1 << 2) }
                if extendedGamepad.dpad.right.isPressed { buttons |= (1 << 3) }
                
                if extendedGamepad.buttonA.isPressed { buttons |= (1 << 4) }
                if extendedGamepad.buttonB.isPressed { buttons |= (1 << 5) }
                if extendedGamepad.buttonX.isPressed { buttons |= (1 << 6) }
                if extendedGamepad.buttonY.isPressed { buttons |= (1 << 7) }
                
                if extendedGamepad.buttonMenu.isPressed {
                    menuPressedState = true
                }
            } else if let microGamepad = controller.microGamepad {
                if microGamepad.dpad.up.isPressed { buttons |= (1 << 0) }
                if microGamepad.dpad.down.isPressed { buttons |= (1 << 1) }
                if microGamepad.dpad.left.isPressed { buttons |= (1 << 2) }
                if microGamepad.dpad.right.isPressed { buttons |= (1 << 3) }
                
                if microGamepad.buttonA.isPressed { buttons |= (1 << 4) }
                if microGamepad.buttonX.isPressed { buttons |= (1 << 5) }
                
                if microGamepad.buttonMenu.isPressed {
                    menuPressedState = true
                }
            }
            padState |= (buttons << (index * 8))
        }
        controllerState = padState
        menuPressed = menuPressedState
    }
}
