/****************************************************************************
** Meta object code from reading C++ file 'IdleMonitor.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/wayland/IdleMonitor.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'IdleMonitor.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7quattro16IdleNotificationE_t {};
} // unnamed namespace

template <> constexpr inline auto quattro::IdleNotification::qt_create_metaobjectdata<qt_meta_tag_ZN7quattro16IdleNotificationE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "quattro::IdleNotification",
        "idled",
        "",
        "resumed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'idled'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'resumed'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<IdleNotification, qt_meta_tag_ZN7quattro16IdleNotificationE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject quattro::IdleNotification::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro16IdleNotificationE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro16IdleNotificationE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7quattro16IdleNotificationE_t>.metaTypes,
    nullptr
} };

void quattro::IdleNotification::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<IdleNotification *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->idled(); break;
        case 1: _t->resumed(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (IdleNotification::*)()>(_a, &IdleNotification::idled, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (IdleNotification::*)()>(_a, &IdleNotification::resumed, 1))
            return;
    }
}

const QMetaObject *quattro::IdleNotification::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *quattro::IdleNotification::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro16IdleNotificationE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QtWayland::ext_idle_notification_v1"))
        return static_cast< QtWayland::ext_idle_notification_v1*>(this);
    return QObject::qt_metacast(_clname);
}

int quattro::IdleNotification::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void quattro::IdleNotification::idled()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void quattro::IdleNotification::resumed()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
namespace {
struct qt_meta_tag_ZN7quattro11IdleMonitorE_t {};
} // unnamed namespace

template <> constexpr inline auto quattro::IdleMonitor::qt_create_metaobjectdata<qt_meta_tag_ZN7quattro11IdleMonitorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "quattro::IdleMonitor",
        "screensaverTriggered",
        "",
        "lockTriggered",
        "activityResumed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'screensaverTriggered'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'lockTriggered'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'activityResumed'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<IdleMonitor, qt_meta_tag_ZN7quattro11IdleMonitorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject quattro::IdleMonitor::staticMetaObject = { {
    QMetaObject::SuperData::link<QWaylandClientExtensionTemplate<IdleMonitor>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro11IdleMonitorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro11IdleMonitorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7quattro11IdleMonitorE_t>.metaTypes,
    nullptr
} };

void quattro::IdleMonitor::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<IdleMonitor *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->screensaverTriggered(); break;
        case 1: _t->lockTriggered(); break;
        case 2: _t->activityResumed(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (IdleMonitor::*)()>(_a, &IdleMonitor::screensaverTriggered, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (IdleMonitor::*)()>(_a, &IdleMonitor::lockTriggered, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (IdleMonitor::*)()>(_a, &IdleMonitor::activityResumed, 2))
            return;
    }
}

const QMetaObject *quattro::IdleMonitor::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *quattro::IdleMonitor::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro11IdleMonitorE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QtWayland::ext_idle_notifier_v1"))
        return static_cast< QtWayland::ext_idle_notifier_v1*>(this);
    return QWaylandClientExtensionTemplate<IdleMonitor>::qt_metacast(_clname);
}

int quattro::IdleMonitor::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWaylandClientExtensionTemplate<IdleMonitor>::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void quattro::IdleMonitor::screensaverTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void quattro::IdleMonitor::lockTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void quattro::IdleMonitor::activityResumed()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
