pragma Singleton
import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Hyprland

// What the shell needs to know about the running compositor, in one place.
//
// Hyprland is read natively through Quickshell.Hyprland. Everything else (sway,
// niri, or any compositor you wire up yourself) goes through
// `quattro-compositor watch`, a long-running helper that prints one JSON line
// every time its state changes:
//
//   {"focusedOutput":"DP-1","workspaces":[
//      {"id":"1","label":"1","output":"DP-1","active":true,"focused":true,"occupied":true}]}
//
// A compositor with no workspace feed (plain dwl, say) simply reports none, and
// the Workspaces widget hides itself. Everything else in the shell works from
// Wayland protocols and does not depend on this file.
QtObject {
  id: root

  // QUATTRO_COMPOSITOR overrides detection: hyprland | sway | niri | generic
  readonly property string kind: {
    var forced = Quickshell.env("QUATTRO_COMPOSITOR") || ""
    if (forced !== "") return forced
    if (Quickshell.env("HYPRLAND_INSTANCE_SIGNATURE")) return "hyprland"
    if (Quickshell.env("SWAYSOCK")) return "sway"
    if (Quickshell.env("NIRI_SOCKET")) return "niri"
    return "generic"
  }

  readonly property bool isHyprland: kind === "hyprland"

  // Whether per-output scale and geometry are known to the shell ahead of Qt.
  // Overlays wait for it on Hyprland and go ahead without it elsewhere.
  readonly property bool hasMonitorInfo: isHyprland

  // ---------------------------------------------------------------- stream

  property var streamState: ({ focusedOutput: "", workspaces: [] })

  function ingest(line) {
    var text = String(line || "").trim()
    if (text === "") return
    try {
      var parsed = JSON.parse(text)
      if (parsed && Array.isArray(parsed.workspaces)) {
        root.streamState = {
          focusedOutput: String(parsed.focusedOutput || ""),
          workspaces: parsed.workspaces
        }
      }
    } catch (e) {
      console.warn("quattro-compositor: unreadable line:", text.slice(0, 120))
    }
  }

  property Process watcher: Process {
    command: ["quattro-compositor", "watch"]
    running: !root.isHyprland
    stdout: SplitParser {
      onRead: function(data) { root.ingest(data) }
    }
    // A helper that dies with an error is started again once the compositor is
    // back. Exit code 0 means there is nothing to watch, and it stays off.
    onExited: function(exitCode) {
      if (exitCode !== 0 && !root.isHyprland) restartTimer.restart()
    }
  }

  property Timer restartTimer: Timer {
    interval: 3000
    onTriggered: root.watcher.running = true
  }

  // --------------------------------------------------------------- hyprland

  readonly property var hyprWorkspaces: {
    var out = []
    if (!root.isHyprland) return out
    var values = Hyprland.workspaces.values
    for (var i = 0; i < values.length; i++) {
      var workspace = values[i]
      if (workspace.id <= 0) continue
      var monitor = workspace.monitor
      out.push({
        id: String(workspace.id),
        label: String(workspace.id),
        output: monitor ? String(monitor.name || "") : "",
        active: !!monitor && !!monitor.activeWorkspace && monitor.activeWorkspace.id === workspace.id,
        focused: workspace.focused === true,
        occupied: workspace.toplevels.values.length > 0
      })
    }
    return out
  }

  // --------------------------------------------------------------- uniform

  readonly property var workspaces: isHyprland ? hyprWorkspaces : streamState.workspaces
  readonly property bool hasWorkspaces: workspaces.length > 0

  // The output that has focus, which is where a keyboard-summoned panel, the
  // OSD or a notification deck belongs. Empty when the compositor does not say.
  readonly property string focusedMonitorName: {
    if (isHyprland) {
      var monitor = Hyprland.focusedMonitor
      return monitor ? String(monitor.name || "") : ""
    }
    return streamState.focusedOutput
  }

  function workspacesOn(outputName) {
    var all = root.workspaces
    var outputs = {}
    for (var i = 0; i < all.length; i++) outputs[all[i].output] = true
    // One output, or a compositor that does not say which workspace lives where:
    // everything belongs to it.
    if (Object.keys(outputs).length <= 1) return all
    var mine = []
    for (var j = 0; j < all.length; j++)
      if (all[j].output === outputName) mine.push(all[j])
    return mine
  }

  function focusWorkspace(id, outputName) {
    Quickshell.execDetached(["quattro-compositor", "focus-workspace", String(id), String(outputName || "")])
  }

  function refreshMonitors() {
    if (root.isHyprland) Hyprland.refreshMonitors()
  }
}
