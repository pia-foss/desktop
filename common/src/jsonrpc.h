// Copyright (c) 2024 Private Internet Access, Inc.
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
#line HEADER_FILE("jsonrpc.h")

#ifndef JSONRPC_H
#define JSONRPC_H
#pragma once

#include "async.h"
#include "json.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QSet>

#include <cmath>
#include <initializer_list>


COMMON_EXPORT QJsonObject parseJsonRPCMessage(const QByteArray& msg) throws(Error);
COMMON_EXPORT void parseJsonRPCRequest(const QJsonObject& request, QString& method, QJsonArray& params) throws(Error);


// Helper type that wraps a callable of a given signature and converts the
// result to an Async<QJsonValue> value (synchronous functions return an
// already resolved Async<QJsonValue>; void functions resolve to Undefined).
//
template<typename Return, typename ArgsTuple>
struct LocalMethodWrapper
{
    template<typename Func, size_t... I>
    static Async<QJsonValue> invoke(Func&& func, const QJsonArray& params, std::index_sequence<I...>)
    {
        return Async<QJsonValue>::resolve(func(json_cast<std::decay_t<std::tuple_element_t<I, ArgsTuple>>>(params[I], HERE)...));
    }
};
template<typename ArgsTuple>
struct LocalMethodWrapper<void, ArgsTuple>
{
    template<typename Func, size_t... I>
    static Async<QJsonValue> invoke(Func&& func, const QJsonArray& params, std::index_sequence<I...>)
    {
        func(json_cast<std::decay_t<std::tuple_element_t<I, ArgsTuple>>>(params[I], HERE)...);
        return Async<QJsonValue>::resolve(QJsonValue::Undefined);
    }
};
template<typename Return, typename ArgsTuple>
struct LocalMethodWrapper<Async<Return>, ArgsTuple>
{
    template<typename Func, size_t... I>
    static Async<QJsonValue> invoke(Func&& func, const QJsonArray& params, std::index_sequence<I...>)
    {
        return func(json_cast<std::decay_t<std::tuple_element_t<I, ArgsTuple>>>(params[I], HERE)...)->then([](const Return& value) { return json_cast<QJsonValue>(value, HERE); });
    }
};
template<typename ArgsTuple>
struct LocalMethodWrapper<Async<void>, ArgsTuple>
{
    template<typename Func, size_t... I>
    static Async<QJsonValue> invoke(Func&& func, const QJsonArray& params, std::index_sequence<I...>)
    {
        // Transform the void return to QJsonValue::Undefined
        return func(json_cast<std::decay_t<std::tuple_element_t<I, ArgsTuple>>>(params[I], HERE)...)->then([]() -> QJsonValue { return QJsonValue::Undefined; });
    }
};

template<typename Class, typename Ptr, typename Return, typename... Args>
static inline auto bind_this(Return (Class::*fn)(Args...), Ptr instance)
{
    return [instance = std::move(instance), fn](Args&&... args) -> Return { return ((*instance).*fn)(std::forward<Args>(args)...); };
}
template<typename Class, typename Ptr, typename... Args>
static inline auto bind_this(void (Class::*fn)(Args...), Ptr instance)
{
    return [instance = std::move(instance), fn](Args&&... args) { ((*instance).*fn)(std::forward<Args>(args)...); };
}

// Encapsulates a local function, member function, functor or lambda
// so that it can be invoked by a JSON-RPC request.
//
class COMMON_EXPORT LocalMethod
{
    typedef std::function<Async<QJsonValue>(const QJsonArray&)> Func;

    Func _fn;
    QString _name;
    int _paramCount;
    QJsonArray _defaultArguments;

public:
    template<typename Result, typename... Args>
    LocalMethod(const QString& name, std::function<Result(Args...)> fn) : _name(name) { wrap<Result, Args...>(std::move(fn)); }
    template<typename Result, typename... Args, class Class, typename Context>
    LocalMethod(const QString& name, Context&& context, Result (Class::*fn)(Args...)) : _name(name) { wrap<Result, Args...>(bind_this(fn, std::forward<Context>(context))); }
    template<typename Result, typename... Args>
    LocalMethod(const QString& name, Result (*fn)(Args...)) : _name(name) { wrap<Result, Args...>(fn); }
    template<typename Functor>
    LocalMethod(const QString& name, Functor&& functor) : _name(name) { wrap_functor(std::forward<Functor>(functor), &Functor::operator()); }

    // Attach a set of defaults arguments that will be used to fill in the
    // last sizeof...(Args) arguments if they are not supplied in the RPC
    // function invocation.
    template<typename... Args>
    LocalMethod& defaultArguments(Args&&... args) { _defaultArguments = QJsonArray { json_cast<QJsonValue>(args, HERE)... }; return *this; }

    const QString& name() const { return _name; }

    // Invoke the registered function, catching any Errors or exceptions
    // and converting them to rejected Async Tasks.
    Async<QJsonValue> operator()(const QJsonArray& params) noexcept;

private:
    // Helper function to wrap the logic to unpack and cast the QJsonArray
    // elements and invoke the registered function. The resulting function
    // can throw Errors.
    template<typename Result, typename... Args, typename Func>
    inline void wrap(Func&& fn)
    {
        _paramCount = sizeof...(Args);
        // Note: Qt Creator has trouble parsing the move-initialized capture
        // variable here, so you might get incorrect syntax highlighting here.
        _fn = [this, fn = std::move(fn)](const QJsonArray& params) -> Async<QJsonValue> {
            return LocalMethodWrapper<Result, std::tuple<Args...>>::invoke(fn, params, std::index_sequence_for<Args...>{});
        };
    }
    // Helper function to deduce the signature of a functor/lambda by
    // inspecting the signature of its operator().
    template<typename Result, typename... Args, typename Functor, typename Class>
    inline void wrap_functor(Functor&& functor, Result (Class::*)(Args...) const)
    {
        wrap<Result, Args...>(std::forward<Functor>(functor));
    }

