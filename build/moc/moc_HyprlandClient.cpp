/****************************************************************************
** Meta object code from reading C++ file 'HyprlandClient.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/core/HyprlandClient.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'HyprlandClient.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7quattro14HyprlandClientE_t {};
} // unnamed namespace

template <> constexpr inline auto quattro::HyprlandClient::qt_create_metaobjectdata<qt_meta_tag_ZN7quattro14HyprlandClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "quattro::HyprlandClient",
        "stateChanged",
        "",
        "activeWindowChanged",
        "keyboardLayoutChanged",
        "layout",
        "optionsChanged",
        "rounding",
        "gapsOut",
        "animationsEnabled",
        "configReloaded",
        "fullscreenChanged",
        "screencastChanged",
        "onEventReadyRead",
        "reconnectEventSocket"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'stateChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'activeWindowChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'keyboardLayoutChanged'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'optionsChanged'
        QtMocHelpers::SignalData<void(int, int, bool)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::Int, 8 }, { QMetaType::Bool, 9 },
        }}),
        // Signal 'configReloaded'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'fullscreenChanged'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'screencastChanged'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onEventReadyRead'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'reconnectEventSocket'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<HyprlandClient, qt_meta_tag_ZN7quattro14HyprlandClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject quattro::HyprlandClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro14HyprlandClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro14HyprlandClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7quattro14HyprlandClientE_t>.metaTypes,
    nullptr
} };

void quattro::HyprlandClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<HyprlandClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->stateChanged(); break;
        case 1: _t->activeWindowChanged(); break;
        case 2: _t->keyboardLayoutChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->optionsChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 4: _t->configReloaded(); break;
        case 5: _t->fullscreenChanged(); break;
        case 6: _t->screencastChanged(); break;
        case 7: _t->onEventReadyRead(); break;
        case 8: _t->reconnectEventSocket(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (HyprlandClient::*)()>(_a, &HyprlandClient::stateChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (HyprlandClient::*)()>(_a, &HyprlandClient::activeWindowChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (HyprlandClient::*)(const QString & )>(_a, &HyprlandClient::keyboardLayoutChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (HyprlandClient::*)(int , int , bool )>(_a, &HyprlandClient::optionsChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (HyprlandClient::*)()>(_a, &HyprlandClient::configReloaded, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (HyprlandClient::*)()>(_a, &HyprlandClient::fullscreenChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (HyprlandClient::*)()>(_a, &HyprlandClient::screencastChanged, 6))
            return;
    }
}

const QMetaObject *quattro::HyprlandClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *quattro::HyprlandClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro14HyprlandClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int quattro::HyprlandClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void quattro::HyprlandClient::stateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void quattro::HyprlandClient::activeWindowChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void quattro::HyprlandClient::keyboardLayoutChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void quattro::HyprlandClient::optionsChanged(int _t1, int _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3);
}

// SIGNAL 4
void quattro::HyprlandClient::configReloaded()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void quattro::HyprlandClient::fullscreenChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void quattro::HyprlandClient::screencastChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
