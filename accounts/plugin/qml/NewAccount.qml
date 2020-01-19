import QtQuick 2.4
import Ubuntu.Components 1.3
import Ubuntu.OnlineAccounts 0.1

Item {
    id: root

    signal finished

    height: contents.height

    property var __account: account
    property string __host: ""
    property bool __busy: false
    property string __hostError: i18n.dtr("sync-monitor", "Invalid host URL")

    Column {
        id: contents
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: units.gu(2)
        }
        spacing: units.gu(2)

        Label {
            id: errorLabel
            anchors { left: parent.left; right: parent.right }
            font.bold: true
            color: theme.palette.normal.negative
            wrapMode: Text.Wrap
            visible: !__busy && text != ""
        }

        Label {
            anchors { left: parent.left; right: parent.right }
            text: i18n.dtr("sync-monitor", "URL:")
        }

        TextField {
            id: urlField
            anchors { left: parent.left; right: parent.right }
            placeholderText: i18n.dtr("sync-monitor", "http://myserver.com/caldav/")
            focus: true
            enabled: !__busy

            inputMethodHints: Qt.ImhUrlCharactersOnly
        }

        Label {
            anchors { left: parent.left; right: parent.right }
            text: i18n.dtr("sync-monitor", "Username:")
        }

        TextField {
            id: usernameField
            anchors { left: parent.left; right: parent.right }
            placeholderText: i18n.dtr("sync-monitor", "Your username")
            enabled: !__busy
            inputMethodHints: Qt.ImhNoAutoUppercase + Qt.ImhNoPredictiveText + Qt.ImhPreferLowercase

            KeyNavigation.tab: passwordField
        }

        Label {
            anchors { left: parent.left; right: parent.right }
            text: i18n.dtr("sync-monitor", "Password:")
        }

        TextField {
            id: passwordField
            anchors { left: parent.left; right: parent.right }
            placeholderText: i18n.dtr("sync-monitor", "Your password")
            echoMode: TextInput.Password
            enabled: !__busy

            inputMethodHints: Qt.ImhSensitiveData
            Keys.onReturnPressed: login()
        }

        Row {
            id: buttons
            anchors { left: parent.left; right: parent.right }
            height: units.gu(5)
            spacing: units.gu(1)
            Button {
                id: btnCancel
                text: i18n.dtr("sync-monitor", "Cancel")
                width: (parent.width / 2) - 0.5 * parent.spacing
                onClicked: finished()
            }
            Button {
                id: btnContinue
                text: i18n.dtr("sync-monitor", "Continue")
                color: theme.palette.normal.positive
                width: (parent.width / 2) - 0.5 * parent.spacing
                onClicked: login()
                enabled: !__busy
            }
        }
    }

    ActivityIndicator {
        anchors.centerIn: parent
        running: __busy
    }

    Credentials {
        id: creds
        caption: account.provider.id
        acl: [ "unconfined" ]
        storeSecret: true
        onCredentialsIdChanged: root.credentialsStored()
    }

    AccountService {
        id: globalAccountSettings
        objectHandle: account.accountServiceHandle
        autoSync: false
    }

    function login() {
        var host = cleanUrl(urlField.text)
        var username = usernameField.text
        var password = passwordField.text

        errorLabel.text = ""
        __busy = true

        /* At this point we should make an HTTP request and verify if the given
         * URL and credentials are correct. However, let's leave it as a TODO
         * for now. */
        saveData(host, username, password)
    }

    function saveData(host, username, password) {
        __host = host
        var strippedHost = host.replace(/^https?:\/\//, '')
        account.updateDisplayName(username + '@' + strippedHost)
        creds.userName = username
        creds.secret = password
        creds.sync()
    }

    function showError(message) {
        if (!errorLabel.text) errorLabel.text = message
    }

    function credentialsStored() {
        console.log("Credentials stored, id: " + creds.credentialsId)
        if (creds.credentialsId == 0) return

        globalAccountSettings.updateServiceEnabled(true)
        globalAccountSettings.credentials = creds
        globalAccountSettings.updateSettings({
            "host": __host
        })
        account.synced.connect(finished)
        account.sync()
        __busy = false
    }

    // check host url for http
    function cleanUrl(url) {
        var host = url.trim()
        return host
    }

}
