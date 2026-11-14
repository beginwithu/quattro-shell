#pragma once

#include "../CommonUi.h"
#include <QNetworkAccessManager>
#include <QPolygonF>
#include <QTimeZone>
#include <QVector>

namespace quattro {

struct HourlyForecastSlot {
    QString hour;
    double tempC = 0.0;
    int code = 0;
};

struct DailyForecastSlot {
    QString dayName;
    double minC = 0.0;
    double maxC = 0.0;
    int code = 0;
};

struct GeoCandidate {
    QString name;
    QString admin;
    QString country;
    double lat = 0.0;
    double lon = 0.0;
};

class WeatherPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    WeatherPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(420, 500); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    bool handleKey(QKeyEvent *event) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void loadSettings();
    void saveSettings();
    void fetchWeather();
    void searchLocation(const QString &q);
    static QString wmoLabel(int code);
    static QString wmoGlyph(int code);
    QString formatTemp(double tempC) const;

    QNetworkAccessManager m_nam;
    QString m_locationName = QStringLiteral("Jakarta");
    double m_lat = -6.21;
    double m_lon = 106.85;
    bool m_fahrenheit = false;

    bool m_loading = false;
    bool m_searchMode = false;
    QString m_searchText;
    QVector<GeoCandidate> m_geoResults;
    QVector<QPair<int, QRect>> m_geoRows;

    double m_curTempC = 0.0;
    double m_apparentC = 0.0;
    double m_windKmh = 0.0;
    int m_humidity = 0;
    int m_curCode = 0;
    bool m_hasData = false;
    QVector<HourlyForecastSlot> m_hourly;
    QVector<DailyForecastSlot> m_daily;

    QRect m_unitToggleRect;
    QRect m_searchToggleRect;
    QRect m_refreshRect;
};

struct WorldCity {
    QString name;
    QString tz;
    double lat = 0.0;
    double lon = 0.0;
    int tier = 1;
};

struct ElsewhenSlot {
    QString label;
    QString tz;
    double lat = 0.0;
    double lon = 0.0;
};

class ElsewhenPanel : public ShellPopupWindow {
    Q_OBJECT
public:
    ElsewhenPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent = nullptr);

protected:
    QSize desiredCardSize() const override { return QSize(560, 520); }
    void onOpening() override;
    void paintCardContent(QPainter &p, const QRect &card) override;
    bool handleKey(QKeyEvent *event) override;
    void handleCardMousePress(QMouseEvent *event, const QRect &card) override;

private:
    void ensureAssetsLoaded();
    void loadSlots();
    void saveSlots();

    bool m_assetsLoaded = false;
    QVector<QVector<QPointF>> m_worldPolys; // lon/lat pairs
    QVector<WorldCity> m_cities;
    QVector<ElsewhenSlot> m_slots;

    bool m_addingCity = false;
    QString m_cityQuery;
    QVector<int> m_cityMatches;
    int m_hourOffset = 0; // +/- interactive scrub across 24h daylight bar

    QRect m_addBtnRect;
    QRect m_stripRect;
    QVector<QPair<int, QRect>> m_removeSlotRects;
    QVector<QPair<int, QRect>> m_cityResultRects;
};

} // namespace quattro
