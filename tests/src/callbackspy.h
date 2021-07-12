// Copyright (c) 2021 Private Internet Access, Inc.
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

#ifndef CALLBACKSPY_H
#define CALLBACKSPY_H

#include "common.h"
#include <QtTest>

// Adapt a callback to a QObject signal so it can be spied in unit tests; used
// to implement CallbackSpy.
class CallbackAdapter : public QObject
{
    Q_OBJECT

public:
    // The callback functor returned by callback() - this is movable/copiable;
    // when called it causes CallbackAdapter::called() to be emitted
    class Callback
    {
    public:
        Callback(CallbackAdapter &adapter) : _pAdapter{&adapter} {}
        // Callback for non-void results
        template<class ResultT>
        void operator()(const Error &err, const ResultT &result) const
        {
            if(_pAdapter)
                emit _pAdapter->called(err, QVariant::fromValue(result));
        }
        // Callback for void results
        void operator()(const Error &err) const
        {
            if(_pAdapter)
                emit _pAdapter->called(err, {});
        }
    private:
        QPointer<CallbackAdapter> _pAdapter;
    };

signals:
    // Spy on the 'called' signal to observe when a callback is called.
    // QObject doesn't support templates, so the result here is in a QVariant.
    // This is normally used with QSignalSpy, so this doesn't really impact
    // usability.
    void called(const Error &err, const QVariant &result);

public:
    // Pass the object returned by callback() to a method expecting a callback
    // functor
    Callback callback() {return {*this};}
};

// Spy on a callback.
// Use callback() to get a callback functor to pass to an API, and use spy() to
// get the QSignalSpy hooked up to that callback.
// Also a QObject so it can be used as the owning object for callback.
class CallbackSpy : public QObject
{
public:
    CallbackSpy()
        : _adapter{}, _spy{&_adapter, &CallbackAdapter::called}
    {}

public:
    auto callback() {return _adapter.callback();}
    QSignalSpy &spy() {return _spy;}

    // Check that there is exactly one response in this spy
    bool checkSingle() const;
    // Check that there is exactly one response, and that it is an error with
    // the specified code
    bool checkError(Error::Code code) const;
    // Check that there is exactly one response, and that it is a success (the
    // result value is not checked)
    bool checkSuccess() const;
    // Check that there is exactly one response, that it is a success, and that
    // the result value is of the type given (the actual value is not checked).
    template<class ResultT>
    bool checkSuccessType() const
    {
        if(!checkSuccess())
            return false;   // Traced by checkSuccess()
        if(!_spy[0][1].canConvert<ResultT>())
        {
            qWarning() << "Received value was not the required type:" << _spy[0][1];
            return false;
        }
        return true;
    }
    // Check that there is exactly one response, that it is a success, that the
    // result is of the type specified, and that the result value satisfies the
    // predicate given.
    template<class ResultT, class PredicateT>
    bool checkSuccessValue(PredicateT predicate) const
    {
        if(!checkSuccessType<ResultT>())
            return false;
        ResultT result{_spy[0][1].value<ResultT>()};
        if(!predicate(std::move(result)))
        {
            qWarning() << "Received value did not satisfy predicate:"
                << _spy[0][1];
            return false;
        }
        return true;
    }
    // Common case of checkSuccessValue() for a QJsonDocument response
    template<class PredicateT>
    bool checkSuccessJson(PredicateT predicate) const
    {
        return checkSuccessValue<QJsonDocument>(std::move(predicate));
    }

private:
    CallbackAdapter _adapter;
    QSignalSpy _spy;
};

#endif
