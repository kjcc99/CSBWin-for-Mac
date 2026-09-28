// CSBwin.app launcher: picks a game and its options, then runs the game
// (Contents/MacOS/CSBwin) and steps out of the way until it quits.
import AppKit
import SwiftUI

enum Speed: String, CaseIterable, Identifiable {
   case glacial, molasses, veryslow, slow, normal, fast, quick
   var id: String { rawValue }
   var title: String {
      switch self {
      case .glacial: return "Glacial"
      case .molasses: return "Molasses"
      case .veryslow: return "Very Slow"
      case .slow: return "Slow"
      case .normal: return "Normal"
      case .fast: return "Fast"
      case .quick: return "Quick as a Bunny"
      }
   }
}

enum Volume: String, CaseIterable, Identifiable {
   case full, half, quarter, eighth, off
   var id: String { rawValue }
   var title: String {
      switch self {
      case .full: return "Full"
      case .half: return "Half"
      case .quarter: return "Quarter"
      case .eighth: return "Eighth"
      case .off: return "Off"
      }
   }
}

final class GameRunner: ObservableObject {
   @Published private(set) var running = false

   static let userFolder: URL = {
      let support = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
      return support.appendingPathComponent("CSBwin", isDirectory: true)
   }()

   func play(_ arguments: [String]) {
      guard !running, let game = Bundle.main.url(forAuxiliaryExecutable: "CSBwin") else { return }
      let defaults = UserDefaults.standard
      var options = arguments
      options.append("speed=\(defaults.string(forKey: "speed") ?? Speed.normal.rawValue)")
      options.append("volume=\(defaults.string(forKey: "volume") ?? Volume.full.rawValue)")
      let width = defaults.integer(forKey: "windowWidth")
      options.append("width=\(width > 0 ? width : 960)")
      if defaults.bool(forKey: "fullscreen") {
         options.append("size=full")
      }
      let process = Process()
      process.executableURL = game
      process.arguments = options
      process.terminationHandler = { _ in
         DispatchQueue.main.async { self.gameEnded() }
      }
      do {
         try process.run()
      } catch {
         let alert = NSAlert(error: error)
         alert.runModal()
         return
      }
      running = true
      bringToFront(process.processIdentifier, attempts: 50)
   }

   // Since macOS 14 an app can't bring itself in front of the active app (the
   // launcher), so the launcher activates the game once it has started, and
   // only then hides itself.
   private func bringToFront(_ pid: pid_t, attempts: Int) {
      guard running else { return }
      let game = NSRunningApplication(processIdentifier: pid)
      if game?.isFinishedLaunching == true || attempts == 0 {
         if #available(macOS 14, *) {
            game?.activate(from: .current, options: [])
         } else {
            game?.activate(options: .activateIgnoringOtherApps)
         }
         // Hide the launcher, and its Dock icon, while the game has its own.
         for window in NSApp.windows {
            window.orderOut(nil)
         }
         NSApp.setActivationPolicy(.accessory)
         return
      }
      DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
         self.bringToFront(pid, attempts: attempts - 1)
      }
   }

   private func gameEnded() {
      running = false
      NSApp.setActivationPolicy(.regular)
      for window in NSApp.windows where window.canBecomeMain {
         window.makeKeyAndOrderFront(nil)
      }
      NSApp.activate(ignoringOtherApps: true)
   }

   func openUserFolder() {
      try? FileManager.default.createDirectory(at: Self.userFolder, withIntermediateDirectories: true)
      NSWorkspace.shared.open(Self.userFolder)
   }
}

struct PlayView: View {
   @ObservedObject var runner: GameRunner

