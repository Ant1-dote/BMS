import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCharts

ApplicationWindow {
    id: root
    width: 1440
    height: 920
    visible: true
    title: "控制台"
    color: "#f3f7fb"

    property color cWindowTop: "#f6f9fc"
    property color cWindowMid: "#edf3f8"
    property color cWindowBottom: "#e5edf5"
    property color cPanel: "#f8fbff"
    property color cPanelBorder: "#c9d8e8"
    property color cCard: "#ffffff"
    property color cCardBorder: "#d4e1ef"
    property color cTitle: "#304861"
    property color cText: "#17324a"
    property color cSubtle: "#6f859c"
    property color cChartBg: "#f9fcff"
    property color cGrid: "#d8e2ec"
    property color cStatusBg: "#eaf2fb"
    property color cStatusBorder: "#c3d5e8"

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
    property var scanVisibleFlags: [true, true, true, true, true, true, true, true]

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

    SplitView {
        anchors.fill: parent
        anchors.margins: 16
        orientation: Qt.Horizontal

        ScrollView {
            id: leftPanelScroll
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 340
            clip: true

            ColumnLayout {
                width: leftPanelScroll.availableWidth
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    radius: 12
                    color: "#d8e9fb"
                    border.color: "#b4cce5"
                    border.width: 1
                    implicitHeight: 88

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        Label {
                            text: "^_^"
                            font.pixelSize: 24
                            color: "#24425f"
                            font.bold: true
                        }
                        Label {
                            text: "控制中心"
                            color: "#4c6985"
                        }
                    }
                }

                GroupBox {
                    title: "串口连接"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 3
                        rowSpacing: 8
                        columnSpacing: 8

                        Label { text: "端口" }
                        ComboBox {
                            id: portCombo
                            Layout.fillWidth: true
                            model: controller.availablePorts
                        }
                        Button {
                            text: "刷新端口"
                            onClicked: controller.refreshPorts()
                        }

                        Label { text: "波特率" }
                        ComboBox {
                            id: baudCombo
                            Layout.fillWidth: true
                            model: [9600, 57600, 115200, 230400, 460800, 921600]
                            currentIndex: 2
                        }
                        Button {
                            text: controller.connected ? "断开" : "连接"
                            onClicked: {
                                let dev = controller.portDeviceAt(portCombo.currentIndex)
                                controller.toggleConnection(dev, Number(baudCombo.currentText))
                            }
                        }

                        Label { text: "采样" }
                        Button {
                            Layout.columnSpan: 2
                            text: controller.acquisitionEnabled ? "停止连续采样" : "开始连续采样"
                            onClicked: controller.toggleCapture()
                        }

                        Label { text: "状态" }
                        Rectangle {
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            implicitHeight: 34
                            color: root.cStatusBg
                            radius: 8
                            border.color: root.cStatusBorder

                            Label {
                                anchors.centerIn: parent
                                text: controller.statusText
                                color: root.cText
                            }
                        }
                    }
                }

                GroupBox {
                    title: "运行配置"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 4
                        rowSpacing: 8
                        columnSpacing: 8

                        Label { text: "参考电压" }
                        SpinBox {
                            id: vrefSpin
                            Layout.fillWidth: true
                            from: 100
                            to: 5000
                            stepSize: 50
                            value: Math.round(controller.vref * 1000)
                            editable: true
                            textFromValue: function(v) { return (v / 1000.0).toFixed(3) }
                            valueFromText: function(txt) { return Math.round(parseFloat(txt) * 1000) }
                            onValueModified: controller.vref = value / 1000.0
                        }

                        Label { text: "PGA" }
                        ComboBox {
                            id: pgaCombo
                            Layout.fillWidth: true
                            model: pgaValues
                            onActivated: controller.pga = Number(currentText)
                            Component.onCompleted: {
                                const idx = pgaValues.indexOf(controller.pga)
                                currentIndex = idx >= 0 ? idx : 0
                            }
                        }

                        Label { text: "AINP" }
                        ComboBox {
                            id: pselCombo
                            Layout.fillWidth: true
                            enabled: controller.acqMode !== "SCAN8"
                            model: channelNames
                            onActivated: controller.psel = currentText
                            Component.onCompleted: currentIndex = channelNames.indexOf(controller.psel)
                        }

                        Label { text: "AINN" }
                        ComboBox {
                            id: nselCombo
                            Layout.fillWidth: true
                            enabled: controller.acqMode !== "SCAN8"
                            model: channelNames
                            onActivated: controller.nsel = currentText
                            Component.onCompleted: currentIndex = channelNames.indexOf(controller.nsel)
                        }

                        Label { text: "采样率" }
                        ComboBox {
                            id: drateCombo
                            Layout.fillWidth: true
                            Layout.columnSpan: 3
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

                        Label { text: "采样模式" }
                        ComboBox {
                            id: acqModeCombo
                            Layout.fillWidth: true
                            model: ["SINGLE", "SCAN8"]
                            onActivated: controller.acqMode = currentText
                            Component.onCompleted: currentIndex = model.indexOf(controller.acqMode)
                        }

                        Label { text: "显示通道" }
                        ComboBox {
                            Layout.fillWidth: true
                            model: ["AIN0", "AIN1", "AIN2", "AIN3", "AIN4", "AIN5", "AIN6", "AIN7"]
                            enabled: controller.acqMode === "SCAN8"
                            onActivated: controller.viewChannel = currentIndex
                            Component.onCompleted: currentIndex = controller.viewChannel
                        }

                        Label { text: "横轴模式" }
                        ComboBox {
                            id: xAxisModeCombo
                            Layout.fillWidth: true
                            model: ["FULL", "WINDOW"]
                            onActivated: controller.xAxisMode = currentText
                            Component.onCompleted: currentIndex = model.indexOf(controller.xAxisMode)
                        }

                        Label { text: "滑窗秒数" }
                        SpinBox {
                            id: windowSpin
                            Layout.fillWidth: true
                            enabled: controller.xAxisMode === "WINDOW"
                            from: 2
                            to: 21600
                            value: Math.round(controller.windowSeconds)
                            onValueModified: controller.windowSeconds = value
                        }

                        Item { Layout.fillWidth: true }
                        Button {
                            Layout.columnSpan: 3
                            text: "下发配置到设备"
                            onClicked: controller.sendApplyConfig()
                        }
                    }
                }

                GroupBox {
                    title: "滤波 (EKF)"
                    Layout.fillWidth: true

                    GridLayout {
                        anchors.fill: parent
                        columns: 4
                        rowSpacing: 8
                        columnSpacing: 8

                        CheckBox {
                            Layout.columnSpan: 4
                            text: "启用 EKF"
                            checked: controller.ekfEnabled
                            onToggled: controller.ekfEnabled = checked
                        }

                        Label { text: "Q" }
                        TextField {
                            Layout.fillWidth: true
                            text: Number(controller.ekfQ).toExponential(3)
                            onEditingFinished: controller.ekfQ = Number(text)
                        }

                        Label { text: "R" }
                        TextField {
                            Layout.fillWidth: true
                            text: Number(controller.ekfR).toExponential(3)
                            onEditingFinished: controller.ekfR = Number(text)
                        }

                        Label { text: "P0" }
                        TextField {
                            Layout.fillWidth: true
                            text: controller.ekfP0.toString()
                            onEditingFinished: controller.ekfP0 = Number(text)
                        }

                        Item { Layout.fillWidth: true }
                        Button {
                            Layout.columnSpan: 2
                            text: "重置滤波状态"
                            onClicked: controller.resetEkf()
                        }
                    }
                }

                GroupBox {
                    title: "高级命令"
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 8
                            columnSpacing: 8

                            Button { text: "复位 RESET"; onClicked: controller.sendCommand("RESET") }
                            Button { text: "自校准 SELFCAL"; onClicked: controller.sendCommand("SELFCAL") }
                            Button { text: "同步 SYNC"; onClicked: controller.sendCommand("SYNC") }
                            Button { text: "唤醒 WAKEUP"; onClicked: controller.sendCommand("WAKEUP") }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                id: customCmdEdit
                                Layout.fillWidth: true
                                placeholderText: "手动命令，例如: CFG PSEL=AIN0 NSEL=AINCOM PGA=8"
                            }
                            Button {
                                text: "发送"
                                onClicked: {
                                    controller.sendCustomCommand(customCmdEdit.text)
                                    customCmdEdit.clear()
                                }
                            }
                        }
                    }
                }

                GroupBox {
                    title: "8通道显隐"
                    Layout.fillWidth: true
                    enabled: controller.acqMode === "SCAN8"

                    GridLayout {
                        anchors.fill: parent
                        columns: 4
                        rowSpacing: 6
                        columnSpacing: 8

                        Repeater {
                            model: 8
                            delegate: CheckBox {
                                required property int index
                                text: "AIN" + index
                                checked: scanVisibleFlags[index]
                                onToggled: {
                                    scanVisibleFlags[index] = checked
                                    controller.setScanChannelVisible(index, checked)
                                }
                            }
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

                GridLayout {
                    Layout.fillWidth: true
                    columns: width > 900 ? 4 : 2
                    rowSpacing: 8
                    columnSpacing: 8

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
                            text: controller.acqMode === "SCAN8" ? "实时 ADC 曲线（8通道）" : "实时 ADC 曲线"
                            color: root.cTitle
                            font.pixelSize: 14
                            font.bold: true
                        }

                        ChartView {
                            id: chartView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            antialiasing: true
                            backgroundColor: "#ffffff"
                            legend.visible: controller.acqMode === "SCAN8"
                            margins.left: 8
                            margins.right: 8
                            margins.top: 8
                            margins.bottom: 8

                            ValueAxis {
                                id: axisX
                                min: controller.axisXMin
                                max: controller.axisXMax
                                labelsColor: "#445c74"
                                titleText: "时间 (s)"
                                gridLineColor: root.cGrid
                            }

                            ValueAxis {
                                id: axisY
                                min: controller.axisYMin
                                max: controller.axisYMax
                                labelsColor: "#445c74"
                                titleText: "电压 (V)"
                                gridLineColor: root.cGrid
                            }

                            LineSeries {
                                id: singleShadowSeries
                                axisX: axisX
                                axisY: axisY
                                color: "#bcc6d2"
                                width: 2.8
                                visible: controller.acqMode !== "SCAN8"
                                name: "Raw"
                            }
                            LineSeries {
                                id: singleSeries
                                axisX: axisX
                                axisY: axisY
                                color: "#ff9e58"
                                width: 2.2
                                visible: controller.acqMode !== "SCAN8"
                                name: "Filtered"
                            }

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
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    color: root.cCard
                    radius: 10
                    border.color: root.cPanelBorder
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "串口流 + SQL 日志"
                                color: root.cTitle
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "清空显示"
                                onClicked: controller.clearLogView()
                            }
                            Button {
                                text: "清空SQL"
                                onClicked: controller.clearLogStorage()
                            }
                        }

                        ListView {
                            id: logList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: controller.logModel
                            clip: true
                            spacing: 2

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

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.cWindowTop }
            GradientStop { position: 0.45; color: root.cWindowMid }
            GradientStop { position: 1.0; color: root.cWindowBottom }
        }
    }
}
