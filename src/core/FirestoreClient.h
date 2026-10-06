#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <functional>

class QNetworkAccessManager;
class AuthManager;

// Client minimale per Firestore via REST (stesso progetto Firebase del sito: etagames-b5d6a).
// I documenti si leggono/scrivono come JSON "normale": i tipi Firestore (stringValue, booleanValue, ...)
// vengono convertiti qui. Un timestamp si rappresenta come {"$ts": "2026-10-05T21:00:00Z"}.
class FirestoreClient : public QObject {
    Q_OBJECT
public:
    using DocCb   = std::function<void(bool ok, const QJsonObject& fields, const QString& error)>;
    using QueryCb = std::function<void(bool ok, const QJsonArray& docs, const QString& error)>;

    explicit FirestoreClient(AuthManager* auth, QObject* parent = nullptr);

    // Legge un documento (path es. "users/<uid>"). Se non esiste → ok=true con oggetto vuoto.
    void getDocument(const QString& path, DocCb cb);
    // Come set(..., {merge:true}) del SDK web: aggiorna solo i campi indicati.
    void mergeFields(const QString& path, const QJsonObject& fields, DocCb cb = nullptr);
    // Sostituisce l'intero documento (lo crea se manca).
    void setDocument(const QString& path, const QJsonObject& fields, DocCb cb = nullptr);
    // structuredQuery di Firestore; ogni elemento restituito è l'oggetto del documento con "_id".
    void runQuery(const QJsonObject& structuredQuery, QueryCb cb);

    static QJsonObject timestampNow(int secondsOffset = 0);

private:
    AuthManager* m_auth;
    QNetworkAccessManager* m_nam;

    void send(const QByteArray& verb, const QString& urlSuffix, const QJsonObject& body, bool hasBody,
              std::function<void(bool, const QJsonDocument&, const QString&)> cb);
    static QJsonValue encodeValue(const QJsonValue& v);
    static QJsonValue decodeValue(const QJsonObject& v);
    static QJsonObject decodeFields(const QJsonObject& fields);
    static QJsonObject encodeFields(const QJsonObject& fields);
};
