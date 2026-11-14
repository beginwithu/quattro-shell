#include "ThemeManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace quattro {

namespace {

QString readTextFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

bool parseBoolToken(const QString &raw, bool fallback) {
    const QString s = raw.trimmed().toLower();
    if (s == QLatin1String("true") || s == QLatin1String("1") || s == QLatin1String("yes") || s == QLatin1String("on"))
        return true;
    if (s == QLatin1String("false") || s == QLatin1String("0") || s == QLatin1String("no") || s == QLatin1String("off"))
        return false;
    return fallback;
}

} // namespace

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_home(QDir::homePath())
    , m_stateHome(qEnvironmentVariable("XDG_STATE_HOME", m_home + QStringLiteral("/.local/state")))
    , m_themeDir(m_stateHome + QStringLiteral("/quattro/current/theme"))
    , m_userShellPath(qEnvironmentVariable("XDG_CONFIG_HOME", m_home + QStringLiteral("/.config"))
                      + QStringLiteral("/quattro/shell.toml"))
{
    const QByteArray menuFont = qgetenv("QUATTRO_MENU_FONT");
    m_menuFontFamily = menuFont.isEmpty() ? m_fontFamily : QString::fromUtf8(menuFont);

    const QString fontConfPath = qEnvironmentVariable("XDG_CONFIG_HOME", m_home + QStringLiteral("/.config"))
                                 + QStringLiteral("/fontconfig/fonts.conf");
    if (QFileInfo::exists(m_userShellPath))
        m_watcher.addPath(m_userShellPath);
    if (QFileInfo::exists(fontConfPath))
        m_watcher.addPath(fontConfPath);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this, fontConfPath](const QString &path) {
        if (QFileInfo::exists(path) && !m_watcher.files().contains(path))
            m_watcher.addPath(path);
        if (path == m_userShellPath) {
            loadUserShellText(readTextFile(m_userShellPath));
        } else if (path == fontConfPath) {
            resolveFontFamilyAsync();
        }
    });

    loadFromDisk();
    resolveFontFamilyAsync();
}

ThemeManager::~ThemeManager() {
    for (QProcess *p : findChildren<QProcess *>()) {
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(100);
        }
    }
}

void ThemeManager::loadFromDisk() {
    loadColorsText(readTextFile(m_themeDir + QStringLiteral("/colors.toml")));
    m_themeShellValues = parseShellToml(readTextFile(m_themeDir + QStringLiteral("/shell.toml")));
    m_userShellValues = parseShellToml(readTextFile(m_userShellPath));
    mergeShellValues();
}

void ThemeManager::loadColorsText(const QString &raw) {
    QColor nextFg = QStringLiteral("#cacccc");
    QColor nextBg = QStringLiteral("#101315");
    QColor nextAccent = QStringLiteral("#cacccc");
    QColor nextUrgent = QStringLiteral("#a55555");
    QColor nextMuted = QStringLiteral("#707880");

    bool foundAccent = false;
    bool foundMuted = false;
    bool loadedFg = false;
    bool loadedBg = false;
    QColor c0, c4, c7, c8;

    static const QRegularExpression lineRe(QStringLiteral("^\\s*([A-Za-z0-9_-]+)\\s*=\\s*[\"']?(#[0-9A-Fa-f]{6})"));
    const QStringList lines = raw.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const auto m = lineRe.match(line);
        if (!m.hasMatch())
            continue;
        const QString key = m.captured(1);
        const QColor col(m.captured(2));
        if (!col.isValid())
            continue;
        if (key == QLatin1String("foreground")) { nextFg = col; loadedFg = true; }
        else if (key == QLatin1String("background")) { nextBg = col; loadedBg = true; }
        else if (key == QLatin1String("accent")) { nextAccent = col; foundAccent = true; }
        else if (key == QLatin1String("muted")) { nextMuted = col; foundMuted = true; }
        else if (key == QLatin1String("color0")) c0 = col;
        else if (key == QLatin1String("color4")) c4 = col;
        else if (key == QLatin1String("color7")) c7 = col;
        else if (key == QLatin1String("color8")) c8 = col;
        else if (key == QLatin1String("red") || key == QLatin1String("color1")) nextUrgent = col;
    }

    if (!loadedBg && c0.isValid()) nextBg = c0;
    if (!loadedFg && c7.isValid()) nextFg = c7;
    if (!foundAccent && c4.isValid()) nextAccent = c4;
    if (!foundMuted) nextMuted = c8.isValid() ? c8 : nextFg;

    m_foreground = nextFg;
    m_background = nextBg;
    m_accent = nextAccent;
    m_urgent = nextUrgent;
    m_muted = nextMuted;

    rebuildDerived();
    emit themeChanged();
}

