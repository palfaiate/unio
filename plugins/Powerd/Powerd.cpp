/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Michael Terry <michael.terry@canonical.com>
 */

#include "Powerd.h"
#include <QDBusPendingCall>

void autoBrightnessChanged(GSettings *settings, const gchar *key, QDBusInterface *unityScreen)
{
    bool value = g_settings_get_boolean(settings, key);
    unityScreen->asyncCall("userAutobrightnessEnable", QVariant(value));
}

void activityTimeoutChanged(GSettings *settings, const gchar *key, QDBusInterface *unityScreen)
{
    int value = g_settings_get_uint(settings, key);
    unityScreen->asyncCall("setInactivityTimeouts", QVariant(value), QVariant(-1));
}

void dimTimeoutChanged(GSettings *settings, const gchar *key, QDBusInterface *unityScreen)
{
    int value = g_settings_get_uint(settings, key);
    unityScreen->asyncCall("setInactivityTimeouts", QVariant(-1), QVariant(value));
}

Powerd::Powerd(QObject* parent)
  : QObject(parent),
    unityScreen(NULL),
    cachedStatus(Status::On)
{
    unityScreen = new QDBusInterface("com.canonical.Unity.Screen",
                                     "/com/canonical/Unity/Screen",
                                     "com.canonical.Unity.Screen",
                                     QDBusConnection::SM_BUSNAME(), this);

    unityScreen->connection().connect("com.canonical.Unity.Screen",
                                      "/com/canonical/Unity/Screen",
                                      "com.canonical.Unity.Screen",
                                      "DisplayPowerStateChange",
                                      this,
                                      SLOT(handleDisplayPowerStateChange(int, int)));

    systemSettings = g_settings_new("com.ubuntu.touch.system");
    g_signal_connect(systemSettings, "changed::auto-brightness", G_CALLBACK(autoBrightnessChanged), unityScreen);
    g_signal_connect(systemSettings, "changed::activity-timeout", G_CALLBACK(activityTimeoutChanged), unityScreen);
    g_signal_connect(systemSettings, "changed::dim-timeout", G_CALLBACK(dimTimeoutChanged), unityScreen);
    autoBrightnessChanged(systemSettings, "auto-brightness", unityScreen);
    activityTimeoutChanged(systemSettings, "activity-timeout", unityScreen);
    dimTimeoutChanged(systemSettings, "dim-timeout", unityScreen);
}

Powerd::~Powerd()
{
    g_signal_handlers_disconnect_by_data(systemSettings, unityScreen);
    g_object_unref(systemSettings);
}

Powerd::Status Powerd::status() const
{
    return cachedStatus;
}

void Powerd::handleDisplayPowerStateChange(int status, int reason)
{
    if (cachedStatus != (Status)status) {
        cachedStatus = (Status)status;
        Q_EMIT statusChanged((DisplayStateChangeReason)reason);
    }
}
