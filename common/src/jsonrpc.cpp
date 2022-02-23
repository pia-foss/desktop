// Copyright (c) 2022 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include "common.h"
#line SOURCE_FILE("jsonrpc.cpp")

#include "jsonrpc.h"

namespace
{
    // The "data" method is used from the daemon to provide updates to clients.
    // It's invoked a lot and is not interesting, suppress normal tracing for
    // this method.
    bool suppressMethodTracing(const QString &method)
    {
        return method == QStringLiteral("data");
    }
}

QJsonObject parseJsonRPCMessage(const QByteArray &msg) throws(Error)
{
    QJsonParseError error;
    QJsonDocument json = QJsonDocument::fromJson(msg, &error);
    if (error.error != QJsonParseError::NoError)
        throw JsonRPCParseError(HERE, error.errorString());
    if (json.isArray())
        throw JsonRPCInvalidRequestError(HERE, "batch messages not supported");
    else if (!json.isObject())
        throw JsonRPCInvalidRequestError(HERE, "unrecognized message");
    return json.object();
}

void parseJsonRPCRequest(const QJsonObject &request, QString &method, QJsonArray &params) throws(Error)
{
    const auto jsonrpc = request[QLatin1String("jsonrpc")];
    if (!jsonrpc.isString() || jsonrpc.toString() != QStringLiteral("2.0"))
        throw JsonRPCInvalidRequestError(HERE, "not JSON-RPC 2.0");
    auto methodValue = request[QLatin1String("method")];
    if (methodValue.isString())
        method = methodValue.toString();
    else
        throw JsonRPCInvalidRequestError(HERE, "invalid method name");
    auto paramsValue = request[QLatin1String("params")];
    if (paramsValue.isArray())
        params = paramsValue.toArray();
    else if (paramsValue.isObject())
        throw JsonRPCInvalidParamsError(HERE, "named parameters not supported");
    else if (!paramsValue.isUndefined())
        throw JsonRPCInvalidParamsError(HERE, "invalid parameter format");
    else
        params = QJsonArray();
}

Async<QJsonValue> LocalMethod::operator()(const QJsonArray &params) noexcept
{
    try
    {
        return invoke(params);
    }
    catch (const Error& e)
    {
        return Async<QJsonValue>::reject(e);
    }
    catch (const std::exception& e)
    {
        return Async<QJsonValue>::reject(UnknownError(HERE, QString::fromLocal8Bit(e.what())));
    }
    catch (...)
    {
        return Async<QJsonValue>::reject(UnknownError(HERE));
    }
}

Async<QJsonValue> LocalMethod::invoke(const QJsonArray& params)
{
    if (params.count() >= _paramCount)
        return _fn(params);
    else if (params.count() + _defaultArguments.count() >= _paramCount)
    {
        QJsonArray amendedParams = params;
        for (int i = params.count() + _defaultArguments.count() - _paramCount; i < _defaultArguments.size(); i++)
            amendedParams.append(_defaultArguments.at(i));
        return _fn(amendedParams);
    }
    else
        throw JsonRPCInvalidParamsError(HERE);
}

LocalMethodRegistry::LocalMethodRegistry(const std::initializer_list<LocalMethod> &methods, QObject* parent)
    : QObject(parent)
{
    add(methods);
}

LocalMethodRegistry::~LocalMethodRegistry()
{

}

void LocalMethodRegistry::add(const LocalMethod &method)
{
    _methods.insert(method.name(), method);
}

void LocalMethodRegistry::add(const std::initializer_list<LocalMethod> &methods)
{
    _methods.reserve(_methods.size() + (int)methods.size());
    for (const auto& method : methods)
        add(method);
}

Async<QJsonValue> LocalMethodRegistry::invoke(const QString &method, const QJsonArray &params)
{
    auto it = _methods.find(method);
    if (it != _methods.end())
    {
        if(!suppressMethodTracing(method))
        {
            qInfo() << "Invoking" << method;
        }
        // Trace the result of this invocation for supportability - important to
        // see why connect requests fail, etc., if that happens.  It might also
        // be traced by LocalCallInterface if the result is being sent back to
        // the caller, but this only happens if the caller is interested in the
        // result (the GUI client frequently ignores the result).
        try
        {
            return (*it)(params)
                ->next([method](const Error &err, const QJsonValue &result)
                {
                    if(err)
                    {
                        qWarning() << "Invocation of" << method
                            << "resulted in error" << err;
                        throw err;
                    }

                    // Don't trace the actual result; we can't redact any
                    // potentially sensitive info since we don't know what it
                    // is.
                    if(!suppressMethodTracing(method))
                    {
                        qInfo() << "Invocation of" << method << "succeeded";
                    }
                    return result;
                });
        }
        catch(const Error &error)
        {
            qWarning() << "Invocation of" << method << "threw error:" << error;
            throw;
        }
        catch(const std::exception &ex)
        {
            qWarning() << "Invocation of" << method << "threw exception:" << ex.what();
            throw;
        }
        catch(...)
        {
            qWarning() << "Invocation of" << method << "threw unknown exception";
            throw;
        }
    }
    else
    {
        qWarning() << "Can't invoke" << method << "- method not found";
        return Async<QJsonValue>::reject(JsonRPCMethodNotFoundError(HERE, method));
    }
}

