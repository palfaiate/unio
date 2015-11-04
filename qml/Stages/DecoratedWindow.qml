/*
 * Copyright (C) 2014-2015 Canonical, Ltd.
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
 * Authors: Michael Zanetti <michael.zanetti@canonical.com>
 */

import QtQuick 2.4
import QtGraphicalEffects 1.0
import Ubuntu.Components 1.2
import Unity.Application 0.1

FocusScope {
    id: root

    property alias window: applicationWindow
    property alias application: applicationWindow.application
    property alias active: decoration.active
    property alias title: decoration.title

    property bool decorationShown: true
    property bool highlightShown: false

    signal close()
    signal maximize()
    signal minimize()
    signal decorationPressed()

    Rectangle {
        id: selectionHighlight
        anchors.fill: parent
        anchors.margins: -units.gu(1)
        color: "white"
        opacity: highlightShown ? 0.15 : 0
    }

    Rectangle {
        anchors { left: selectionHighlight.left; right: selectionHighlight.right; bottom: selectionHighlight.bottom; }
        height: units.dp(2)
        color: UbuntuColors.orange
        visible: highlightShown
    }

    RectangularGlow { // drop shadow around the window
        anchors.fill: parent
        color: "#333333"
        glowRadius: active ? units.dp(6) : units.dp(4)
    }

    WindowDecoration {
        id: decoration
        target: root.parent
        objectName: application ? "appWindowDecoration_" + application.appId : "appWindowDecoration_null"
        anchors { left: parent.left; top: parent.top; right: parent.right }
        height: units.gu(3)
        width: root.width
        title: window.title !== "" ? window.title : ""
        onClose: root.close();
        onMaximize: root.maximize();
        onMinimize: root.minimize();
        onPressed: root.decorationPressed();
        visible: decorationShown
    }

    ApplicationWindow {
        id: applicationWindow
        objectName: application ? "appWindow_" + application.appId : "appWindow_null"
        anchors.top: parent.top
        anchors.topMargin: decoration.height
        anchors.left: parent.left
        width: root.width
        height: root.height - decoration.height
        interactive: true
        focus: true
    }
}
