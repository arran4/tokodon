/**
 * SPDX-FileCopyrightText: 2023 Janet Black
 * SPDX-FileCopyrightText: 2024 Joshua Goins <josh@redstrate.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include <QtQml>

#include "customemoji.h"
#include "identity.h"

/**
 * Handles some miscellaneous text processing tasks.
 */
class TextHandler
{
public:
    /// Fixes bidirectionality in posts by ensuring Right-to-Left languages such as Arabic stay that way, and any
    /// Left-to-Right languages such as English are separated. See BUG 475043 for more details on why this is necessary.
    /// \param html The HTML text to process.
    /// \return The processed HTML, do note that we use QTextDocument so the HTML may be littered with unnecessary styling and other garbage.
    static QString fixBidirectionality(const QString &html);

    /// Parses a HTML body and returns a processed body and a list of tags respectively.
    /// \return The processed HTML as the first item in the pair, and the list of standalone tags (if any) as the second item.
    static QPair<QString, QList<QString>> removeStandaloneTags(QString contentHtml);

    /// Replaces parts of a plaintext string that contain an existing custom emoji
    /// \param emojis The list of custom emojis, given from CustomEmoji::parseCustomEmojis()
    /// \param source The plaintext source to use
    /// \returns HTML to be used as rich text
    static QString replaceCustomEmojis(const QList<CustomEmoji> &emojis, const QString &source);
};
