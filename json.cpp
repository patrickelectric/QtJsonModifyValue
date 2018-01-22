#include "json.h"

Json::Json(bool debugMode, bool strictMode)
{
    this->debugMode = debugMode;
    this->strictMode = strictMode;
}

void Json::setDebugMode(bool debugMode)
{
    this->debugMode = debugMode;
}

void Json::setStrictMode(bool strictMode)
{
    this->strictMode = strictMode;
}

QList<QVariant> Json::createPathParts(const QString &path)
{
    QRegularExpression rx("([^\\[\\]\\.\\\\]|\\\\\\\\|\\\\\\[|\\\\\\]|\\\\\\.)+|\\.\\."); // regex to find parts with escape sequences
    QRegularExpressionMatchIterator it = rx.globalMatch(path);

    QList<QVariant> pathParts; // parts of path
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int prev = match.capturedStart() - 1; // index of char before part
        int next = match.capturedEnd(); // index of char after part

        bool pathPartIsObject; // if part of path is array or object

        if (0 <= prev && next < path.size() && path[prev] == '[' && path[next] == ']') // if part is array
            pathPartIsObject = false;
        else if (((prev == -1 || path[prev] == ']' || path[prev] == '.') && (next == path.size() || path[next] == '.' || path[next] == '[')) || match.captured() == "..") // if part is object
            pathPartIsObject = true;
        else { // error in path
            if (debugMode) {
                qDebug() << "Critical error in path near:" << path.mid(prev + 1, next - prev - 1);
            }
            return QList<QVariant>() << QVariant();
        }

        bool ok = true;

        if (pathPartIsObject)
            pathParts.append(match.captured().replace("..", "").replace("\\\\", "\\").replace("\\[", "[").replace("\\]", "]").replace("\\.", ".")); // insert part into list as object key and remove escape sequences
        else
            pathParts.append(match.captured().toInt(&ok)); // insert part into list as array index and check if index is valid

        if (!ok) { // if array index is invalid
            if (debugMode) {
                qDebug() << "Requested array index is invalid number:" << path.mid(prev, next - prev + 1);
            }
            return QList<QVariant>() << QVariant();
        }
    }

    return pathParts;
}

QList<QJsonValue> Json::getSubPathValues(const QJsonValue &val, const QList<QVariant> &pathParts, const bool &strictMode)
{
    QList<QJsonValue> values; // all sub values in path

    values.append(val); // add root element(value)

    for (int i = 0; i < pathParts.size(); ++i) { // expand whole subpath
        QVariant part = pathParts.at(i);

        QJsonValue lastVal = values.last();
        if (part.type() == static_cast<int>(QMetaType::QString))
            if (lastVal.isObject()) {
                values.append(lastVal.toObject().value(part.toString()));
            } else if (lastVal.isUndefined() && !strictMode) {
                values.replace(values.size() - 1, QJsonObject());
                values.append(values.last().toObject().value(part.toString()));
            } else {
                if (debugMode) {
                    qDebug() << "You have requested Json object:" << part.toString();
                    qDebug() << "But current QJsonValue is not object:" << lastVal;
                }
                return QList<QJsonValue>();
            }
        else
            if (lastVal.isArray()) {
                QJsonArray lastValArr = lastVal.toArray();
                for (int j = lastValArr.size(); j < part.toInt(); ++j) {
                    lastValArr.append(QJsonValue());
                }
                values.replace(values.size() - 1, lastValArr);
                values.append(values.last().toArray().at(part.toInt()));
            } else if (lastVal.isUndefined() && !strictMode) {
                values.replace(values.size() - 1,QJsonArray());
                QJsonArray lastValArr = lastVal.toArray();
                for (int j = lastValArr.size(); j < part.toInt(); ++j) {
                    lastValArr.append(QJsonValue());
                }
                values.replace(values.size() - 1, lastValArr);
                values.append(values.last().toArray().at(part.toInt()));
            } else {
                if (debugMode) {
                    qDebug() << "You have requested Json array with index:" << "[" + part.toString() + "]";
                    qDebug() << "But current QJsonValue is not array:" << lastVal;
                }
                return QList<QJsonValue>();
            }
    }

    if (strictMode && values.last().isUndefined()) {
        if (debugMode) {
            if (pathParts.last().type() == static_cast<int>(QMetaType::QString)) {
                qDebug() << "You are trying to create new key-value:" << pathParts.last().toString();
                qDebug() << "while in strict mode inside of object:" << values.at(values.size() - 2);
            } else {
                qDebug() << "You are trying to create new array index:" << "[" + pathParts.last().toString() + "]";
                qDebug() << "while in strict mode inside of array:" << values.at(values.size() - 2);
            }
        }
        return QList<QJsonValue>();
    }

    return values;
}

