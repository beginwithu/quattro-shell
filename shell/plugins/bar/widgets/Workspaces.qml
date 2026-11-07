import QtQuick
import QtQuick.Layouts
import Quickshell
import qs.Commons
import qs.Ui

// Workspace pills for the output this bar is on. The data comes from
// Commons/Compositor.qml, so the same widget serves Hyprland (native), sway and
// niri (through quattro-compositor) and anything you wire up yourself. On a
// compositor that reports no workspaces the widget takes no space.
BarWidget {
  id: root
  moduleName: "quattro.workspaces"

  // Numbered workspaces that always show, whether or not they exist yet. Only
  // used where workspaces are created on demand (Hyprland, sway).
  readonly property int minimumSlots: Math.max(0, Math.round(Number(root.setting("slots", 5)) || 0))

  readonly property string outputName: {
    var window = root.QsWindow.window
    var screen = window ? window.screen : null
    return screen ? String(screen.name || "") : ""
  }

  function isNumeric(value) {
    return /^[0-9]+$/.test(String(value))
  }

  readonly property var entries: {
    var list = Compositor.workspacesOn(root.outputName).slice()
    var createdOnDemand = Compositor.kind === "hyprland" || Compositor.kind === "sway"

    if (createdOnDemand && root.minimumSlots > 0) {
      var present = {}
      for (var i = 0; i < list.length; i++) present[String(list[i].id)] = true
      var outputs = {}
      var all = Compositor.workspaces
      for (var j = 0; j < all.length; j++) outputs[all[j].output] = true
      // Fillers only make sense when every workspace shares one row of numbers.
      if (Object.keys(outputs).length <= 1) {
        for (var n = 1; n <= root.minimumSlots; n++) {
          if (!present[String(n)])
            list.push({ id: String(n), label: String(n), output: root.outputName,
                        active: false, focused: false, occupied: false })
        }
      }
    }

    var allNumeric = list.length > 0
    for (var k = 0; k < list.length; k++) if (!root.isNumeric(list[k].id)) allNumeric = false
    if (allNumeric) list.sort(function(left, right) { return Number(left.id) - Number(right.id) })
    return list
  }

  readonly property bool hasEntries: entries.length > 0

  function focusWorkspace(id) {
    Compositor.focusWorkspace(id, root.outputName)
  }

  // A wheel notch moves one entry along the row, and a touchpad adds up its
  // smaller deltas until they make a notch's worth.
  property real wheelAccumulator: 0

  function activeIndex() {
    for (var i = 0; i < entries.length; i++) if (entries[i].active) return i
    return -1
  }

  function scrollWorkspaces(delta) {
    var wheel = Util.wheelSteps(root.wheelAccumulator, delta)
    root.wheelAccumulator = wheel.remainder
    if (wheel.steps === 0) return

    // Holds at either end rather than wrapping, so a touchpad swipe that runs
    // long stops on the last workspace.
    var index = root.activeIndex()
    if (index === -1) return
    var target = Util.clamp(index - wheel.steps, 0, entries.length - 1)
    if (target !== index) root.focusWorkspace(entries[target].id)
  }

  readonly property real trailingGap: root.vertical ? 0 : Style.spaceReal(1.5)

  visible: hasEntries
  implicitWidth: hasEntries ? grid.implicitWidth + trailingGap : 0
  implicitHeight: hasEntries ? grid.implicitHeight : 0

  GridLayout {
    id: grid
    anchors.fill: parent
    anchors.rightMargin: root.trailingGap
    columns: root.vertical ? 1 : Math.max(1, root.entries.length)
    columnSpacing: root.vertical ? 0 : Style.space(1)
    rowSpacing: root.vertical ? Style.space(2) : 0

    Repeater {
      model: root.entries

      WidgetButton {
        required property var modelData

        readonly property bool shown: modelData.active === true

        bar: root.bar
        text: shown ? "󱓻" : String(modelData.label)
        opacity: modelData.occupied || shown ? 1 : 0.5
        horizontalMargin: 6
        verticalPadding: 6
        fixedWidth: root.vertical ? root.barSize : Style.space(20)
        fixedHeight: root.barSize
        onPressed: function() {
          if (!shown) root.focusWorkspace(modelData.id)
        }
        onWheelMoved: function(delta) { root.scrollWorkspaces(delta) }
      }
    }
  }
}