void ThemeManager::loadThemeShellText(const QString &raw) {
    m_themeShellValues = parseShellToml(raw);
    mergeShellValues();
}

void ThemeManager::loadUserShellText(const QString &raw) {
    m_userShellValues = parseShellToml(raw);
    mergeShellValues();
}

void ThemeManager::applyThemeBase64(const QByteArray &colorsB64, const QByteArray &shellB64) {
    const QString colorsRaw = QString::fromUtf8(QByteArray::fromBase64(colorsB64));
    const QString shellRaw = QString::fromUtf8(QByteArray::fromBase64(shellB64));
    loadColorsText(colorsRaw);
    loadThemeShellText(shellRaw);
}

QHash<QString, QString> ThemeManager::parseShellToml(const QString &raw) {
    QHash<QString, QString> parsed;
    if (raw.isEmpty())
        return parsed;

    static const QRegularExpression sectionRe(QStringLiteral("^\\[([A-Za-z0-9_-]+)\\]\\s*(#.*)?$"));
    static const QRegularExpression stringRe(QStringLiteral("^([A-Za-z0-9_-]+)\\s*=\\s*[\"']([^\"']+)[\"']\\s*(#.*)?$"));
    static const QRegularExpression numRe(QStringLiteral("^([A-Za-z0-9_-]+)\\s*=\\s*(-?\\d+(?:\\.\\d+)?)\\s*(#.*)?$"));
    static const QRegularExpression widthRe(QStringLiteral("^([A-Za-z0-9_-]+)\\s*=\\s*(-?\\d+(?:\\.\\d+)?(?:\\s+-?\\d+(?:\\.\\d+)?){1,3})\\s*(#.*)?$"));
    static const QRegularExpression bareRe(QStringLiteral("^([A-Za-z0-9_-]+)\\s*=\\s*([A-Za-z][A-Za-z0-9_-]*)\\s*(#.*)?$"));

    QString section;
    const QStringList lines = raw.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (const auto sec = sectionRe.match(line); sec.hasMatch()) {
            section = sec.captured(1);
            continue;
        }
        if (section.isEmpty())
            continue;

        QRegularExpressionMatch kv = stringRe.match(line);
        if (!kv.hasMatch()) kv = numRe.match(line);
        if (!kv.hasMatch()) kv = widthRe.match(line);
        if (!kv.hasMatch()) kv = bareRe.match(line);
        if (!kv.hasMatch())
            continue;

        parsed.insert(section + QLatin1Char('.') + kv.captured(1), kv.captured(2));
    }
    return parsed;
}