   var body: some View {
      VStack(alignment: .leading, spacing: 14) {
         GameButton(title: "Dungeon Master", subtitle: "The original 1987 adventure") {
            runner.play(["directory=DM"])
         }
         GameButton(title: "Chaos Strikes Back", subtitle: "The sequel") {
            runner.play(["directory=CSB"])
         }
         GameButton(title: "Kid Dungeon", subtitle: "A small dungeon for younger players") {
            runner.play(["directory=DM", "dungeon=Dungeon-Kid.dat"])
         }
         Divider()
         HStack {
            Text("Watch a replay:")
            Button("Dungeon Master") { runner.play(["directory=DM", "play=Playfile.replay"]) }
            Button("Chaos Strikes Back") { runner.play(["directory=CSB", "play=Playfile.replay"]) }
         }
         Text("Replays were recorded with an older version, so the game warns about a different version. Choose to continue.")
            .font(.caption)
            .foregroundColor(.secondary)
            .fixedSize(horizontal: false, vertical: true)
         Spacer(minLength: 0)
         DisclosureGroup("Starting a new Chaos Strikes Back game") {
            Text("""
               1. Play Chaos Strikes Back, choose Dungeon, and enter the prison.
               2. Choose your champions, then save the game and quit.
               3. Play Chaos Strikes Back again and choose Utility. Use Make New Adventure (it is safe to replace your save game).
               4. Play Chaos Strikes Back again, choose Dungeon, and restore the new save game.
               """)
               .font(.caption)
               .fixedSize(horizontal: false, vertical: true)
               .padding(.top, 4)
         }
      }
      .disabled(runner.running)
      .padding(20)
   }
}

struct GameButton: View {
   let title: String
   let subtitle: String
   let action: () -> Void

   var body: some View {
      Button(action: action) {
         VStack(alignment: .leading, spacing: 2) {
            Text(title).font(.headline)
            Text(subtitle).font(.caption).foregroundColor(.secondary)
         }
         .frame(maxWidth: .infinity, alignment: .leading)
         .padding(.vertical, 4)
      }
      .controlSize(.large)
   }
}

struct SettingsView: View {
   @ObservedObject var runner: GameRunner
   @AppStorage("speed") var speed = Speed.normal
   @AppStorage("volume") var volume = Volume.full
   @AppStorage("windowWidth") var windowWidth = 960
   @AppStorage("fullscreen") var fullscreen = false

   var body: some View {
      Form {
         Picker("Game speed:", selection: $speed) {
            ForEach(Speed.allCases) { Text($0.title).tag($0) }
         }
         Picker("Volume:", selection: $volume) {
            ForEach(Volume.allCases) { Text($0.title).tag($0) }
         }
         Picker("Window size:", selection: $windowWidth) {
            Text("Small (640 × 480)").tag(640)
            Text("Medium (960 × 720)").tag(960)
            Text("Large (1280 × 960)").tag(1280)
            Text("Huge (1600 × 1200)").tag(1600)
         }
         Toggle("Start in full screen (F11 switches while playing)", isOn: $fullscreen)
         Divider()
         HStack {
            Text("Saved games and config.txt are kept in Application Support.")
               .font(.caption)
               .foregroundColor(.secondary)
            Spacer()
            Button("Show in Finder") { runner.openUserFolder() }
         }
      }
      .padding(20)
   }
}

// MARK: - Key bindings

// A key the game can be told about in config.txt.  For each key press the game
// looks up the PC scan code (scan lines) and then the Windows virtual key
// (mscan lines); see PCScanCode() and VirtualKey() in sdl/SDLMain.cpp.
struct GameKey {
   let id: String // Stored in UserDefaults
   let name: String
   let macKeyCode: UInt16
   let scan: Int
   let vk: Int

