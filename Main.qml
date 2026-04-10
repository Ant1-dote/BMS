import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCharts

ApplicationWindow {
    id: root
    width: 1440
    height: 920
    visible: true
    title: "BMS CONSOLE"
    color: "#f4f6fb"
    font.family: "Segoe UI"

    property color cWindowTop: "#f9fafc"
    property color cWindowMid: "#f3f5f9"
    property color cWindowBottom: "#eef2f8"
    property color cPanel: "#fdfdff"
    property color cPanelBorder: "#d8deea"
    property color cCard: "#ffffff"
    property color cCardBorder: "#e2e8f2"
    property color cTitle: "#1f2a3a"
    property color cText: "#263446"
    property color cSubtle: "#6a788d"
    property color cChartBg: "#ffffff"
    property color cGrid: "#e6ebf3"
    property color cStatusBg: "#f4f7fd"
    property color cStatusBorder: "#d5deed"
    property color cAccent: "#2563eb"
    property color cBtnPrimary: "#2563eb"
    property color cBtnPrimaryHover: "#1d4ed8"
    property color cBtnPrimaryPressed: "#1e40af"
    property color cBtnSecondary: "#f6f8fc"
    property color cBtnSecondaryHover: "#edf2fb"
    property color cBtnSecondaryPressed: "#e3eaf8"
    property color cIndustrialHeader: "#f8fafe"
    property color cIndustrialHeaderSoft: "#f1f5fb"
    property color cIndustrialBorderStrong: "#d7dfec"
    property color cIndustrialSurface: "#ffffff"
    property color cIndustrialSurfaceLow: "#f6f9ff"
    property color cIndustrialTextDim: "#5f6e84"
    property int cSectionRadius: 10
    property int cButtonRadius: 8
    property int cControlH: 36
    property int cPrimaryH: 38
    property int cPanelGap: 14
    property int cFormRowGap: 10
    property int cFormColGap: 10
    property int cLabelW: 78
    property int cSectionOuterPad: 8
    property int cSectionInnerPad: 10
    property int cSectionContentPad: 10
    property int cCollapseAnimMs: 180
    property int cStartCaptureBtnW: 186
    property int cSerialActionBtnW: 96
    property int cStatusFieldW: 240
    property int cApplyConfigBtnW: 220
    property int cResetEkfBtnW: 200
    property int cCmdButtonW: 156
    property int cCmdSendButtonW: 116
    property bool metricsPanelCollapsed: false
    property int metricsPanelCollapsedHeight: 44
    property int metricsPanelAnimMs: 180
    property bool logPanelCollapsed: false
    property int logPanelExpandedHeight: 260
    property int logPanelCollapsedHeight: 48
    property int logPanelAnimMs: 180

    // 布局可调参数：手动改这里即可整体调整按钮和面板布局。
    property int layoutLeftPanelPreferredWidth: 480
    property int layoutLeftPanelMinimumWidth: 380
    property int layoutSerialColumns: 3
    property int layoutRuntimeColumns: 4
    property int layoutEkfColumns: 4
    property int layoutCommandColumns: 2
    property int layoutScanVisibleColumns: 4
    property int layoutMetricsBreakpoint: 900
    property int layoutMetricsColumnsWide: 4
    property int layoutMetricsColumnsNarrow: 2

    property var drateOptions: [
        { text: "30000 次/秒 (0xF0)", value: 0xF0 },
        { text: "15000 次/秒 (0xE0)", value: 0xE0 },
        { text: "7500 次/秒 (0xD0)", value: 0xD0 },
        { text: "3750 次/秒 (0xC0)", value: 0xC0 },
        { text: "2000 次/秒 (0xB0)", value: 0xB0 },
        { text: "1000 次/秒 (0xA1)", value: 0xA1 },
        { text: "500 次/秒 (0x92)", value: 0x92 },
        { text: "100 次/秒 (0x82)", value: 0x82 },
        { text: "60 次/秒 (0x72)", value: 0x72 },
        { text: "50 次/秒 (0x63)", value: 0x63 },
        { text: "30 次/秒 (0x53)", value: 0x53 },
        { text: "25 次/秒 (0x43)", value: 0x43 },
        { text: "15 次/秒 (0x33)", value: 0x33 },
        { text: "10 次/秒 (0x23)", value: 0x23 },
        { text: "5 次/秒 (0x13)", value: 0x13 },
        { text: "2.5 次/秒 (0x03)", value: 0x03 }
    ]

    property var channelNames: ["AIN0", "AIN1", "AIN2", "AIN3", "AIN4", "AIN5", "AIN6", "AIN7", "AINCOM"]
    property var pgaValues: [1, 2, 4, 8, 16, 32, 64]
    property var channelColors: ["#ff9e58", "#00c2a8", "#4ea1ff", "#f06b8f", "#f8bb39", "#9f7aea", "#5ec2ff", "#ff6b6b"]
    property var scanSampleFlags: [true, true, true, true, true, true, true, true]
    property var scanVisibleFlags: [true, true, true, true, true, true, true, true]
    property bool multiMode: controller.acqMode !== "SINGLE"
    property bool stopDisconnectAfterSave: false
    property int captureElapsedSeconds: 0
    property double captureStartEpochMs: 0
    function openStopDialog(disconnectAfterStop) {
        stopDisconnectAfterSave = disconnectAfterStop
        stopConfirmDialog.open()
    }

    function finishStopFlow(saveData) {
        controller.stopCapture(saveData)
        if (stopDisconnectAfterSave && controller.connected) {
            controller.disconnectSerial()
        }
        stopDisconnectAfterSave = false
    }

    function formatDuration(totalSeconds) {
        const seconds = Math.max(0, Math.floor(totalSeconds))
        const hh = Math.floor(seconds / 3600)
        const mm = Math.floor((seconds % 3600) / 60)
        const ss = seconds % 60
        return String(hh).padStart(2, "0")
            + ":" + String(mm).padStart(2, "0")
            + ":" + String(ss).padStart(2, "0")
    }

    function refreshCaptureElapsed() {
        if (!controller.acquisitionEnabled || captureStartEpochMs <= 0) {
            captureElapsedSeconds = 0
            return
        }
        captureElapsedSeconds = Math.floor((Date.now() - captureStartEpochMs) / 1000)
    }

    function levelColor(level) {
        if (level === "ok") {
            return "#0c8a69"
        }
        if (level === "info") {
            return "#5e7387"
        }
        if (level === "tx") {
            return "#b17600"
        }
        if (level === "error") {
            return "#c93838"
        }
        return "#304861"
    }

    component MetricCard: Rectangle {
        required property string title
        required property string value
        required property color accent

        radius: 10
        color: root.cCard
        border.color: root.cCardBorder
        border.width: 1
        implicitHeight: 92

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            Label {
                text: parent.parent.title
                color: root.cSubtle
                font.pixelSize: 12
            }

            Label {
                text: parent.parent.value
                color: root.cText
                font.pixelSize: 20
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: parent.parent.accent
            }
        }
    }

    component SectionCard: GroupBox {
        id: sectionCard
        property bool collapsed: false
        property int headerHeight: 34
        Layout.fillWidth: true
        Layout.preferredHeight: sectionCard.collapsed
                                ? sectionCard.headerHeight + 8
                                : implicitHeight + root.cSectionContentPad
        Layout.maximumHeight: sectionCard.collapsed ? sectionCard.headerHeight + 8 : 16777215
        Layout.leftMargin: root.cSectionOuterPad
        Layout.rightMargin: root.cSectionOuterPad
        padding: sectionCard.collapsed ? 6 : 14
        topPadding: sectionCard.collapsed ? sectionCard.headerHeight + 2 : 42
        font.pixelSize: 14
        font.bold: false
        clip: sectionCard.collapsed

        label: RowLayout {
            x: 8
            y: 8
            width: sectionCard.width - 16
            height: 20
            spacing: 8

            ToolButton {
                text: "⌄"
                implicitWidth: 18
                implicitHeight: 18
                padding: 0
                onClicked: sectionCard.collapsed = !sectionCard.collapsed

                contentItem: Label {
                    text: parent.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: root.cSubtle
                    font.pixelSize: 12
                    font.bold: false
                    rotation: sectionCard.collapsed ? -90 : 0

                    Behavior on rotation {
                        NumberAnimation {
                            duration: root.cCollapseAnimMs
                            easing.type: Easing.InOutCubic
                        }
                    }
                }

                background: Rectangle {
                    radius: 5
                    color: parent.down ? "#dde6f6" : (parent.hovered ? "#eaf0fb" : "#f5f8ff")
                    border.color: "#d1daea"
                    border.width: 1
                }
            }

            Label {
                text: sectionCard.title
                color: root.cTitle
                font.pixelSize: 13
                font.bold: true
                font.family: "Segoe UI"
                font.letterSpacing: 0.2
            }

            Item { Layout.fillWidth: true }
        }

        background: Rectangle {
            radius: root.cSectionRadius
            color: root.cCard
            border.color: sectionCard.collapsed ? "transparent" : root.cIndustrialBorderStrong
            border.width: sectionCard.collapsed ? 0 : 1

            Rectangle {
                visible: !sectionCard.collapsed
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 1
                anchors.topMargin: sectionCard.headerHeight
                color: root.cIndustrialSurface

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#ffffff" }
                    GradientStop { position: 1.0; color: root.cIndustrialSurface }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
                height: sectionCard.headerHeight - 1
                radius: root.cSectionRadius - 1
                gradient: Gradient {
                    GradientStop { position: 0.0; color: root.cIndustrialHeaderSoft }
                    GradientStop { position: 1.0; color: root.cIndustrialHeader }
                }
                border.color: "#dce4f0"
                border.width: 1
            }

            Rectangle {
                visible: !sectionCard.collapsed
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: sectionCard.headerHeight - 2
                height: 1
                color: "#dfe6f2"
                opacity: 0.7
            }
        }
    }

    component FormLabel: Label {
        Layout.preferredWidth: root.cLabelW
        Layout.preferredHeight: root.cControlH
        Layout.alignment: Qt.AlignVCenter
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        color: root.cIndustrialTextDim
        font.pixelSize: 13
        font.bold: false
    }

    component StatusChip: Rectangle {
        required property string label
        required property string value
        required property color valueColor

        radius: 8
        color: root.cIndustrialSurfaceLow
        border.color: root.cIndustrialBorderStrong
        border.width: 1
        implicitHeight: 54

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 2

            Label {
                text: parent.parent.label
                color: root.cIndustrialTextDim
                font.pixelSize: 11
                font.bold: false
            }

            Label {
                text: parent.parent.value
                color: parent.parent.valueColor
                font.pixelSize: 13
                font.bold: true
            }
        }
    }

    component PrimaryActionButton: Button {
        implicitHeight: root.cPrimaryH
        font.pixelSize: 13
        font.bold: false
        Layout.alignment: Qt.AlignVCenter
        property color toneNormal: root.cBtnPrimary
        property color toneHover: root.cBtnPrimaryHover
        property color tonePressed: root.cBtnPrimaryPressed
        property color toneBorder: "#1f4fbe"

        contentItem: Label {
            text: parent.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: "#ffffff"
            font.pixelSize: parent.font.pixelSize
            font.bold: parent.font.bold
        }

        background: Rectangle {
            radius: root.cButtonRadius
            color: parent.down
                 ? parent.tonePressed
                 : (parent.hovered ? parent.toneHover : parent.toneNormal)
             border.color: parent.toneBorder
            border.width: 1

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 1
                height: 1
                color: "#ffffff"
                opacity: 0.18
            }
        }
    }

    component SecondaryActionButton: Button {
        implicitHeight: root.cControlH
        font.pixelSize: 12
        Layout.alignment: Qt.AlignVCenter

        contentItem: Label {
            text: parent.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: root.cText
            font.pixelSize: parent.font.pixelSize
            font.bold: parent.font.bold
        }

        background: Rectangle {
            radius: root.cButtonRadius
            color: parent.down
                   ? root.cBtnSecondaryPressed
                   : (parent.hovered ? root.cBtnSecondaryHover : root.cBtnSecondary)
            border.color: "#d3dceb"
            border.width: 1
        }
    }

    component FluentToolButton: ToolButton {
        implicitHeight: 30
        implicitWidth: 66
        font.pixelSize: 12

        contentItem: Label {
            text: parent.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: root.cText
            font.pixelSize: parent.font.pixelSize
        }

        background: Rectangle {
            radius: 8
            color: parent.down
                   ? root.cBtnSecondaryPressed
                   : (parent.hovered ? root.cBtnSecondaryHover : root.cBtnSecondary)
            border.color: "#d3dceb"
            border.width: 1
        }
    }

    component FluentTextField: TextField {
        implicitHeight: root.cControlH
        font.pixelSize: 12
        color: root.cText
        selectByMouse: true
        padding: 9

        background: Rectangle {
            radius: root.cButtonRadius
            color: parent.enabled ? "#ffffff" : "#f2f4f8"
            border.color: parent.activeFocus ? root.cAccent : "#d3dceb"
            border.width: parent.activeFocus ? 2 : 1
        }
    }

    component FluentComboBox: ComboBox {
        id: fluentCombo
        implicitHeight: root.cControlH
        font.pixelSize: 12
        leftPadding: 10
        rightPadding: 30

        contentItem: Text {
            text: fluentCombo.displayText
            color: fluentCombo.enabled ? root.cText : "#95a1b4"
            font.pixelSize: fluentCombo.font.pixelSize
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        indicator: Canvas {
            x: fluentCombo.width - width - 10
            y: (fluentCombo.height - height) / 2
            width: 10
            height: 6
            contextType: "2d"

            onPaint: {
                context.reset()
                context.moveTo(0, 0)
                context.lineTo(width / 2, height)
                context.lineTo(width, 0)
                context.lineWidth = 1.8
                context.strokeStyle = root.cSubtle
                context.stroke()
            }
        }

        background: Rectangle {
            radius: root.cButtonRadius
            color: fluentCombo.down
                   ? root.cBtnSecondaryPressed
                   : (fluentCombo.hovered ? root.cBtnSecondaryHover : "#ffffff")
            border.color: fluentCombo.visualFocus ? root.cAccent : "#d3dceb"
            border.width: fluentCombo.visualFocus ? 2 : 1
        }
    }

    component FluentSpinBox: SpinBox {
        id: fluentSpin
        implicitHeight: root.cControlH
        font.pixelSize: 12

        up.indicator: Rectangle {
            x: fluentSpin.width - width - 1
            y: 1
            width: 24
            height: (fluentSpin.height / 2) - 1
            radius: 4
            color: fluentSpin.up.pressed ? root.cBtnSecondaryPressed : (fluentSpin.up.hovered ? root.cBtnSecondaryHover : "#f8faff")
            border.color: "#d3dceb"

            Text {
                anchors.centerIn: parent
                text: "+"
                color: root.cSubtle
                font.pixelSize: 11
            }
        }

        down.indicator: Rectangle {
            x: fluentSpin.width - width - 1
            y: fluentSpin.height - height - 1
            width: 24
            height: (fluentSpin.height / 2) - 1
            radius: 4
            color: fluentSpin.down.pressed ? root.cBtnSecondaryPressed : (fluentSpin.down.hovered ? root.cBtnSecondaryHover : "#f8faff")
            border.color: "#d3dceb"

            Text {
                anchors.centerIn: parent
                text: "-"
                color: root.cSubtle
                font.pixelSize: 11
            }
        }

        background: Rectangle {
            radius: root.cButtonRadius
            color: fluentSpin.enabled ? "#ffffff" : "#f2f4f8"
            border.color: fluentSpin.visualFocus ? root.cAccent : "#d3dceb"
            border.width: fluentSpin.visualFocus ? 2 : 1
        }
    }

    component FluentCheckBox: CheckBox {
        id: fluentCheck
        spacing: 8
        font.pixelSize: 12

        indicator: Rectangle {
            implicitWidth: 18
            implicitHeight: 18
            radius: 4
            color: fluentCheck.checked ? root.cAccent : "#ffffff"
            border.color: fluentCheck.checked ? root.cAccent : "#cfd8e9"
            border.width: 1

            Label {
                anchors.centerIn: parent
                text: "✓"
                visible: fluentCheck.checked
                color: "#ffffff"
                font.pixelSize: 12
                font.bold: true
            }
        }

        contentItem: Text {
            text: fluentCheck.text
            leftPadding: fluentCheck.indicator.width + fluentCheck.spacing
            verticalAlignment: Text.AlignVCenter
            color: root.cText
            font.pixelSize: fluentCheck.font.pixelSize
        }
    }

    Timer {
        id: captureElapsedTimer
        interval: 1000
        repeat: true
        running: false
        onTriggered: root.refreshCaptureElapsed()
    }

    Connections {
        target: controller

        function onAcquisitionEnabledChanged() {
            if (controller.acquisitionEnabled) {
                root.captureStartEpochMs = Date.now()
                root.captureElapsedSeconds = 0
                captureElapsedTimer.start()
                return
            }

            captureElapsedTimer.stop()
            root.captureStartEpochMs = 0
            root.captureElapsedSeconds = 0
        }
    }

    Component.onCompleted: {
        if (controller.acquisitionEnabled) {
            captureStartEpochMs = Date.now()
            captureElapsedSeconds = 0
            captureElapsedTimer.start()
        }
    }

    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        height: 70
        radius: 14
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#ffffff" }
            GradientStop { position: 1.0; color: "#f5f8fe" }
        }
        border.color: root.cCardBorder
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            Label {
                text: "控制台"
                color: root.cTitle
                font.pixelSize: 22
                font.bold: true
            }

            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: "#e3e8f1"
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 34
                radius: 8
                color: "#eef3fe"
                border.color: "#d6e0f5"

                Label {
                    anchors.centerIn: parent
                    text: controller.statusText
                    color: root.cText
                    font.pixelSize: 13
                    elide: Label.ElideRight
                }
            }

            Rectangle {
                Layout.preferredWidth: 156
                implicitHeight: 34
                radius: 8
                color: "#eef3fe"
                border.color: "#d6e0f5"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Label {
                        text: "运行时间"
                        color: root.cSubtle
                        font.pixelSize: 12
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: root.formatDuration(root.captureElapsedSeconds)
                        color: root.cTitle
                        font.pixelSize: 13
                        font.family: "Consolas"
                        font.bold: true
                    }
                }
            }
        }
    }

    SplitView {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: topBar.bottom
        anchors.margins: 16
        orientation: Qt.Horizontal

        ScrollView {
            id: leftPanelScroll
            SplitView.preferredWidth: root.layoutLeftPanelPreferredWidth
            SplitView.minimumWidth: root.layoutLeftPanelMinimumWidth
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            leftPadding: root.cSectionOuterPad + 6
            rightPadding: root.cSectionOuterPad + 6
            topPadding: 10
            bottomPadding: 14

            ColumnLayout {
                width: leftPanelScroll.availableWidth
                spacing: root.cPanelGap

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: root.cSectionOuterPad
                    Layout.rightMargin: root.cSectionOuterPad
                    implicitHeight: 96
                    radius: root.cSectionRadius
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#ffffff" }
                        GradientStop { position: 1.0; color: "#f5f8fe" }
                    }
                    border.color: root.cIndustrialBorderStrong
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true

                            Label {
                                text: "工控操作面板"
                                color: root.cTitle
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                radius: 4
                                color: root.multiMode ? "#e6f8f1" : "#edf2fb"
                                border.color: root.multiMode ? "#b5e3d1" : "#cad9ee"
                                border.width: 1
                                implicitWidth: 78
                                implicitHeight: 22

                                Label {
                                    anchors.centerIn: parent
                                    text: root.multiMode ? "MULTI" : "SINGLE"
                                    color: root.multiMode ? "#0e8e66" : "#48607a"
                                    font.pixelSize: 11
                                    font.bold: false
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            StatusChip {
                                Layout.fillWidth: true
                                label: "通讯"
                                value: controller.connected ? "在线" : "离线"
                                valueColor: controller.connected ? "#0f8a67" : "#a95a4a"
                            }

                            StatusChip {
                                Layout.fillWidth: true
                                label: "采样"
                                value: controller.acquisitionEnabled ? "运行中" : "已停止"
                                valueColor: controller.acquisitionEnabled ? "#0f8a67" : "#6c7f93"
                            }

                            StatusChip {
                                Layout.fillWidth: true
                                label: "窗口"
                                value: controller.xAxisMode
                                valueColor: "#3a5d80"
                            }
                        }
                    }
                }

                SectionCard {
                    id: serialSection
                    title: "01 串口连接"

                    GridLayout {
                        x: root.cSectionContentPad
                        y: root.cSectionContentPad
                        width: parent.width - root.cSectionContentPad * 2
                        visible: height > 0.5 || opacity > 0.01
                        enabled: !serialSection.collapsed
                        opacity: serialSection.collapsed ? 0.0 : 1.0
                        height: serialSection.collapsed ? 0 : implicitHeight
                        clip: true
                        columns: root.layoutSerialColumns
                        rowSpacing: root.cFormRowGap
                        columnSpacing: root.cFormColGap

                        Behavior on height {
                            NumberAnimation {
                                duration: root.cCollapseAnimMs
                                easing.type: Easing.InOutCubic
                            }
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Math.round(root.cCollapseAnimMs * 0.7)
                                easing.type: Easing.InOutQuad
                            }
                        }

                        FormLabel { text: "端口" }
                        FluentComboBox {
                            id: portCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            model: controller.availablePorts
                        }
                        SecondaryActionButton {
                            text: "刷新端口"
                            Layout.preferredHeight: root.cControlH
                            Layout.preferredWidth: root.cSerialActionBtnW
                            onClicked: controller.refreshPorts()
                        }

                        FormLabel { text: "波特率" }
                        FluentComboBox {
                            id: baudCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            model: [9600, 57600, 115200, 230400, 460800, 921600]
                            currentIndex: 2
                        }
                        SecondaryActionButton {
                            text: controller.connected ? "断开" : "连接"
                            Layout.preferredHeight: root.cControlH
                            Layout.preferredWidth: root.cSerialActionBtnW
                            onClicked: {
                                if (controller.connected) {
                                    if (controller.acquisitionEnabled) {
                                        root.openStopDialog(true)
                                    } else {
                                        controller.disconnectSerial()
                                    }
                                } else {
                                    let dev = controller.portDeviceAt(portCombo.currentIndex)
                                    controller.toggleConnection(dev, Number(baudCombo.currentText))
                                }
                            }
                        }

                        FormLabel { text: "采样" }
                        RowLayout {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            spacing: 8

                            PrimaryActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.cPrimaryH
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                toneNormal: controller.acquisitionEnabled ? "#da6d56" : "#0f9f7a"
                                toneHover: controller.acquisitionEnabled ? "#c95d48" : "#0d916f"
                                tonePressed: controller.acquisitionEnabled ? "#b84f3c" : "#0b7f61"
                                toneBorder: controller.acquisitionEnabled ? "#a64636" : "#0a7056"
                                text: controller.acquisitionEnabled ? "停止连续采样" : "开始连续采样"
                                onClicked: {
                                    if (controller.acquisitionEnabled) {
                                        root.openStopDialog(false)
                                    } else {
                                        controller.openCaptureDirectoryDialog()
                                    }
                                }
                            }

                            SecondaryActionButton {
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.cPrimaryH
                                text: controller.cycleLoopEnabled ? "停止循环充放电" : "循环充放电"
                                enabled: controller.connected
                                onClicked: {
                                    if (controller.cycleLoopEnabled) {
                                        controller.stopCycleLoop()
                                    } else {
                                        controller.startCycleLoop()
                                    }
                                }
                            }
                        }

                        FormLabel { text: "状态" }
                        Rectangle {
                            Layout.columnSpan: 2
                            Layout.preferredWidth: root.cStatusFieldW
                            Layout.alignment: Qt.AlignHCenter
                            Layout.bottomMargin: root.cSectionContentPad
                            implicitHeight: root.cControlH
                            color: root.cStatusBg
                            radius: 8
                            border.color: root.cStatusBorder

                            Label {
                                anchors.centerIn: parent
                                anchors.verticalCenterOffset: -2
                                text: controller.statusText
                                color: root.cText
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 4 }

                SectionCard {
                    id: runtimeSection
                    title: "02 运行配置"

                    GridLayout {
                        x: root.cSectionContentPad
                        y: root.cSectionContentPad
                        width: parent.width - root.cSectionContentPad * 2
                        visible: height > 0.5 || opacity > 0.01
                        enabled: !runtimeSection.collapsed
                        opacity: runtimeSection.collapsed ? 0.0 : 1.0
                        height: runtimeSection.collapsed ? 0 : implicitHeight
                        clip: true
                        columns: root.layoutRuntimeColumns
                        rowSpacing: root.cFormRowGap
                        columnSpacing: root.cFormColGap

                        Behavior on height {
                            NumberAnimation {
                                duration: root.cCollapseAnimMs
                                easing.type: Easing.InOutCubic
                            }
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Math.round(root.cCollapseAnimMs * 0.7)
                                easing.type: Easing.InOutQuad
                            }
                        }

                        FormLabel { text: "参考电压" }
                        FluentSpinBox {
                            id: vrefSpin
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            from: 100
                            to: 5000
                            stepSize: 50
                            value: Math.round(controller.vref * 1000)
                            editable: true
                            textFromValue: function(v) { return (v / 1000.0).toFixed(3) }
                            valueFromText: function(txt) { return Math.round(parseFloat(txt) * 1000) }
                            onValueModified: controller.vref = value / 1000.0
                        }

                        FormLabel { text: "PGA" }
                        FluentComboBox {
                            id: pgaCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            model: pgaValues
                            onActivated: controller.pga = Number(currentText)
                            Component.onCompleted: {
                                const idx = pgaValues.indexOf(controller.pga)
                                currentIndex = idx >= 0 ? idx : 0
                            }
                        }

                        FormLabel { text: "AINP" }
                        FluentComboBox {
                            id: pselCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            enabled: !root.multiMode
                            model: channelNames
                            onActivated: controller.psel = currentText
                            Component.onCompleted: currentIndex = channelNames.indexOf(controller.psel)
                        }

                        FormLabel { text: "AINN" }
                        FluentComboBox {
                            id: nselCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            enabled: !root.multiMode
                            model: channelNames
                            onActivated: controller.nsel = currentText
                            Component.onCompleted: currentIndex = channelNames.indexOf(controller.nsel)
                        }

                        FormLabel { text: "采样率" }
                        FluentComboBox {
                            id: drateCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.columnSpan: 3
                            Layout.alignment: Qt.AlignVCenter
                            model: drateOptions
                            textRole: "text"
                            onActivated: controller.drate = drateOptions[currentIndex].value
                            Component.onCompleted: {
                                for (let i = 0; i < drateOptions.length; ++i) {
                                    if (drateOptions[i].value === controller.drate) {
                                        currentIndex = i
                                        break
                                    }
                                }
                            }
                        }

                        FormLabel { text: "采样模式" }
                        FluentComboBox {
                            id: acqModeCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            model: ["SINGLE", "MULTI"]
                            onActivated: controller.acqMode = currentText
                            Component.onCompleted: currentIndex = root.multiMode ? 1 : 0

                            Connections {
                                target: controller
                                function onConfigChanged() {
                                    acqModeCombo.currentIndex = root.multiMode ? 1 : 0
                                }
                            }
                        }

                        FormLabel { text: "显示通道" }
                        FluentComboBox {
                            id: viewChannelCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            model: ["AIN0", "AIN1", "AIN2", "AIN3", "AIN4", "AIN5", "AIN6", "AIN7"]
                            enabled: root.multiMode
                            onActivated: controller.viewChannel = currentIndex
                            Component.onCompleted: currentIndex = controller.viewChannel

                            Connections {
                                target: controller
                                function onConfigChanged() {
                                    viewChannelCombo.currentIndex = controller.viewChannel
                                }
                            }
                        }

                        FormLabel { text: "横轴模式" }
                        FluentComboBox {
                            id: xAxisModeCombo
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            model: ["FULL", "WINDOW"]
                            onActivated: controller.xAxisMode = currentText
                            Component.onCompleted: currentIndex = model.indexOf(controller.xAxisMode)
                        }

                        FormLabel { text: "滑窗秒数" }
                        FluentSpinBox {
                            id: windowSpin
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            enabled: controller.xAxisMode === "WINDOW"
                            from: 2
                            to: 21600
                            value: Math.round(controller.windowSeconds)
                            onValueModified: controller.windowSeconds = value
                        }

                        FormLabel { text: "放电终点" }
                        FluentSpinBox {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            from: 100
                            to: 10000
                            stepSize: 10
                            value: Math.round(controller.cycleDischargeEndVoltage * 1000)
                            editable: true
                            validator: IntValidator { bottom: 100; top: 10000 }
                            textFromValue: function(v) { return (v / 1000).toFixed(3) }
                            valueFromText: function(text) {
                                const parsed = Number(text)
                                if (isNaN(parsed)) {
                                    return value
                                }
                                return Math.max(from, Math.min(to, Math.round(parsed * 1000)))
                            }
                            onValueModified: controller.cycleDischargeEndVoltage = value / 1000
                        }

                        FormLabel { text: "充电终点" }
                        FluentSpinBox {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            from: 100
                            to: 10000
                            stepSize: 10
                            value: Math.round(controller.cycleChargeEndVoltage * 1000)
                            editable: true
                            validator: IntValidator { bottom: 100; top: 10000 }
                            textFromValue: function(v) { return (v / 1000).toFixed(3) }
                            valueFromText: function(text) {
                                const parsed = Number(text)
                                if (isNaN(parsed)) {
                                    return value
                                }
                                return Math.max(from, Math.min(to, Math.round(parsed * 1000)))
                            }
                            onValueModified: controller.cycleChargeEndVoltage = value / 1000
                        }

                        FormLabel { text: "判定点数" }
                        FluentSpinBox {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            from: 1
                            to: 200
                            value: controller.cycleConfirmSamples
                            onValueModified: controller.cycleConfirmSamples = value
                        }

                        FormLabel { text: "循环次数" }
                        FluentSpinBox {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            from: 0
                            to: 9999
                            value: controller.cycleMaxCount
                            onValueModified: controller.cycleMaxCount = value
                        }

                        FormLabel { text: "循环状态" }
                        Rectangle {
                            Layout.columnSpan: 3
                            Layout.fillWidth: true
                            implicitHeight: root.cControlH
                            color: root.cStatusBg
                            radius: 8
                            border.color: root.cStatusBorder

                            Label {
                                anchors.centerIn: parent
                                anchors.verticalCenterOffset: -1
                                text: controller.cycleOverlayVisible
                                      ? (controller.cyclePhaseText + " | 完成循环: " + controller.cycleCompletedCount)
                                      : "空闲"
                                color: root.cText
                                font.pixelSize: 12
                            }
                        }

                        Item { Layout.fillWidth: true }
                        PrimaryActionButton {
                            Layout.columnSpan: 3
                            Layout.preferredHeight: root.cPrimaryH
                            Layout.preferredWidth: root.cApplyConfigBtnW
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                            Layout.bottomMargin: root.cSectionContentPad
                            toneNormal: "#2f6df5"
                            toneHover: "#285fdd"
                            tonePressed: "#224fc0"
                            toneBorder: "#1f48ae"
                            text: "下发配置到设备"
                            onClicked: controller.sendApplyConfig()
                        }
                    }
                }

                SectionCard {
                    id: ekfSection
                    title: "03 滤波 (EKF)"

                    GridLayout {
                        x: root.cSectionContentPad
                        y: root.cSectionContentPad
                        width: parent.width - root.cSectionContentPad * 2
                        visible: height > 0.5 || opacity > 0.01
                        enabled: !ekfSection.collapsed
                        opacity: ekfSection.collapsed ? 0.0 : 1.0
                        height: ekfSection.collapsed ? 0 : implicitHeight
                        clip: true
                        columns: root.layoutEkfColumns
                        rowSpacing: root.cFormRowGap
                        columnSpacing: root.cFormColGap

                        Behavior on height {
                            NumberAnimation {
                                duration: root.cCollapseAnimMs
                                easing.type: Easing.InOutCubic
                            }
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Math.round(root.cCollapseAnimMs * 0.7)
                                easing.type: Easing.InOutQuad
                            }
                        }

                        FluentCheckBox {
                            Layout.columnSpan: 4
                            text: "启用 EKF"
                            checked: controller.ekfEnabled
                            onToggled: controller.ekfEnabled = checked
                        }

                        FormLabel { text: "Q" }
                        FluentTextField {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            text: Number(controller.ekfQ).toExponential(3)
                            onEditingFinished: controller.ekfQ = Number(text)
                        }

                        FormLabel { text: "R" }
                        FluentTextField {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            text: Number(controller.ekfR).toExponential(3)
                            onEditingFinished: controller.ekfR = Number(text)
                        }

                        FormLabel { text: "P0" }
                        FluentTextField {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.cControlH
                            Layout.alignment: Qt.AlignVCenter
                            text: controller.ekfP0.toString()
                            onEditingFinished: controller.ekfP0 = Number(text)
                        }

                        Item { Layout.fillWidth: true }
                        SecondaryActionButton {
                            Layout.columnSpan: 2
                            Layout.preferredHeight: root.cControlH
                            Layout.preferredWidth: root.cResetEkfBtnW
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                            Layout.bottomMargin: root.cSectionContentPad
                            text: "重置滤波状态"
                            onClicked: controller.resetEkf()
                        }
                    }
                }

                SectionCard {
                    id: commandSection
                    title: "04 高级命令"

                    ColumnLayout {
                        x: root.cSectionContentPad
                        y: root.cSectionContentPad
                        width: parent.width - root.cSectionContentPad * 2
                        visible: height > 0.5 || opacity > 0.01
                        enabled: !commandSection.collapsed
                        opacity: commandSection.collapsed ? 0.0 : 1.0
                        height: commandSection.collapsed ? 0 : implicitHeight
                        clip: true
                        spacing: root.cFormRowGap

                        Behavior on height {
                            NumberAnimation {
                                duration: root.cCollapseAnimMs
                                easing.type: Easing.InOutCubic
                            }
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Math.round(root.cCollapseAnimMs * 0.7)
                                easing.type: Easing.InOutQuad
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                            columns: root.layoutCommandColumns
                            rowSpacing: root.cFormRowGap
                            columnSpacing: root.cFormColGap

                            SecondaryActionButton {
                                Layout.preferredWidth: root.cCmdButtonW
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                text: "复位 RESET"
                                onClicked: controller.sendCommand("RESET")
                            }
                            SecondaryActionButton {
                                Layout.preferredWidth: root.cCmdButtonW
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                text: "自校准 SELFCAL"
                                onClicked: controller.sendCommand("SELFCAL")
                            }
                            SecondaryActionButton {
                                Layout.preferredWidth: root.cCmdButtonW
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                text: "同步 SYNC"
                                onClicked: controller.sendCommand("SYNC")
                            }
                            SecondaryActionButton {
                                Layout.preferredWidth: root.cCmdButtonW
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                text: "唤醒 WAKEUP"
                                onClicked: controller.sendCommand("WAKEUP")
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: 8
                            color: root.cIndustrialSurfaceLow
                            border.color: root.cIndustrialBorderStrong
                            border.width: 1
                            implicitHeight: relayGrid.implicitHeight + 20

                            GridLayout {
                                id: relayGrid
                                anchors.fill: parent
                                anchors.margins: 10
                                columns: 2
                                rowSpacing: 10
                                columnSpacing: 12

                                Label {
                                    Layout.columnSpan: 2
                                    text: "继电器控制"
                                    color: root.cTitle
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                Label {
                                    text: "1路 充电"
                                    color: root.cText
                                    font.pixelSize: 13
                                }
                                Switch {
                                    id: relayChargeSwitch
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    enabled: controller.connected && !controller.cycleLoopEnabled
                                    text: checked ? "开启" : "关闭"
                                    onClicked: {
                                        controller.sendCommand("RELAY 1 " + (checked ? "ON" : "OFF"))
                                    }
                                }

                                Label {
                                    text: "2路 放电"
                                    color: root.cText
                                    font.pixelSize: 13
                                }
                                Switch {
                                    id: relayDischargeSwitch
                                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                                    enabled: controller.connected && !controller.cycleLoopEnabled
                                    text: checked ? "开启" : "关闭"
                                    onClicked: {
                                        controller.sendCommand("RELAY 2 " + (checked ? "ON" : "OFF"))
                                    }
                                }

                                SecondaryActionButton {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    text: "全部断开"
                                    enabled: controller.connected && !controller.cycleLoopEnabled
                                    onClicked: {
                                        relayChargeSwitch.checked = false
                                        relayDischargeSwitch.checked = false
                                        controller.sendCommand("RELAY ALL OFF")
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.bottomMargin: root.cSectionContentPad
                            FluentTextField {
                                id: customCmdEdit
                                Layout.fillWidth: true
                                Layout.preferredHeight: root.cControlH
                                placeholderText: "手动命令，例如: CFG PSEL=AIN0 NSEL=AINCOM PGA=8 CHMASK=0x0F"
                            }
                            SecondaryActionButton {
                                text: "发送"
                                Layout.preferredHeight: root.cControlH
                                Layout.preferredWidth: root.cCmdSendButtonW
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                onClicked: {
                                    controller.sendCustomCommand(customCmdEdit.text)
                                    customCmdEdit.clear()
                                }
                            }
                        }
                    }
                }

                SectionCard {
                    id: scanSection
                    title: "05 多通道采样与显示"
                    enabled: root.multiMode

                    GridLayout {
                        x: root.cSectionContentPad
                        y: root.cSectionContentPad
                        width: parent.width - root.cSectionContentPad * 2
                        visible: height > 0.5 || opacity > 0.01
                        enabled: !scanSection.collapsed
                        opacity: scanSection.collapsed ? 0.0 : 1.0
                        height: scanSection.collapsed ? 0 : implicitHeight
                        clip: true
                        columns: root.layoutScanVisibleColumns
                        rowSpacing: 8
                        columnSpacing: 10

                        Behavior on height {
                            NumberAnimation {
                                duration: root.cCollapseAnimMs
                                easing.type: Easing.InOutCubic
                            }
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Math.round(root.cCollapseAnimMs * 0.7)
                                easing.type: Easing.InOutQuad
                            }
                        }

                        Label {
                            Layout.columnSpan: root.layoutScanVisibleColumns
                            text: "采样通道（多通道模式下生效，影响 CHMASK）"
                            color: root.cSubtle
                            font.pixelSize: 12
                        }

                        Repeater {
                            model: 8
                            delegate: FluentCheckBox {
                                required property int index
                                Layout.preferredHeight: root.cControlH
                                Layout.alignment: Qt.AlignVCenter
                                text: "AIN" + index
                                checked: scanSampleFlags[index]
                                onToggled: {
                                    const accepted = controller.setScanChannelSampling(index, checked)
                                    if (!accepted) {
                                        checked = !checked
                                        scanSampleFlags[index] = checked
                                        return
                                    }
                                    scanSampleFlags[index] = checked
                                }
                            }
                        }

                        Label {
                            Layout.columnSpan: root.layoutScanVisibleColumns
                            text: "显示通道（仅影响曲线显示）"
                            color: root.cSubtle
                            font.pixelSize: 12
                        }

                        Repeater {
                            model: 8
                            delegate: FluentCheckBox {
                                required property int index
                                Layout.preferredHeight: root.cControlH
                                Layout.alignment: Qt.AlignVCenter
                                text: "AIN" + index
                                checked: scanVisibleFlags[index]
                                onToggled: {
                                    scanVisibleFlags[index] = checked
                                    controller.setScanChannelVisible(index, checked)
                                }
                            }
                        }

                        Item {
                            Layout.columnSpan: root.layoutScanVisibleColumns
                            Layout.preferredHeight: root.cSectionContentPad
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        Rectangle {
            SplitView.fillWidth: true
            color: root.cPanel
            radius: 12
            border.color: root.cPanelBorder
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Rectangle {
                    id: metricsPanel
                    property real expandedHeight: metricsHeader.implicitHeight + metricsGrid.implicitHeight + 18
                    property real panelHeight: root.metricsPanelCollapsed
                                               ? root.metricsPanelCollapsedHeight
                                               : Math.max(root.metricsPanelCollapsedHeight, expandedHeight)

                    Layout.fillWidth: true
                    Layout.preferredHeight: panelHeight
                    color: root.cCard
                    radius: 10
                    border.color: root.cPanelBorder
                    border.width: 1
                    clip: true

                    Behavior on panelHeight {
                        NumberAnimation {
                            duration: root.metricsPanelAnimMs
                            easing.type: Easing.InOutCubic
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        RowLayout {
                            id: metricsHeader
                            Layout.fillWidth: true

                            Label {
                                text: "实时指标"
                                color: root.cTitle
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            FluentToolButton {
                                text: root.metricsPanelCollapsed ? "展开" : "收起"
                                onClicked: root.metricsPanelCollapsed = !root.metricsPanelCollapsed
                            }
                        }

                        GridLayout {
                            id: metricsGrid
                            visible: !root.metricsPanelCollapsed || opacity > 0.01
                            enabled: !root.metricsPanelCollapsed
                            opacity: root.metricsPanelCollapsed ? 0.0 : 1.0
                            Layout.fillWidth: true
                            columns: width > root.layoutMetricsBreakpoint
                                     ? root.layoutMetricsColumnsWide
                                     : root.layoutMetricsColumnsNarrow
                            rowSpacing: 8
                            columnSpacing: 8

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: Math.round(root.metricsPanelAnimMs * 0.65)
                                    easing.type: Easing.InOutQuad
                                }
                            }

                            MetricCard {
                                Layout.fillWidth: true
                                title: "最新 ADC"
                                value: controller.latestAdText
                                accent: "#ff8a3d"
                            }
                            MetricCard {
                                Layout.fillWidth: true
                                title: "电压"
                                value: controller.latestVoltageText
                                accent: "#00c2a8"
                            }
                            MetricCard {
                                Layout.fillWidth: true
                                title: "采样数"
                                value: controller.sampleCount.toString()
                                accent: "#4ea1ff"
                            }
                            MetricCard {
                                Layout.fillWidth: true
                                title: "十六进制"
                                value: controller.latestHexText
                                accent: "#f06b8f"
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 420
                    color: root.cChartBg
                    radius: 10
                    border.color: root.cPanelBorder
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        Label {
                            text: root.multiMode ? "实时曲线（多通道）" : "实时曲线"
                            color: root.cTitle
                            font.pixelSize: 14
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: controller.zoomActive
                                      ? "框选放大已启用"
                                      : "左键拖拽可框选放大"
                                color: root.cSubtle
                                elide: Label.ElideRight
                            }

                            SecondaryActionButton {
                                visible: controller.zoomActive
                                text: "退出放大"
                                onClicked: controller.resetZoom()
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            property bool selecting: false
                            property real startX: 0
                            property real startY: 0
                            function zoomMapSeries() {
                                if (!root.multiMode) {
                                    const cycleSeries = [cycle0, cycle1, cycle2, cycle3, cycle4, cycle5, cycle6, cycle7]
                                    for (let i = 0; i < cycleSeries.length; ++i) {
                                        if (cycleSeries[i] && cycleSeries[i].visible) {
                                            return cycleSeries[i]
                                        }
                                    }
                                    return singleSeries
                                }

                                const scanSeries = [ch0, ch1, ch2, ch3, ch4, ch5, ch6, ch7]
                                for (let i = 0; i < scanSeries.length; ++i) {
                                    if (scanSeries[i] && scanSeries[i].visible) {
                                        return scanSeries[i]
                                    }
                                }

                                return ch0 ? ch0 : singleSeries
                            }

                            ChartView {
                                id: chartView
                                anchors.fill: parent
                                antialiasing: true
                                theme: ChartView.ChartThemeLight
                                backgroundColor: "#fdfefe"
                                plotAreaColor: "#ffffff"
                                backgroundRoundness: 8
                                dropShadowEnabled: false
                                legend.visible: root.multiMode || controller.cycleOverlayVisible || controller.cycleLoopEnabled
                                legend.alignment: Qt.AlignBottom
                                legend.labelColor: root.cSubtle
                                legend.font.pixelSize: 11
                                margins.left: 8
                                margins.right: 8
                                margins.top: 8
                                margins.bottom: 8

                                ValueAxis {
                                    id: axisX
                                    min: controller.axisXMin
                                    max: controller.axisXMax
                                    labelsColor: root.cSubtle
                                    labelsFont.pixelSize: 11
                                    labelFormat: controller.axisXInHours ? "%.2f" : "%.0f"
                                    titleText: controller.axisXInHours ? "时间 (h)" : "时间 (s)"
                                    titleFont.pixelSize: 12
                                    titleFont.bold: true
                                    gridLineColor: root.cGrid
                                }

                                ValueAxis {
                                    id: axisY
                                    min: controller.axisYMin
                                    max: controller.axisYMax
                                    labelsColor: root.cSubtle
                                    labelsFont.pixelSize: 11
                                    titleText: "电压 (V)"
                                    titleFont.pixelSize: 12
                                    titleFont.bold: true
                                    gridLineColor: root.cGrid
                                }

                                LineSeries {
                                    id: singleShadowSeries
                                    axisX: axisX
                                    axisY: axisY
                                    color: "#bcc6d2"
                                    width: 2.8
                                    visible: !root.multiMode
                                    name: "Raw"
                                }
                                LineSeries {
                                    id: singleSeries
                                    axisX: axisX
                                    axisY: axisY
                                    color: "#ff9e58"
                                    width: 2.2
                                    visible: !root.multiMode
                                    name: "Filtered"
                                }

                                LineSeries { id: cycle0; axisX: axisX; axisY: axisY; color: "#ff5c5c"; width: 2.1; visible: false; name: "Cycle 1" }
                                LineSeries { id: cycle1; axisX: axisX; axisY: axisY; color: "#ff9f43"; width: 2.1; visible: false; name: "Cycle 2" }
                                LineSeries { id: cycle2; axisX: axisX; axisY: axisY; color: "#f6c445"; width: 2.1; visible: false; name: "Cycle 3" }
                                LineSeries { id: cycle3; axisX: axisX; axisY: axisY; color: "#26c281"; width: 2.1; visible: false; name: "Cycle 4" }
                                LineSeries { id: cycle4; axisX: axisX; axisY: axisY; color: "#2d98da"; width: 2.1; visible: false; name: "Cycle 5" }
                                LineSeries { id: cycle5; axisX: axisX; axisY: axisY; color: "#6c5ce7"; width: 2.1; visible: false; name: "Cycle 6" }
                                LineSeries { id: cycle6; axisX: axisX; axisY: axisY; color: "#e667af"; width: 2.1; visible: false; name: "Cycle 7" }
                                LineSeries { id: cycle7; axisX: axisX; axisY: axisY; color: "#00b894"; width: 2.1; visible: false; name: "Cycle 8" }

                                LineSeries { id: ch0; axisX: axisX; axisY: axisY; color: channelColors[0]; name: "AIN0" }
                                LineSeries { id: ch1; axisX: axisX; axisY: axisY; color: channelColors[1]; name: "AIN1" }
                                LineSeries { id: ch2; axisX: axisX; axisY: axisY; color: channelColors[2]; name: "AIN2" }
                                LineSeries { id: ch3; axisX: axisX; axisY: axisY; color: channelColors[3]; name: "AIN3" }
                                LineSeries { id: ch4; axisX: axisX; axisY: axisY; color: channelColors[4]; name: "AIN4" }
                                LineSeries { id: ch5; axisX: axisX; axisY: axisY; color: channelColors[5]; name: "AIN5" }
                                LineSeries { id: ch6; axisX: axisX; axisY: axisY; color: channelColors[6]; name: "AIN6" }
                                LineSeries { id: ch7; axisX: axisX; axisY: axisY; color: channelColors[7]; name: "AIN7" }

                                Component.onCompleted: {
                                    controller.attachSeries(
                                        singleShadowSeries,
                                        singleSeries,
                                        ch0,
                                        ch1,
                                        ch2,
                                        ch3,
                                        ch4,
                                        ch5,
                                        ch6,
                                        ch7)

                                    controller.attachCycleSeries(
                                        cycle0,
                                        cycle1,
                                        cycle2,
                                        cycle3,
                                        cycle4,
                                        cycle5,
                                        cycle6,
                                        cycle7)
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                hoverEnabled: true

                                onPressed: function(mouse) {
                                    parent.selecting = true
                                    parent.startX = mouse.x
                                    parent.startY = mouse.y
                                    selectionRect.x = mouse.x
                                    selectionRect.y = mouse.y
                                    selectionRect.width = 0
                                    selectionRect.height = 0
                                    selectionRect.visible = true
                                }

                                onPositionChanged: function(mouse) {
                                    if (!parent.selecting) {
                                        return
                                    }

                                    const left = Math.min(parent.startX, mouse.x)
                                    const top = Math.min(parent.startY, mouse.y)
                                    const right = Math.max(parent.startX, mouse.x)
                                    const bottom = Math.max(parent.startY, mouse.y)

                                    selectionRect.x = left
                                    selectionRect.y = top
                                    selectionRect.width = right - left
                                    selectionRect.height = bottom - top
                                }

                                onReleased: function(mouse) {
                                    if (!parent.selecting) {
                                        return
                                    }

                                    parent.selecting = false
                                    selectionRect.visible = false

                                    const width = Math.abs(mouse.x - parent.startX)
                                    const height = Math.abs(mouse.y - parent.startY)
                                    if (width < 12 || height < 12) {
                                        return
                                    }

                                    const mapSeries = parent.zoomMapSeries()
                                    if (!mapSeries) {
                                        return
                                    }

                                    const p1 = chartView.mapToValue(Qt.point(selectionRect.x, selectionRect.y), mapSeries)
                                    const p2 = chartView.mapToValue(
                                                   Qt.point(selectionRect.x + selectionRect.width,
                                                            selectionRect.y + selectionRect.height),
                                                   mapSeries)

                                    const xMin = Math.min(p1.x, p2.x)
                                    const xMax = Math.max(p1.x, p2.x)
                                    const yMin = Math.min(p1.y, p2.y)
                                    const yMax = Math.max(p1.y, p2.y)

                                    if (isFinite(xMin) && isFinite(xMax) && isFinite(yMin) && isFinite(yMax)) {
                                        controller.setZoomRange(xMin, xMax, yMin, yMax)
                                    }
                                }
                            }

                            Rectangle {
                                id: selectionRect
                                visible: false
                                color: "#3a76d422"
                                border.color: "#3a76d4"
                                border.width: 1
                                radius: 2
                            }
                        }
                    }
                }

                Rectangle {
                    id: logPanel
                    property real panelHeight: root.logPanelCollapsed
                                               ? root.logPanelCollapsedHeight
                                               : root.logPanelExpandedHeight
                    Layout.fillWidth: true
                    Layout.preferredHeight: panelHeight
                    color: root.cCard
                    radius: 10
                    border.color: root.cPanelBorder
                    border.width: 1
                    clip: true

                    Behavior on panelHeight {
                        NumberAnimation {
                            duration: root.logPanelAnimMs
                            easing.type: Easing.InOutCubic
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: root.logPanelCollapsed ? 6 : 8
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "串口日志"
                                color: root.cTitle
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            ColumnLayout {
                                spacing: 6
                                Layout.alignment: Qt.AlignRight | Qt.AlignTop

                                RowLayout {
                                    spacing: 8

                                    SecondaryActionButton {
                                        visible: !root.logPanelCollapsed
                                        text: "打开日志文件"
                                        onClicked: controller.openLogFileDialog()
                                    }

                                    SecondaryActionButton {
                                        visible: !root.logPanelCollapsed
                                        text: "打开数据文件"
                                        onClicked: controller.openDataFileDialog()
                                    }

                                    SecondaryActionButton {
                                        visible: !root.logPanelCollapsed
                                        text: "清空显示"
                                        onClicked: controller.clearLogView()
                                    }

                                    SecondaryActionButton {
                                        visible: !root.logPanelCollapsed
                                        text: "清空日志文件"
                                        onClicked: controller.clearLogStorage()
                                    }
                                }

                                FluentToolButton {
                                    Layout.alignment: Qt.AlignRight
                                    text: root.logPanelCollapsed ? "展开" : "收起"
                                    onClicked: root.logPanelCollapsed = !root.logPanelCollapsed
                                }
                            }
                        }

                        ListView {
                            id: logList
                            visible: !root.logPanelCollapsed || opacity > 0.01
                            enabled: !root.logPanelCollapsed
                            opacity: root.logPanelCollapsed ? 0.0 : 1.0
                            Layout.fillWidth: true
                            Layout.fillHeight: !root.logPanelCollapsed
                            Layout.preferredHeight: root.logPanelCollapsed ? 0 : -1
                            Layout.maximumHeight: root.logPanelCollapsed ? 0 : 16777215
                            model: controller.logModel
                            clip: true
                            spacing: 2

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: Math.round(root.logPanelAnimMs * 0.6)
                                    easing.type: Easing.InOutQuad
                                }
                            }

                            delegate: Label {
                                required property string timestamp
                                required property string level
                                required property string message

                                text: "[" + timestamp + "] " + message
                                color: root.levelColor(level)
                                font.family: "Consolas"
                                font.pixelSize: 12
                            }

                            onCountChanged: {
                                if (count > 0) {
                                    positionViewAtEnd()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: stopConfirmDialog
        title: "结束采样"
        modal: true
        anchors.centerIn: Overlay.overlay
        closePolicy: Popup.NoAutoClose
        padding: 16

        Overlay.modal: Rectangle {
            color: "#5f0f172a"
        }

        background: Rectangle {
            radius: 12
            color: "#ffffff"
            border.color: root.cPanelBorder
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text: "本次会话采样数: " + controller.sampleCount + "。结束前是否提交最后 SQL 缓冲？"
                wrapMode: Text.WordWrap
                color: root.cText
                font.pixelSize: 13
                Layout.preferredWidth: 340
            }

            Label {
                text: "原始数据已实时写入 CSV，即使异常退出也可查看已写入部分。"
                color: root.cSubtle
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                PrimaryActionButton {
                    text: "提交 SQL 并结束"
                    onClicked: {
                        stopConfirmDialog.close()
                        root.finishStopFlow(true)
                    }
                }

                SecondaryActionButton {
                    text: "直接结束"
                    onClicked: {
                        stopConfirmDialog.close()
                        root.finishStopFlow(false)
                    }
                }

                SecondaryActionButton {
                    text: "继续采样"
                    onClicked: {
                        stopConfirmDialog.close()
                        root.stopDisconnectAfterSave = false
                    }
                }
            }
        }
    }

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.cWindowTop }
            GradientStop { position: 0.45; color: root.cWindowMid }
            GradientStop { position: 1.0; color: root.cWindowBottom }
        }
    }
}