void ThemeManager::mergeShellValues() {
    m_shellValues = m_themeShellValues;
    for (auto it = m_userShellValues.cbegin(); it != m_userShellValues.cend(); ++it)
        m_shellValues.insert(it.key(), it.value());

    m_fontOverrides.clear();
    m_barOverrides.clear();
    m_spacingOverrides.clear();
    m_styleOverrides.clear();

    int nextBase = 12;
    double nextSpacingScale = 1.0;
    bool nextSpacingScaleWithFont = true;
    bool nextBarScaleWithFont = true;

    for (auto it = m_shellValues.cbegin(); it != m_shellValues.cend(); ++it) {
        const int dot = it.key().indexOf(QLatin1Char('.'));
        if (dot < 0) continue;
        const QString section = it.key().left(dot);
        const QString key = it.key().mid(dot + 1);
        const QString &val = it.value();

        if (section == QLatin1String("font")) {
            bool ok = false;
            const int ival = val.toInt(&ok);
            if (!ok) continue;
            if (key == QLatin1String("base-size")) nextBase = ival;
            else m_fontOverrides.insert(key, ival);
        } else if (section == QLatin1String("bar")) {
            if (key == QLatin1String("scale-with-font")) {
                nextBarScaleWithFont = parseBoolToken(val, nextBarScaleWithFont);
            } else if (key == QLatin1String("size-horizontal") || key == QLatin1String("size-vertical")
                       || key == QLatin1String("notch-height") || key == QLatin1String("icon-slot")
                       || key == QLatin1String("icon-canvas") || key == QLatin1String("icon-font")
                       || key == QLatin1String("icon-size") || key == QLatin1String("status-slot")) {
                bool ok = false;
                const int b = val.toInt(&ok);
                if (ok) m_barOverrides.insert(key, b);
            }
        } else if (section == QLatin1String("spacing")) {
            if (key == QLatin1String("scale-with-font")) {
                nextSpacingScaleWithFont = parseBoolToken(val, nextSpacingScaleWithFont);
            } else {
                bool ok = false;
                const double fval = val.toDouble(&ok);
                if (!ok) continue;
                if (key == QLatin1String("scale")) nextSpacingScale = fval;
                else m_spacingOverrides.insert(key, fval);
            }
        } else if (section == QLatin1String("controls") || section == QLatin1String("style")) {
            m_styleOverrides.insert(key, val);
        }
    }

    m_fontBaseSize = std::max(1, nextBase);
    m_spacingScale = nextSpacingScale >= 0.0 ? nextSpacingScale : 1.0;
    m_spacingScaleWithFont = nextSpacingScaleWithFont;
    m_barScaleWithFont = nextBarScaleWithFont;

    rebuildDerived();
    emit themeChanged();
}

QColor ThemeManager::withAlpha(const QColor &c, double alpha) {
    QColor out = c;
    const double clamped = std::clamp(alpha, 0.0, 1.0);
    out.setAlpha(std::clamp(static_cast<int>(qRound(c.alpha() * clamped)), 0, 255));
    return out;
}

QColor ThemeManager::parseColorToken(const QString &token, const QColor &fallback) {
    const QString s = token.trimmed();
    if (s.isEmpty())
        return fallback;

    if (s.startsWith(QLatin1Char('#'))) {
        if (s.size() == 4 || s.size() == 7) {
            QColor c(s);
            return c.isValid() ? c : fallback;
        }
        if (s.size() == 9) {
            // CSS #RRGGBBAA -> QColor takes #AARRGGBB
            bool ok = false;
            const uint rgba = s.mid(1).toUInt(&ok, 16);
            if (ok) {
                return QColor((rgba >> 24) & 0xff, (rgba >> 16) & 0xff,
                              (rgba >> 8) & 0xff, rgba & 0xff);
            }
        }
    }

    static const QRegularExpression rgbHexRe(QStringLiteral("^rgb\\(([0-9A-Fa-f]{6})\\)$"), QRegularExpression::CaseInsensitiveOption);
    if (const auto m = rgbHexRe.match(s); m.hasMatch())
        return QColor(QLatin1Char('#') + m.captured(1));

    static const QRegularExpression rgbaHexRe(QStringLiteral("^rgba\\(([0-9A-Fa-f]{6})([0-9A-Fa-f]{2})\\)$"), QRegularExpression::CaseInsensitiveOption);
    if (const auto m = rgbaHexRe.match(s); m.hasMatch()) {
        QColor c(QLatin1Char('#') + m.captured(1));
        c.setAlpha(m.captured(2).toInt(nullptr, 16));
        return c;
    }

    static const QRegularExpression rgbDecRe(QStringLiteral("^rgb\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*\\)$"), QRegularExpression::CaseInsensitiveOption);
    if (const auto m = rgbDecRe.match(s); m.hasMatch()) {
        return QColor(std::clamp(m.captured(1).toInt(), 0, 255),
                      std::clamp(m.captured(2).toInt(), 0, 255),
                      std::clamp(m.captured(3).toInt(), 0, 255));
    }

    static const QRegularExpression rgbaDecRe(QStringLiteral("^rgba\\(\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*(\\d+)\\s*,\\s*([0-9.]+)\\s*\\)$"), QRegularExpression::CaseInsensitiveOption);
    if (const auto m = rgbaDecRe.match(s); m.hasMatch()) {
        QColor c(std::clamp(m.captured(1).toInt(), 0, 255),
                 std::clamp(m.captured(2).toInt(), 0, 255),
                 std::clamp(m.captured(3).toInt(), 0, 255));
        c.setAlphaF(static_cast<float>(std::clamp(m.captured(4).toDouble(), 0.0, 1.0)));
        return c;
    }

    static const QRegularExpression oxRe(QStringLiteral("^0x([0-9A-Fa-f]{2})([0-9A-Fa-f]{6})$"));
    if (const auto m = oxRe.match(s); m.hasMatch()) {
        QColor c(QLatin1Char('#') + m.captured(2));
        c.setAlpha(m.captured(1).toInt(nullptr, 16));
        return c;
    }

    return fallback;
}

