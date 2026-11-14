import QtQuick
import Quickshell
import Quickshell.Hyprland
import Quickshell.Io
import Quickshell.Wayland
import qs.Commons
import qs.Commons as Commons
import qs.Ui
import "SharePickerModel.js" as SharePickerModel

Item {
  id: root

  property string quattroPath: Quickshell.env("QUATTRO_PATH")
  property var shell: null
  property var manifest: null
  property bool standalone: false

  Commons.Translations {
    id: strings
    directory: Qt.resolvedUrl("i18n")
  }

  // Request state
  property bool opened: false
  property bool hidingForRegion: false
  property bool requestActive: false
  property string selectionFile: ""
  property string doneFile: ""
  property bool allowToken: false
  property bool useWorkspaceId: true
  property bool rememberChoice: false
  property int activeTab: 0 // 0=Monitors, 1=Windows, 2=Workspaces, 3=Region

  // Raw lists from XDPH payload
  property var rawOutputs: []
  property var rawWindows: []
  property var rawWorkspaces: []

  // Selection indices per tab
  property int selectedMonitorIndex: 0
  property int selectedWindowIndex: 0
  property int selectedWorkspaceIndex: 0
  property string filterText: ""
  property bool filterFocused: false

  // Theme tokens
  property string fontFamily: Style.font.menuFamily
  property color background: Commons.Color.menu.background
  property color foreground: Commons.Color.menu.text
  property color border: Commons.Color.menu.border
  property color accent: Commons.Color.accent
  property color muted: Commons.Color.muted
  property color scrim: Commons.Color.menu.scrim
  property var borderSpec: Border.surfaceSpec("menu", "border", border, Math.max(1, Style.space(2)))
  readonly property int cornerRadius: Style.cornerRadius

  // Built entries bound to live Quickshell / Hyprland state so ScreencopyView
  // captures monitors and windows cleanly.
  readonly property var monitorEntries: SharePickerModel.buildMonitorEntries(
    root.rawOutputs,
    Quickshell.screens,
    Hyprland.monitors.values
  )
  readonly property var allWindowEntries: SharePickerModel.buildWindowEntries(
    root.rawWindows,
    Hyprland.toplevels.values
  )
  readonly property var windowEntries: SharePickerModel.filterWindows(
    root.allWindowEntries,
    root.filterText
  )
  readonly property var workspaceEntries: SharePickerModel.buildWorkspaceEntries(
    root.rawWorkspaces,
    Hyprland.workspaces.values,
    Quickshell.screens,
    Hyprland.toplevels.values
  )

  readonly property int cardWidth: Math.min(Style.space(980), Math.max(Style.space(520), panel.width - Style.gapsOut * 2))
  readonly property int cardHeight: Math.min(Style.space(660), Math.max(Style.space(420), panel.height - Style.gapsOut * 2))
  readonly property int gridColumns: {
    var innerW = cardWidth - Style.spacing.panelPadding * 2
    if (activeTab === 0) {
      if (monitorEntries.length <= 1) return 1
      if (monitorEntries.length === 2 || innerW < Style.space(760)) return 2
      return 3
    }
    if (innerW >= Style.space(780)) return 3
    if (innerW >= Style.space(500)) return 2
    return 1
  }

  readonly property bool canShareCurrent: {
    if (activeTab === 0) return monitorEntries.length > 0 && selectedMonitorIndex >= 0 && selectedMonitorIndex < monitorEntries.length
    if (activeTab === 1) return windowEntries.length > 0 && selectedWindowIndex >= 0 && selectedWindowIndex < windowEntries.length
    if (activeTab === 2) return workspaceEntries.length > 0 && selectedWorkspaceIndex >= 0 && selectedWorkspaceIndex < workspaceEntries.length
    if (activeTab === 3) return monitorEntries.length > 0
    return false
  }

  onWindowEntriesChanged: {
    if (selectedWindowIndex >= windowEntries.length) {
      selectedWindowIndex = Math.max(0, windowEntries.length - 1)
    }
  }

  onMonitorEntriesChanged: {
    if (selectedMonitorIndex >= monitorEntries.length) {
      selectedMonitorIndex = Math.max(0, monitorEntries.length - 1)
    }
  }

  onWorkspaceEntriesChanged: {
    if (selectedWorkspaceIndex >= workspaceEntries.length) {
      selectedWorkspaceIndex = Math.max(0, workspaceEntries.length - 1)
    }
  }

  function open(payloadJson) {
    var payload = ({})
    try { payload = JSON.parse(payloadJson || "{}") } catch (e) { payload = ({}) }

    // If a prior request was still active without finishing, release its doneFile.
    if (root.requestActive && root.doneFile && root.doneFile !== String(payload.doneFile || "")) {
      releaseDoneFile(root.doneFile)
    }

    Hyprland.refreshMonitors()
    Hyprland.refreshWorkspaces()
    Hyprland.refreshToplevels()

    root.selectionFile = String(payload.selectionFile || "")
    root.doneFile = String(payload.doneFile || "")
    root.allowToken = payload.allowToken === true
    root.useWorkspaceId = payload.useWorkspaceId !== false
    root.rememberChoice = false
    root.rawOutputs = Array.isArray(payload.outputs) ? payload.outputs : []
    root.rawWindows = Array.isArray(payload.windows) ? payload.windows : []
    root.rawWorkspaces = Array.isArray(payload.workspaces) ? payload.workspaces : []
    root.filterText = ""
    root.filterFocused = false
    root.hidingForRegion = false

    // Default to focused monitor on Monitors tab, or Windows if no monitors.
    var focusedMon = Hyprland.focusedMonitor ? String(Hyprland.focusedMonitor.name || "") : ""
    var monIdx = 0
    for (var i = 0; i < root.rawOutputs.length; i++) {
      if (String(root.rawOutputs[i].name || "") === focusedMon) {
        monIdx = i
        break
      }
    }
    root.selectedMonitorIndex = monIdx
    root.selectedWindowIndex = 0
    root.selectedWorkspaceIndex = 0
    root.activeTab = 0
    root.requestActive = !!root.doneFile
    root.opened = true

    Qt.callLater(function() { keyCatcher.forceActiveFocus() })
  }

  function close() {
    cancel()
  }

  function releaseDoneFile(path) {
    if (!path) return
    releaseProc.command = ["bash", "-c", ": > " + Util.shellQuote(path)]
    releaseProc.running = true
  }

  function finishWithSelection(target) {
    var activeSelectionFile = root.selectionFile
    var activeDoneFile = root.doneFile
    root.requestActive = false
    root.hidingForRegion = false
    root.opened = false
    root.selectionFile = ""
    root.doneFile = ""

    if (root.shell && typeof root.shell.hide === "function") {
      root.shell.hide((root.manifest && root.manifest.id) || "quattro.share-picker")
    }

    var line = SharePickerModel.formatSelection(target, root.allowToken && root.rememberChoice)
    if (activeSelectionFile && activeDoneFile) {
      resultProc.command = [
        "bash", "-c",
        "printf '%s\\n' " + Util.shellQuote(line) + " > " + Util.shellQuote(activeSelectionFile)
        + "; : > " + Util.shellQuote(activeDoneFile)
      ]
      resultProc.running = true
    } else {
      console.log(line)
      if (root.standalone) Qt.quit()
    }
  }

  function cancel() {
    var activeDoneFile = root.doneFile
    root.requestActive = false
    root.hidingForRegion = false
    root.opened = false
    root.selectionFile = ""
    root.doneFile = ""
    root.filterText = ""

    if (root.shell && typeof root.shell.hide === "function") {
      root.shell.hide((root.manifest && root.manifest.id) || "quattro.share-picker")
    }

    if (activeDoneFile) {
      resultProc.command = ["bash", "-c", ": > " + Util.shellQuote(activeDoneFile)]
      resultProc.running = true
    } else if (root.standalone) {
      Qt.quit()
    }
  }

  function confirmSelection() {
    if (activeTab === 0) {
      if (selectedMonitorIndex < 0 || selectedMonitorIndex >= monitorEntries.length) return
      finishWithSelection("screen:" + monitorEntries[selectedMonitorIndex].name)
    } else if (activeTab === 1) {
      if (selectedWindowIndex < 0 || selectedWindowIndex >= windowEntries.length) return
      finishWithSelection("window:" + windowEntries[selectedWindowIndex].id)
    } else if (activeTab === 2) {
      if (selectedWorkspaceIndex < 0 || selectedWorkspaceIndex >= workspaceEntries.length) return
      var ws = workspaceEntries[selectedWorkspaceIndex]
      if (root.useWorkspaceId && ws.id) {
        finishWithSelection("workspace-id:" + ws.id)
      } else {
        finishWithSelection("workspace:" + ws.name)
      }
    } else if (activeTab === 3) {
      startRegionSelection()
    }
  }

  function startRegionSelection() {
    if (monitorEntries.length === 0 || slurpProc.running) return
    root.hidingForRegion = true
    root.opened = false
    slurpDelayTimer.restart()
  }

  function currentCount() {
    if (activeTab === 0) return monitorEntries.length
    if (activeTab === 1) return windowEntries.length
    if (activeTab === 2) return workspaceEntries.length
    return 0
  }

  function currentIndex() {
    if (activeTab === 0) return selectedMonitorIndex
    if (activeTab === 1) return selectedWindowIndex
    if (activeTab === 2) return selectedWorkspaceIndex
    return 0
  }

  function setCurrentIndex(idx) {
    var n = currentCount()
    if (n <= 0) return
    var clamped = Math.max(0, Math.min(n - 1, idx))
    if (activeTab === 0) selectedMonitorIndex = clamped
    else if (activeTab === 1) selectedWindowIndex = clamped
    else if (activeTab === 2) selectedWorkspaceIndex = clamped
  }

  function moveGridSelection(dx, dy) {
    var n = currentCount()
    if (n <= 0) return
    var cols = Math.max(1, gridColumns)
    var cur = currentIndex()
    var next = cur + dx + dy * cols
    if (next >= 0 && next < n) setCurrentIndex(next)
  }

  Process {
    id: resultProc
    onExited: function() {
      if (root.standalone) Qt.quit()
    }
  }

  Process {
    id: releaseProc
  }

  Timer {
    id: slurpDelayTimer
    interval: 140
    repeat: false
    onTriggered: {
      slurpProc.stdoutText = ""
      slurpProc.running = true
    }
  }

  Process {
    id: slurpProc
    property string stdoutText: ""
    command: ["slurp", "-f", "%o@%x,%y,%w,%h"]
    stdout: SplitParser {
      splitMarker: ""
      onRead: function(data) { slurpProc.stdoutText += String(data || "") }
    }
    onExited: function(exitCode) {
      var geom = String(slurpProc.stdoutText || "").trim()
      if (exitCode === 0 && geom.length > 0) {
        root.finishWithSelection("region:" + geom)
      } else {
        // Re-open the picker if slurp was cancelled with Escape.
        root.hidingForRegion = false
        root.opened = true
        Qt.callLater(function() { keyCatcher.forceActiveFocus() })
      }
    }
  }

  OverlayWindow {
    id: panel
    shown: root.opened && !root.hidingForRegion
    WlrLayershell.namespace: "quattro-share-picker"

    Rectangle {
      anchors.fill: parent
      color: root.scrim
    }

    MouseArea {
      anchors.fill: parent
      onClicked: root.cancel()
    }

    BorderSurface {
      id: card
      width: root.cardWidth
      height: root.cardHeight
      radius: root.cornerRadius
      anchors.centerIn: parent
      color: root.background
      borderSpec: root.borderSpec
      padding: Style.spacing.panelPadding

      scale: panel.contentRevealed ? 1.0 : 0.96
      Behavior on scale {
        NumberAnimation { duration: Style.duration(160); easing.type: Easing.Bezier; easing.bezierCurve: Style.easeOut }
      }

      // Swallow clicks inside the dialog card so they don't dismiss the scrim.
      MouseArea { anchors.fill: parent; onClicked: {} }

      Item {
        id: keyCatcher
        anchors.fill: parent
        focus: true

        Keys.priority: Keys.BeforeItem
        Keys.onPressed: function(event) {
          if (event.key === Qt.Key_Escape) {
            if (root.activeTab === 1 && root.filterText.length > 0) {
              root.filterText = ""
              filterInput.text = ""
            } else {
              root.cancel()
            }
            event.accepted = true
          } else if (event.key === Qt.Key_Tab) {
            root.filterFocused = false
            root.activeTab = (root.activeTab + 1) % 4
            keyCatcher.forceActiveFocus()
            event.accepted = true
          } else if (event.key === Qt.Key_Backtab) {
            root.filterFocused = false
            root.activeTab = (root.activeTab + 3) % 4
            keyCatcher.forceActiveFocus()
            event.accepted = true
          } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.confirmSelection()
            event.accepted = true
          } else if (event.key === Qt.Key_Left) {
            root.moveGridSelection(-1, 0)
            event.accepted = true
          } else if (event.key === Qt.Key_Right) {
            root.moveGridSelection(1, 0)
            event.accepted = true
          } else if (event.key === Qt.Key_Up) {
            root.moveGridSelection(0, -1)
            event.accepted = true
          } else if (event.key === Qt.Key_Down) {
            root.moveGridSelection(0, 1)
            event.accepted = true
          } else if (!root.filterFocused && event.key === Qt.Key_1) {
            root.activeTab = 0
            event.accepted = true
          } else if (!root.filterFocused && event.key === Qt.Key_2) {
            root.activeTab = 1
            event.accepted = true
          } else if (!root.filterFocused && event.key === Qt.Key_3) {
            root.activeTab = 2
            event.accepted = true
          } else if (!root.filterFocused && event.key === Qt.Key_4) {
            root.activeTab = 3
            event.accepted = true
          } else if (!root.filterFocused && root.activeTab === 1 && event.key === Qt.Key_Slash) {
            root.filterFocused = true
            filterInput.forceActiveFocus()
            event.accepted = true
          }
        }
      }

      Item {
        id: body
        anchors.fill: parent
        anchors.topMargin: card.contentTopInset
        anchors.rightMargin: card.contentRightInset
        anchors.bottomMargin: card.contentBottomInset
        anchors.leftMargin: card.contentLeftInset

        // Header
        Column {
          id: headerBlock
          anchors.top: parent.top
          anchors.left: parent.left
          anchors.right: parent.right
          spacing: Style.spacing.xs

          Text {
            textFormat: Text.PlainText
            text: strings.t("title")
            color: root.foreground
            font.family: root.fontFamily
            font.pixelSize: Style.font.title
            font.bold: true
            width: parent.width
            elide: Text.ElideRight
          }

          Text {
            textFormat: Text.PlainText
            text: strings.t("subtitle")
            color: root.muted
            font.family: root.fontFamily
            font.pixelSize: Style.font.caption
            width: parent.width
            elide: Text.ElideRight
          }
        }

        // Tabs + optional window filter row
        Item {
          id: tabRow
          anchors.top: headerBlock.bottom
          anchors.topMargin: Style.spacing.md
          anchors.left: parent.left
          anchors.right: parent.right
          height: Math.max(tabButtons.implicitHeight, filterBox.height)

          Row {
            id: tabButtons
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.spacing.sm

            Repeater {
              model: [
                { idx: 0, label: strings.t("tab.monitors"), count: root.monitorEntries.length, icon: "󰍹" },
                { idx: 1, label: strings.t("tab.windows"), count: root.allWindowEntries.length, icon: "󰖯" },
                { idx: 2, label: strings.t("tab.workspaces"), count: root.workspaceEntries.length, icon: "󱂬" },
                { idx: 3, label: strings.t("tab.region"), count: -1, icon: "󰆞" }
              ]

              delegate: Button {
                required property var modelData
                text: modelData.count >= 0
                  ? modelData.label + " (" + modelData.count + ")"
                  : modelData.label
                iconText: modelData.icon
                fontFamily: root.fontFamily
                selected: root.activeTab === modelData.idx
                bordered: true
                foreground: root.foreground
                accent: root.accent
                onClicked: {
                  root.filterFocused = false
                  root.activeTab = modelData.idx
                  keyCatcher.forceActiveFocus()
                }
              }
            }
          }

          BorderSurface {
            id: filterBox
            visible: root.activeTab === 1
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(Style.space(220), Math.max(Style.space(110), tabRow.width - tabButtons.width - Style.spacing.md))
            height: Style.spacing.controlHeight
            radius: root.cornerRadius
            color: Style.normalFillFor(root.foreground, root.accent)
            borderSpec: Border.controlSpec(root.filterFocused ? "focus" : "normal", root.foreground, root.accent)

            Row {
              anchors.fill: parent
              anchors.leftMargin: filterBox.borderLeft + Style.spacing.sm
              anchors.rightMargin: filterBox.borderRight + Style.spacing.sm
              spacing: Style.spacing.xs

              Text {
                textFormat: Text.PlainText
                text: "󰍉"
                color: root.muted
                font.family: root.fontFamily
                font.pixelSize: Style.font.body
                anchors.verticalCenter: parent.verticalCenter
              }

              Item {
                width: parent.width - Style.space(24)
                height: parent.height
                anchors.verticalCenter: parent.verticalCenter

                Text {
                  textFormat: Text.PlainText
                  visible: root.filterText.length === 0
                  text: strings.t("filter.placeholder")
                  color: root.muted
                  font.family: root.fontFamily
                  font.pixelSize: Style.font.caption
                  anchors.verticalCenter: parent.verticalCenter
                  anchors.left: parent.left
                  anchors.right: parent.right
                  elide: Text.ElideRight
                }

                FilterInput {
                  id: filterInput
                  anchors.verticalCenter: parent.verticalCenter
                  anchors.left: parent.left
                  anchors.right: parent.right
                  color: root.foreground
                  font.family: root.fontFamily
                  font.pixelSize: Style.font.caption
                  keyHandler: keyCatcher
                  onActiveFocusChanged: root.filterFocused = activeFocus
                  onTextChanged: root.filterText = text
                }
              }
            }

            MouseArea {
              anchors.fill: parent
              cursorShape: Qt.IBeamCursor
              onClicked: {
                root.filterFocused = true
                filterInput.forceActiveFocus()
              }
            }
          }
        }

        // Main preview area
        Item {
          id: contentArea
          anchors.top: tabRow.bottom
          anchors.topMargin: Style.spacing.md
          anchors.left: parent.left
          anchors.right: parent.right
          anchors.bottom: footerRow.top
          anchors.bottomMargin: Style.spacing.md
          clip: true

          // Tab 0: Monitors
          Flickable {
            id: monitorFlick
            visible: root.activeTab === 0
            anchors.fill: parent
            contentWidth: width
            contentHeight: monitorGrid.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            Text {
              textFormat: Text.PlainText
              visible: root.monitorEntries.length === 0
              anchors.centerIn: parent
              text: strings.t("empty.monitors")
              color: root.muted
              font.family: root.fontFamily
              font.pixelSize: Style.font.body
            }

            Grid {
              id: monitorGrid
              visible: root.monitorEntries.length > 0
              width: parent.width
              columns: root.gridColumns
              spacing: Style.spacing.md

              Repeater {
                model: root.monitorEntries

                delegate: PreviewCard {
                  required property var modelData
                  required property int index

                  width: (monitorGrid.width - monitorGrid.spacing * (monitorGrid.columns - 1)) / monitorGrid.columns
                  previewHeight: root.monitorEntries.length === 1
                    ? Math.min(Style.space(340), contentArea.height - Style.space(76))
                    : Style.space(180)
                  selected: root.selectedMonitorIndex === index
                  captureSource: modelData.screen
                  live: root.opened && !root.hidingForRegion && root.activeTab === 0
                  titleText: modelData.name + " (" + modelData.width + "×" + modelData.height + " @ " + modelData.x + "," + modelData.y + ")"
                  subtitleText: modelData.description
                  fallbackIcon: "󰍹"
                  fallbackLabel: strings.t("preview.unavailable")
                  foreground: root.foreground
                  accent: root.accent
                  muted: root.muted
                  fontFamily: root.fontFamily
                  cornerRadius: root.cornerRadius
                  onCardClicked: root.selectedMonitorIndex = index
                  onCardDoubleClicked: {
                    root.selectedMonitorIndex = index
                    root.confirmSelection()
                  }
                }
              }
            }
          }

          // Tab 1: Windows
          Flickable {
            id: windowFlick
            visible: root.activeTab === 1
            anchors.fill: parent
            contentWidth: width
            contentHeight: windowGrid.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            Text {
              textFormat: Text.PlainText
              visible: root.windowEntries.length === 0
              anchors.centerIn: parent
              text: strings.t("empty.windows")
              color: root.muted
              font.family: root.fontFamily
              font.pixelSize: Style.font.body
            }

            Grid {
              id: windowGrid
              visible: root.windowEntries.length > 0
              width: parent.width
              columns: root.gridColumns
              spacing: Style.spacing.md

              Repeater {
                model: root.windowEntries

                delegate: PreviewCard {
                  required property var modelData
                  required property int index

                  width: (windowGrid.width - windowGrid.spacing * (windowGrid.columns - 1)) / windowGrid.columns
                  previewHeight: Style.space(160)
                  selected: root.selectedWindowIndex === index
                  captureSource: modelData.captureSource
                  live: root.opened && !root.hidingForRegion && root.activeTab === 1 && (hovered || selected || root.windowEntries.length <= 8)
                  titleText: modelData.title
                  subtitleText: modelData.appClass
                  fallbackIcon: "󰖯"
                  fallbackLabel: strings.t("preview.unavailable")
                  foreground: root.foreground
                  accent: root.accent
                  muted: root.muted
                  fontFamily: root.fontFamily
                  cornerRadius: root.cornerRadius
                  onCardClicked: root.selectedWindowIndex = index
                  onCardDoubleClicked: {
                    root.selectedWindowIndex = index
                    root.confirmSelection()
                  }
                }
              }
            }
          }

          // Tab 2: Workspaces
          Flickable {
            id: workspaceFlick
            visible: root.activeTab === 2
            anchors.fill: parent
            contentWidth: width
            contentHeight: workspaceGrid.implicitHeight
            boundsBehavior: Flickable.StopAtBounds

            Text {
              textFormat: Text.PlainText
              visible: root.workspaceEntries.length === 0
              anchors.centerIn: parent
              text: strings.t("empty.workspaces")
              color: root.muted
              font.family: root.fontFamily
              font.pixelSize: Style.font.body
            }

            Grid {
              id: workspaceGrid
              visible: root.workspaceEntries.length > 0
              width: parent.width
              columns: root.gridColumns
              spacing: Style.spacing.md

              Repeater {
                model: root.workspaceEntries

                delegate: PreviewCard {
                  required property var modelData
                  required property int index

                  width: (workspaceGrid.width - workspaceGrid.spacing * (workspaceGrid.columns - 1)) / workspaceGrid.columns
                  previewHeight: Style.space(160)
                  selected: root.selectedWorkspaceIndex === index
                  captureSource: modelData.captureSource
                  live: root.opened && !root.hidingForRegion && root.activeTab === 2 && (hovered || selected || root.workspaceEntries.length <= 6)
                  titleText: strings.t("workspace.label").arg(modelData.name) + (modelData.monitorName ? " · " + modelData.monitorName : "")
                  subtitleText: modelData.windowCount > 0
                    ? strings.tn("workspace.windows", modelData.windowCount).arg(modelData.windowCount) + (modelData.windowSummary ? " — " + modelData.windowSummary : "")
                    : strings.t("preview.emptyWorkspace")
                  fallbackIcon: "󱂬"
                  fallbackLabel: modelData.windowCount === 0 ? strings.t("preview.emptyWorkspace") : strings.t("preview.unavailable")
                  foreground: root.foreground
                  accent: root.accent
                  muted: root.muted
                  fontFamily: root.fontFamily
                  cornerRadius: root.cornerRadius
                  onCardClicked: root.selectedWorkspaceIndex = index
                  onCardDoubleClicked: {
                    root.selectedWorkspaceIndex = index
                    root.confirmSelection()
                  }
                }
              }
            }
          }

          // Tab 3: Region
          Item {
            visible: root.activeTab === 3
            anchors.fill: parent

            BorderSurface {
              anchors.centerIn: parent
              width: Math.min(parent.width, Style.space(460))
              height: regionCol.implicitHeight + Style.spacing.panelPadding * 2
              radius: root.cornerRadius
              color: Style.normalFillFor(root.foreground, root.accent)
              borderSpec: Border.controlSpec("normal", root.foreground, root.accent)

              Column {
                id: regionCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Style.spacing.panelPadding
                spacing: Style.spacing.md

                Text {
                  textFormat: Text.PlainText
                  text: "󰆞"
                  color: root.accent
                  font.family: root.fontFamily
                  font.pixelSize: Style.space(36)
                  anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                  textFormat: Text.PlainText
                  text: strings.t("region.heading")
                  color: root.foreground
                  font.family: root.fontFamily
                  font.pixelSize: Style.font.subtitle
                  font.bold: true
                  horizontalAlignment: Text.AlignHCenter
                  width: parent.width
                  wrapMode: Text.WordWrap
                }

                Text {
                  textFormat: Text.PlainText
                  text: strings.t("region.description")
                  color: root.muted
                  font.family: root.fontFamily
                  font.pixelSize: Style.font.caption
                  horizontalAlignment: Text.AlignHCenter
                  width: parent.width
                  wrapMode: Text.WordWrap
                }

                Button {
                  anchors.horizontalCenter: parent.horizontalCenter
                  text: strings.t("region.button")
                  iconText: "󰆞"
                  selected: true
                  bordered: true
                  foreground: root.foreground
                  accent: root.accent
                  fontFamily: root.fontFamily
                  onClicked: root.startRegionSelection()
                }
              }
            }
          }
        }

        // Footer bar
        Item {
          id: footerRow
          anchors.bottom: parent.bottom
          anchors.left: parent.left
          anchors.right: parent.right
          height: Math.max(Style.spacing.controlHeight, rememberContainer.height)

          Item {
            id: rememberContainer
            visible: root.allowToken
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: rememberRow.implicitWidth
            implicitHeight: Math.max(Style.spacing.controlHeight, rememberRow.implicitHeight)
            width: implicitWidth
            height: implicitHeight

            Row {
              id: rememberRow
              anchors.verticalCenter: parent.verticalCenter
              spacing: Style.spacing.sm

              ToggleSwitch {
                checked: root.rememberChoice
                foreground: root.foreground
                accent: root.accent
                interactive: false
                anchors.verticalCenter: parent.verticalCenter
              }

              Text {
                textFormat: Text.PlainText
                text: strings.t("remember")
                color: root.foreground
                font.family: root.fontFamily
                font.pixelSize: Style.font.caption
                anchors.verticalCenter: parent.verticalCenter
              }
            }

            MouseArea {
              anchors.fill: parent
              cursorShape: Qt.PointingHandCursor
              onClicked: root.rememberChoice = !root.rememberChoice
            }
          }

          Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.spacing.sm

            Button {
              text: strings.t("cancel")
              bordered: true
              foreground: root.foreground
              accent: root.accent
              fontFamily: root.fontFamily
              onClicked: root.cancel()
            }

            Button {
              text: root.activeTab === 3 ? strings.t("region.button") : strings.t("share")
              selected: root.canShareCurrent
              bordered: true
              enabled: root.canShareCurrent
              opacity: root.canShareCurrent ? 1.0 : 0.45
              foreground: root.foreground
              accent: root.accent
              fontFamily: root.fontFamily
              onClicked: root.confirmSelection()
            }
          }
        }
      }
    }
  }

  // Reusable preview card with live ScreencopyView and aspect-preserving fit
  component PreviewCard: BorderSurface {
    id: cardRoot

    property var captureSource: null
    property bool live: false
    property bool selected: false
    property real previewHeight: Style.space(160)
    property string titleText: ""
    property string subtitleText: ""
    property string fallbackIcon: "󰍹"
    property string fallbackLabel: ""
    property color foreground: Commons.Color.foreground
    property color accent: Commons.Color.accent
    property color muted: Commons.Color.muted
    property string fontFamily: Style.font.menuFamily
    property int cornerRadius: Style.cornerRadius

    readonly property bool hovered: cardMouse.containsMouse

    signal cardClicked()
    signal cardDoubleClicked()

    height: cardCol.implicitHeight + Style.spacing.sm * 2 + borderTop + borderBottom
    radius: cornerRadius
    color: selected
      ? Style.selectedFillFor(foreground, accent)
      : (hovered ? Style.hoverFillFor(foreground, accent) : Style.normalFillFor(foreground, accent))
    borderSpec: Border.controlSpec(selected ? "selected" : (hovered ? "hover-cursor" : "normal"), foreground, accent)

    Behavior on color { ColorAnimation { duration: Style.duration(100) } }

    Timer {
      interval: 1500
      repeat: true
      running: !cardRoot.live && !!cardRoot.captureSource && root.opened && !root.hidingForRegion
      onTriggered: if (screencopy.captureSource) screencopy.captureFrame()
    }

    Column {
      id: cardCol
      anchors.left: parent.left
      anchors.right: parent.right
      anchors.top: parent.top
      anchors.margins: Style.spacing.sm
      anchors.topMargin: cardRoot.borderTop + Style.spacing.sm
      anchors.leftMargin: cardRoot.borderLeft + Style.spacing.sm
      anchors.rightMargin: cardRoot.borderRight + Style.spacing.sm
      spacing: Style.spacing.xs

      Rectangle {
        id: previewFrame
        width: parent.width
        height: cardRoot.previewHeight
        radius: Math.max(0, cardRoot.cornerRadius - 2)
        color: Qt.rgba(0, 0, 0, 0.35)
        clip: true

        ScreencopyView {
          id: screencopy
          captureSource: cardRoot.captureSource
          live: cardRoot.live
          paintCursor: false
          visible: hasContent

          readonly property real srcW: sourceSize.width > 0 ? sourceSize.width : 16
          readonly property real srcH: sourceSize.height > 0 ? sourceSize.height : 9
          readonly property real fitScale: Math.min(previewFrame.width / srcW, previewFrame.height / srcH)
          width: Math.max(1, Math.round(srcW * fitScale))
          height: Math.max(1, Math.round(srcH * fitScale))
          anchors.centerIn: parent
        }

        Column {
          visible: !screencopy.hasContent
          anchors.centerIn: parent
          spacing: Style.spacing.xs

          Text {
            textFormat: Text.PlainText
            text: cardRoot.fallbackIcon
            color: cardRoot.muted
            font.family: cardRoot.fontFamily
            font.pixelSize: Style.space(28)
            anchors.horizontalCenter: parent.horizontalCenter
          }

          Text {
            textFormat: Text.PlainText
            text: cardRoot.fallbackLabel
            color: cardRoot.muted
            font.family: cardRoot.fontFamily
            font.pixelSize: Style.font.caption
            anchors.horizontalCenter: parent.horizontalCenter
          }
        }
      }

      Text {
        textFormat: Text.PlainText
        text: cardRoot.titleText
        color: cardRoot.selected ? Style.selectedStateColor(cardRoot.foreground, cardRoot.accent) : cardRoot.foreground
        font.family: cardRoot.fontFamily
        font.pixelSize: Style.font.body
        font.bold: true
        width: parent.width
        elide: Text.ElideRight
      }

      Text {
        textFormat: Text.PlainText
        visible: cardRoot.subtitleText.length > 0
        text: cardRoot.subtitleText
        color: cardRoot.muted
        font.family: cardRoot.fontFamily
        font.pixelSize: Style.font.caption
        width: parent.width
        elide: Text.ElideRight
      }
    }

    MouseArea {
      id: cardMouse
      anchors.fill: parent
      hoverEnabled: true
      cursorShape: Qt.PointingHandCursor
      onClicked: cardRoot.cardClicked()
      onDoubleClicked: cardRoot.cardDoubleClicked()
    }
  }
}