LocalNotificationInterface::LocalNotificationInterface(LocalMethodRegistry *registry, QObject *parent)
    : QObject(parent), _registry(registry)
{
    connect(registry, &QObject::destroyed, this, [this]() { _registry = nullptr; });
}

bool LocalNotificationInterface::processMessage(const QByteArray &msg)
{
    try
    {
        return processRequest(parseJsonRPCMessage(msg));
    }
    catch (const Error& error)
    {
        qWarning() << error;
        return false;
    }
}

bool LocalNotificationInterface::processRequest(const QJsonObject &request)
{
    QString method;
    QJsonArray params;
    try
    {
        parseJsonRPCRequest(request, method, params);
        // Sanity check that we only receive
        auto id = request[QLatin1String("id")];
        if (!id.isUndefined() && !id.isNull())
        {
            qWarning() << "Call request sent to Notification-only interface";
        }
        // Note: intentionally ignoring return value; we don't care about the result
        _registry->invoke(method, params)->runUntilFinished(this);
        // Signal success
        return true;
    }
    catch (const Error& error)
    {
        qWarning() << error;
        return false;
    }
}

bool LocalCallInterface::processMessage(const QByteArray &msg)
{
    try
    {
        return processRequest(parseJsonRPCMessage(msg));
    }
    catch (const Error& error)
    {
        qWarning() << error;
        respondWithError(QJsonValue::Null, error);
        return false;
    }
}

bool LocalCallInterface::processRequest(const QJsonObject &request)
{
    auto id = request[QLatin1String("id")];
    QString method;
    QJsonArray params;
    try
    {
        if (id.isUndefined() || id.isNull())
            id = QJsonValue::Undefined; // ignore errors
        else if (!id.isDouble() && !id.isString())
        {
            id = QJsonValue::Null; // send errors
            throw JsonRPCInvalidRequestError(HERE, "bad ID");
        }
        // Parse out the method name and parameters
        parseJsonRPCRequest(request, method, params);
        // Invoke the method and watch the result
        qInfo() << "Request" << id << "- invoking RPC method" << method;
        if (auto task = _registry->invoke(method, params))
        {
            // The task is kept alive by the capture of 'task', and will be
            // disposed either when it finishes, or when we are destroyed.
            task->notify(this, [this, id](const Error& error, const QJsonValue& result) {
                if (id.isUndefined())
                    return;
                if (error)
                    respondWithError(id, error);
                else
                    respondWithResult(id, result);
            });
        }
        else
            throw JsonRPCInternalError(HERE, "Async method returned null");
        // Signal success
        return true;
    }
    catch (const Error& error)
    {
        qWarning() << error;
        if (!id.isUndefined())
            respondWithError(id, error);
        return false;
    }
    catch (const std::exception& e)
    {
        qWarning() << "Caught exception in RPC invoke:" << e.what();
        if (!id.isUndefined())
            respondWithError(id, UnknownError(HERE, QString::fromLocal8Bit(e.what())));
        return false;
    }
    catch (...)
    {
        qWarning() << "Caught unknown exception in RPC invoke";
        if (!id.isUndefined())
            respondWithError(id, UnknownError(HERE));
        return false;
    }
}

