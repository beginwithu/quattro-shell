#include "WeatherElsewhenPanels.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>

namespace quattro {

// ---------------- WeatherPanel ----------------

WeatherPanel::WeatherPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-weather-panel"), false, parent)
{
    loadSettings();
}

void WeatherPanel::loadSettings() {
    QFile f(QDir::homePath() + QStringLiteral("/.local/state/quattro/settings/weather.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
        if (!obj.value(QStringLiteral("location")).toString().isEmpty())
            m_locationName = obj.value(QStringLiteral("location")).toString();
        if (obj.contains(QStringLiteral("lat"))) m_lat = obj.value(QStringLiteral("lat")).toDouble(m_lat);
        if (obj.contains(QStringLiteral("lon"))) m_lon = obj.value(QStringLiteral("lon")).toDouble(m_lon);
        m_fahrenheit = (obj.value(QStringLiteral("unit")).toString() == QLatin1String("F"));
    }
}

void WeatherPanel::saveSettings() {
    const QString dir = QDir::homePath() + QStringLiteral("/.local/state/quattro/settings");
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/weather.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonObject obj;
        obj.insert(QStringLiteral("location"), m_locationName);
        obj.insert(QStringLiteral("lat"), m_lat);
        obj.insert(QStringLiteral("lon"), m_lon);
        obj.insert(QStringLiteral("unit"), m_fahrenheit ? QStringLiteral("F") : QStringLiteral("C"));
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
}

void WeatherPanel::onOpening() {
    m_searchMode = false;
    m_searchText.clear();
    m_geoResults.clear();
    fetchWeather();
}

QString WeatherPanel::formatTemp(double tempC) const {
    if (m_fahrenheit)
        return QStringLiteral("%1°F").arg(qRound(tempC * 9.0 / 5.0 + 32.0));
    return QStringLiteral("%1°C").arg(qRound(tempC));
}

QString WeatherPanel::wmoLabel(int code) {
    if (code == 0) return QStringLiteral("Clear Sky");
    if (code <= 3) return QStringLiteral("Partly Cloudy");
    if (code <= 48) return QStringLiteral("Foggy");
    if (code <= 57) return QStringLiteral("Drizzle");
    if (code <= 67) return QStringLiteral("Rain");
    if (code <= 77) return QStringLiteral("Snow");
    if (code <= 82) return QStringLiteral("Showers");
    if (code >= 95) return QStringLiteral("Thunderstorm");
    return QStringLiteral("Cloudy");
}

QString WeatherPanel::wmoGlyph(int code) {
    if (code == 0) return QStringLiteral("󰖙");
    if (code <= 3) return QStringLiteral("󰖕");
    if (code <= 48) return QStringLiteral("󰖑");
    if (code <= 67 || (code >= 80 && code <= 82)) return QStringLiteral("󰖗");
    if (code <= 77) return QStringLiteral("󰖘");
    if (code >= 95) return QStringLiteral("󰖓");
    return QStringLiteral("󰖐");
}

void WeatherPanel::fetchWeather() {
    m_loading = true;
    update();
    QUrl url(QStringLiteral("https://api.open-meteo.com/v1/forecast"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("latitude"), QString::number(m_lat, 'f', 4));
    q.addQueryItem(QStringLiteral("longitude"), QString::number(m_lon, 'f', 4));
    q.addQueryItem(QStringLiteral("current"), QStringLiteral("temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code"));
    q.addQueryItem(QStringLiteral("hourly"), QStringLiteral("temperature_2m,weather_code"));
    q.addQueryItem(QStringLiteral("daily"), QStringLiteral("weather_code,temperature_2m_max,temperature_2m_min"));
    q.addQueryItem(QStringLiteral("timezone"), QStringLiteral("auto"));
    q.addQueryItem(QStringLiteral("forecast_days"), QStringLiteral("5"));
    url.setQuery(q);

    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        const auto err = reply->error();
        reply->deleteLater();
        m_loading = false;
        if (err != QNetworkReply::NoError) {
            if (isOpened()) update();
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(raw).object();
        const QJsonObject cur = root.value(QStringLiteral("current")).toObject();
        m_curTempC = cur.value(QStringLiteral("temperature_2m")).toDouble();
        m_apparentC = cur.value(QStringLiteral("apparent_temperature")).toDouble();
        m_windKmh = cur.value(QStringLiteral("wind_speed_10m")).toDouble();
        m_humidity = cur.value(QStringLiteral("relative_humidity_2m")).toInt();
        m_curCode = cur.value(QStringLiteral("weather_code")).toInt();
        m_hasData = true;

        m_hourly.clear();
        const QJsonObject hObj = root.value(QStringLiteral("hourly")).toObject();
        const QJsonArray hTimes = hObj.value(QStringLiteral("time")).toArray();
        const QJsonArray hTemps = hObj.value(QStringLiteral("temperature_2m")).toArray();
        const QJsonArray hCodes = hObj.value(QStringLiteral("weather_code")).toArray();
        const int curHour = QTime::currentTime().hour();
        for (int i = curHour; i < hTimes.size() && m_hourly.size() < 6; i += 2) {
            HourlyForecastSlot slot;
            slot.hour = hTimes.at(i).toString().mid(11, 5);
            slot.tempC = hTemps.at(i).toDouble();
            slot.code = hCodes.at(i).toInt();
            m_hourly.push_back(slot);
        }

        m_daily.clear();
        const QJsonObject dObj = root.value(QStringLiteral("daily")).toObject();
        const QJsonArray dTimes = dObj.value(QStringLiteral("time")).toArray();
        const QJsonArray dMin = dObj.value(QStringLiteral("temperature_2m_min")).toArray();
        const QJsonArray dMax = dObj.value(QStringLiteral("temperature_2m_max")).toArray();
        const QJsonArray dCodes = dObj.value(QStringLiteral("weather_code")).toArray();
        for (int i = 0; i < dTimes.size() && i < 5; ++i) {
            DailyForecastSlot ds;
            const QDate dt = QDate::fromString(dTimes.at(i).toString(), Qt::ISODate);
            ds.dayName = (i == 0) ? QStringLiteral("Today") : dt.toString(QStringLiteral("ddd"));
            ds.minC = dMin.at(i).toDouble();
            ds.maxC = dMax.at(i).toDouble();
            ds.code = dCodes.at(i).toInt();
            m_daily.push_back(ds);
        }
        if (isOpened()) update();
    });
}

void WeatherPanel::searchLocation(const QString &q) {
    if (q.trimmed().size() < 2) return;
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery uq;
    uq.addQueryItem(QStringLiteral("name"), q.trimmed());
    uq.addQueryItem(QStringLiteral("count"), QStringLiteral("6"));
    url.setQuery(uq);
    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray raw = reply->readAll();
        reply->deleteLater();
        m_geoResults.clear();
        const QJsonArray arr = QJsonDocument::fromJson(raw).object().value(QStringLiteral("results")).toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            m_geoResults.push_back({
                o.value(QStringLiteral("name")).toString(),
                o.value(QStringLiteral("admin1")).toString(),
                o.value(QStringLiteral("country")).toString(),
                o.value(QStringLiteral("latitude")).toDouble(),
                o.value(QStringLiteral("longitude")).toDouble()
            });
        }
        if (isOpened()) update();
    });
}

void WeatherPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;
    int y = card.top() + pad;

    const QString title = m_hasData ? QStringLiteral("%1  %2").arg(m_locationName, formatTemp(m_curTempC))
                                    : m_locationName;
    const QString sub = m_hasData ? QStringLiteral("%1 • Feels %2 • Wind %3 km/h • %4%")
                                        .arg(wmoLabel(m_curCode))
                                        .arg(formatTemp(m_apparentC))
                                        .arg(qRound(m_windKmh))
                                        .arg(m_humidity)
                                  : (m_loading ? QStringLiteral("Fetching forecast...") : QStringLiteral("Weather unavailable"));
    UiPaint::drawHero(p, QRect(card.left() + pad, y, innerW, 50), theme(),
                      m_hasData ? wmoGlyph(m_curCode) : QStringLiteral("󰖐"),
                      title, sub, sc.text, muted, accent);
    y += 58;

    const int bw = (innerW - 12) / 3;
    m_searchToggleRect = QRect(card.left() + pad, y, bw, 28);
    m_unitToggleRect = QRect(m_searchToggleRect.right() + 6, y, bw, 28);
    m_refreshRect = QRect(m_unitToggleRect.right() + 6, y, bw, 28);
    UiPaint::drawPillButton(p, m_searchToggleRect, theme(), QStringLiteral("Location"), m_searchMode, m_searchToggleRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_unitToggleRect, theme(), m_fahrenheit ? QStringLiteral("Unit: °F") : QStringLiteral("Unit: °C"),
                            false, m_unitToggleRect.contains(m_hoverPos));
    UiPaint::drawPillButton(p, m_refreshRect, theme(), QStringLiteral("Refresh"), false, m_refreshRect.contains(m_hoverPos));
    y += 38;

    if (m_searchMode) {
        UiPaint::drawSearchHeader(p, QRect(card.left() + pad, y, innerW, 34), theme(), m_searchText,
                                  QStringLiteral("Type city name and press Enter..."), sc.text, muted, accent);
        y += 42;
        m_geoRows.clear();
        for (int i = 0; i < m_geoResults.size(); ++i) {
            const QRect r(card.left() + pad, y + i * 38, innerW, 34);
            m_geoRows.push_back({i, r});
            p.setBrush(r.contains(m_hoverPos) ? ThemeManager::withAlpha(accent, 0.16) : ThemeManager::withAlpha(sc.text, 0.05));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 8, 8);
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(), QFont::DemiBold));
            p.setPen(sc.text);
            const auto &g = m_geoResults.at(i);
            p.drawText(r.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                       QStringLiteral("%1, %2 (%3)").arg(g.name, g.admin.isEmpty() ? g.country : g.admin, g.country));
        }
        return;
    }

    if (!m_hourly.isEmpty()) {
        UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, innerW, 20), theme(), QStringLiteral("Hourly"), muted);
        y += 24;
        const int cw = (innerW - (m_hourly.size() - 1) * 6) / std::max(1, static_cast<int>(m_hourly.size()));
        for (int i = 0; i < m_hourly.size(); ++i) {
            const QRect box(card.left() + pad + i * (cw + 6), y, cw, 68);
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.06));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(box, 8, 8);
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
            p.setPen(muted);
            p.drawText(box.adjusted(0, 4, 0, -44), Qt::AlignCenter, m_hourly.at(i).hour);
            p.setFont(QFont(theme()->resolvedFontFamily(), 15));
            p.setPen(accent);
            p.drawText(box.adjusted(0, 22, 0, -22), Qt::AlignCenter, wmoGlyph(m_hourly.at(i).code));
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption(), QFont::DemiBold));
            p.setPen(sc.text);
            p.drawText(box.adjusted(0, 46, 0, -4), Qt::AlignCenter, formatTemp(m_hourly.at(i).tempC));
        }
        y += 80;
    }

    if (!m_daily.isEmpty()) {
        UiPaint::drawSectionHeader(p, QRect(card.left() + pad, y, innerW, 20), theme(), QStringLiteral("5-Day Forecast"), muted);
        y += 24;
        for (const auto &d : m_daily) {
            const QRect row(card.left() + pad, y, innerW, 34);
            p.setBrush(ThemeManager::withAlpha(sc.text, 0.05));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(row, 8, 8);
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall(), QFont::DemiBold));
            p.setPen(sc.text);
            p.drawText(row.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, d.dayName);
            p.setPen(accent);
            p.drawText(row, Qt::AlignCenter, QStringLiteral("%1  %2").arg(wmoGlyph(d.code), wmoLabel(d.code)));
            p.setPen(muted);
            p.drawText(row.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight,
                       QStringLiteral("%1 / %2").arg(formatTemp(d.minC), formatTemp(d.maxC)));
            y += 40;
        }
    }
}

