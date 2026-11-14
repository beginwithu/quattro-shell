import Quickshell
import QtQuick
import qs.Commons

ShellRoot {
  id: standaloneRoot

  readonly property string quattroPath: Quickshell.env("QUATTRO_PATH")

  Loader {
    id: pickerLoader
    source: Qt.resolvedUrl("plugins/share-picker/SharePicker.qml")
    onLoaded: {
      item.quattroPath = standaloneRoot.quattroPath
      item.standalone = true
      var payload = Quickshell.env("QUATTRO_SHARE_PICKER_PAYLOAD") || "{}"
      item.open(payload)
    }
  }
}