QJsonValue Json::joinSubPathValues(QList<QJsonValue> values, const QList<QVariant> &pathParts)
{
    for (int i = pathParts.count() - 1; i >= 0; --i) { // assign back subpath
        QJsonValue lastVal = values.last();
        values.removeLast();

        QVariant part = pathParts.at(i);

        if (part.type() == static_cast<int>(QMetaType::QString)) {
            QJsonObject updateVal = values.last().toObject();
            if (lastVal.isUndefined()) {
                updateVal.remove(part.toString());
            } else {
                updateVal.insert(part.toString(), lastVal);
            }
            values.removeLast();
            values.append(updateVal);
        } else {
            QJsonArray updateVal = values.last().toArray();
            if (lastVal.isUndefined()) {
                updateVal.removeAt(part.toInt());
            } else {
                updateVal.removeAt(part.toInt());
                updateVal.insert(part.toInt(), lastVal);
            }
            values.removeLast();
            values.append(updateVal);
        }
    }

    return values.first(); // assign new root element(value)
}

bool Json::create(QJsonDocument *doc, const QString &path, const QJsonValue &newValue)
{
    QJsonValue val;
    if (doc->isArray())
        val = doc->array();
    else if (doc->isObject())
        val = doc->object();
    else
        return false;

    bool returnValue = create(&val, path, newValue);

    if (val.isArray())
        doc->setArray(val.toArray());
    else if (val.isObject())
        doc->setObject(val.toObject());
    else
        return false;

    return returnValue;
}

bool Json::create(QJsonValue *val, const QString &path, const QJsonValue &newValue)
{
    QList<QVariant> pathParts = createPathParts(path); // parts of path
    if (pathParts.size() && !pathParts.at(0).isValid()) {
        return false;
    }

    QList<QJsonValue> values = getSubPathValues(*val, pathParts, false); // all sub values in path
    if (values.isEmpty()) {
        return false;
    }

    values.removeLast(); // remove
    values.append(newValue); // and assign new value

    *val = joinSubPathValues(values, pathParts); // assign new root element(value)

    return true;
}

bool Json::modify(QJsonDocument *doc, const QString &path, const QJsonValue &newValue)
{
    QJsonValue val;
    if (doc->isArray())
        val = doc->array();
    else if (doc->isObject())
        val = doc->object();
    else
        return false;

    bool returnValue = modify(&val, path, newValue);

    if (val.isArray())
        doc->setArray(val.toArray());
    else if (val.isObject())
        doc->setObject(val.toObject());
    else
        return false;

    return returnValue;
}

bool Json::modify(QJsonValue *val, const QString &path, const QJsonValue &newValue)
{
    QList<QVariant> pathParts = createPathParts(path); // parts of path
    if (pathParts.size() && !pathParts.at(0).isValid()) {
        return false;
    }

    QList<QJsonValue> values = getSubPathValues(*val, pathParts, strictMode); // all sub values in path
    if (values.isEmpty()) {
        return false;
    }

    values.removeLast(); // remove
    values.append(newValue); // and assign new value

    *val = joinSubPathValues(values, pathParts); // assign new root element(value)

    return true;
}

QJsonValue Json::get(const QJsonDocument &doc, const QString &path)
{
    QJsonValue val;
    if (doc.isArray())
        val = doc.array();
    else if (doc.isObject())
        val = doc.object();
    else
        return QJsonValue(QJsonValue::Undefined);

    return get(val, path);
}

QJsonValue Json::get(const QJsonValue &val, const QString &path)
{
    QList<QVariant> pathParts = createPathParts(path); // parts of path
    if (pathParts.size() && !pathParts.at(0).isValid()) {
        return QJsonValue(QJsonValue::Undefined);
    }

    QList<QJsonValue> values = getSubPathValues(val, pathParts, true); // all sub values in path
    if (values.isEmpty()) {
        return QJsonValue(QJsonValue::Undefined);
    }

    return values.last();
}

bool Json::remove(QJsonDocument *doc, const QString &path)
{
    return modify(doc, path, QJsonValue(QJsonValue::Undefined));
}

bool Json::remove(QJsonValue *val, const QString &path)
{
    return modify(val, path, QJsonValue(QJsonValue::Undefined));
}
