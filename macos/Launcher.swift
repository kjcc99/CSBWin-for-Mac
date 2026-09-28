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

struct LauncherView: View {
   @StateObject private var runner = GameRunner()

   var body: some View {
      TabView {
         PlayView(runner: runner).tabItem { Text("Play") }
         SettingsView(runner: runner).tabItem { Text("Settings") }
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
