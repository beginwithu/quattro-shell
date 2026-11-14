.pragma library

function normalizeAddress(addr) {
  var s = String(addr || "").trim().toLowerCase()
  if (s.indexOf("0x") === 0) s = s.slice(2)
  return s
}

function findScreenByName(screens, name) {
  var target = String(name || "")
  if (!screens || !target) return null
  for (var i = 0; i < screens.length; i++) {
    if (String(screens[i].name || "") === target) return screens[i]
  }
  return null
}

function findHyprToplevel(toplevels, win) {
  if (!toplevels || !win) return null
  var addr = normalizeAddress(win.address)
  if (addr) {
    for (var i = 0; i < toplevels.length; i++) {
      if (normalizeAddress(toplevels[i].address) === addr) return toplevels[i]
    }
  }
  var title = String(win.title || "")
  var appClass = String(win.appClass || "").toLowerCase()
  for (var j = 0; j < toplevels.length; j++) {
    var t = toplevels[j]
    var tTitle = String(t.title || "")
    var tClass = t.wayland ? String(t.wayland.appId || "").toLowerCase() : ""
    if (tTitle === title && (!appClass || !tClass || tClass === appClass)) return t
  }
  for (var k = 0; k < toplevels.length; k++) {
    var tk = toplevels[k]
    var tkClass = tk.wayland ? String(tk.wayland.appId || "").toLowerCase() : ""
    if (appClass && tkClass === appClass) return tk
  }
  return null
}

function findHyprWorkspace(workspaces, ws) {
  if (!workspaces || !ws) return null
  var idStr = String(ws.id || "")
  var nameStr = String(ws.name || "")
  for (var i = 0; i < workspaces.length; i++) {
    if (idStr && String(workspaces[i].id) === idStr) return workspaces[i]
  }
  for (var j = 0; j < workspaces.length; j++) {
    if (nameStr && String(workspaces[j].name || "") === nameStr) return workspaces[j]
  }
  return null
}

function toplevelsForWorkspace(toplevels, hyprWorkspace, fallbackWsId, fallbackWsName) {
  var out = []
  if (!toplevels) return out
  var targetId = hyprWorkspace ? String(hyprWorkspace.id) : String(fallbackWsId || "")
  var targetName = hyprWorkspace ? String(hyprWorkspace.name || "") : String(fallbackWsName || "")
  for (var i = 0; i < toplevels.length; i++) {
    var t = toplevels[i]
    if (!t || !t.workspace) continue
    if (targetId && String(t.workspace.id) === targetId) {
      out.push(t)
    } else if (!targetId && targetName && String(t.workspace.name || "") === targetName) {
      out.push(t)
    }
  }
  return out
}

function buildMonitorEntries(rawOutputs, screens, hyprMonitors) {
  var out = []
  var list = Array.isArray(rawOutputs) ? rawOutputs : []
  if (list.length === 0 && hyprMonitors) {
    for (var m = 0; m < hyprMonitors.length; m++) {
      var hm = hyprMonitors[m]
      list.push({
        name: String(hm.name || ""),
        description: String(hm.description || ""),
        x: Number(hm.x || 0),
        y: Number(hm.y || 0),
        width: Number(hm.width || 0),
        height: Number(hm.height || 0)
      })
    }
  }
  for (var i = 0; i < list.length; i++) {
    var o = list[i]
    if (!o || !o.name) continue
    var screen = findScreenByName(screens, o.name)
    out.push({
      name: String(o.name),
      description: String(o.description || o.name),
      x: Number(o.x || 0),
      y: Number(o.y || 0),
      width: Number(o.width || (screen ? screen.width : 0)),
      height: Number(o.height || (screen ? screen.height : 0)),
      screen: screen
    })
  }
  return out
}

function buildWindowEntries(rawWindows, toplevels) {
  var out = []
  var list = Array.isArray(rawWindows) ? rawWindows : []
  for (var i = 0; i < list.length; i++) {
    var w = list[i]
    if (!w || !w.id) continue
    var hypr = findHyprToplevel(toplevels, w)
    out.push({
      id: String(w.id),
      appClass: String(w.appClass || (hypr && hypr.wayland ? hypr.wayland.appId : "") || "window"),
      title: String(w.title || (hypr ? hypr.title : "") || w.appClass || w.id),
      address: String(w.address || (hypr ? hypr.address : "")),
      captureSource: hypr && hypr.wayland ? hypr.wayland : null
    })
  }
  return out
}

function filterWindows(windows, query) {
  var q = String(query || "").trim().toLowerCase()
  if (!q) return windows || []
  var out = []
  for (var i = 0; i < (windows || []).length; i++) {
    var w = windows[i]
    var hay = (w.appClass + " " + w.title).toLowerCase()
    if (hay.indexOf(q) !== -1) out.push(w)
  }
  return out
}

function buildWorkspaceEntries(rawWorkspaces, hyprWorkspaces, screens, toplevels) {
  var out = []
  var list = Array.isArray(rawWorkspaces) ? rawWorkspaces : []
  if (list.length === 0 && hyprWorkspaces) {
    for (var h = 0; h < hyprWorkspaces.length; h++) {
      var hw = hyprWorkspaces[h]
      list.push({
        id: String(hw.id),
        name: String(hw.name || hw.id),
        monitor: hw.monitor ? String(hw.monitor.name || "") : ""
      })
    }
  }
  for (var i = 0; i < list.length; i++) {
    var ws = list[i]
    if (!ws) continue
    var hyprWs = findHyprWorkspace(hyprWorkspaces, ws)
    var monitorName = String(ws.monitor || (hyprWs && hyprWs.monitor ? hyprWs.monitor.name : "") || "")
    var screen = findScreenByName(screens, monitorName)
    var isActiveOnScreen = !!(hyprWs && hyprWs.active)
    var wsToplevels = toplevelsForWorkspace(toplevels, hyprWs, ws.id, ws.name)
    var primaryCapture = null
    if (isActiveOnScreen && screen) {
      primaryCapture = screen
    } else {
      for (var t = 0; t < wsToplevels.length; t++) {
        if (wsToplevels[t].wayland) {
          primaryCapture = wsToplevels[t].wayland
          break
        }
      }
    }
    var windowSummary = ""
    if (wsToplevels.length > 0) {
      var firstTitle = String(wsToplevels[0].title || "")
      windowSummary = firstTitle
    }
    out.push({
      id: String(ws.id || (hyprWs ? hyprWs.id : "")),
      name: String(ws.name || (hyprWs ? hyprWs.name : "") || ws.id || ""),
      monitorName: monitorName,
      active: isActiveOnScreen,
      windowCount: wsToplevels.length,
      windowSummary: windowSummary,
      captureSource: primaryCapture
    })
  }
  return out
}

function formatSelection(target, remember) {
  var flags = remember ? "r" : ""
  return "[SELECTION]" + flags + "/" + target
}
