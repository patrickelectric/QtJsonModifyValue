#ifndef JSON_H
#define JSON_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>

class Json
{
public:
    Json(bool debugMode = false, bool strictMode = false);
    void setDebugMode(bool debugMode);
    void setStrictMode(bool strictMode);
    bool create(QJsonDocument *doc, const QString &path, const QJsonValue &newValue);
    bool create(QJsonValue *val, const QString &path, const QJsonValue &newValue);
    bool modify(QJsonDocument *doc, const QString &path, const QJsonValue &newValue);
    bool modify(QJsonValue *val, const QString &path, const QJsonValue &newValue);
    QJsonValue get(const QJsonDocument &doc, const QString &path);
    QJsonValue get(const QJsonValue &val, const QString &path);
    bool remove(QJsonDocument *doc, const QString &path);
    bool remove(QJsonValue *val, const QString &path);

private:
    bool debugMode;
    bool strictMode;
    QList<QVariant> createPathParts(const QString &path);
    QList<QJsonValue> getSubPathValues(const QJsonValue &val, const QList<QVariant> &pathParts, const bool &strictModeL);
    QJsonValue joinSubPathValues(QList<QJsonValue> values, const QList<QVariant> &pathParts);
};

#endif // JSON_H