QString ThemeManager::pick(const QString &key, const QString &fallback) const {
    const QString v = m_shellValues.value(key);
    return v.isEmpty() ? fallback : v;
}

double ThemeManager::pickAlpha(const QString &key, double fallback) const {
    const QString v = m_shellValues.value(key);
    if (v.isEmpty()) return fallback;
    bool ok = false;
    const double n = v.toDouble(&ok);
    return ok ? std::clamp(n, 0.0, 1.0) : fallback;
}

QString ThemeManager::resolveValueRef(const QString &raw) const {
    QString s = raw.trimmed();
    QSet<QString> seen;
    static const QRegularExpression refRe(QStringLiteral("^[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+$"));
    while (refRe.match(s).hasMatch() && !seen.contains(s)) {
        seen.insert(s);
        const QString next = m_shellValues.value(s).trimmed();
        if (next.isEmpty()) break;
        s = next;
    }
    return s;
}

QColor ThemeManager::flatColor(const QString &value, const QColor &fallback) const {
    static const QRegularExpression degRe(QStringLiteral("^-?\\d+(?:\\.\\d+)?deg$"));
    QString token;
    const QStringList parts = value.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (!degRe.match(part).hasMatch()) {
            token = part;
            break;
        }
    }
    if (token.isEmpty()) token = value.trimmed();

    QSet<QString> seen;
    QString role = token.toLower();
    while (m_shellValues.contains(role) && m_shellValues.value(role) != token) {
        if (seen.contains(role)) return fallback;
        seen.insert(role);
        const QString next = m_shellValues.value(role);
        const QStringList nextParts = next.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        token.clear();
        for (const QString &p : nextParts) {
            if (!degRe.match(p).hasMatch()) { token = p; break; }
        }
        if (token.isEmpty()) token = next.trimmed();
        role = token.toLower();
    }

    if (role == QLatin1String("foreground") || role == QLatin1String("text")) return m_foreground;
    if (role == QLatin1String("accent")) return m_accent;
    if (role == QLatin1String("urgent")) return m_urgent;
    if (role == QLatin1String("muted")) return m_muted;
    if (role == QLatin1String("background")) return m_background;
    if (role == QLatin1String("transparent")) return QColor(0, 0, 0, 0);

    return parseColorToken(token, fallback);
}

QColor ThemeManager::composed(const QString &colorKey, const QString &alphaKey,
                              const QColor &colorFallback, double alphaFallback) const {
    const QColor base = flatColor(pick(colorKey, QString()), colorFallback);
    const double alpha = pickAlpha(alphaKey, alphaFallback);
    return withAlpha(base, alpha);
}

