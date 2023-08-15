// SPDX-FileCopyrightText: 2021 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import org.kde.kirigami 2.19 as Kirigami
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import QtQml.Models 2.15
import org.kde.kmasto 1.0
import org.kde.kirigamiaddons.formcard 1.0 as FormCard

MastoPage {
    objectName: 'loginPage'
    title: i18n("Login")

    leftPadding: 0
    rightPadding: 0

    Connections {
        target: Controller
        function onNetworkErrorOccurred(error) {
            applicationWindow().showPassiveNotification(i18nc("@info:status Network status", "Failed to contact server: %1. Please check your settings.", error));
        }
    }

    ColumnLayout {
        width: parent.width

        FormCard.FormHeader {
            title: i18n("Welcome to Tokodon")
        }

        FormCard.FormCard {
            FormCard.FormTextFieldDelegate {
                id: instanceUrl
                label: i18n("Instance Url:")
                onAccepted: continueButton.clicked()
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }

            FormCard.FormDelegateSeparator { above: adminScopeDelegate }

            FormCard.FormCheckDelegate {
                id: adminScopeDelegate
                text: i18n("Enable moderation tools")
                description: i18n("Allow Tokodon to access moderation tools. Try disabling this if you have trouble logging into your server.")
                checked: true
            }

            FormCard.FormDelegateSeparator { above: continueButton }

            FormCard.FormButtonDelegate {
                id: continueButton
                text: i18n("Continue")
                onClicked: {
                    if (!instanceUrl.text) {
                        applicationWindow().showPassiveNotification(i18n("Instance URL must not be empty!"));
                        return;
                    }

                    const account = AccountManager.createNewAccount(instanceUrl.text, sslErrors.checked, adminScopeDelegate.checked);

                    account.registered.connect(() => {
                        const page = pageStack.layers.push('qrc:/content/ui/AuthorizationPage.qml', {
                            account: account,
                        });

                    });
                }
            }
        }

        FormCard.FormHeader {
            title: i18nc("@title:group Login page", "Network Settings")
        }

        FormCard.FormCard {
            FormCard.FormSwitchDelegate {
                id: sslErrors
                text: i18nc("@option:check Login page", "Ignore SSL errors")
            }

            FormCard.FormDelegateSeparator { above: proxySettingDelegate; below: sslErrors }

            FormCard.FormButtonDelegate {
                id: proxySettingDelegate
                text: i18n("Proxy Settings")
                onClicked: pageStack.layers.push('qrc:/content/ui/Settings/NetworkProxyPage.qml')
            }
        }
    }
}