void LocalCallInterface::respondWithResult(const QJsonValue &id, const QJsonValue &result)
{
    // Indicate success, but don't trace the result (can't clean the result for
    // tracing since we don't know anything about the request semantics)
    qInfo() << "Request" << id << "- responding with result";
    QJsonObject msg {
        { QStringLiteral("jsonrpc"), QStringLiteral("2.0") },
        { QStringLiteral("id"), id },
        { QStringLiteral("result"), result.isUndefined() ? QJsonValue::Null : result },
    };
    emit messageReady(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void LocalCallInterface::respondWithError(const QJsonValue &id, const Error &error)
{
    respondWithError(id, error.toJsonObject());
}

void LocalCallInterface::respondWithError(const QJsonValue &id, const QJsonObject &error)
{
    qInfo() << "Request" << id << "- responding with error" << error;
    QJsonObject msg;
    msg[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    msg[QStringLiteral("id")] = (id.isString() || id.isDouble()) ? id : QJsonValue(QJsonValue::Null);
    msg[QStringLiteral("error")] = error;
    emit messageReady(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void RemoteNotificationInterface::postWithParams(const QString& method, const QJsonArray& params)
{
    request(QJsonValue::Undefined, method, params);
}

void RemoteNotificationInterface::request(const QJsonValue &id, const QString &method, const QJsonArray &params)
{
    if(!suppressMethodTracing(method))
    {
        qInfo() << "Sending request" << id << "to invoke RPC method" << method;
    }
    QJsonObject msg;
    msg[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    if (id.isString() || id.isDouble())
        msg[QStringLiteral("id")] = id;
    msg[QStringLiteral("method")] = method;
    msg[QStringLiteral("params")] = params;
    emit messageReady(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

double RemoteCallInterface::getNextId()
{
    return _lastId = std::ceil(std::nextafter(_lastId, INFINITY));
}

bool RemoteCallInterface::getId(const QJsonObject &msg, double &id)
{
    auto idValue = msg[QLatin1String("id")];
    if (!idValue.isDouble())
        return false;

    id = idValue.toDouble();
    return true;
}

bool RemoteCallInterface::processMessage(const QByteArray &msg)
{
    try
    {
        return processResponse(parseJsonRPCMessage(msg));
    }
    catch (const Error& error)
    {
        qWarning() << error;
        return false;
    }
}

bool RemoteCallInterface::processResponse(const QJsonObject &response)
{
    // This function just silently returns on failures, as it's usually
    // called prior to an attempt to parse the message as a Request; as
    // such, any severe failures will be reported then instead.

    const auto jsonrpc = response[QLatin1String("jsonrpc")];
    if (!jsonrpc.isString() || jsonrpc.toString() != QStringLiteral("2.0"))
        return false;
    double id;
    if(!getId(response, id))
        return false;

    auto it = _responses.find(id);
    if (it == _responses.end())
    {
        qWarning() << "Received response for unknown ID" << id;
        return false;
    }

    QWeakPointer<Task<QJsonValue>> weakTask = _responses.take(id);
    auto task = weakTask.toStrongRef();

    if (!task)
    {
        qDebug() << "Received response for ID" << id << "but no one is listening";
        return false;
    }

    auto result = response[QLatin1String("result")];
    auto error = response[QLatin1String("error")];

    if (!error.isUndefined() || result.isUndefined()) // note: error takes precedence over result
    {
        if (error.isObject())
        {
            auto errorObject = error.toObject();
            auto code = static_cast<Error::Code>(static_cast<int>(errorObject[QLatin1String("code")].toDouble()));
            auto message = errorObject[QLatin1String("message")].toString("Unknown error");
            auto data = errorObject[QLatin1String("data")].toArray();
            QStringList params;
            params.reserve(data.size());
            for (auto v : data)
            {
                params.push_back(v.toString());
            }
            qWarning() << "Request" << id << "received error:" << message;
            task->reject(Error(HERE, code, params));
        }
        else
        {
            qWarning() << "Request" << id << "received unknown error";
            task->reject(UnknownError(HERE));
        }
    }
    else
    {
        qInfo() << "Request" << id << "succeeded";
        task->resolve(result);
    }
    return true;
}

void RemoteCallInterface::requestSendError(const Error &error, const QByteArray &msg)
{
    try
    {
        const auto &request = parseJsonRPCMessage(msg);
        double id;
        if(!getId(request, id))
            return;

        // Look up the task
        QWeakPointer<Task<QJsonValue>> weakTask = _responses.take(id);
        auto pTask = weakTask.toStrongRef();

        // If the task is still around, reject it
        if (pTask)
            pTask->reject(error);
    }
    catch (const Error& error)
    {
        // This _really_ shouldn't happen, this is a message object that we
        // previously tried to send
        qError() << "Unable to send request:" << error;
    }
}

void RemoteCallInterface::connectionLost()
{
    Error connectionLostErr{HERE, Error::Code::JsonRPCConnectionLost};
    // Pull out all responses before rejecting any (if any rejection creates new
    // requests, handle them normally)
    QHash<double, QWeakPointer<Task<QJsonValue>>> rejectResponses;
    _responses.swap(rejectResponses);
    if(!rejectResponses.isEmpty())
    {
        qInfo() << "Reject" << rejectResponses.size() << "responses due to connection loss";
        for(const auto &weakTask : rejectResponses)
        {
            auto pTask = weakTask.toStrongRef();
            if(pTask)
                pTask->reject(connectionLostErr);
        }
    }
}

Async<QJsonValue> RemoteCallInterface::callWithParams(const QString &method, const QJsonArray &params)
{
    static const QMetaMethod messageReadySignal = QMetaMethod::fromSignal(&RemoteCallInterface::messageReady);

    auto id = getNextId();
    auto result = Async<QJsonValue>::create();
    if (isSignalConnected(messageReadySignal))
    {
        _responses.insert(id, result);
        request(id, method, params);
    }
    else
    {
        // No one is going to get this message, so fail here
        result->reject(JsonRPCInternalError(HERE));
    }
    return result;
}

ClientSideInterface::ClientSideInterface(LocalMethodRegistry *methods, QObject *parent)
    : RemoteCallInterface(parent), _local(methods)
{

}

bool ClientSideInterface::processMessage(const QByteArray &msg)
{
    try
    {
        QJsonObject object = parseJsonRPCMessage(msg);
        return RemoteCallInterface::processResponse(object) || _local.processRequest(object);
    }
    catch (const Error& error)
    {
        qWarning() << error;
        return false;
    }
}

ServerSideInterface::ServerSideInterface(LocalMethodRegistry *methods, QObject *parent)
    : RemoteNotificationInterface(parent), _local(methods)
{
    connect(&_local, &LocalCallInterface::messageReady, this, &ServerSideInterface::messageReady);
}

bool ServerSideInterface::processMessage(const QByteArray &msg)
{
    return _local.processMessage(msg);
}
