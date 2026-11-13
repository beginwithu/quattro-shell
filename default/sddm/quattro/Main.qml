import QtQuick 2.0
import SddmComponents 2.0

Rectangle {
  id: root
  width: 640
  height: 480
  color: "#1a1b26"

  property int userIndex: {
    if (userModel.lastUser) {
      for (var i = 0; i < userModel.rowCount(); i++) {
        var u = (userModel.data(userModel.index(i, 0), Qt.UserRole + 1) || "").toString()
        if (u === userModel.lastUser)
          return i
      }
    }
    if (userModel.lastIndex >= 0 && userModel.lastIndex < userModel.rowCount())
      return userModel.lastIndex
    return 0
  }

  property string currentUser: {
    if (userModel.rowCount() > 0 && root.userIndex >= 0 && root.userIndex < userModel.rowCount()) {
      var byRole = (userModel.data(userModel.index(root.userIndex, 0), Qt.UserRole + 1) || "").toString()
      if (byRole)
        return byRole
      var byDisplay = (userModel.data(userModel.index(root.userIndex, 0), Qt.DisplayRole) || "").toString()
      if (byDisplay)
        return byDisplay
    }
    return userModel.lastUser || ""
  }

  property bool loginFailed: false
  property int sessionIndex: {
    for (var i = 0; i < sessionModel.rowCount(); i++) {
      var name = (sessionModel.data(sessionModel.index(i, 0), Qt.DisplayRole) || "").toString()
      if (name.toLowerCase().indexOf("uwsm") !== -1)
        return i
    }
    return sessionModel.lastIndex >= 0 ? sessionModel.lastIndex : 0
  }

  Connections {
    target: sddm
    function onLoginFailed() {
      root.loginFailed = true
      password.text = ""
      password.focus = true
    }
    function onLoginSucceeded() {
      root.loginFailed = false
    }
  }

  Column {
    anchors.centerIn: parent
    spacing: 40

    Image {
      id: logo
      source: "logo.png"
      width: Math.min(sourceSize.width, root.width * 0.8)
      height: sourceSize.width > 0 ? Math.round(width * sourceSize.height / sourceSize.width) : 0
      fillMode: Image.PreserveAspectFit
      anchors.horizontalCenter: parent.horizontalCenter
    }

    Column {
      anchors.horizontalCenter: parent.horizontalCenter
      spacing: 14

      Row {
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 15

        Image {
          source: root.loginFailed ? "lock-failed.png" : "lock.png"
          width: 34
          height: 38
          fillMode: Image.PreserveAspectFit
          anchors.verticalCenter: parent.verticalCenter
        }

        Item {
          width: entry.width
          height: entry.height

          Image {
            id: entry
            source: root.loginFailed ? "entry-failed.png" : "entry.png"
            anchors.centerIn: parent
          }

          Row {
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

            Repeater {
              model: Math.min(password.text.length, 21)

              Image {
                source: "bullet.png"
                width: 7
                height: 7
              }
            }
          }

          TextInput {
            id: password
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            verticalAlignment: TextInput.AlignVCenter
            echoMode: TextInput.Password
            font.family: "JetBrainsMono Nerd Font"
            font.pixelSize: 24
            font.letterSpacing: 5
            passwordCharacter: "\u2022"
            color: "transparent"
            selectionColor: "transparent"
            selectedTextColor: "transparent"
            cursorDelegate: Item {}
            focus: true

            onTextChanged: root.loginFailed = false

            Keys.onPressed: {
              if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                sddm.login(root.currentUser, password.text, root.sessionIndex)
                event.accepted = true
              }
            }
          }
        }
      }

      Text {
        visible: root.currentUser.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.currentUser
        color: "#565f89"
        font.family: "JetBrainsMono Nerd Font"
        font.pixelSize: 13

        MouseArea {
          anchors.fill: parent
          enabled: userModel.rowCount() > 1
          cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
          onClicked: {
            root.userIndex = (root.userIndex + 1) % userModel.rowCount()
            password.forceActiveFocus()
          }
        }
      }
    }

  }

  Component.onCompleted: password.forceActiveFocus()
}