   // Keys missing here can't be bound: F11 is full screen, and the keypad's
   // . * / + - and Enter have codes that collide with other keys.
   static let all: [GameKey] = {
      var keys: [GameKey] = []
      let letters: [(String, UInt16, Int)] = [
         ("A", 0x00, 0x1e), ("B", 0x0b, 0x30), ("C", 0x08, 0x2e), ("D", 0x02, 0x20), ("E", 0x0e, 0x12),
         ("F", 0x03, 0x21), ("G", 0x05, 0x22), ("H", 0x04, 0x23), ("I", 0x22, 0x17), ("J", 0x26, 0x24),
         ("K", 0x28, 0x25), ("L", 0x25, 0x26), ("M", 0x2e, 0x32), ("N", 0x2d, 0x31), ("O", 0x1f, 0x18),
         ("P", 0x23, 0x19), ("Q", 0x0c, 0x10), ("R", 0x0f, 0x13), ("S", 0x01, 0x1f), ("T", 0x11, 0x14),
         ("U", 0x20, 0x16), ("V", 0x09, 0x2f), ("W", 0x0d, 0x11), ("X", 0x07, 0x2d), ("Y", 0x10, 0x15),
         ("Z", 0x06, 0x2c)]
      for (name, mac, scan) in letters {
         keys.append(GameKey(id: name, name: name, macKeyCode: mac, scan: scan, vk: Int(name.unicodeScalars.first!.value)))
      }
      let digitKeyCodes: [UInt16] = [0x1d, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1a, 0x1c, 0x19]
      for digit in 0...9 {
         keys.append(GameKey(id: "\(digit)", name: "\(digit)", macKeyCode: digitKeyCodes[digit],
                             scan: digit == 0 ? 0x0b : 0x01 + digit, vk: 0x30 + digit))
      }
      let keypadKeyCodes: [UInt16] = [0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5b, 0x5c]
      let keypadScans = [0x52, 0x4f, 0x50, 0x51, 0x4b, 0x4c, 0x4d, 0x47, 0x48, 0x49]
      for digit in 0...9 {
         keys.append(GameKey(id: "Keypad\(digit)", name: "Keypad \(digit)", macKeyCode: keypadKeyCodes[digit],
                             scan: keypadScans[digit], vk: 0x60 + digit))
      }
      let functionKeyCodes: [UInt16] = [0x7a, 0x78, 0x63, 0x76, 0x60, 0x61, 0x62, 0x64, 0x65, 0x6d]
      for n in 1...10 {
         keys.append(GameKey(id: "F\(n)", name: "F\(n)", macKeyCode: functionKeyCodes[n - 1], scan: 0x3a + n, vk: 0x6f + n))
      }
      keys.append(GameKey(id: "F12", name: "F12", macKeyCode: 0x6f, scan: 0x58, vk: 0x7b))
      keys += [
         GameKey(id: "Escape", name: "Esc", macKeyCode: 0x35, scan: 0x01, vk: 0x1b),
         GameKey(id: "Tab", name: "Tab", macKeyCode: 0x30, scan: 0x0f, vk: 0x09),
         GameKey(id: "Space", name: "Space", macKeyCode: 0x31, scan: 0x39, vk: 0x20),
         GameKey(id: "Return", name: "Return", macKeyCode: 0x24, scan: 0x1c, vk: 0x0d),
         GameKey(id: "Backspace", name: "Delete", macKeyCode: 0x33, scan: 0x0e, vk: 0x08),
         GameKey(id: "ForwardDelete", name: "⌦", macKeyCode: 0x75, scan: 0x53, vk: 0x2e),
         GameKey(id: "Minus", name: "-", macKeyCode: 0x1b, scan: 0x0c, vk: 0xbd),
         GameKey(id: "Equals", name: "=", macKeyCode: 0x18, scan: 0x0d, vk: 0xbb),
         GameKey(id: "LeftBracket", name: "[", macKeyCode: 0x21, scan: 0x1a, vk: 0xdb),
         GameKey(id: "RightBracket", name: "]", macKeyCode: 0x1e, scan: 0x1b, vk: 0xdd),
         GameKey(id: "Semicolon", name: ";", macKeyCode: 0x29, scan: 0x27, vk: 0xba),
         GameKey(id: "Quote", name: "'", macKeyCode: 0x27, scan: 0x28, vk: 0xde),
         GameKey(id: "Grave", name: "`", macKeyCode: 0x32, scan: 0x29, vk: 0xc0),
         GameKey(id: "Backslash", name: "\\", macKeyCode: 0x2a, scan: 0x2b, vk: 0xdc),
         GameKey(id: "Comma", name: ",", macKeyCode: 0x2b, scan: 0x33, vk: 0xbc),
         GameKey(id: "Period", name: ".", macKeyCode: 0x2f, scan: 0x34, vk: 0xbe),
         GameKey(id: "Slash", name: "/", macKeyCode: 0x2c, scan: 0x35, vk: 0xbf),
         // These share scan codes with keypad keys, as on a PC.
         GameKey(id: "Up", name: "↑", macKeyCode: 0x7e, scan: 0x48, vk: 0x26),
         GameKey(id: "Down", name: "↓", macKeyCode: 0x7d, scan: 0x50, vk: 0x28),
         GameKey(id: "Left", name: "←", macKeyCode: 0x7b, scan: 0x4b, vk: 0x25),
         GameKey(id: "Right", name: "→", macKeyCode: 0x7c, scan: 0x4d, vk: 0x27),
         GameKey(id: "Home", name: "Home", macKeyCode: 0x73, scan: 0x47, vk: 0x24),
         GameKey(id: "End", name: "End", macKeyCode: 0x77, scan: 0x4f, vk: 0x23),
         GameKey(id: "PageUp", name: "Page Up", macKeyCode: 0x74, scan: 0x49, vk: 0x21),
         GameKey(id: "PageDown", name: "Page Down", macKeyCode: 0x79, scan: 0x51, vk: 0x22),
      ]
      return keys
   }()

