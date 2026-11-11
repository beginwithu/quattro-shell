pragma Singleton
import QtQuick
import Quickshell
import Quickshell.Hyprland

// What the shell needs to know about Hyprland's workspaces and monitors, in
// one place. Quattro runs on Hyprland only, so everything is read natively
// through Quickshell.Hyprland.
QtObject {
  id: root

  // Every regular workspace (special and named-negative ones are left out) as
  //   { id, label, output, active, focused, occupied }
  readonly property var workspaces: {
    var out = []
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

  readonly property bool hasWorkspaces: workspaces.length > 0

  // The output that has focus, which is where a keyboard-summoned panel, the
  // OSD or a notification deck belongs. Empty until Hyprland has reported one.
  readonly property string focusedMonitorName: {
    var monitor = Hyprland.focusedMonitor
    return monitor ? String(monitor.name || "") : ""
  }

  function workspacesOn(outputName) {
    var all = root.workspaces
    var outputs = {}
    for (var i = 0; i < all.length; i++) outputs[all[i].output] = true
    // With a single output everything belongs to it.
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
    Hyprland.refreshMonitors()
  }
}