bool WeatherPanel::handleKey(QKeyEvent *event) {
    if (!m_searchMode) return false;
    const int k = event->key();
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        searchLocation(m_searchText);
        return true;
    }
    if (k == Qt::Key_Backspace) {
        if (!m_searchText.isEmpty()) { m_searchText.chop(1); update(); }
        return true;
    }
    const QString t = event->text();
    if (!t.isEmpty() && t.at(0).isPrint()) {
        m_searchText += t;
        update();
        return true;
    }
    return false;
}

void WeatherPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_searchToggleRect.contains(event->pos())) {
        m_searchMode = !m_searchMode;
        update();
        return;
    }
    if (m_unitToggleRect.contains(event->pos())) {
        m_fahrenheit = !m_fahrenheit;
        saveSettings();
        update();
        return;
    }
    if (m_refreshRect.contains(event->pos())) {
        fetchWeather();
        return;
    }
    if (m_searchMode) {
        for (const auto &gr : m_geoRows) {
            if (gr.second.contains(event->pos()) && gr.first < m_geoResults.size()) {
                const auto &g = m_geoResults.at(gr.first);
                m_locationName = g.name;
                m_lat = g.lat;
                m_lon = g.lon;
                m_searchMode = false;
                saveSettings();
                fetchWeather();
                return;
            }
        }
    }
}