   static func withID(_ id: String) -> GameKey? { all.first { $0.id == id } }
   static func withMacKeyCode(_ code: UInt16) -> GameKey? { all.first { $0.macKeyCode == code } }

   // All keys that look the same to the game, e.g. ↑ and Keypad 8.
   var sameToGame: [GameKey] { GameKey.all.filter { $0.scan == scan } }
}

struct KeyAction: Identifiable {
   enum Effect {
      case key(Int) // The value config.txt gives the game
      case click(x: Int, y: Int) // A left click on the 320x200 screen
   }
   let id: String
   let title: String
   let section: String
   let effect: Effect
   let defaultKeys: [String]

   // The mode 1 (adventuring) lines of the config.txt that ships with the game.
   static let all: [KeyAction] = {
      var actions = [
         KeyAction(id: "forward", title: "Move forward", section: "Movement", effect: .key(0x480000), defaultKeys: ["K", "Keypad8"]),
         KeyAction(id: "backward", title: "Move backward", section: "Movement", effect: .key(0x500000), defaultKeys: ["Comma", "Keypad5"]),
         KeyAction(id: "left", title: "Move left", section: "Movement", effect: .key(0x4b0000), defaultKeys: ["M", "Keypad4"]),
         KeyAction(id: "right", title: "Move right", section: "Movement", effect: .key(0x4d0000), defaultKeys: ["Period", "Keypad6"]),
         KeyAction(id: "turnLeft", title: "Turn left", section: "Movement", effect: .key(0x520000), defaultKeys: ["J", "Keypad7"]),
         KeyAction(id: "turnRight", title: "Turn right", section: "Movement", effect: .key(0x470000), defaultKeys: ["L", "Keypad9"]),
         KeyAction(id: "freeze", title: "Freeze game", section: "Movement", effect: .key(0x1b), defaultKeys: ["Escape"]),
      ]
      let attackKeys = [["Q", "A", "Z"], ["W", "S", "X"], ["E", "D", "C"], ["R", "F", "V"]]
      let attackX = [0xf0, 0x104, 0x11c, 0x138]
      let attackY = [0x5c, 0x68, 0x74]
      let ordinals = ["first", "second", "third", "fourth", "fifth", "sixth"]
      for champion in 0..<4 {
         for attack in 0..<3 {
            actions.append(KeyAction(id: "attack\(champion + 1)\(attack + 1)",
                                     title: "Champion \(champion + 1): \(ordinals[attack]) attack",
                                     section: "Weapons", effect: .click(x: attackX[champion], y: attackY[attack]),
                                     defaultKeys: [attackKeys[champion][attack]]))
         }
      }
      let runeX = [0xf0, 0xfe, 0x10a, 0x11d, 0x128, 0x136]
      for rune in 0..<6 {
         actions.append(KeyAction(id: "rune\(rune + 1)", title: "\(ordinals[rune].capitalized) rune",
                                  section: "Magic", effect: .click(x: runeX[rune], y: 0x37), defaultKeys: ["\(rune + 1)"]))
      }
      actions += [
         KeyAction(id: "runeBack", title: "Take back a rune", section: "Magic", effect: .click(x: 0x136, y: 0x43), defaultKeys: ["Grave"]),
         KeyAction(id: "cast", title: "Cast spell", section: "Magic", effect: .click(x: 0x10d, y: 0x41), defaultKeys: ["Space"]),
      ]
      for caster in 0..<4 {
         actions.append(KeyAction(id: "caster\(caster + 1)", title: "Champion \(caster + 1) casts",
                                  section: "Magic", effect: .click(x: 0x200 + caster, y: 0x2e), defaultKeys: ["F\(caster + 1)"]))
      }
      actions.append(KeyAction(id: "pass", title: "Pass (cancel attack)", section: "Weapons", effect: .click(x: 0x131, y: 0x50), defaultKeys: ["Tab"]))
      return actions
   }()

   static let sections = ["Movement", "Weapons", "Magic"]
}