void ThemeManager::rebuildDerived() {
    m_bar.background = composed(QStringLiteral("bar.background"), QStringLiteral("bar.background-alpha"), m_background, 1.0);
    m_bar.text = flatColor(pick(QStringLiteral("bar.text"), QStringLiteral("foreground")), m_foreground);
    m_bar.active = flatColor(pick(QStringLiteral("bar.active"), QStringLiteral("urgent")), m_urgent);

    m_popups.background = composed(QStringLiteral("popups.background"), QStringLiteral("popups.background-alpha"), m_background, 1.0);
    m_popups.text = flatColor(pick(QStringLiteral("popups.text"), QStringLiteral("foreground")), m_foreground);
    m_popups.border = composed(QStringLiteral("popups.border"), QStringLiteral("popups.border-alpha"), m_accent, 1.0);

    m_tooltip.background = composed(QStringLiteral("tooltip.background"), QStringLiteral("tooltip.background-alpha"), m_background, 1.0);
    m_tooltip.text = flatColor(pick(QStringLiteral("tooltip.text"), QStringLiteral("foreground")), m_foreground);
    m_tooltip.border = composed(QStringLiteral("tooltip.border"), QStringLiteral("tooltip.border-alpha"), m_foreground, 1.0);

    m_notifications.background = composed(QStringLiteral("notifications.background"), QStringLiteral("notifications.background-alpha"), m_background, 1.0);
    m_notifications.text = flatColor(pick(QStringLiteral("notifications.text"), QStringLiteral("foreground")), m_foreground);
    m_notifications.border = composed(QStringLiteral("notifications.border"), QStringLiteral("notifications.border-alpha"), m_accent, 1.0);
    m_notifications.countdown = flatColor(pick(QStringLiteral("notifications.countdown"), QStringLiteral("accent")), m_accent);

    m_menu.background = composed(QStringLiteral("menu.background"), QStringLiteral("menu.background-alpha"), m_background, 1.0);
    m_menu.text = flatColor(pick(QStringLiteral("menu.text"), QStringLiteral("foreground")), m_foreground);
    m_menu.border = composed(QStringLiteral("menu.border"), QStringLiteral("menu.border-alpha"), m_foreground, 1.0);
    m_menu.scrim = composed(QStringLiteral("menu.scrim"), QStringLiteral("menu.scrim-alpha"), m_background, 0.5);
    m_menu.selectedBackground = composed(QStringLiteral("menu.selected-background"), QStringLiteral("menu.selected-background-alpha"), m_foreground, 0.08);
    m_menu.selectedText = flatColor(pick(QStringLiteral("menu.selected-text"), QStringLiteral("accent")), m_accent);
    m_menu.selectedBorder = composed(QStringLiteral("menu.selected-border"), QStringLiteral("menu.selected-border-alpha"), m_foreground, 0.0);

    m_launcher.background = composed(QStringLiteral("launcher.background"), QStringLiteral("launcher.background-alpha"), m_menu.background, 0.95);
    m_launcher.text = flatColor(pick(QStringLiteral("launcher.text"), QStringLiteral("foreground")), m_menu.text);
    m_launcher.border = composed(QStringLiteral("launcher.border"), QStringLiteral("launcher.border-alpha"), m_menu.border, 1.0);
    m_launcher.scrim = composed(QStringLiteral("launcher.scrim"), QStringLiteral("launcher.scrim-alpha"), m_menu.scrim, 0.5);
    m_launcher.selectedBackground = composed(QStringLiteral("launcher.selected-background"), QStringLiteral("launcher.selected-background-alpha"), m_foreground, 0.08);
    m_launcher.selectedText = flatColor(pick(QStringLiteral("launcher.selected-text"), QStringLiteral("accent")), m_menu.selectedText);
    m_launcher.selectedBorder = composed(QStringLiteral("launcher.selected-border"), QStringLiteral("launcher.selected-border-alpha"), m_foreground, 0.25);

    m_polkit.background = composed(QStringLiteral("polkit.background"), QStringLiteral("polkit.background-alpha"), m_background, 1.0);
    m_polkit.text = flatColor(pick(QStringLiteral("polkit.text"), QStringLiteral("foreground")), m_foreground);
    m_polkit.textError = flatColor(pick(QStringLiteral("polkit.text-error"), QStringLiteral("urgent")), m_urgent);
    m_polkit.border = composed(QStringLiteral("polkit.border"), QStringLiteral("polkit.border-alpha"), m_accent, 1.0);
    m_polkit.borderError = composed(QStringLiteral("polkit.border-error"), QStringLiteral("polkit.border-alpha"), m_urgent, 1.0);
    m_polkit.accent = flatColor(pick(QStringLiteral("polkit.accent"), QStringLiteral("accent")), m_accent);
    m_polkit.scrim = composed(QStringLiteral("polkit.scrim"), QStringLiteral("polkit.scrim-alpha"), m_background, 0.5);

    m_lock.background = composed(QStringLiteral("lock.background"), QStringLiteral("lock.background-alpha"), m_background, 0.8);
    m_lock.text = flatColor(pick(QStringLiteral("lock.text"), QStringLiteral("foreground")), m_foreground);
    m_lock.placeholder = m_shellValues.contains(QStringLiteral("lock.placeholder"))
                             ? flatColor(m_shellValues.value(QStringLiteral("lock.placeholder")), withAlpha(m_foreground, 0.66))
                             : withAlpha(m_foreground, 0.66);
    m_lock.textError = flatColor(pick(QStringLiteral("lock.text-error"), QStringLiteral("urgent")), m_urgent);
    m_lock.border = composed(QStringLiteral("lock.border"), QStringLiteral("lock.border-alpha"), m_foreground, 1.0);
    m_lock.borderActive = composed(QStringLiteral("lock.border-active"), QStringLiteral("lock.border-alpha"), m_accent, 1.0);
    m_lock.borderError = composed(QStringLiteral("lock.border-error"), QStringLiteral("lock.border-alpha"), m_urgent, 1.0);
    m_lock.selection = composed(QStringLiteral("lock.selection"), QStringLiteral("lock.selection-alpha"), m_accent, 0.45);

    m_imagePicker.background = m_popups.background;
    m_imagePicker.text = m_popups.text;
    m_imagePicker.border = m_popups.border;
    m_imagePicker.scrim = composed(QStringLiteral("image-picker.scrim"), QStringLiteral("image-picker.scrim-alpha"), m_background, 0.72);
    m_imagePicker.selectedBorder = composed(QStringLiteral("image-picker.selected-border"), QStringLiteral("image-picker.selected-border-alpha"), m_accent, 1.0);
    m_imagePicker.unselectedBorder = composed(QStringLiteral("image-picker.unselected-border"), QStringLiteral("image-picker.unselected-border-alpha"), m_foreground, 0.35);
}