// ---------------- ElsewhenPanel ----------------

ElsewhenPanel::ElsewhenPanel(ThemeManager *theme, ShellConfig *config, QWidget *parent)
    : ShellPopupWindow(theme, config, QStringLiteral("quattro-elsewhen-panel"), false, parent)
{
    loadSlots();
}

void ElsewhenPanel::ensureAssetsLoaded() {
    if (m_assetsLoaded) return;
    m_assetsLoaded = true;

    QFile cw(QStringLiteral(":/assets/cities.json"));
    if (cw.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(cw.readAll()).array();
        for (const QJsonValue &v : arr) {
            const QJsonArray a = v.toArray();
            if (a.size() >= 4) {
                m_cities.push_back({a[0].toString(), a[1].toString(), a[2].toDouble(), a[3].toDouble(), a.size() > 4 ? a[4].toInt(1) : 1});
            }
        }
    }

    QFile wf(QStringLiteral(":/assets/world.json"));
    if (wf.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(wf.readAll()).array();
        for (const QJsonValue &v : arr) {
            const QJsonArray a = v.toArray();
            QVector<QPointF> poly;
            poly.reserve(a.size() / 2);
            for (int i = 0; i + 1 < a.size(); i += 2)
                poly.push_back(QPointF(a[i].toDouble(), a[i + 1].toDouble()));
            if (!poly.isEmpty())
                m_worldPolys.push_back(poly);
        }
    }
}

void ElsewhenPanel::loadSlots() {
    m_slots.clear();
    QFile f(QDir::homePath() + QStringLiteral("/.local/state/quattro/settings/elsewhen.json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            m_slots.push_back({o.value(QStringLiteral("label")).toString(),
                               o.value(QStringLiteral("tz")).toString(),
                               o.value(QStringLiteral("lat")).toDouble(),
                               o.value(QStringLiteral("lon")).toDouble()});
        }
    }
    if (m_slots.isEmpty()) {
        m_slots.push_back({QStringLiteral("London"), QStringLiteral("Europe/London"), 51.51, -0.13});
        m_slots.push_back({QStringLiteral("New York"), QStringLiteral("America/New_York"), 40.71, -74.01});
        m_slots.push_back({QStringLiteral("Tokyo"), QStringLiteral("Asia/Tokyo"), 35.69, 139.69});
    }
}

void ElsewhenPanel::saveSlots() {
    const QString dir = QDir::homePath() + QStringLiteral("/.local/state/quattro/settings");
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/elsewhen.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QJsonArray arr;
        for (const auto &s : m_slots) {
            QJsonObject o;
            o.insert(QStringLiteral("label"), s.label);
            o.insert(QStringLiteral("tz"), s.tz);
            o.insert(QStringLiteral("lat"), s.lat);
            o.insert(QStringLiteral("lon"), s.lon);
            arr.push_back(o);
        }
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    }
}

void ElsewhenPanel::onOpening() {
    ensureAssetsLoaded();
    m_addingCity = false;
    m_cityQuery.clear();
    m_hourOffset = 0;
    m_scrollOffset = 0;
}

