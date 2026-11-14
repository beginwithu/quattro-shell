import QtQuick
import Quickshell
import Quickshell.Hyprland
import Quickshell.Wayland
import Quickshell.Io
import qs.Commons
import qs.Commons as Commons

Item {
  id: root

  property string quattroPath: Quickshell.env("QUATTRO_PATH")
  property var shell: null
  property var manifest: null
  property bool standalone: false

  readonly property string home: Quickshell.env("HOME")
  readonly property string userArtPath: home + "/.config/quattro/branding/screensaver.txt"
  readonly property string fallbackArtPath: (quattroPath ? quattroPath : home + "/.local/share/quattro") + "/icon.txt"

  property bool opened: false
  property bool armed: false
  property string userArtText: ""
  property string fallbackArtText: ""
  readonly property string artText: {
    var u = trimArt(userArtText)
    if (u.length > 0) return u
    var f = trimArt(fallbackArtText)
    if (f.length > 0) return f
    return "QUATTRO"
  }

  property var palette: [Commons.Color.accent, Commons.Color.foreground]
  property string clockTime: ""
  property string clockDate: ""

  function trimArt(raw) {
    var s = String(raw || "").replace(/\r\n/g, "\n").replace(/^\n+|\n+$/g, "")
    return s
  }

  function parsePalette(raw) {
    var keys = ["accent", "blue", "cyan", "green", "magenta", "orange", "yellow", "red", "color4", "color6", "color2", "color5", "color3", "color1"]
    var found = ({})
    var lines = String(raw || "").split("\n")
    for (var i = 0; i < lines.length; i++) {
      var m = lines[i].match(/^\s*([A-Za-z0-9_-]+)\s*=\s*["']?(#[0-9A-Fa-f]{6})/)
      if (m) found[m[1]] = m[2]
    }
    var out = []
    for (var k = 0; k < keys.length; k++) {
      var c = found[keys[k]]
      if (c && out.indexOf(c) === -1) out.push(c)
    }
    if (out.length === 0) out = [Commons.Color.accent, Commons.Color.foreground]
    palette = out
  }

  function updateClock() {
    var now = new Date()
    clockTime = Qt.formatDateTime(now, "HH:mm")
    clockDate = Qt.formatDateTime(now, "dddd, d MMMM")
  }

  function open(_payloadJson) {
    userArtFile.reload()
    colorsFile.reload()
    updateClock()
    armed = false
    opened = true
    armTimer.restart()
  }

  function close() {
    if (!opened) return
    armTimer.stop()
    armed = false
    opened = false
    if (root.standalone) Qt.quit()
  }

  function dismiss() {
    if (!opened) return
    if (root.shell && typeof root.shell.hide === "function") {
      root.shell.hide((root.manifest && root.manifest.id) || "quattro.screensaver")
    } else {
      close()
    }
  }

  Timer {
    id: armTimer
    interval: 450
    repeat: false
    onTriggered: root.armed = true
  }

  Timer {
    interval: 1000
    repeat: true
    running: root.opened
    onTriggered: root.updateClock()
  }

  FileView {
    id: userArtFile
    path: root.userArtPath
    watchChanges: true
    printErrors: false
    onLoaded: root.userArtText = text()
    onLoadFailed: root.userArtText = ""
    onFileChanged: reload()
  }

  FileView {
    id: fallbackArtFile
    path: root.fallbackArtPath
    watchChanges: false
    printErrors: false
    onLoaded: root.fallbackArtText = text()
    onLoadFailed: root.fallbackArtText = ""
  }

  FileView {
    id: colorsFile
    path: Commons.Color.currentThemePath + "/colors.toml"
    watchChanges: false
    printErrors: false
    onLoaded: root.parsePalette(text())
  }

  Component.onCompleted: updateClock()

  Variants {
    model: Quickshell.screens

    delegate: PanelWindow {
      id: win
      required property var modelData

      readonly property bool isFocusedScreen: Hyprland.focusedMonitor && String(Hyprland.focusedMonitor.name || "") === String(modelData.name || "")

      visible: root.opened
      screen: modelData
      anchors { top: true; bottom: true; left: true; right: true }
      color: "transparent"
      exclusionMode: ExclusionMode.Ignore
      WlrLayershell.namespace: "org.quattro.screensaver"
      WlrLayershell.layer: WlrLayer.Overlay
      WlrLayershell.keyboardFocus: isFocusedScreen ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.OnDemand

      onVisibleChanged: {
        contentRevealed = visible
        if (visible) {
          pointerArea.hasInitialPos = false
          canvas.resetMotion()
          if (isFocusedScreen) Qt.callLater(function() { keyCatcher.forceActiveFocus() })
        }
      }

      property bool contentRevealed: false

      Item {
        id: keyCatcher
        anchors.fill: parent
        focus: true
        Keys.onPressed: function(event) {
          if (root.armed || event.key === Qt.Key_Escape || event.key === Qt.Key_Space) {
            event.accepted = true
            root.dismiss()
          }
        }
      }

      Rectangle {
        id: backdrop
        anchors.fill: parent
        color: Commons.Color.background
        opacity: win.contentRevealed ? 1.0 : 0.0
        Behavior on opacity {
          NumberAnimation { duration: Style.duration(350); easing.type: Easing.Bezier; easing.bezierCurve: Style.easeOut }
        }

        // Subtle ambient radial accent glow behind the artwork
        Rectangle {
          width: Math.max(artContainer.width * 1.9, Style.space(420))
          height: width
          radius: width / 2
          x: artContainer.x + artContainer.width / 2 - width / 2
          y: artContainer.y + artContainer.height / 2 - height / 2
          color: artContainer.currentColor
          opacity: 0.06
        }

        // Drifting / bouncing ASCII branding container
        Item {
          id: canvas
          anchors.fill: parent

          property real posX: 0
          property real posY: 0
          property real velX: 110
          property real velY: 82
          property int colorIndex: 0
          property bool initialized: false
          readonly property real edgeMargin: Style.space(36)

          function resetMotion() {
            var maxX = Math.max(edgeMargin, width - artContainer.width - edgeMargin)
            var maxY = Math.max(edgeMargin, height - artContainer.height - edgeMargin)
            posX = Math.max(edgeMargin, Math.min(maxX, (width - artContainer.width) / 2))
            posY = Math.max(edgeMargin, Math.min(maxY, (height - artContainer.height) / 2))
            // Offset velocity slightly per screen index so multi-monitor setups diverge organically
            var dirSign = (Quickshell.screens.indexOf(win.modelData) % 2 === 0) ? 1 : -1
            velX = 115 * dirSign
            velY = 84
            colorIndex = 0
            initialized = width > 0 && height > 0
          }

          onWidthChanged: if (root.opened && !initialized) resetMotion()
          onHeightChanged: if (root.opened && !initialized) resetMotion()

          FrameAnimation {
            running: root.opened && !Style.reduceMotion && win.width > 0 && win.height > 0
            onTriggered: {
              if (!canvas.initialized) canvas.resetMotion()
              var dt = Math.min(0.05, frameTime)
              var minX = canvas.edgeMargin
              var minY = canvas.edgeMargin
              var maxX = Math.max(minX, canvas.width - artContainer.width - canvas.edgeMargin)
              var maxY = Math.max(minY, canvas.height - artContainer.height - canvas.edgeMargin)

              var nx = canvas.posX + canvas.velX * dt
              var ny = canvas.posY + canvas.velY * dt
              var bounced = false

              if (nx <= minX) {
                nx = minX
                canvas.velX = Math.abs(canvas.velX)
                bounced = true
              } else if (nx >= maxX) {
                nx = maxX
                canvas.velX = -Math.abs(canvas.velX)
                bounced = true
              }

              if (ny <= minY) {
                ny = minY
                canvas.velY = Math.abs(canvas.velY)
                bounced = true
              } else if (ny >= maxY) {
                ny = maxY
                canvas.velY = -Math.abs(canvas.velY)
                bounced = true
              }

              canvas.posX = nx
              canvas.posY = ny
              if (bounced && root.palette.length > 0) {
                canvas.colorIndex = (canvas.colorIndex + 1) % root.palette.length
              }
            }
          }

          Item {
            id: artContainer
            readonly property color currentColor: root.palette.length > 0
              ? root.palette[canvas.colorIndex % root.palette.length]
              : Commons.Color.accent

            readonly property real fitScale: {
              if (artTextItem.implicitWidth <= 0 || artTextItem.implicitHeight <= 0 || canvas.width <= 0 || canvas.height <= 0)
                return 1.0
              var maxW = canvas.width * 0.55
              var maxH = canvas.height * 0.55
              return Math.min(1.2, maxW / artTextItem.implicitWidth, maxH / artTextItem.implicitHeight)
            }

            width: Math.max(1, Math.round(artTextItem.implicitWidth * fitScale))
            height: Math.max(1, Math.round(artTextItem.implicitHeight * fitScale))
            x: Style.reduceMotion ? Math.round((canvas.width - width) / 2) : Math.round(canvas.posX)
            y: Style.reduceMotion ? Math.round((canvas.height - height) / 2) : Math.round(canvas.posY)

            Text {
              id: artTextItem
              anchors.centerIn: parent
              scale: artContainer.fitScale
              textFormat: Text.PlainText
              text: root.artText
              font.family: Style.font.family
              font.pixelSize: Style.space(18)
              font.bold: true
              lineHeight: 0.95
              color: artContainer.currentColor

              Behavior on color {
                ColorAnimation { duration: Style.duration(450); easing.type: Easing.Bezier; easing.bezierCurve: Style.easeOut }
              }
            }
          }
        }

        // Subtle clock in bottom-right corner
        Column {
          anchors.right: parent.right
          anchors.bottom: parent.bottom
          anchors.rightMargin: Style.space(32)
          anchors.bottomMargin: Style.space(28)
          spacing: Style.spacing.xs
          opacity: 0.45

          Text {
            textFormat: Text.PlainText
            text: root.clockTime
            color: Commons.Color.foreground
            font.family: Style.font.family
            font.pixelSize: Style.font.heading
            font.bold: true
            anchors.right: parent.right
          }

          Text {
            textFormat: Text.PlainText
            text: root.clockDate
            color: Commons.Color.muted
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
            anchors.right: parent.right
          }
        }

        MouseArea {
          id: pointerArea
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.BlankCursor
          property bool hasInitialPos: false
          property real startX: 0
          property real startY: 0

          onPressed: {
            if (root.armed) root.dismiss()
          }

          onPositionChanged: function(mouse) {
            if (!root.armed) {
              startX = mouse.x
              startY = mouse.y
              hasInitialPos = true
              return
            }
            if (!hasInitialPos) {
              startX = mouse.x
              startY = mouse.y
              hasInitialPos = true
              return
            }
            var dist = Math.abs(mouse.x - startX) + Math.abs(mouse.y - startY)
            if (dist > 16) root.dismiss()
          }
        }
      }
    }
  }
}
