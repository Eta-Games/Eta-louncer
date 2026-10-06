#include "PlayStats.h"
#include "I18n.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <algorithm>

PlayStats::PlayStats() { load(); }

PlayStats& PlayStats::instance() {
    static PlayStats s;
    return s;
}

QString PlayStats::path() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath("playstats.json");
}

void PlayStats::load() {
    QFile f(path());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QJsonObject o = it.value().toObject();
        GameStats s;
        s.totalSecs  = qint64(o.value("secs").toDouble());
        s.lastPlayed = qint64(o.value("last").toDouble());
        s.sessions   = o.value("n").toInt();
        m_data.insert(it.key(), s);
    }
}

void PlayStats::save() const {
    QJsonObject root;
    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        QJsonObject o;
        o["secs"] = double(it->totalSecs);
        o["last"] = double(it->lastPlayed);
        o["n"]    = it->sessions;
        root[it.key()] = o;
    }
    QSaveFile f(path());   // scrittura atomica: niente file a metà se il PC si spegne
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.commit();
}

GameStats PlayStats::get(const QString& id) const {
    return m_data.value(id);
}

QList<QPair<QString, GameStats>> PlayStats::ranking() const {
    QList<QPair<QString, GameStats>> out;
    for (auto it = m_data.begin(); it != m_data.end(); ++it)
        if (it->sessions > 0) out.append({it.key(), it.value()});
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.second.totalSecs > b.second.totalSecs;
    });
    return out;
}

void PlayStats::addSession(const QString& id, qint64 endEpoch, qint64 secs) {
    if (secs < 5) return;   // avvii a vuoto / crash immediati non contano
    GameStats& s = m_data[id];
    s.totalSecs += secs;
    s.lastPlayed = endEpoch;
    s.sessions += 1;
    save();
    emit changed();
}

QString PlayStats::formatDuration(qint64 secs) {
    if (secs < 60) return T("meno di 1 min");
    const qint64 h = secs / 3600, m = (secs % 3600) / 60;
    return h == 0 ? QString("%1 min").arg(m) : QString("%1 h %2 min").arg(h).arg(m);
}

QString PlayStats::formatLast(qint64 epoch) {
    if (epoch <= 0) return T("mai");
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(epoch);
    const qint64 days = dt.date().daysTo(QDate::currentDate());
    if (days == 0) return T("oggi alle ") + dt.toString("HH:mm");
    if (days == 1) return T("ieri alle ") + dt.toString("HH:mm");
    return dt.toString("dd/MM/yyyy");
}