void ElsewhenPanel::paintCardContent(QPainter &p, const QRect &card) {
    const SurfaceColors &sc = theme()->popups();
    const QColor muted = theme()->muted();
    const QColor accent = sc.accent.isValid() ? sc.accent : theme()->accent();
    const int pad = 16;
    const int innerW = card.width() - pad * 2;
    int y = card.top() + pad;

    const QDateTime localNow = QDateTime::currentDateTime().addSecs(m_hourOffset * 3600);
    UiPaint::drawHero(p, QRect(card.left() + pad, y, innerW - 110, 46), theme(),
                      QStringLiteral("󰖈"),
                      QStringLiteral("Elsewhen World Clock"),
                      m_hourOffset == 0 ? QStringLiteral("Local: %1").arg(localNow.toString(QStringLiteral("ddd HH:mm")))
                                        : QStringLiteral("Preview %1%2h • %3").arg(m_hourOffset > 0 ? QStringLiteral("+") : QString()).arg(m_hourOffset).arg(localNow.toString(QStringLiteral("ddd HH:mm"))),
                      sc.text, muted, accent);
    m_addBtnRect = QRect(card.right() - pad - 102, y + 8, 102, 30);
    UiPaint::drawPillButton(p, m_addBtnRect, theme(), m_addingCity ? QStringLiteral("Done") : QStringLiteral("+ Add City"),
                            m_addingCity, m_addBtnRect.contains(m_hoverPos));
    y += 56;

    // Equirectangular vector world map + pin markers
    const QRect mapR(card.left() + pad, y, innerW, 132);
    p.save();
    p.setClipRect(mapR);
    p.setBrush(ThemeManager::withAlpha(sc.text, 0.05));
    p.setPen(QPen(ThemeManager::withAlpha(sc.text, 0.14), 1));
    p.drawRoundedRect(mapR, 10, 10);

    auto project = [&mapR](double lon, double lat) -> QPointF {
        const double x = mapR.left() + ((lon + 180.0) / 360.0) * mapR.width();
        const double py = mapR.top() + ((90.0 - lat) / 180.0) * mapR.height();
        return QPointF(x, py);
    };

    p.setBrush(ThemeManager::withAlpha(sc.text, 0.16));
    p.setPen(Qt::NoPen);
    for (const auto &polyPts : m_worldPolys) {
        QPolygonF poly;
        poly.reserve(polyPts.size());
        for (const QPointF &ll : polyPts)
            poly.push_back(project(ll.x(), ll.y()));
        p.drawPolygon(poly);
    }

    // Pins for configured timezones
    p.setBrush(accent);
    for (const auto &s : m_slots) {
        const QPointF pt = project(s.lon, s.lat);
        p.drawEllipse(pt, 4.0, 4.0);
    }
    p.restore();
    y += 140;

    if (m_addingCity) {
        UiPaint::drawSearchHeader(p, QRect(card.left() + pad, y, innerW, 34), theme(), m_cityQuery,
                                  QStringLiteral("Search cities (London, Tokyo, Jakarta...)"), sc.text, muted, accent);
        y += 40;
        m_cityMatches.clear();
        m_cityResultRects.clear();
        const QString needle = m_cityQuery.trimmed();
        for (int i = 0; i < m_cities.size() && m_cityMatches.size() < 7; ++i) {
            if (needle.isEmpty() || m_cities.at(i).name.contains(needle, Qt::CaseInsensitive) ||
                m_cities.at(i).tz.contains(needle, Qt::CaseInsensitive)) {
                m_cityMatches.push_back(i);
            }
        }
        for (int idx = 0; idx < m_cityMatches.size(); ++idx) {
            const QRect r(card.left() + pad, y + idx * 34, innerW, 30);
            const int cityIdx = m_cityMatches.at(idx);
            m_cityResultRects.push_back({cityIdx, r});
            p.setBrush(r.contains(m_hoverPos) ? ThemeManager::withAlpha(accent, 0.18) : ThemeManager::withAlpha(sc.text, 0.06));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(r, 7, 7);
            p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBodySmall()));
            p.setPen(sc.text);
            p.drawText(r.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, m_cities.at(cityIdx).name);
            p.setPen(muted);
            p.drawText(r.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight, m_cities.at(cityIdx).tz);
        }
        return;
    }

    // Interactive 24h scrub strip
    m_stripRect = QRect(card.left() + pad, y, innerW, 24);
    for (int h = 0; h < 24; ++h) {
        const int x0 = m_stripRect.left() + (h * innerW) / 24;
        const int x1 = m_stripRect.left() + ((h + 1) * innerW) / 24;
        const bool isDay = (h >= 7 && h < 19);
        const bool isActive = (((localNow.time().hour()) % 24) == h);
        QColor seg = isActive ? accent : (isDay ? ThemeManager::withAlpha(accent, 0.28) : ThemeManager::withAlpha(sc.text, 0.10));
        p.fillRect(QRect(x0 + 1, m_stripRect.top(), std::max(2, x1 - x0 - 2), 24), seg);
        if (h % 6 == 0) {
            p.setFont(QFont(theme()->resolvedFontFamily(), 8));
            p.setPen(isActive ? theme()->background() : muted);
            p.drawText(QRect(x0, m_stripRect.top(), 24, 24), Qt::AlignCenter, QString::number(h));
        }
    }
    y += 32;

    // Timezone cards
    m_removeSlotRects.clear();
    const QRect listR(card.left() + pad, y, innerW, card.bottom() - pad - y);
    p.save();
    p.setClipRect(listR);
    const int rowH = 54;
    m_maxScroll = std::max(0, static_cast<int>(m_slots.size()) * rowH - listR.height());
    m_scrollOffset = std::clamp(m_scrollOffset, 0, m_maxScroll);

    for (int i = 0; i < m_slots.size(); ++i) {
        const int ry = listR.top() + i * rowH - m_scrollOffset;
        if (ry + rowH < listR.top() || ry > listR.bottom()) continue;
        const QRect r(listR.left(), ry, listR.width(), rowH - 6);
        p.setBrush(ThemeManager::withAlpha(sc.text, 0.06));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(r, 9, 9);

        const auto &slot = m_slots.at(i);
        const QTimeZone tz(slot.tz.toUtf8());
        const QDateTime dt = tz.isValid() ? localNow.toTimeZone(tz) : localNow;
        const int diffHrs = qRound((dt.offsetFromUtc() - localNow.offsetFromUtc()) / 3600.0);

        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontBody(), QFont::DemiBold));
        p.setPen(sc.text);
        p.drawText(r.adjusted(12, 4, -140, -22), Qt::AlignVCenter | Qt::AlignLeft, slot.label);

        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(muted);
        p.drawText(r.adjusted(12, 24, -140, -4), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("%1 (%2%3h)").arg(slot.tz, diffHrs >= 0 ? QStringLiteral("+") : QString()).arg(diffHrs));

        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontTitle(), QFont::Bold));
        p.setPen(accent);
        p.drawText(r.adjusted(0, 0, -40, 0), Qt::AlignVCenter | Qt::AlignRight, dt.toString(QStringLiteral("HH:mm")));

        const QRect rm(r.right() - 28, r.center().y() - 10, 20, 20);
        m_removeSlotRects.push_back({i, rm});
        p.setFont(QFont(theme()->resolvedFontFamily(), theme()->fontCaption()));
        p.setPen(rm.contains(m_hoverPos) ? theme()->urgent() : muted);
        p.drawText(rm, Qt::AlignCenter, QStringLiteral("✕"));
    }
    p.restore();
}