// Each action has two slots, primary and alternate.  The bindings live in
// UserDefaults and are written to config.txt in the user's folder, which the
// game reads instead of the one inside the app.
final class KeyBindings: ObservableObject {
   struct Slot: Hashable {
      let action: String
      let index: Int
   }

   @Published private(set) var keys: [String: [String]] // "" is an empty slot
   @Published var capturing: Slot?
   @Published private(set) var unbound: Set<Slot> = []
   @Published private(set) var message: String?
   private var monitor: Any?

   static let slotCount = 2
   private static let defaultsKey = "keyBindings"
   private static let marker = "; Written by the CSBwin launcher (Controls tab)."

   init() {
      let stored = UserDefaults.standard.dictionary(forKey: Self.defaultsKey) as? [String: [String]]
      keys = stored ?? Self.defaults
      monitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { [weak self] event in
         guard let self = self, let slot = self.capturing, !event.modifierFlags.contains(.command) else { return event }
         if let key = GameKey.withMacKeyCode(event.keyCode) {
            self.bind(key, to: slot)
         } else if event.keyCode == 0x67 {
            self.message = "F11 switches full screen in the game."
         } else {
            self.message = "That key can't be used in the game."
         }
         return nil
      }
   }

   static var defaults: [String: [String]] {
      var keys: [String: [String]] = [:]
      for action in KeyAction.all {
         keys[action.id] = (0..<slotCount).map { $0 < action.defaultKeys.count ? action.defaultKeys[$0] : "" }
      }
      return keys
   }

   func key(in slot: Slot) -> GameKey? {
      GameKey.withID(keys[slot.action]?[slot.index] ?? "")
   }

   func isUnbound(_ slot: Slot) -> Bool { unbound.contains(slot) }

   func toggleCapture(_ slot: Slot) {
      capturing = capturing == slot ? nil : slot
      message = nil
   }

   // Binding a key takes it away from whatever else had it.
   func bind(_ key: GameKey, to slot: Slot) {
      capturing = nil
      unbound = []
      message = nil
      let sameToGame = Set(key.sameToGame.map(\.id))
      var notes: [String] = []
      for action in KeyAction.all {
         for index in 0..<Self.slotCount {
            let other = Slot(action: action.id, index: index)
            guard other != slot, let old = self.key(in: other), sameToGame.contains(old.id) else { continue }
            set(other, to: "")
            if action.id != slot.action {
               unbound.insert(other)
               let alias = old.id == key.id ? "" : " (the game can't tell it from \(key.name))"
               notes.append("\(old.name)\(alias) was unbound from “\(action.title)”.")
            }
         }
      }
      set(slot, to: key.id)
      message = notes.isEmpty ? nil : notes.joined(separator: " ")
      save()
   }

   func clear(_ slot: Slot) {
      capturing = nil
      unbound = []
      message = nil
      set(slot, to: "")
      save()
   }

   func resetToDefaults() {
      capturing = nil
      unbound = []
      message = "Controls were reset to the defaults."
      keys = Self.defaults
      save()
   }

   private func set(_ slot: Slot, to id: String) {
      var slots = keys[slot.action] ?? Array(repeating: "", count: Self.slotCount)
      slots[slot.index] = id
      keys[slot.action] = slots
   }

   private func save() {
      UserDefaults.standard.set(keys, forKey: Self.defaultsKey)
      do {
         try writeConfig()
      } catch {
         message = "Couldn't write config.txt: \(error.localizedDescription)"
      }
   }