BorderWidths ThemeManager::parseWidthSpec(const QString &raw, int fallback) {
    const int fb = std::max(0, fallback);
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return {fb, fb, fb, fb};

    static const QRegularExpression numRe(QStringLiteral("-?\\d+(?:\\.\\d+)?"));
    QVector<int> nums;
    auto it = numRe.globalMatch(s);
    while (it.hasNext() && nums.size() < 4) {
        const int v = static_cast<int>(std::lround(it.next().captured(0).toDouble()));
        nums.push_back(std::max(0, v));
    }
    if (nums.isEmpty()) return {fb, fb, fb, fb};
    if (nums.size() == 1) return {nums[0], nums[0], nums[0], nums[0]};
    if (nums.size() == 2) return {nums[0], nums[1], nums[0], nums[1]};
    if (nums.size() == 3) return {nums[0], nums[1], nums[2], nums[1]};
    return {nums[0], nums[1], nums[2], nums[3]};
}

BorderWidths ThemeManager::surfaceWidths(const QString &section, const QString &token, int fallbackWidth) const {
    const QString baseKey = token == QLatin1String("border") ? QStringLiteral("border-width") : token + QStringLiteral("-width");
    QString baseVal = m_shellValues.value(section + QLatin1Char('.') + baseKey);
    if (baseVal.isEmpty() && token != QLatin1String("border"))
        baseVal = m_shellValues.value(section + QStringLiteral(".border-width"));

    BorderWidths w = parseWidthSpec(baseVal, fallbackWidth);
    auto sideVal = [&](const QString &suffix) -> QString {
        QString v = m_shellValues.value(section + QLatin1Char('.') + baseKey + suffix);
        if (v.isEmpty() && token != QLatin1String("border"))
            v = m_shellValues.value(section + QStringLiteral(".border-width") + suffix);
        return v;
    };
    auto applySide = [](int &dst, const QString &v) {
        if (!v.isEmpty()) {
            bool ok = false;
            const int n = static_cast<int>(std::lround(v.toDouble(&ok)));
            if (ok) dst = std::max(0, n);
        }
    };
    applySide(w.top, sideVal(QStringLiteral("-top")));
    applySide(w.right, sideVal(QStringLiteral("-right")));
    applySide(w.bottom, sideVal(QStringLiteral("-bottom")));
    applySide(w.left, sideVal(QStringLiteral("-left")));
    return w;
}