bool ElsewhenPanel::handleKey(QKeyEvent *event) {
    if (!m_addingCity) return false;
    const int k = event->key();
    if (k == Qt::Key_Backspace) {
        if (!m_cityQuery.isEmpty()) { m_cityQuery.chop(1); update(); }
        return true;
    }
    const QString t = event->text();
    if (!t.isEmpty() && t.at(0).isPrint()) {
        m_cityQuery += t;
        update();
        return true;
    }
    return false;
}

void ElsewhenPanel::handleCardMousePress(QMouseEvent *event, const QRect & /*card*/) {
    if (m_addBtnRect.contains(event->pos())) {
        m_addingCity = !m_addingCity;
        m_cityQuery.clear();
        update();
        return;
    }
    if (m_addingCity) {
        for (const auto &cr : m_cityResultRects) {
            if (cr.second.contains(event->pos()) && cr.first < m_cities.size()) {
                const auto &c = m_cities.at(cr.first);
                m_slots.push_back({c.name, c.tz, c.lat, c.lon});
                saveSlots();
                m_addingCity = false;
                update();
                return;
            }
        }
        return;
    }
    if (m_stripRect.contains(event->pos())) {
        const int clickedH = std::clamp(((event->pos().x() - m_stripRect.left()) * 24) / std::max(1, m_stripRect.width()), 0, 23);
        const int curH = QDateTime::currentDateTime().time().hour();
        m_hourOffset = clickedH - curH;
        update();
        return;
    }
    for (const auto &rm : m_removeSlotRects) {
        if (rm.second.contains(event->pos()) && rm.first < m_slots.size()) {
            m_slots.removeAt(rm.first);
            saveSlots();
            update();
            return;
        }
    }
}

} // namespace quattro
