import Quickshell
import QtQuick
import qs.Commons

ShellRoot {
  id: standaloneRoot

  readonly property string quattroPath: Quickshell.env("QUATTRO_PATH")

  Loader {
    id: screensaverLoader
    source: Qt.resolvedUrl("plugins/screensaver/Screensaver.qml")
    onLoaded: {
      item.quattroPath = standaloneRoot.quattroPath
      item.standalone = true
      item.open("{}")
    }
  }
}
