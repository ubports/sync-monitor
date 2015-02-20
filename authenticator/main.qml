import QtQuick 2.2

import Ubuntu.Components 1.1
import Ubuntu.Components.ListItems 1.0 as ListItem
import Ubuntu.OnlineAccounts 0.1

MainView {
    id: root

    property bool accountFound: false

    width: units.gu(40)
    height: units.gu(71)
    useDeprecatedToolbar: false

    AccountServiceModel {
        id: accountServiceModel

        accountId: ONLINE_ACCOUNT.accountId
        service: ONLINE_ACCOUNT.serviceName
        onCountChanged: {
            console.debug("Account count changed:" + count)
            if ((count == 1) && (pageStack.depth == 0)) {
                var _acc = {}
                _acc["displayName"] = accountServiceModel.get(0, "displayName")
                _acc["providerName"] = accountServiceModel.get(0, "providerName")
                _acc["serviceName"] = accountServiceModel.get(0, "serviceName")
                _acc["accountServiceHandle"] = accountServiceModel.get(0, "accountServiceHandle")
                _acc["accountId"] = accountServiceModel.get(0, "accountId")
                _acc["accountHandle"] = accountServiceModel.get(0, "accountHandle")
                root.accountFound = true
                pageStack.push(accountPageComponent, {"account" : _acc})
            }
        }
    }

    PageStack {
        id: pageStack

        Component.onCompleted: {
            if (!root.accountFound) {
                pageStack.push(loadingPageComponent)
                accountNotFoundTimeout.start()
            }
        }
    }

    Timer {
        id: accountNotFoundTimeout

        interval: 5000
        repeat: false
        onTriggered: {
            if (!root.accountFound) {
                pageStack.push(accountNotFoundPageComponent)
            }
        }
    }

    Component {
        id: loadingPageComponent

        Page {
            id: loadingPage

            title: i18n.tr("Accounts")

            ActivityIndicator {
                id: activity

                anchors.centerIn : parent
                running: visible
                visible: loadingPage.active
            }
        }
    }

    Component {
        id: accountNotFoundPageComponent

        Page {
            id: accountNotFoundPage

            title: i18n.tr("Accounts")

            head.backAction: Action {
                iconName: "back"
                text: i18n.tr("Quit")
                onTriggered: Qt.quit()
            }

            Label {
                anchors.centerIn: parent
                text: i18n.tr("Fail to load account information.")
            }
        }
    }

    Component {
        id: accountPageComponent

        Page {
            id: accountPage

            property var account
            property bool loginInProcess: false

            function getProviderIcon(providerName)
            {
                switch(providerName)
                {
                case "Google":
                    return "google"
                default:
                    return "contact-group"
                }
            }

            title: i18n.tr("Fail to sync")

            head.backAction: Action {
                iconName: "back"
                text: i18n.tr("Quit")
                onTriggered: Qt.quit()
            }

            AccountService {
                id: accountService

                objectHandle: accountPage.account.accountServiceHandle
                onAuthenticated: {
                    accountPage.loginInProcess = false
                    Qt.quit()
                }
                onAuthenticationError: {
                    accountPage.loginInProcess = false
                    console.log("Authentication failed, code " + error.code)
                }
            }

            Column {
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left
                    right: parent.right
                    margins: units.gu(2)
                }
                spacing: units.gu(4)
                width: parent.width
                visible: !accountPage.loginInProcess

                Label {
                    id: lblTitle

                    anchors {
                        left: parent.left
                        right: parent.right
                    }

                    text: i18n.tr("Fail to authenticate your account while sync contacts, please click in the button to re-authenticate it.")
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    fontSize: "large"
                }

                Button {
                    anchors.horizontalCenter: lblTitle.horizontalCenter
                    text: accountPage.account.displayName
                    iconName: accountPage.getProviderIcon(accountPage.account.providerName)
                    onClicked: {
                        if (!accountPage.busy) {
                            accountPage.loginInProcess = true
                            accountService.authenticate(null)
                        }
                    }
                }
            }

            ActivityIndicator {
                id: activity

                anchors.centerIn: parent
                running: visible
                visible: accountPage.loginInProcess
            }
        }
    }
}
