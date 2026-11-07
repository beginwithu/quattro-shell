import QtQuick
import Quickshell
import qs.Commons

// The app launcher. Quattro's floating-workspace dock was tied to Hyprland and
// its layout scripts, so only the app grid survives here: `quattro-shell
// quattro.dock apps` toggles it, and the menu's Apps entry opens it.
Item {
  id: root

  property var shell: null
  property var manifest: null

  readonly property var appLibrary: shell ? shell.appLibrary : null

  ShellIpc {
    target: "quattro.dock"

    function refresh(): void { }
    function apps(): void { appGrid.toggle() }
  }

  AppGrid {
    id: appGrid
    appLibrary: root.appLibrary
    dock: null
  }
}
