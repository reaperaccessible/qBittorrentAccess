/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrentAccess
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#pragma once

#include <functional>
#include <initializer_list>

#include <QListWidget>

class QAbstractItemView;
class QGridLayout;
class QLabel;
class QKeyEvent;
class QModelIndex;
class QObject;
class QString;
class QTreeView;
class QVariant;
class QWidget;

// Screen reader helpers shared by the accessibility work (qBittorrentAccess)
namespace Access
{
    // Spoken by NVDA, JAWS and Narrator through the platform accessibility API (Qt 6.8+).
    // Assertive interrupts current speech; polite waits for it (after a focus change, for example).
    void announce(QObject *source, const QString &text, bool polite = false);

    // Application-wide <modifiers>+<digit key>, matched on the physical digit key (any keyboard layout,
    // AZERTY included): `handler` gets the digit and returns true when it used it. When the focus is in
    // a text field and the keys type a character (AltGr), the character is left alone.
    // `owner` keeps the filter alive.
    void installDigitKeys(QObject *owner, Qt::KeyboardModifiers modifiers, std::function<bool (int digit)> handler);

    // Give the focus to `widget`; when it already has it, announce what the screen reader says on arrival
    // (name, then current item or value), so a shortcut pressed twice speaks the same thing twice
    void focusAndSpeak(QWidget *widget, Qt::FocusReason reason = Qt::ShortcutFocusReason);

    // Keyboard shortcut to a place: exactly one screen reader message, the same whether the user comes
    // from elsewhere or is already there. `showTarget` makes the target's page current (tab switch...);
    // it runs after the target is remembered as the focus widget of its hidden page, so Qt gives it the
    // focus directly, without stopping on the tab bar or on the page's previous widget. An item view gets
    // a current item; an empty one says "Empty" unless sayEmpty is false (the name already has a count).
    void goTo(QWidget *target, const std::function<void ()> &showTarget = {}, bool sayEmpty = true);

    // Call `afterPaste` once a paste by keyboard (Ctrl+V, Shift+Insert) into `edit` is done
    void onKeyboardPaste(QWidget *edit, std::function<void ()> afterPaste);

    // First widget of `page` reachable with Tab (page shown or not), or `page` itself
    QWidget *firstTabStop(QWidget *page);
    // Child of `page` named `objectName` when it can take the focus, else the first Tab stop of `page`
    QWidget *namedTabStop(QWidget *page, const QString &objectName);

    // Give the focus to the first widget of `page` reachable with Tab, as if the user tabbed into it
    void focusFirstChild(QWidget *page);

    // Application-wide: a popup menu opens with its first enabled item selected, as in Windows,
    // so the screen reader reads it at once (Qt opens it with nothing selected: silent).
    // `owner` keeps the filter alive.
    void installMenuFocusFix(QObject *owner);

    // Application-wide: every dialog gets labelControls() when it is shown, so fields of dialogs we
    // never touched are named after their labels too. `owner` keeps the filter alive.
    void installDialogLabeling(QObject *owner);

    // QTreeWidget / QListWidget own their model: set the whole-row text on every item
    // (Qt::AccessibleTextRole), and again whenever items change
    void keepItemRowTexts(QTreeView *view, int primaryColumn);

    // Label text as a screen reader should say it: no '&' mnemonic, no HTML, no trailing ':'
    QString cleanLabel(const QString &text);

    // Name an input control; for a path editor (field + "..." button) the field gets `name`
    // and the button "Browse"
    void setControlName(QWidget *control, const QString &name);

    // Dialog pass: every input control (spin box, combo box, line edit, path editor, list, time edit)
    // without an explicit accessible name is named after the label on its left in the same layout row
    // (or the check box that enables it), and the label becomes its buddy. Without this, Qt names such
    // a control after its group box title, so 4 fields of one group all say the group title.
    // Scroll areas stop being nameless Tab stops.
    void labelControls(QWidget *root);

    // Up on the first row / Down on the last row: announce "First, <row>" / "Last, <row>"
    // instead of staying silent. In a tree, the first / last VISIBLE item. Returns true when handled.
    bool announceListEdge(QAbstractItemView *view, const QKeyEvent *event);

    // Same, as an event filter installed on `view` (for views we do not subclass)
    void installListEdgeAnnouncer(QAbstractItemView *view);

    // Whole-row text of a multi-column view: the primary column first, then every visible column
    // in on-screen order as "Header value", empty ones skipped
    QString rowText(const QTreeView *view, const QModelIndex &index, int primaryColumn);

    // Screen readers read Qt::AccessibleTextRole of the focused cell. Register `view` so that
    // rowTextFor() returns its whole-row text; the view's proxy model returns rowTextFor(index)
    // from data(Qt::AccessibleTextRole).
    void registerRowText(QTreeView *view, int primaryColumn);
    QVariant rowTextFor(const QModelIndex &index);

    // "Caption: value" rows of grid layouts made of caption labels and value labels (value labels are
    // the ones whose objectName ends with "Val" or "Data", possibly inside a scroll area), in reading
    // order; empty values are skipped, hidden pairs too unless includeHidden (grids hidden because a
    // list mirrors them). `isValue` replaces the objectName rule when the dialog names them otherwise.
    QStringList captionValueRows(std::initializer_list<const QGridLayout *> grids, bool includeHidden = false
        , const std::function<bool (const QLabel *label)> &isValue = {});

    // Read-only list of "Label: value" rows: "First," / "Last," at the edges, Ctrl+C copies the value
    class ReadOnlyList final : public QListWidget
    {
    public:
        explicit ReadOnlyList(QWidget *parent = nullptr);

        // Replace the rows, keeping the current row (no screen reader noise when only values change)
        void setRows(const QStringList &rows);

    protected:
        void keyPressEvent(QKeyEvent *event) override;
    };
}