BorderGradient ThemeManager::resolveGradient(const QString &raw, const QColor &fallback, double opacity) const {
    BorderGradient g;
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return g;

    static const QRegularExpression degRe(QStringLiteral("^(-?\\d+(?:\\.\\d+)?)deg$"));
    const QStringList parts = s.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (const auto m = degRe.match(part); m.hasMatch()) {
            g.angleDeg = m.captured(1).toDouble();
        } else {
            QColor c = flatColor(part, fallback);
            c = withAlpha(c, opacity);
            g.colors.push_back(c);
        }
    }
    if (g.colors.isEmpty())
        g.colors.push_back(withAlpha(fallback, opacity));
    g.enabled = g.colors.size() > 1;
    return g;
}

BorderSpec ThemeManager::surfaceBorderSpec(const QString &section, const QString &token,
                                             const QColor &fallbackColor, int fallbackWidth,
                                             const QString &alphaKey) const {
    const double opacity = pickAlpha(section + QLatin1Char('.') + (alphaKey.isEmpty() ? token + QStringLiteral("-alpha") : alphaKey), 1.0);
    const QString rawRef = resolveValueRef(m_shellValues.value(section + QLatin1Char('.') + token));
    BorderGradient grad = resolveGradient(rawRef, fallbackColor, opacity);
    const QColor primary = grad.colors.isEmpty() ? withAlpha(fallbackColor, opacity) : grad.colors.first();
    if (!grad.enabled) {
        const QString legacy = m_shellValues.value(section + QLatin1Char('.') + (token == QLatin1String("border") ? QStringLiteral("border-gradient") : token + QStringLiteral("-gradient")));
        if (!legacy.trimmed().isEmpty()) {
            const BorderGradient lg = resolveGradient(legacy, primary, opacity);
            if (lg.enabled) grad = lg;
        }
    }
    return BorderSpec{primary, surfaceWidths(section, token, fallbackWidth), grad};
}

int ThemeManager::fontPx(double mult) const {
    return std::max(1, static_cast<int>(std::lround(m_fontBaseSize * mult)));
}

int ThemeManager::fontToken(const QString &key, int fallback) const {
    auto it = m_fontOverrides.constFind(key);
    if (it != m_fontOverrides.cend() && it.value() > 0)
        return it.value();
    return fallback;
}

int ThemeManager::fontCaption() const { return fontToken(QStringLiteral("caption"), fontPx(0.833)); }
int ThemeManager::fontBodySmall() const { return fontToken(QStringLiteral("body-small"), fontPx(0.917)); }
int ThemeManager::fontBody() const { return fontToken(QStringLiteral("body"), fontPx(1.0)); }
int ThemeManager::fontSubtitle() const { return fontToken(QStringLiteral("subtitle"), fontPx(1.083)); }
int ThemeManager::fontTitle() const { return fontToken(QStringLiteral("title"), fontPx(1.167)); }
int ThemeManager::fontHeading() const { return fontToken(QStringLiteral("heading"), fontPx(1.333)); }
int ThemeManager::fontDisplay() const { return fontToken(QStringLiteral("display"), fontPx(2.0)); }
int ThemeManager::fontDisplayLarge() const { return fontToken(QStringLiteral("display-large"), fontPx(2.333)); }

double ThemeManager::spaceReal(double px) const {
    if (px <= 0.0) return 0.0;
    const double eff = m_spacingScale * (m_spacingScaleWithFont ? fontScale() : 1.0);
    return px * eff;
}

int ThemeManager::space(double px) const {
    const double r = spaceReal(px);
    if (r <= 0.0) return 0;
    return std::max(1, static_cast<int>(std::lround(r)));
}

int ThemeManager::spacingToken(const QString &key, int fallback) const {
    auto it = m_spacingOverrides.constFind(key);
    if (it != m_spacingOverrides.cend() && it.value() >= 0.0)
        return static_cast<int>(std::lround(it.value()));
    return space(fallback);
}

int ThemeManager::barToken(const QString &key, int fallback) const {
    double base = m_barOverrides.contains(key) && m_barOverrides.value(key) > 0
                      ? static_cast<double>(m_barOverrides.value(key))
                      : static_cast<double>(fallback);
    if (m_barScaleWithFont)
        base *= fontScale();
    return std::max(1, static_cast<int>(std::lround(base)));
}