   // config.txt is the bundled one with its mode 1 key lines replaced.
   private func writeConfig() throws {
      guard let bundled = Bundle.main.url(forResource: "config", withExtension: "txt") else { return }
      let folder = GameRunner.userFolder
      let url = folder.appendingPathComponent("config.txt")
      try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
      // Keep a config.txt the player wrote by hand.
      if let existing = try? String(contentsOf: url, encoding: .isoLatin1), !existing.hasPrefix(Self.marker) {
         let backup = folder.appendingPathComponent("config.txt.bak")
         try? FileManager.default.removeItem(at: backup)
         try FileManager.default.moveItem(at: url, to: backup)
      }
      var lines = [Self.marker, "; Change the adventuring keys there; they replace any key/scan/mscan mode 1 lines.", ";"]
      for line in try String(contentsOf: bundled, encoding: .isoLatin1).components(separatedBy: .newlines) {
         let fields = line.split(whereSeparator: { $0 == " " || $0 == "\t" })
         if fields.count > 1, ["key", "scan", "mscan"].contains(fields[0].lowercased()), fields[1] == "1" {
            continue
         }
         lines.append(line)
      }
      lines.append(";")
      lines.append("; Adventuring keys (mode 1)")
      for action in KeyAction.all {
         for id in keys[action.id] ?? [] {
            guard let key = GameKey.withID(id) else { continue }
            let comment = "; \(key.name) = \(action.title)"
            switch action.effect {
            case .key(let value) where key.id == "Escape":
               // Typed rather than scanned, so Esc also pauses a replay.
               lines.append(String(format: "key  1 %06x %06x  ", 0x1b, value) + comment)
            case .key(let value):
               lines.append(String(format: "scan 1 %06x %06x  ", key.scan, value) + comment)
            case .click(let x, let y):
               for alias in key.sameToGame {
                  lines.append(String(format: "mscan 1 %04x %x %x L  ", alias.vk, x, y) + comment)
               }
            }
         }
      }
      try (lines.joined(separator: "\n") + "\n").write(to: url, atomically: true, encoding: .utf8)
   }
}

struct ControlsView: View {
   @ObservedObject var bindings: KeyBindings

   var body: some View {
      VStack(alignment: .leading, spacing: 8) {
         HStack {
            Text("Action").frame(maxWidth: .infinity, alignment: .leading)
            Text("Primary").frame(width: 100)
            Text("Alternate").frame(width: 100)
         }
         .font(.caption)
         .foregroundColor(.secondary)
         .padding(.horizontal, 8)
         ScrollView {
            VStack(alignment: .leading, spacing: 4) {
               ForEach(KeyAction.sections, id: \.self) { section in
                  Text(section).font(.headline).padding(.top, 6)
                  ForEach(KeyAction.all.filter { $0.section == section }) { action in
                     HStack {
                        Text(action.title).frame(maxWidth: .infinity, alignment: .leading)
                        ForEach(0..<KeyBindings.slotCount, id: \.self) { index in
                           KeySlotButton(bindings: bindings, slot: KeyBindings.Slot(action: action.id, index: index))
                        }
                     }
                  }
               }
            }
            .padding(.horizontal, 8)
         }
         HStack(alignment: .top) {
            if let message = bindings.message {
               Image(systemName: "exclamationmark.triangle.fill").foregroundColor(.orange)
               Text(message).fixedSize(horizontal: false, vertical: true)
            } else {
               Text("Click a key, then press the new one. Right-click to clear. The arrows, Home, End and Page keys work like the keypad keys they share with (↑ is Keypad 8).")
                  .foregroundColor(.secondary)
                  .fixedSize(horizontal: false, vertical: true)
            }
            Spacer()
            Button("Reset to Defaults") { bindings.resetToDefaults() }
         }
         .font(.caption)
      }
      .padding(12)
      .onDisappear { bindings.capturing = nil }
   }
}

struct KeySlotButton: View {
   @ObservedObject var bindings: KeyBindings
   let slot: KeyBindings.Slot

   var body: some View {
      let capturing = bindings.capturing == slot
      Button(action: { bindings.toggleCapture(slot) }) {
         Text(capturing ? "Press a key…" : bindings.key(in: slot)?.name ?? "—")
            .frame(width: 88)
      }
      .overlay(RoundedRectangle(cornerRadius: 6)
         .stroke(capturing ? Color.accentColor : Color.orange, lineWidth: 2)
         .opacity(capturing || bindings.isUnbound(slot) ? 1 : 0))
      .contextMenu {
         Button("Clear") { bindings.clear(slot) }
      }
      .frame(width: 100)
   }
}

struct LauncherView: View {
   @StateObject private var runner = GameRunner()
   @StateObject private var bindings = KeyBindings()

   var body: some View {
      TabView {
         PlayView(runner: runner).tabItem { Text("Play") }
         SettingsView(runner: runner).tabItem { Text("Settings") }
         ControlsView(bindings: bindings).tabItem { Text("Controls") }
      }
      .padding(12)
      .frame(width: 520, height: 470)
   }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
   func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

@main
struct CSBwinLauncher: App {
   @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

   var body: some Scene {
      WindowGroup("CSBwin") {
         LauncherView()
      }
      .commands {
         CommandGroup(replacing: .newItem) {}
      }
   }
}
