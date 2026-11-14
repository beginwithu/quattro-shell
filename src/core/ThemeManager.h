#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

namespace quattro {

struct BorderWidths {
    int top = 0;
    int right = 0;
    int bottom = 0;
    int left = 0;

    int maxWidth() const {
        return std::max({top, right, bottom, left});
    }
    bool isUniform() const {
        return top == right && right == bottom && bottom == left;
    }
    bool isNone() const {
        return maxWidth() <= 0;
    }
};

struct BorderGradient {
    QVector<QColor> colors;
    double angleDeg = 0.0;
    bool enabled = false;
};

struct BorderSpec {
    QColor color;
    BorderWidths widths;
    BorderGradient gradient;

    bool isNone() const { return widths.isNone(); }
};

struct SurfaceColors {
    QColor background;
    QColor text;
    QColor border;
    QColor active;
    QColor scrim;
    QColor selectedBackground;
    QColor selectedText;
    QColor selectedBorder;
    QColor countdown;
    QColor placeholder;
    QColor textError;
    QColor borderActive;
    QColor borderError;
    QColor selection;
    QColor accent;
    QColor unselectedBorder;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override;

    void loadFromDisk();
    void loadColorsText(const QString &raw);
    void loadThemeShellText(const QString &raw);
    void loadUserShellText(const QString &raw);
    void applyThemeBase64(const QByteArray &colorsB64, const QByteArray &shellB64);

    // Foundational palette
    QColor foreground() const { return m_foreground; }
    QColor background() const { return m_background; }
    QColor accent() const { return m_accent; }
    QColor urgent() const { return m_urgent; }
    QColor muted() const { return m_muted; }

    // Per-surface resolved colors
    const SurfaceColors &bar() const { return m_bar; }
    const SurfaceColors &popups() const { return m_popups; }
    const SurfaceColors &tooltip() const { return m_tooltip; }
    const SurfaceColors &notifications() const { return m_notifications; }
    const SurfaceColors &menu() const { return m_menu; }
    const SurfaceColors &launcher() const { return m_launcher; }
    const SurfaceColors &polkit() const { return m_polkit; }
    const SurfaceColors &lock() const { return m_lock; }
    const SurfaceColors &imagePicker() const { return m_imagePicker; }

    BorderSpec surfaceBorderSpec(const QString &section, const QString &token,
                                 const QColor &fallbackColor, int fallbackWidth,
                                 const QString &alphaKey = QString()) const;

    // Typography & layout metrics (mirrors Style.qml)
    QString fontFamily() const { return m_fontFamily; }
    QString resolvedFontFamily() const { return m_resolvedFontFamily; }
    QString menuFontFamily() const { return m_menuFontFamily; }

    int fontBaseSize() const { return m_fontBaseSize; }
    double fontScale() const { return std::max(1.0 / 12.0, m_fontBaseSize / 12.0); }
    int fontCaption() const;
    int fontBodySmall() const;
    int fontBody() const;
    int fontSubtitle() const;
    int fontTitle() const;
    int fontHeading() const;
    int fontDisplay() const;
    int fontDisplayLarge() const;

    int space(double px) const;
    double spaceReal(double px) const;
    int spacingToken(const QString &key, int fallback) const;

    int barSizeHorizontal() const;
    int barSizeVertical() const;
    int barIconSlot() const;
    int barIconCanvas() const;
    int barIconFont() const;
    int barIconSize() const;
    int barStatusSlot() const;

    int cornerRadius() const { return m_cornerRadius; }
    int gapsOut() const { return m_gapsOut; }
    bool reduceMotion() const { return m_reduceMotion; }
    int duration(int ms) const { return m_reduceMotion ? 0 : ms; }

    void setHyprlandMetrics(int cornerRadius, int gapsOut, bool reduceMotion);

    // Control state colors
    QColor normalFill() const;
    QColor hoverFill() const;
    QColor selectedFill() const;
    QColor pressedFill() const;
    QColor normalBorderColor() const;
    QColor hoverBorderColor() const;

    // Exposed static helpers for unit tests
    static QHash<QString, QString> parseShellToml(const QString &raw);
    static QColor parseColorToken(const QString &token, const QColor &fallback);
    static BorderWidths parseWidthSpec(const QString &raw, int fallback);
    static QColor withAlpha(const QColor &c, double alpha);

signals:
    void themeChanged();

private:
    void mergeShellValues();
    void rebuildDerived();
    void resolveFontFamilyAsync();
    QString pick(const QString &key, const QString &fallback) const;
    double pickAlpha(const QString &key, double fallback) const;
    QColor flatColor(const QString &value, const QColor &fallback) const;
    QColor composed(const QString &colorKey, const QString &alphaKey,
                    const QColor &colorFallback, double alphaFallback) const;
    QString resolveValueRef(const QString &raw) const;
    BorderGradient resolveGradient(const QString &raw, const QColor &fallback, double opacity) const;
    BorderWidths surfaceWidths(const QString &section, const QString &token, int fallbackWidth) const;

    int fontPx(double mult) const;
    int fontToken(const QString &key, int fallback) const;
    int barToken(const QString &key, int fallback) const;
    double styleNum(const QString &key, double fallback) const;
    double styleAlpha(const QString &key, double fallback) const;
    QString styleString(const QString &key, const QString &fallback) const;

    QString m_home;
    QString m_stateHome;
    QString m_themeDir;
    QString m_userShellPath;

    QColor m_foreground{QStringLiteral("#cacccc")};
    QColor m_background{QStringLiteral("#101315")};
    QColor m_accent{QStringLiteral("#cacccc")};
    QColor m_urgent{QStringLiteral("#a55555")};
    QColor m_muted{QStringLiteral("#707880")};

    QHash<QString, QString> m_themeShellValues;
    QHash<QString, QString> m_userShellValues;
    QHash<QString, QString> m_shellValues;

    QHash<QString, int> m_fontOverrides;
    QHash<QString, int> m_barOverrides;
    QHash<QString, double> m_spacingOverrides;
    QHash<QString, QString> m_styleOverrides;

    int m_fontBaseSize = 12;
    double m_spacingScale = 1.0;
    bool m_spacingScaleWithFont = true;
    bool m_barScaleWithFont = true;

    QString m_fontFamily{QStringLiteral("monospace")};
    QString m_resolvedFontFamily{QStringLiteral("monospace")};
    QString m_menuFontFamily{QStringLiteral("monospace")};

    int m_cornerRadius = 0;
    int m_gapsOut = 5;
    bool m_reduceMotion = false;

    SurfaceColors m_bar;
    SurfaceColors m_popups;
    SurfaceColors m_tooltip;
    SurfaceColors m_notifications;
    SurfaceColors m_menu;
    SurfaceColors m_launcher;
    SurfaceColors m_polkit;
    SurfaceColors m_lock;
    SurfaceColors m_imagePicker;

    QFileSystemWatcher m_watcher;
};

} // namespace quattro
