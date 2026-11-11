import QtQuick
import Quickshell.Io
import "NightlightModel.js" as NightlightModel
import qs.Commons

Item {
  id: root

  // Injected by quattro-shell (the first-party service loader).
  property var shell: null

  // Keep in sync with bin/quattro-toggle-nightlight, which sets the same
  // temperatures for callers outside the shell (keybindings, menu, ssh).
  readonly property int nightTemperature: 4000
  readonly property int dayTemperature: 6500

  property bool stateLoaded: false
  property var temperature: null
  readonly property bool enabled: stateLoaded && NightlightModel.isNightlight(temperature)

  property bool hasPendingTemperature: false
  property int pendingTemperature: 0
  property bool refreshPending: false

  function refresh() {
    if (statusProbe.running) {
      root.refreshPending = true
      return
    }
    root.refreshPending = false
    statusProbe.running = true
  }

  function setNightlight(value) {
    applyTemperature(value ? nightTemperature : dayTemperature)
  }

  function toggle() {
    setNightlight(!enabled)
  }

  function applyTemperature(temp) {
    root.temperature = temp
    root.stateLoaded = true

    if (applyProcess.running) {
      root.pendingTemperature = temp
      root.hasPendingTemperature = true
      return
    }

    runApply(temp)
  }

  function runApply(temp) {
    applyProcess.command = ["quattro-nightlight", "set", String(Number(temp))]
    applyProcess.running = true
  }

  Process {
    id: statusProbe
    command: ["quattro-nightlight", "get"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        if (applyProcess.running || root.hasPendingTemperature) return
        root.temperature = NightlightModel.temperatureFromOutput(text)
        root.stateLoaded = true
      }
    }
    onExited: function(exitCode) {
      if (exitCode !== 0 && !applyProcess.running && !root.hasPendingTemperature) {
        root.temperature = null
        root.stateLoaded = true
      }
      if (root.refreshPending) Qt.callLater(root.refresh)
    }
  }

  Process {
    id: applyProcess
    onExited: function() {
      if (root.hasPendingTemperature) {
        var pending = root.pendingTemperature
        root.hasPendingTemperature = false
        Qt.callLater(function() { root.runApply(pending) })
        return
      }

      root.refresh()
    }
  }

  Component.onCompleted: refresh()

  ShellIpc {
    target: "nightlight"

    function status(): string {
      return JSON.stringify({ enabled: root.enabled, temperature: root.temperature })
    }

    function refresh(): void {
      root.refresh()
    }

    function enable(): string {
      root.setNightlight(true)
      return "enabled"
    }

    function disable(): string {
      root.setNightlight(false)
      return "disabled"
    }

    function toggle(): string {
      var enabling = !root.enabled
      root.setNightlight(enabling)
      return enabling ? "enabled" : "disabled"
    }
  }
}