int ThemeManager::barSizeHorizontal() const { return barToken(QStringLiteral("size-horizontal"), 26); }
int ThemeManager::barSizeVertical() const { return barToken(QStringLiteral("size-vertical"), 28); }
int ThemeManager::barIconSlot() const { return barToken(QStringLiteral("icon-slot"), 27); }
int ThemeManager::barIconCanvas() const { return barToken(QStringLiteral("icon-canvas"), 16); }
int ThemeManager::barIconFont() const { return barToken(QStringLiteral("icon-font"), 13); }
int ThemeManager::barIconSize() const { return std::min(barIconCanvas(), barToken(QStringLiteral("icon-size"), 12)); }
int ThemeManager::barStatusSlot() const { return barToken(QStringLiteral("status-slot"), 21); }

void ThemeManager::setHyprlandMetrics(int cornerRadius, int gapsOut, bool reduceMotion) {
    bool changed = false;
    if (cornerRadius >= 0 && cornerRadius != m_cornerRadius) { m_cornerRadius = cornerRadius; changed = true; }
    if (gapsOut >= 0 && gapsOut != m_gapsOut) { m_gapsOut = gapsOut; changed = true; }
    if (reduceMotion != m_reduceMotion) { m_reduceMotion = reduceMotion; changed = true; }
    if (changed) emit themeChanged();
}

double ThemeManager::styleNum(const QString &key, double fallback) const {
    auto it = m_styleOverrides.constFind(key);
    if (it == m_styleOverrides.cend()) return fallback;
    bool ok = false;
    const double n = it.value().toDouble(&ok);
    return ok ? n : fallback;
}

double ThemeManager::styleAlpha(const QString &key, double fallback) const {
    return std::clamp(styleNum(key, fallback), 0.0, 1.0);
}

QString ThemeManager::styleString(const QString &key, const QString &fallback) const {
    const QString v = m_styleOverrides.value(key).trimmed();
    return v.isEmpty() ? fallback : v;
}

QColor ThemeManager::normalFill() const {
    return withAlpha(flatColor(styleString(QStringLiteral("normal-color"), QStringLiteral("foreground")), m_foreground),
                     styleAlpha(QStringLiteral("normal-fill-alpha"), 0.04));
}

QColor ThemeManager::hoverFill() const {
    return withAlpha(flatColor(styleString(QStringLiteral("hover-cursor-color"), QStringLiteral("foreground")), m_foreground),
                     styleAlpha(QStringLiteral("hover-cursor-fill-alpha"), 0.08));
}

QColor ThemeManager::selectedFill() const {
    return withAlpha(flatColor(styleString(QStringLiteral("selected-color"), QStringLiteral("foreground")), m_foreground),
                     styleAlpha(QStringLiteral("selected-fill-alpha"), 0.18));
}

QColor ThemeManager::pressedFill() const {
    return withAlpha(flatColor(styleString(QStringLiteral("pressed-color"), styleString(QStringLiteral("hover-cursor-color"), QStringLiteral("foreground"))), m_foreground),
                     styleAlpha(QStringLiteral("pressed-fill-alpha"), 0.22));
}

QColor ThemeManager::normalBorderColor() const {
    return withAlpha(flatColor(styleString(QStringLiteral("normal-color"), QStringLiteral("foreground")), m_foreground),
                     styleAlpha(QStringLiteral("normal-border-alpha"), 0.4));
}

QColor ThemeManager::hoverBorderColor() const {
    return withAlpha(flatColor(styleString(QStringLiteral("hover-cursor-color"), QStringLiteral("foreground")), m_foreground),
                     styleAlpha(QStringLiteral("hover-cursor-border-alpha"), 0.25));
}

void ThemeManager::resolveFontFamilyAsync() {
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            const QString name = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
            if (!name.isEmpty() && name != m_resolvedFontFamily) {
                m_resolvedFontFamily = name;
                emit themeChanged();
            }
        }
        proc->deleteLater();
    });
    proc->start(QStringLiteral("fc-match"), {QStringLiteral("-f"), QStringLiteral("%{family[0]}"), QStringLiteral("monospace")});
}

} // namespace quattro
