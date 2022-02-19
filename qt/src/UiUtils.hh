/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * UiUtils.hh
 * Copyright (C) 2021-2022 Donn Terry <aesopPlayer@gmail.com>
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

#ifndef UIUTILS_HH
#define UIUTILS_HH

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QMenu>

#include <functional>

class QTabWidget;

class FocusableMenu : public QMenu {
public:
	FocusableMenu(QWidget* parent);
	FocusableMenu(const QString& title, QWidget* parent);
	~FocusableMenu() {}

	QAction* addMenu(FocusableMenu* menu);
	QAction* addMenu(FocusableMenu* menu, const std::function<void()>& action);
	QAction* addMenu(const QString& title, const std::function<void()>& action);
	QAction* addMenu(const QString& title);
	QAction* addCheckable(const QString& title, QCheckBox* checkable);
	QAction* addCheckable(const QString& title, QAction* checkable);
	QAction* addDialog(const QIcon& icon, const QString& title, const std::function<bool()>& createDialog, const std::function<QDialog*(FocusableMenu *)>& checkDialogForReady);
	QAction* addDialog(const QString& title, const std::function<void()>& action);
	QAction* addDialog(const QIcon& icon, const QString& title, const std::function<void()>& action);
	QAction* addDialog(const QString& title, FocusableMenu* menu, QDialog *dialog);
	QAction* addDialog(const QIcon& icon, const QString& title, FocusableMenu* menu, QDialog *dialog);
	QAction* addFileDialog(const QIcon& icon, const QString& title, const std::function<bool()>& createDialog);
	QAction* addFileDialog(const QString& title, const std::function<bool()>& createDialog);
	int execWithMenu(QDialog *activeDialog);

	void useButtons();
	void mapMenuName(const QString& oldTitle, const QString& newTitle, const QKeySequence&);
	void mapMenuName(const QString& oldTitle, const QString& newTitle);
	void mapButtonBoxDefault();
	void mapButtonBoxFiles();
	void showDisabled(bool show);
	void showInMenu(QWidget *item, bool show);


	static bool showFileDialogMenu(FocusableMenu *keyParent, const std::function<bool()>& createDialog);
	static void sequenceFocus(QWidget *root, QWidget* distinguishedNode); 
	static QChar shortcutKeyOf(const QString& string);

	// Set focus and visually indicate that
	static void showFocusSet(QTabWidget* widget, int index);
	static void showFocusSet(QWidget *widget);

private:
	void keyPressEvent(QKeyEvent* event) override;
	bool eventFilter(QObject *target, QEvent *ev);
	void relocate();
	void setupDialog(QDialog* activeDialog, FocusableMenu* menu);
	void cleanupDialog();
	void pollForDialogReady(QTimer& timer, std::function<QDialog*(FocusableMenu*)> dialog, FocusableMenu*menu);
	void searchForButtons(QWidget *activeDialog, FocusableMenu *menu);
	static void pollForDialogReadyStatic(QTimer& timer, FocusableMenu* menu);

	QWidget* m_focusWidget = nullptr;
	struct KeyPair {QString n; QKeySequence k;};
	QMap<QString, KeyPair> m_titleMap;
	bool m_useButtons = false;
	bool m_showDisabled = true;
	QAction* m_actionEnter = nullptr;
	QAction* m_actionEscape = nullptr;

	struct info {
		bool secondary;
		std::function<void()> action;  // the action needed to *insert this menu item*, not the menu item's action!
	};
	void insertItem(FocusableMenu* menu, QMap<int, struct info>& entries, int pos, const QString &title, QWidget* item);
	bool commonActions(QMap<int, struct info>& entries, FocusableMenu* menu, QWidget* item, const QString& title, 
		const QString& defaultText, QString& resultText, QKeySequence& keySeq);
};

class KeyDialogButtonBox : public QDialogButtonBox {
public:
	KeyDialogButtonBox(KeyDialogButtonBox::StandardButtons buttons, Qt::Orientation orientation, QWidget *parent = nullptr);
	KeyDialogButtonBox(KeyDialogButtonBox::StandardButtons buttons, QWidget *parent = nullptr);
	KeyDialogButtonBox(Qt::Orientation orientation, QWidget *parent = nullptr);
	KeyDialogButtonBox(QWidget *parent = nullptr);

	void setStandardButtons(KeyDialogButtonBox::StandardButtons buttons);
private:
	void fixStandardButtons(KeyDialogButtonBox::StandardButtons buttons);
};

class KeyMessageBox : public QMessageBox {
public:
	KeyMessageBox(KeyMessageBox::Icon icon, const QString &title, 
	              const QString &text, QMessageBox::StandardButtons buttons = NoButton, 
				  QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
	KeyMessageBox(QWidget *parent = nullptr);

	void setStandardButtons(KeyMessageBox::StandardButtons buttons);

static KeyMessageBox::StandardButton critical(QWidget *parent, const QString &title, const QString &text, 
		KeyMessageBox::StandardButtons buttons = Ok, KeyMessageBox::StandardButton defaultButton = NoButton);
static KeyMessageBox::StandardButton information(QWidget *parent, const QString &title, const QString &text, 
		KeyMessageBox::StandardButtons buttons = Ok, KeyMessageBox::StandardButton defaultButton = NoButton);
static KeyMessageBox::StandardButton question(QWidget *parent, const QString &title, const QString &text, 
		KeyMessageBox::StandardButtons buttons = StandardButtons(Yes | No), KeyMessageBox::StandardButton defaultButton = NoButton);
static KeyMessageBox::StandardButton warning(QWidget *parent, const QString &title, const QString &text, 
		KeyMessageBox::StandardButtons buttons = Ok, KeyMessageBox::StandardButton defaultButton = NoButton);

private:
	void fixStandardButtons(KeyMessageBox::StandardButtons buttons);
	static KeyMessageBox::StandardButton showNewMessageBox(QWidget *parent,
		KeyMessageBox::Icon icon,
		const QString& title, const QString& text,
		KeyMessageBox::StandardButtons buttons,
		KeyMessageBox::StandardButton defaultButton);
};

class BlinkWidget : QObject {
	Q_OBJECT
public:
	BlinkWidget(int count, const std::function<void()>& on, const std::function<void()>& off, QObject*parent = nullptr);
protected:
	~BlinkWidget() {}; // class must be used with 'new'

private:
	QTimer *m_timer;
	int m_count;
	const std::function<void()> m_on;
	const std::function<void()> m_off;
};


#endif