/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * KeyMapManager.hh
 * Copyright (C) 2021 Donn Terry <aesopPlayer@gmail.com>
 *
 * gImageReader is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gImageReader is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEYMAP_MANAGER_HH
#define KEYMAP_MANAGER_HH

#include <QDialog>
#include <QStack>

class QCheckBox;
class QItemSelection;
class OutputTextEdit;
class QTableWidget;
class QKeyEvent;
class QDialogButtonBox;
class FocusableMenu;

typedef QVector<Qt::Key> KeyString;

class KeyEvent : public QKeyEvent {
	// To identify our synthetic key events reliably (recursion breaker)
public:
	KeyEvent(QEvent::Type ev, Qt::Key key, Qt::KeyboardModifiers modifiers, const QString& text) : QKeyEvent(ev, key, modifiers, text) {
		m_sequence = s_sequence++;
	}
	ulong sequence() {return m_sequence;}
private:
	// sequence serves instead of timestamp() because for this timestamp() always == 0
	ulong m_sequence;
	static ulong s_sequence;
};

class KeyMapManager : public QDialog {
	Q_OBJECT
public:
	KeyMapManager(FocusableMenu *keyParent, QWidget* parent = nullptr);
	void doShow();
	void showCloseButton(bool show);
	static void waitableStarted();
	static void waitableDone();
	void startupSending();

private:
	void refreshKeyMap() const;
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
	bool eventFilter(QObject* target, QEvent* ev);
	bool startSending(Qt::Key keyCode);
	void sendOnePress();
	void sendOneRelease();
	void sendOneCharacter();
	bool advance();
	void sendAlt();
	void awaitOneRelease();
	void awaitMouseUp();
	void awaitWaitable();
	void reset();

private:
	QAction* m_removeAction;
	QString m_currentFile;
	QTableWidget* m_tableWidget;
	QDialogButtonBox* m_buttonBox;
	ulong m_lastTimestamp;
	FocusableMenu* m_menu;

	int m_currentPosition;
	KeyString *m_currentKeys = nullptr;
	class KeyStackEntry {
	public:
		KeyStackEntry() : m_string(), m_pos() {}
		KeyStackEntry(KeyString* string, int pos) : m_string(string), m_pos(pos) {}
		KeyString* m_string;
		int m_pos;
	};
	QStack<KeyStackEntry> m_keyStack;
	int m_interval;
	bool m_posted;
	bool m_mousePosted;
	static bool m_awaitingFinish;

private slots:
	void addRow();
	bool clearList();
	void onTableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
	void openList(bool append);
	void removeRows();
	void sortTable();
	bool saveList();
	void tableCellChanged(int row, int column);
};

#endif // KEYMAP_MANAGER_HH