    Async<QJsonValue> invoke(const QJsonArray& params);
};


// Class to handle the set of local functions to expose via JSON-RPC.
//
class COMMON_EXPORT LocalMethodRegistry : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("jsonrpc")

public:
    explicit LocalMethodRegistry(QObject *parent = nullptr) : QObject(parent) {}
    LocalMethodRegistry(const std::initializer_list<LocalMethod>& methods, QObject* parent = nullptr);
    ~LocalMethodRegistry();
    void add(const LocalMethod& method);
    void add(const std::initializer_list<LocalMethod>& methods);

public:
    Async<QJsonValue> invoke(const QString& method, const QJsonArray& params);

private:
    QHash<QString, std::function<Async<QJsonValue>(const QJsonArray&)>> _methods;
};


// Class for receiving and handling JSON-RPC Notifications. A node using
// this class never responds to requests, not even with critical errors.
//
class COMMON_EXPORT LocalNotificationInterface : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("jsonrpc")

public:
    explicit LocalNotificationInterface(LocalMethodRegistry* registry, QObject* parent = nullptr);

public slots:
    virtual bool processMessage(const QByteArray& msg);
    virtual bool processRequest(const QJsonObject& request);

protected:
    LocalMethodRegistry* _registry;
};


// Class for receiving and handling JSON-RPC Requests (both Notifications
// and calls with return values). For Notifications, errors are suppressed
// but severe errors like parse errors will be returned to the sender.
//
class COMMON_EXPORT LocalCallInterface : public LocalNotificationInterface
{
    Q_OBJECT

public:
    using LocalNotificationInterface::LocalNotificationInterface;

public slots:
    virtual bool processMessage(const QByteArray& msg) override;
    virtual bool processRequest(const QJsonObject& request) override;

protected:
    void respondWithResult(const QJsonValue& id, const QJsonValue& result);
    void respondWithError(const QJsonValue& id, const Error& error);
    void respondWithError(const QJsonValue& id, const QJsonObject& error);

signals:
    void messageReady(const QByteArray& response);
};


// Class for sending JSON-RPC Notifications to a remote node. Does not
// wait for or expect any error responses from the remote end.
//
class COMMON_EXPORT RemoteNotificationInterface : public QObject
{
    Q_OBJECT
    CLASS_LOGGING_CATEGORY("jsonrpc")

public:
    using QObject::QObject;

    template<typename... Args>
    inline void post(const QString& name, Args&&... args);

    void postWithParams(const QString& method, const QJsonArray& params);

protected:
    void request(const QJsonValue& id, const QString& method, const QJsonArray& params);

signals:
    void messageReady(const QByteArray& msg);
};


// Class for sending JSON-RPC Requests (including Notifications) to a
// remote node. For non-Notifications, asynchronously waits for the
// result to be posted back from the remote node, which you can wait
// for using the returned Async<QJsonValue> object.
//
class COMMON_EXPORT RemoteCallInterface : public RemoteNotificationInterface
{
    Q_OBJECT

    double getNextId();

public:
    explicit RemoteCallInterface(QObject* parent = nullptr)
        : RemoteNotificationInterface(parent), _lastId(0.0) {}

    template<typename... Args>
    inline Async<QJsonValue> call(const QString& method, Args&&... args);

    Async<QJsonValue> callWithParams(const QString& method, const QJsonArray& params);

private:
    // Get the ID from a request/response.
    bool getId(const QJsonObject &msg, double &id);

public slots:
    virtual bool processMessage(const QByteArray& msg);
    bool processResponse(const QJsonObject& response);
    // Call if a message could not be sent (rejects the request)
    void requestSendError(const Error &error, const QByteArray &msg);
    // Call if the connection is lost (rejects all outstanding requests)
    void connectionLost();

private:
    QHash<double, QWeakPointer<Task<QJsonValue>>> _responses;
    double _lastId;
};

// Convenience class for the "client" side (makes calls, receives notifications)
class COMMON_EXPORT ClientSideInterface : public RemoteCallInterface
{
    Q_OBJECT
public:
    explicit ClientSideInterface(LocalMethodRegistry* methods, QObject* parent = nullptr);

public slots:
    virtual bool processMessage(const QByteArray& msg) override;

private:
    LocalNotificationInterface _local;
};

// Convenience class for the "server" side (receives calls, posts notifications)
class COMMON_EXPORT ServerSideInterface : public RemoteNotificationInterface
{
    Q_OBJECT
public:
    explicit ServerSideInterface(LocalMethodRegistry* methods, QObject* parent = nullptr);

public slots:
    bool processMessage(const QByteArray& msg);

private:
    LocalCallInterface _local;
};

// Inline function definitions

template<typename... Args>
inline void RemoteNotificationInterface::post(const QString& method, Args&&... args)
{
    postWithParams(method, QJsonArray { json_cast<QJsonValue>(args, HERE)... });
}

template<typename... Args>
Async<QJsonValue> RemoteCallInterface::call(const QString& method, Args&&... args)
{
    return callWithParams(method, QJsonArray { json_cast<QJsonValue>(args, HERE)... });
}


#endif // JSONRPC_H
