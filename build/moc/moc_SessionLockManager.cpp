/****************************************************************************
** Meta object code from reading C++ file 'SessionLockManager.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/wayland/SessionLockManager.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SessionLockManager.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7quattro18SessionLockManagerE_t {};
} // unnamed namespace

template <> constexpr inline auto quattro::SessionLockManager::qt_create_metaobjectdata<qt_meta_tag_ZN7quattro18SessionLockManagerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "quattro::SessionLockManager",
        "locked",
        "",
        "finished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'locked'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'finished'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SessionLockManager, qt_meta_tag_ZN7quattro18SessionLockManagerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject quattro::SessionLockManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QWaylandClientExtensionTemplate<SessionLockManager>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro18SessionLockManagerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro18SessionLockManagerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7quattro18SessionLockManagerE_t>.metaTypes,
    nullptr
} };

void quattro::SessionLockManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SessionLockManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->locked(); break;
        case 1: _t->finished(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SessionLockManager::*)()>(_a, &SessionLockManager::locked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SessionLockManager::*)()>(_a, &SessionLockManager::finished, 1))
            return;
    }
}

const QMetaObject *quattro::SessionLockManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *quattro::SessionLockManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro18SessionLockManagerE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QtWayland::ext_session_lock_manager_v1"))
        return static_cast< QtWayland::ext_session_lock_manager_v1*>(this);
    return QWaylandClientExtensionTemplate<SessionLockManager>::qt_metacast(_clname);
}

int quattro::SessionLockManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWaylandClientExtensionTemplate<SessionLockManager>::qt_metacall(_c, _id, _a);
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
void quattro::SessionLockManager::locked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void quattro::SessionLockManager::finished()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
namespace {
struct qt_meta_tag_ZN7quattro17SessionLockObjectE_t {};
} // unnamed namespace

template <> constexpr inline auto quattro::SessionLockObject::qt_create_metaobjectdata<qt_meta_tag_ZN7quattro17SessionLockObjectE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "quattro::SessionLockObject",
        "locked",
        "",
        "finished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'locked'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'finished'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SessionLockObject, qt_meta_tag_ZN7quattro17SessionLockObjectE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject quattro::SessionLockObject::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro17SessionLockObjectE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro17SessionLockObjectE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7quattro17SessionLockObjectE_t>.metaTypes,
    nullptr
} };

void quattro::SessionLockObject::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SessionLockObject *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->locked(); break;
        case 1: _t->finished(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SessionLockObject::*)()>(_a, &SessionLockObject::locked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SessionLockObject::*)()>(_a, &SessionLockObject::finished, 1))
            return;
    }
}

const QMetaObject *quattro::SessionLockObject::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *quattro::SessionLockObject::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro17SessionLockObjectE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QtWayland::ext_session_lock_v1"))
        return static_cast< QtWayland::ext_session_lock_v1*>(this);
    return QObject::qt_metacast(_clname);
}

int quattro::SessionLockObject::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void quattro::SessionLockObject::locked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void quattro::SessionLockObject::finished()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
namespace {
struct qt_meta_tag_ZN7quattro18SessionLockSurfaceE_t {};
} // unnamed namespace

template <> constexpr inline auto quattro::SessionLockSurface::qt_create_metaobjectdata<qt_meta_tag_ZN7quattro18SessionLockSurfaceE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "quattro::SessionLockSurface"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SessionLockSurface, qt_meta_tag_ZN7quattro18SessionLockSurfaceE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject quattro::SessionLockSurface::staticMetaObject = { {
    QMetaObject::SuperData::link<QtWaylandClient::QWaylandShellSurface::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro18SessionLockSurfaceE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro18SessionLockSurfaceE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7quattro18SessionLockSurfaceE_t>.metaTypes,
    nullptr
} };

void quattro::SessionLockSurface::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SessionLockSurface *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *quattro::SessionLockSurface::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *quattro::SessionLockSurface::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7quattro18SessionLockSurfaceE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QtWayland::ext_session_lock_surface_v1"))
        return static_cast< QtWayland::ext_session_lock_surface_v1*>(this);
    return QtWaylandClient::QWaylandShellSurface::qt_metacast(_clname);
}

int quattro::SessionLockSurface::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QtWaylandClient::QWaylandShellSurface::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
