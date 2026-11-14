import QtQuick
import qs.Ui

BarWidget {
  id: root
  moduleName: "quattro.menu"

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "󰣇"
    onPressed: function(button) {
      if (!root.bar) return
      if (button === Qt.RightButton) root.bar.run("xdg-terminal-exec")
      else root.bar.run("quattro-shell shell toggle quattro.menu '{\"menu\":\"root\"}'")
    }
  }
}
