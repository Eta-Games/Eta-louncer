/****************************************************************************
** Meta object code from reading C++ file 'SelfUpdater.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/core/SelfUpdater.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SelfUpdater.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN11SelfUpdaterE_t {};
} // unnamed namespace

template <> constexpr inline auto SelfUpdater::qt_create_metaobjectdata<qt_meta_tag_ZN11SelfUpdaterE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SelfUpdater",
        "updateAvailable",
        "",
        "SelfUpdateInfo",
        "info",
        "upToDate",
        "checkFailed",
        "error",
        "progress",
        "received",
        "total",
        "downloaded",
        "file",
        "downloadFailed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'updateAvailable'
        QtMocHelpers::SignalData<void(SelfUpdateInfo)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'upToDate'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'checkFailed'
        QtMocHelpers::SignalData<void(QString)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Signal 'progress'
        QtMocHelpers::SignalData<void(qint64, qint64)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 9 }, { QMetaType::LongLong, 10 },
        }}),
        // Signal 'downloaded'
        QtMocHelpers::SignalData<void(QString, SelfUpdateInfo)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 12 }, { 0x80000000 | 3, 4 },
        }}),
        // Signal 'downloadFailed'
        QtMocHelpers::SignalData<void(QString)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SelfUpdater, qt_meta_tag_ZN11SelfUpdaterE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SelfUpdater::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SelfUpdaterE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SelfUpdaterE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11SelfUpdaterE_t>.metaTypes,
    nullptr
} };

void SelfUpdater::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SelfUpdater *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->updateAvailable((*reinterpret_cast<std::add_pointer_t<SelfUpdateInfo>>(_a[1]))); break;
        case 1: _t->upToDate(); break;
        case 2: _t->checkFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->progress((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2]))); break;
        case 4: _t->downloaded((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<SelfUpdateInfo>>(_a[2]))); break;
        case 5: _t->downloadFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SelfUpdater::*)(SelfUpdateInfo )>(_a, &SelfUpdater::updateAvailable, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfUpdater::*)()>(_a, &SelfUpdater::upToDate, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfUpdater::*)(QString )>(_a, &SelfUpdater::checkFailed, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfUpdater::*)(qint64 , qint64 )>(_a, &SelfUpdater::progress, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfUpdater::*)(QString , SelfUpdateInfo )>(_a, &SelfUpdater::downloaded, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SelfUpdater::*)(QString )>(_a, &SelfUpdater::downloadFailed, 5))
            return;
    }
}

const QMetaObject *SelfUpdater::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SelfUpdater::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11SelfUpdaterE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SelfUpdater::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void SelfUpdater::updateAvailable(SelfUpdateInfo _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void SelfUpdater::upToDate()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void SelfUpdater::checkFailed(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void SelfUpdater::progress(qint64 _t1, qint64 _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void SelfUpdater::downloaded(QString _t1, SelfUpdateInfo _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void SelfUpdater::downloadFailed(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
