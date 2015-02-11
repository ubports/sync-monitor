import QtQuick 2.2

import Ubuntu.Components 1.1
import Ubuntu.Components.ListItems 1.0 as ListItem
import Ubuntu.OnlineAccounts 0.1

MainView {
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
    width: units.gu(40)
    height: units.gu(71)
    useDeprecatedToolbar: false

    Page {
        title: i18n.tr("Accounts")

        head.backAction: Action {
            visible: mainPage.allowToQuit
            iconName: "back"
            text: i18n.tr("Quit")
            onTriggered: Qt.quit()
        }

        AccountServiceModel {
            id: accountServiceModel
            accountId: ONLINE_ACCOUNT.accountId
            service: ONLINE_ACCOUNT.serviceName
        }

        ListView {
            anchors.fill: parent
            model: accountServiceModel

            delegate:  ListItem.Subtitled {
                id: delegate

                property bool hasToken: false
                property bool running: false

                text: displayName
                subText: i18n.tr("Click to authenticate")
                iconName: getProviderIcon(providerName)
                height: delegate.hasToken ? 0 : units.gu(8)

                AccountService {
                    id: accountService

                    objectHandle: accountServiceHandle
                    onAuthenticated: {
                        delegate.running = false
                        console.log("Recevied new token.")
                        delegate.hasToken = true
                        Qt.quit()
                    }
                    onAuthenticationError: {
                        delegate.running = false
                        delegate.hasToken = false
                        console.log("Authentication failed, code " + error.code)
                    }
                }
                onClicked: {
                    if (!delegate.running) {
                        delegate.running = true
                        accountService.authenticate(null)
                    }
            }
        }
    }
}
