#include "FirestoreClient.h"
#include "AuthManager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>

static const char* BASE = "https://firestore.googleapis.com/v1/projects/etagames-b5d6a/databases/(default)/documents";

FirestoreClient::FirestoreClient(AuthManager* auth, QObject* parent)
    : QObject(parent), m_auth(auth) {
    m_nam = new QNetworkAccessManager(this);
}

QJsonObject FirestoreClient::timestampNow(int secondsOffset) {
    QJsonObject o;
    o["$ts"] = QDateTime::currentDateTimeUtc().addSecs(secondsOffset).toString(Qt::ISODate); // ...Z
    return o;
}

// ── JSON normale → valori Firestore ─────────────────────────────────────
QJsonValue FirestoreClient::encodeValue(const QJsonValue& v) {
    QJsonObject o;
    switch (v.type()) {
    case QJsonValue::Bool:   o["booleanValue"] = v.toBool(); break;
    case QJsonValue::String: o["stringValue"] = v.toString(); break;
    case QJsonValue::Double: {
        const double d = v.toDouble();
        if (d == static_cast<double>(static_cast<qint64>(d))) o["integerValue"] = QString::number(static_cast<qint64>(d));
        else o["doubleValue"] = d;
        break;
    }
    case QJsonValue::Array: {
        QJsonArray vals;
        for (const auto& e : v.toArray()) vals.append(encodeValue(e));
        QJsonObject arr; arr["values"] = vals;
        o["arrayValue"] = arr;
        break;
    }
    case QJsonValue::Object: {
        const QJsonObject obj = v.toObject();
        if (obj.contains("$ts")) { o["timestampValue"] = obj.value("$ts").toString(); break; }
        QJsonObject m; m["fields"] = encodeFields(obj);
        o["mapValue"] = m;
        break;
    }
    default: o["nullValue"] = QJsonValue::Null; break;
    }
    return o;
}

QJsonObject FirestoreClient::encodeFields(const QJsonObject& fields) {
    QJsonObject out;
    for (auto it = fields.begin(); it != fields.end(); ++it) out[it.key()] = encodeValue(it.value());
    return out;
}

// ── valori Firestore → JSON normale ─────────────────────────────────────
QJsonValue FirestoreClient::decodeValue(const QJsonObject& v) {
    if (v.contains("stringValue"))    return v.value("stringValue");
    if (v.contains("booleanValue"))   return v.value("booleanValue");
    if (v.contains("integerValue"))   return v.value("integerValue").toString().toLongLong();
    if (v.contains("doubleValue"))    return v.value("doubleValue");
    if (v.contains("timestampValue")) { QJsonObject o; o["$ts"] = v.value("timestampValue"); return o; }
    if (v.contains("arrayValue")) {
        QJsonArray out;
        for (const auto& e : v.value("arrayValue").toObject().value("values").toArray()) out.append(decodeValue(e.toObject()));
        return out;
    }
    if (v.contains("mapValue")) return decodeFields(v.value("mapValue").toObject().value("fields").toObject());
    return QJsonValue::Null;
}

QJsonObject FirestoreClient::decodeFields(const QJsonObject& fields) {
    QJsonObject out;
    for (auto it = fields.begin(); it != fields.end(); ++it) out[it.key()] = decodeValue(it.value().toObject());
    return out;
}

// ── HTTP ────────────────────────────────────────────────────────────────
void FirestoreClient::send(const QByteArray& verb, const QString& urlSuffix, const QJsonObject& body, bool hasBody,
                           std::function<void(bool, const QJsonDocument&, const QString&)> cb) {
    m_auth->withFreshToken([=](const QString& token) {
        QUrl url(QString::fromLatin1(BASE) + urlSuffix);
        if (token.isEmpty()) { // senza utente: solo con la chiave pubblica (utile per leggere dati pubblici)
            QUrlQuery q(url);
            q.addQueryItem("key", AuthManager::apiKey());
            url.setQuery(q);
        }
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        if (!token.isEmpty()) req.setRawHeader("Authorization", "Bearer " + token.toUtf8());

        const QByteArray data = hasBody ? QJsonDocument(body).toJson(QJsonDocument::Compact) : QByteArray();
        QNetworkReply* reply = (verb == "GET") ? m_nam->get(req)
                              : (verb == "POST") ? m_nam->post(req, data)
                              : m_nam->sendCustomRequest(req, verb, data);
        connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
            const QByteArray raw = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(raw);
            if (reply->error() != QNetworkReply::NoError) {
                QString msg = doc.object().value("error").toObject().value("message").toString();
                if (reply->error() == QNetworkReply::ContentNotFoundError) { cb(true, QJsonDocument(), QString()); }
                else cb(false, doc, msg.isEmpty() ? reply->errorString() : msg);
            } else {
                cb(true, doc, QString());
            }
            reply->deleteLater();
        });
    });
}

void FirestoreClient::getDocument(const QString& path, DocCb cb) {
    send("GET", "/" + path, QJsonObject(), false, [cb](bool ok, const QJsonDocument& doc, const QString& err) {
        if (!ok) { cb(false, QJsonObject(), err); return; }
        cb(true, decodeFields(doc.object().value("fields").toObject()), QString());
    });
}

void FirestoreClient::mergeFields(const QString& path, const QJsonObject& fields, DocCb cb) {
    QUrlQuery q;
    for (auto it = fields.begin(); it != fields.end(); ++it) q.addQueryItem("updateMask.fieldPaths", "`" + it.key() + "`");
    QJsonObject body; body["fields"] = encodeFields(fields);
    send("PATCH", "/" + path + "?" + q.toString(QUrl::FullyEncoded), body, true,
         [cb](bool ok, const QJsonDocument&, const QString& err) { if (cb) cb(ok, QJsonObject(), err); });
}

void FirestoreClient::setDocument(const QString& path, const QJsonObject& fields, DocCb cb) {
    QJsonObject body; body["fields"] = encodeFields(fields);
    send("PATCH", "/" + path, body, true,
         [cb](bool ok, const QJsonDocument&, const QString& err) { if (cb) cb(ok, QJsonObject(), err); });
}

void FirestoreClient::runQuery(const QJsonObject& structuredQuery, QueryCb cb) {
    QJsonObject body; body["structuredQuery"] = structuredQuery;
    send("POST", ":runQuery", body, true, [cb](bool ok, const QJsonDocument& doc, const QString& err) {
        if (!ok) { cb(false, QJsonArray(), err); return; }
        QJsonArray out;
        for (const auto& e : doc.array()) {
            const QJsonObject d = e.toObject().value("document").toObject();
            if (d.isEmpty()) continue; // l'ultimo elemento senza documento contiene solo readTime
            QJsonObject plain = decodeFields(d.value("fields").toObject());
            plain["_id"] = d.value("name").toString().section('/', -1);
            out.append(plain);
        }
        cb(true, out, QString());
    });
}
