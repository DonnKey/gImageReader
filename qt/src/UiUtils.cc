/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * UiUtils.cc
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

#include <QDialogButtonBox>
#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QMap>
#include <QShortcut>
#include <QTimer>

#include "MainWindow.hh"
#include "KeyMapManager.hh"
#include "UiUtils.hh"

BlinkWidget::BlinkWidget(int count, const std::function<void()>& on, const std::function<void()>& off, QObject* parent) :
	m_on(on), m_off(off), m_count(count) {
	m_timer = new QTimer(parent);
	QApplication::connect(m_timer, &QTimer::timeout, [this](){
		if (m_count-- %2 == 1) {
			m_off();
		} else {
			m_on();
		}
		if (m_count <= 0) {
			m_timer->stop();
			QApplication::disconnect(m_timer, &QTimer::timeout, nullptr, nullptr);
			delete m_timer;
			delete this;
		}
	});
	m_timer->start(500);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

class CheckMenu : public QMenu {
public:
	CheckMenu(QCheckBox *checkable, QWidget *parent = nullptr): QMenu(parent), m_parent(parent) {
		setStyleSheet("background-color: lightblue");
		addAction(_("&Check\t"), [this, checkable] {checkable->setCheckState(Qt::Checked); });
		addAction(_("&Uncheck\t"), [this, checkable] {checkable->setCheckState(Qt::Unchecked); });
		addAction(_("&Toggle\t"), [this, checkable] {checkable->toggle(); });
		if (checkable->isTristate()) {
			addAction(_("&Partial\t"), [this, checkable] {checkable->setCheckState(Qt::PartiallyChecked); });
		}
	}

	CheckMenu(QAction *checkable, QWidget *parent = nullptr) : QMenu(parent), m_parent(parent) {
		setStyleSheet("background-color: lightblue");
		addAction(_("&Check\t"), [this, checkable] {checkable->setChecked(true); });
		addAction(_("&Uncheck\t"), [this, checkable] {checkable->setChecked(false); });
		addAction(_("&Toggle\t"), [this, checkable] {checkable->toggle(); });
	}

private:
	void showEvent(QShowEvent* ev) override {
		QMenu::showEvent(ev);
		m_parent->show();
		setFocus();
	}
	void hideEvent(QHideEvent* ev) override {
		QMenu::hideEvent(ev);
		m_parent->hide();
		deleteLater();
	}
	QWidget *m_parent;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////

void FocusableMenu::showFocusSet(QTabWidget* w, int index) {
	w->setCurrentIndex(index);
	w->currentWidget()->setFocus();
	new BlinkWidget(4,
		[=]() {w->setStyleSheet("QTabBar::tab::selected{background-color: lightblue}");},
		[=]() {w->setStyleSheet("");}, w
	);
}

void FocusableMenu::showFocusSet(QWidget* w) {
	w->setFocus();
	new BlinkWidget(4,
		[=]() {w->setStyleSheet("background-color: lightblue");},
		[=]() {w->setStyleSheet("");}, w
	);
}

QChar FocusableMenu::shortcutKeyOf(const QString& string) {
	int pos = string.indexOf('&');
	if (pos < 0 || pos >= string.length()-1) {
		return 0;
	}
	return string.at(pos+1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

class DialogItems {
public:
	DialogItems(QDialog *widget, FocusableMenu* menu) : m_widget(widget), m_menu(menu) {}
	DialogItems() {}

	FocusableMenu* m_menu;
	QDialog* m_widget;
};

static QStack<DialogItems> dialogWidgets;
static const QKeySequence ks_empty = QKeySequence("");
static const QKeySequence ks_enter = QKeySequence("Enter");
static const QKeySequence ks_return = QKeySequence("Return");
static const QKeySequence ks_escape = QKeySequence("Escape");

bool FocusableMenu::eventFilter(QObject *target, QEvent *ev) {
	// This is an application-wide event filter.
	// If the user toggles Key_Alt, bring up the menu at the top of the dialog stack
	// N.B. This will fail if you use it with a dialog that is NOT modal!
	static ulong lastTimestamp;
	if (ev->type() == QEvent::KeyPress) {
		QKeyEvent* kev = static_cast<QKeyEvent*>(ev);
		if(kev->key() == Qt::Key_Alt) {
			// Don't stutter 
			ulong timestamp;
			if (kev->timestamp() == 0) {
				KeyEvent* kev2 = static_cast<KeyEvent*>(kev);
				timestamp = kev2->sequence();
			} else {
				timestamp = kev->timestamp();
			}
			if (timestamp == lastTimestamp) {
				return false;
			}
			if (dialogWidgets.size() > 0) {
				DialogItems& top = dialogWidgets.top();
				Q_ASSERT(top.m_widget != nullptr); // should not happen
				if (top.m_widget->isVisible()) {
					lastTimestamp = timestamp;
					QTimer::singleShot(0, [this, top] {
						// This must be done here (just-in-time) to handle dynamically changing items in the underlying dialog
						QWidget* focusRoot = top.m_widget->property("focusRoot").value<QWidget*>();
						Q_ASSERT(focusRoot != nullptr); // dialog isn't sequenced.
						sequenceFocus(top.m_widget, focusRoot);
						searchForButtons(top.m_widget, top.m_menu);
						Q_ASSERT(this->children().count() > 0); // menu will be empty, thus invisible and useless.
						top.m_menu->relocate();
						top.m_menu->show();
						top.m_menu->setFocus();
						}
					);
				}
			}
		}
	}
	return false;
}

void FocusableMenu::keyPressEvent(QKeyEvent* event) {
	// Qt overrides menu shortcuts to Enter/Return and Escape.
	// (Return is on the main keyboard, Enter on the keypad)
	if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_actionEnter != nullptr) {
		m_actionEnter->trigger();
	}
	if (event->key() == Qt::Key_Escape && m_actionEscape != nullptr) {
		m_actionEscape->trigger();
	}
	QMenu::keyPressEvent(event);
}

void FocusableMenu::relocate() {
	// Position menu next to "Key Access" label, make space for long names as needed
	QWidget* topParent = static_cast<QWidget*>(parent());
	if (topParent == nullptr) {
		return;
	}
	while (dynamic_cast<QMenu*>(topParent) != nullptr) {
		topParent = topParent->parentWidget();
	}
	QPoint menuPos = MAIN->mapToGlobal(topParent->geometry().bottomLeft());

	int available = MAIN->geometry().topRight().x() - menuPos.x();
	QSize size = sizeHint();
	if (available < size.width()) {
		this->move(menuPos - QPoint(size.width()-available,0));
	} else {
		this->move(menuPos);
	}
}

void FocusableMenu::setupDialog(QDialog* activeDialog, FocusableMenu *menu) {
	if (dialogWidgets.size() == 0) {
		QApplication::instance()->installEventFilter(this);
	}
	dialogWidgets.push(DialogItems(activeDialog, menu));
}

void FocusableMenu::cleanupDialog() {
	dialogWidgets.pop();
	if (dialogWidgets.size() == 0) {
		QApplication::instance()->removeEventFilter(this);
	}
}

QDialog *findDialogEditor(QLineEdit*& e) {
	QLineEdit* editorWidget = MAIN->getDialogHost()->findChild<QLineEdit*>("fileNameEdit");
	if (editorWidget == nullptr) {
		return nullptr;
	}
	e = editorWidget;
	QWidget* activeDialog = static_cast<QDialog*>(editorWidget->parentWidget());
	while (activeDialog->parentWidget() != MAIN->getDialogHost()) {
		activeDialog = activeDialog->parentWidget();
	}
	return static_cast<QDialog*>(activeDialog);
}


void FocusableMenu::pollForDialogReady(QTimer& timer, std::function<QDialog*(FocusableMenu*)> getTargetDialog, FocusableMenu* menu) {
	// Wait for the dialog to actually come up
	QDialog* activeDialog = getTargetDialog(menu);
	if (activeDialog == nullptr) {
		// can loop here e.g. when doing export PDF, while waiting for 2nd (files) dialog
		timer.start(500);
		return;
	}
	dialogWidgets.top().m_widget = activeDialog;
}

void FocusableMenu::sequenceFocus(QWidget *root, QWidget* distinguishedNode) {
	// Add sequence numbers in the tab focus list starting at distinguishedNode
	QVariant v;
	v.setValue(distinguishedNode);
	root->setProperty("focusRoot", v);

	int itemCtr = 0;
	QWidget* i = distinguishedNode;
	do {
		i->setProperty("focusPosition", itemCtr);
		i = i->nextInFocusChain();
		itemCtr++;
	} while (i != distinguishedNode);
}

// Find the focusable items in activeDialog and put them into the menu in the order of the focus chain.
// * The layout (from Designer, probably) should have the items focusable (at least tab focus), and the tab order should be reasonable.
// * This works on FileDialog and other dialogs provided by Qt! See addFileDialog() and showFileDialogMenu().
// * For widgets (such as checkboxes) that have intrinsic labels, it uses those.
// * For widgets that have a buddy QLabel, it uses the QLabel text (note, the QLabel is the one with the buddy()
//   member; it deals with that properly.) QLabels without buddys are ignored.
// * QCheckBox is handled specially: clicking it presents a mini-menu to check or uncheck it, so that it can be set
//   to a specific state rather than simply toggling (from unknown to a different unknown).
// * QLineEdit widgets may be created as children of certain widgets such as ComboBoxes and appear in the tab list.
//   These are ignored.
// * Various pushbutton and tab widgets get special treatment to get the text value. See the code.
// * You must call sequenceFocus() before creating the menu so it knows how to handle the (circular) focus list.
// * It tries to do a "best guess" on the text value and uses any & shortcuts in the menu. As an override (or last resort)
//   the accessibleName() value can used for the text: that can (and must!) contain & shortcut letters.
// * Individual items can be renamed (and new shortcuts created) using mapMenuName() if all else fails. Mapping
//   is done after stripping any & characters.
// * Only items with & shortcut letters (after all other manipulations) or with explicit QShortcuts are shown.  Some menus
//   put data in QLabels (etc.), this cause them not to appear in the menu, but you must be sure everything important
//   gets a shortcut letter (&).
// * Invisible items in the dialog are not shown in the menu, and disabled items are disabled in the menu.
// * Note that if the same shortcut character appears twice in the menu, it doesn't work at all (does nothing) and
//   no error is generated. (This is Qt's doing. This may be related to multi-key shortcuts?) This only applies to the visible items
//   in the menu. Two mutually exclusive items could (and do in at least PdfExport settings) use the same shortcut character.
// * Note you can show a given dialog without menu using exec(), but you need execWithMenu() to get the menu to show when pressing Alt.
// * Not all widget types are supported (yet).
// * Note also: Designer will change tab order in surprising ways: always check it after any change to a dialog.

void FocusableMenu::searchForButtons(QWidget *activeDialog, FocusableMenu *menu) {
	// Outer loop
	if (!menu->m_useButtons) {
		return;
	}
	QMap<int, struct info> entries;
	menu->clear();

	QWidget* start = activeDialog->property("focusRoot").value<QWidget*>();
	Q_ASSERT(start != nullptr); //sequenceFocus() was not called!
	activeDialog->show(); // to get isVisible() below right
	
	QWidget *i = start;
	do {
		int pos = i->property("focusPosition").toInt();
		if (QLabel *lab = dynamic_cast<QLabel*>(i)) {
			QWidget* bud = lab->buddy();
			if (bud != nullptr) {
				int bPos = bud->property("focusPosition").toInt();
				QString t = lab->text();
				insertItem(menu, entries, bPos, t, bud);
				entries[pos] = {
					false,
					[pos]{ }};
			}
		} else {
			insertItem(menu, entries, pos, "", i);
		}

		i = i->nextInFocusChain();
	} while (i != start);

    for (auto item : entries) {
		if (!item.secondary) {
			item.action();
		}
	}
	menu->addSeparator();
    for (auto item : entries) {
		if (item.secondary) {
			item.action();
		}
	}
}

bool FocusableMenu::commonActions(QMap<int, struct info>& entries, FocusableMenu* menu, QWidget* item, const QString& title, 
	const QString& defaultText, QString& resultText, QKeySequence& keySeq) {
	int pos = item->property("focusPosition").toInt();
	if (entries.contains(pos) && title == "") {
		// labels always "win"
		return false;
	}
	QVariant showPro = item->property("showInMenu");
	if (showPro.isValid() && !showPro.toBool()) {
		return false;
	}
	resultText = item->accessibleName();
	if (resultText == " !Skip") { // NOTE space
		return false;
	}
	if (resultText == "") {
		resultText = title;
	}
	if (resultText == "") {
		resultText = defaultText;
	}
	if (resultText == "") {
		resultText = "?";
	}
	QString cleanedName = QString(resultText);
	cleanedName.remove(QChar('&'), Qt::CaseSensitive);
	QMap<QString, FocusableMenu::KeyPair>::const_iterator i = menu->m_titleMap.find(resultText);
	if (i != menu->m_titleMap.end()) {
		resultText = i.value().n;
		keySeq = i.value().k;
	}
	if (resultText.length() > 1 && resultText.indexOf('&') < 0 && keySeq == ks_empty) {
		// No & or explicit shortcut, it won't work from a key. This happens in (at least) QFontDialog with stuff we don't want in the menu.
		// (No empty text items (here "?") allowed; QTabWidget should only have items suitable for a menu.)
		return false;
	}
	return true;
}

void FocusableMenu::insertItem(FocusableMenu *menu, QMap<int, struct info>& entries, int pos, const QString& title, QWidget *item) {
	QKeySequence k;
	QString t;
	if (QPushButton* b = dynamic_cast<QPushButton*>(item)) {
		if (!commonActions(entries, menu, b, title, b->text(), t, k)) {
			return;
		}
		entries[pos] = {
			k != ks_empty,
			{[this, menu, b, t, k]{
				QAction* a = menu->addAction(t, [this,b] {
					showFocusSet(b);
					b->click();
				});
				if (k != ks_empty) {
					QShortcut* s = new QShortcut(k,menu);
					s->setContext(Qt::WidgetShortcut);
					connect(s, &QShortcut::activated, a, &QAction::trigger);
					if (k == ks_return || k == ks_enter) {
						menu->m_actionEnter = a;
					}
					if (k == ks_escape) {
						menu->m_actionEscape = a;
					}
				}
				a->setEnabled(b->isEnabled());
				a->setVisible(b->isVisible() && (b->isEnabled() || menu->m_showDisabled));
				} 
			}
		};
		return;
	}

	if (QCheckBox* c = dynamic_cast<QCheckBox*>(item)) {
		if (!commonActions(entries, menu, c, title, c->text(), t, k)) {
			return;
		}
		entries[pos] = {
			false,
			{[this, menu, c, t]{
				QAction* a = menu->addCheckable(t,c);
				a->setEnabled(c->isEnabled());
				a->setVisible(c->isVisible() && (c->isEnabled() || menu->m_showDisabled));
				}
			}
		};
		return;
	}

	if (QTabWidget *e = dynamic_cast<QTabWidget*>(item)) {
		if (!commonActions(entries, menu, e, title, "", t, k)) {
			return;
		}
		entries[pos] = {
			false,
			{[this, menu, e]{
				for (int i=0; i < e->count(); i++) {
					QString t = e->tabText(i);
					QString cleanedName = QString(t);
					cleanedName.remove(QChar('&'), Qt::CaseSensitive);
					QMap<QString, KeyPair>::const_iterator ii = menu->m_titleMap.find(cleanedName);
					if (ii != menu->m_titleMap.end()) {
						t = ii.value().n;
					}
					QAction* a = menu->addAction(t, [this,e, i] {e->setCurrentIndex(i);});
					a->setEnabled(e->isEnabled());
					a->setVisible(e->isVisible() && (e->isEnabled() || menu->m_showDisabled));
				} 
				}
			}
		};
		return;
	}

	if (QToolButton *f = dynamic_cast<QToolButton*>(item)) {
		QAction* da = f->defaultAction();
		if (da == nullptr) {
			if (!commonActions(entries, menu, f, title, f->text(), t, k)) {
				return;
			}
			entries[pos] = {
				false,
				[this, menu, f, t]{
					QAction* a = menu->addAction(t, [this, f] { f->click(); });
					a->setEnabled(f->isEnabled());
					a->setVisible(f->isVisible() && (f->isEnabled() || menu->m_showDisabled));
				}
			};
			return;
		}
		if (!commonActions(entries, menu, f, title, da->text(), t, k)) {
			return;
		}
		entries[pos] = {
			false,
			[this, menu, da, t]{
				QAction* a = menu->addAction(t, [this, da] { da->trigger(); });
				a->setEnabled(da->isEnabled());
				a->setVisible(da->isVisible() && (da->isEnabled() || menu->m_showDisabled));
			}
		};
		return;
	}

	if (QLineEdit *g = dynamic_cast<QLineEdit*>(item)) {
		QWidget* par = g->parentWidget();
		if (dynamic_cast<QComboBox*>(par) != nullptr || dynamic_cast<QSpinBox*>(par) != nullptr
			|| dynamic_cast<QDoubleSpinBox*>(par) != nullptr) {
			// Combo/spin boxes have a child edit widget that we will ignore
			return;
		}
		if (!commonActions(entries, menu, g, title, g->text(), t, k)) {
			return;
		}
		entries[pos] = {
			false,
			[this, menu, g, t]{
				QAction* a = menu->addAction(t, [this, g] { showFocusSet(g); }); 
				a->setEnabled(g->isEnabled());
				a->setVisible(g->isVisible() && (g->isEnabled() || menu->m_showDisabled));
			}
		};
		return;
	}

	if (title != nullptr || item->accessibleName() != nullptr) {
		// QSpinBox, QComboBox, etc.
		if (!commonActions(entries, menu, item, title, "", t, k)) {
			return;
		}
		entries[pos] = {
			false,
			[this, menu, item, t]{
				QAction* a = menu->addAction(t, [this, item] { showFocusSet(item); }); 
				a->setEnabled(item->isEnabled());
				a->setVisible(item->isVisible() && (item->isEnabled() || menu->m_showDisabled));
			}
		};
		return;
	}
}

void FocusableMenu::showDisabled(bool show) {
	m_showDisabled = show;
}

void FocusableMenu::showInMenu(QWidget *item, bool show) {
	for (QObject* i: item->children()) {
		showInMenu(static_cast<QWidget*>(i), show);
	}
	item->setProperty("showInMenu", show);
}

void FocusableMenu::useButtons() {
	m_useButtons = true;
}

int FocusableMenu::execWithMenu(QDialog *activeDialog) {
	setupDialog(activeDialog, this);
	int result = activeDialog->exec(); // expected to block (modal dialog)
	cleanupDialog();
	return result;
}

QAction* FocusableMenu::addMenu(FocusableMenu* menu, const std::function<void()>& action) {
	QAction* a = new QAction(menu);
	a->setText(menu->title() + "  \t\u27a4");
	QMenu::addAction(a);
	connect(a, &QAction::triggered, this, [this, menu, action](){
		menu->relocate();
		menu->show();
		action();
		menu->setFocus(); // allow action() to set focus if it is also used for non-menu objects
	});
	return a;
}

QAction* FocusableMenu::addMenu(const QString& title) {
	QAction* a = new QAction();
	a->setText(title);
	QWidget::addAction(a);
	return a;
}

QAction* FocusableMenu::addMenu(FocusableMenu* menu) {
	return addMenu(menu, []{});
}

QAction* FocusableMenu::addDialog(const QIcon& icon, const QString&title, const std::function<void()>& action) {
	QAction* a = new QAction();
	if (!icon.isNull()) {
		QMenu::setStyleSheet(
			"QMenu::item {padding: 2px 2px 3px 25px;}" 
			"QMenu::icon {padding: 2px 2px 3px 5px;}" 
			);
		a->setIcon(icon);
	} 
	a->setText(title + "  \t\u27a1");
	QMenu::addAction(a);
	connect(a, &QAction::triggered, this, [this, action](){ action(); });
	return a;
}

QAction* FocusableMenu::addDialog(const QString&title, const std::function<void()>& action) {
	return addDialog(QIcon(), title, action);
}

QAction* FocusableMenu::addDialog(
	// used by addFileDialog
	const QIcon &icon,
	const QString& title, 
	const std::function<bool()>& createDialog,
	const std::function<QDialog*(FocusableMenu *)>& checkDialogForReady) {
	QAction* a = new QAction(title + "  \t\u27a1", this);
	if (!icon.isNull()) {
		QMenu::setStyleSheet(
			"QMenu::item {padding: 2px 2px 3px 25px;}" 
			"QMenu::icon {padding: 2px 2px 3px 5px;}" 
			);
		a->setIcon(icon);
	} 
	QMenu::addAction(a);

	connect(a, &QAction::triggered, this, [this, createDialog, checkDialogForReady](){
		FocusableMenu menu(this);
		QTimer timer;
		// Nothing to connect() to yet (created by createDialog below), and that blocks.
		setupDialog(nullptr, &menu);
		connect(&timer, &QTimer::timeout, this, [this, &menu, &timer, checkDialogForReady] {pollForDialogReady(timer, checkDialogForReady, &menu);} );
		timer.setSingleShot(true);
		timer.start(1);
		bool result = createDialog(); // can block; returns false on failure
		// If createDialog fails, we rely on the destructor for timer to stop the poll loop.
		cleanupDialog();
		return result;

	});
	return a;
}

QAction* FocusableMenu::addDialog(const QString& title, FocusableMenu* menu, QDialog *dialog) {
	return addDialog(QIcon(), title, menu, dialog);
}

QAction* FocusableMenu::addDialog(const QIcon& icon, const QString& title, FocusableMenu* menu, QDialog *dialog) {
	QAction* a = new QAction(title + "  \t\u27a1");
	QMenu::addAction(a);
	if (!icon.isNull()) {
		QMenu::setStyleSheet(
			"QMenu::item {padding: 2px 2px 3px 25px;}" 
			"QMenu::icon {padding: 2px 2px 3px 5px;}" 
			);
		a->setIcon(icon);
	} 
	connect(a, &QAction::triggered, this, [this, menu, dialog](){
		menu->execWithMenu(dialog);
	});
	return a;
}

void FocusableMenu::pollForDialogReadyStatic(QTimer& timer, FocusableMenu* menu) {
	// Wait for the dialog to actually come up.
	QLineEdit* editorWidget;
	QDialog* activeDialog = findDialogEditor(editorWidget);
	if (activeDialog == nullptr) {
		timer.start(500);
		return;
	}
	FocusableMenu::sequenceFocus(activeDialog, editorWidget);
	FocusableMenu::showFocusSet(editorWidget);
	menu->setupDialog(activeDialog, menu);
}

bool FocusableMenu::showFileDialogMenu(FocusableMenu *keyParent, const std::function<bool()>& createDialog) {
	FocusableMenu menu(keyParent);
	QTimer timer;
	menu.useButtons();
	menu.mapButtonBoxDefault();
	menu.mapButtonBoxFiles();
	connect(&timer, &QTimer::timeout, [&menu, &timer] {pollForDialogReadyStatic(timer, &menu);} );
	timer.setSingleShot(true);
	timer.start(1);
	bool result = createDialog(); // blocks
	menu.cleanupDialog();
	// destructor cleans up timer.
	return result;
}

QAction* FocusableMenu::addFileDialog(const QIcon& icon, const QString& title,
	const std::function<bool()>& createDialog) {
	return addDialog(icon, title,
		// Setup Action:
		[createDialog] () {
			return createDialog();
		},

		// Find built dialog, build menu
		[] (FocusableMenu* menu) -> QDialog* {
			QLineEdit* editorWidget;
			QDialog* activeDialog = findDialogEditor(editorWidget);
			if (activeDialog == nullptr) {
				return nullptr;
			}

			sequenceFocus(activeDialog, editorWidget);
			menu->useButtons();
			menu->mapButtonBoxDefault();
			menu->mapButtonBoxFiles();
			menu->relocate();
			FocusableMenu::showFocusSet(editorWidget);
			return activeDialog;
		}
	);
}

QAction* FocusableMenu::addFileDialog(const QString& title, const std::function<bool()>& createDialog) {
	return addFileDialog(QIcon(), title, createDialog);
}

QAction* FocusableMenu::addCheckable(const QString& title, QCheckBox* checkable) {
	QFontMetrics metrics(this->font());
	int textWidth = metrics.horizontalAdvance(title);

	checkable->setFocusPolicy(Qt::NoFocus);
	QAction* a = new QAction(checkable);
	a->setText(title + "\t*");
	QMenu::addAction(a);
	connect(a, &QAction::triggered, this, [this, checkable, a, textWidth](){
	    CheckMenu* checkMenu = new CheckMenu(checkable, this);
		int fillWidth = std::max(0, geometry().width() - textWidth);
		QPoint menuPos = this->mapToGlobal(actionGeometry(a).bottomLeft() + 
			QPoint(textWidth,-2*(actionGeometry(a).height()/3)));
        checkMenu->setMinimumWidth(fillWidth); 
		checkMenu->move(menuPos);
		checkMenu->show();
	});
	return a;
}

QAction* FocusableMenu::addCheckable(const QString& title, QAction* checkable) {
	QFontMetrics metrics(this->font());
	int textWidth = metrics.horizontalAdvance(title);

	QAction* a = new QAction(checkable);
	a->setText(title + "\t*");
	QMenu::addAction(a);
	connect(a, &QAction::triggered, this, [this, checkable, a, textWidth](){
	    CheckMenu* checkMenu = new CheckMenu(checkable, this);
		int fillWidth = std::max(0, geometry().width() - textWidth);
		QPoint menuPos = this->mapToGlobal(actionGeometry(a).bottomLeft() + 
			QPoint(textWidth,-2*(actionGeometry(a).height()/3)));
        checkMenu->setMinimumWidth(fillWidth); 
		checkMenu->move(menuPos);
		checkMenu->show();
	});
	return a;
}

void FocusableMenu::mapMenuName(const QString& oldTitle, const QString& newTitle, const QKeySequence& key) {
	QString t = oldTitle;
	t.remove(QChar('&'), Qt::CaseSensitive);
	m_titleMap[t] = KeyPair{newTitle, key};
}

void FocusableMenu::mapMenuName(const QString& oldTitle, const QString& newTitle) {
	QChar p = shortcutKeyOf(newTitle);
	if (p != 0) {
		mapMenuName(oldTitle, newTitle, QKeySequence(p));
	} else {
		mapMenuName(oldTitle, newTitle, ks_empty);
	}
}

void FocusableMenu::mapButtonBoxDefault() {
	// Attempts to shortcut <Enter> and <Esc> are ignored by Qt - see KeyPressed
	mapMenuName(_("OK"), _("(OK <Enter>)"), ks_return);
	mapMenuName(_("Cancel"), _("(Cancel <Esc>)"), ks_escape);
	mapMenuName(_("Apply"), _("(Apply <Enter>)"), ks_return);
	mapMenuName(_("Close"), _("(Close <Esc>)"), ks_escape);
}

void FocusableMenu::mapButtonBoxFiles() {
	mapMenuName(_("Choose"), _("&Choose"));
	mapMenuName(_("Directory:"), _("&Directory:"));
	mapMenuName(_("Back"), _("&Back"));
	mapMenuName(_("Forward"), _("Fo&rward"));
	mapMenuName(_("Parent Directory"), _("&Parent Directory"));
	mapMenuName(_("Create New Folder"), _("Create Ne&w Folder"));
	mapMenuName(_("List View"), _("&List View"));
	mapMenuName(_("Detail View"), _("Detail &View"));
	mapMenuName(_("Sidebar"), _("Side&bar"));
	mapMenuName(_("Files"), _("&Files"));
}

FocusableMenu::FocusableMenu(QWidget* parent) : FocusableMenu("untitled FocusableMenu", parent) { }

FocusableMenu::FocusableMenu(const QString& title, QWidget* parent) : QMenu(title, parent) {
	setObjectName(title);
	connect(this, &QMenu::aboutToShow, this, [this] {
			Q_ASSERT(this->children().count() > 0);
			relocate();
			setFocus();
			}
		 );

	setStyleSheet( "QMenu::item {padding: 2px 2px 3px 5px;}" ); 
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

KeyDialogButtonBox::KeyDialogButtonBox(KeyDialogButtonBox::StandardButtons buttons, Qt::Orientation orientation, QWidget *parent) :
	QDialogButtonBox(buttons, orientation, parent) {
	fixStandardButtons(buttons);
}
KeyDialogButtonBox::KeyDialogButtonBox(KeyDialogButtonBox::StandardButtons buttons, QWidget *parent) :
	QDialogButtonBox(buttons, parent) {
	fixStandardButtons(buttons);
}
KeyDialogButtonBox::KeyDialogButtonBox(Qt::Orientation orientation, QWidget *parent) :
	QDialogButtonBox(orientation, parent) {
}
KeyDialogButtonBox::KeyDialogButtonBox(QWidget *parent) :
	QDialogButtonBox(parent) {
}

void KeyDialogButtonBox::setStandardButtons(QDialogButtonBox::StandardButtons buttons) {
	QDialogButtonBox::setStandardButtons(buttons);
	fixStandardButtons(buttons);
}

void KeyDialogButtonBox::fixStandardButtons(QDialogButtonBox::StandardButtons buttons) {
	if ((buttons & QDialogButtonBox::Close) != 0) {
		QPushButton *button = QDialogButtonBox::button(QDialogButtonBox::Close);
		button->setText(_("&Close"));
	}
	if ((buttons & QDialogButtonBox::Apply) != 0) {
		QPushButton *button = QDialogButtonBox::button(QDialogButtonBox::Apply);
		button->setText(_("&Apply"));
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////

KeyMessageBox::KeyMessageBox(KeyMessageBox::Icon icon, const QString &title, 
				const QString &text, QMessageBox::StandardButtons buttons, 
				QWidget *parent, Qt::WindowFlags f) :
	QMessageBox(icon, title, text, buttons, parent, f) {
	fixStandardButtons(buttons);
}

KeyMessageBox::KeyMessageBox(QWidget *parent) :
	QMessageBox(parent = nullptr) {
}

KeyMessageBox::StandardButton KeyMessageBox::critical(QWidget *parent, const QString &title, const QString &text, 
		KeyMessageBox::StandardButtons buttons, KeyMessageBox::StandardButton defaultButton) {
	return showNewMessageBox(parent, Critical, title, text, buttons, defaultButton);
}
KeyMessageBox::StandardButton KeyMessageBox::information(QWidget *parent, const QString &title, const QString &text, 
		KeyMessageBox::StandardButtons buttons, KeyMessageBox::StandardButton defaultButton) {
	return showNewMessageBox(parent, Information, title, text, buttons, defaultButton);
}
KeyMessageBox::StandardButton KeyMessageBox::question(QWidget *parent, const QString &title, const QString &text, 
		KeyMessageBox::StandardButtons buttons, KeyMessageBox::StandardButton defaultButton) {
	return showNewMessageBox(parent, Question, title, text, buttons, defaultButton);
}
KeyMessageBox::StandardButton KeyMessageBox::warning(QWidget *parent, const QString &title, const QString &text, 
		KeyMessageBox::StandardButtons buttons, KeyMessageBox::StandardButton defaultButton) {
	return showNewMessageBox(parent, Warning, title, text, buttons, defaultButton);
}

void KeyMessageBox::setStandardButtons(KeyMessageBox::StandardButtons buttons) {
	QMessageBox::setStandardButtons(buttons);
	fixStandardButtons(buttons);
}

void KeyMessageBox::fixStandardButtons(KeyMessageBox::StandardButtons buttons) {
	if ((buttons & KeyMessageBox::Save) != 0) {
		QAbstractButton *button = KeyMessageBox::button(KeyMessageBox::Save);
		button->setText(_("&Save"));
	}
	if ((buttons & KeyMessageBox::Discard) != 0) {
		QAbstractButton *button = KeyMessageBox::button(KeyMessageBox::Discard);
		button->setText(_("&Discard"));
	}
	if ((buttons & KeyMessageBox::Cancel) != 0) {
		QAbstractButton *button = KeyMessageBox::button(KeyMessageBox::Cancel);
		button->setText(_("&Cancel"));
	}
}

// Adapted from QMessageBox
KeyMessageBox::StandardButton KeyMessageBox::showNewMessageBox(QWidget *parent,
    KeyMessageBox::Icon icon,
    const QString& title, const QString& text,
    KeyMessageBox::StandardButtons buttons,
    KeyMessageBox::StandardButton defaultButton)
{
    KeyMessageBox msgBox(icon, title, text, KeyMessageBox::NoButton, parent);
    QDialogButtonBox *buttonBox = msgBox.findChild<QDialogButtonBox*>();
    Q_ASSERT(buttonBox != 0);
    uint mask = KeyMessageBox::FirstButton;
    while (mask <= KeyMessageBox::LastButton) {
        uint sb = buttons & mask;
        mask <<= 1;
        if (!sb)
            continue;
        QPushButton *button = msgBox.addButton((KeyMessageBox::StandardButton)sb);
        // Choose the first accept role as the default
        if (msgBox.defaultButton())
            continue;
        if ((defaultButton == KeyMessageBox::NoButton && buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
            || (defaultButton != KeyMessageBox::NoButton && sb == uint(defaultButton)))
            msgBox.setDefaultButton(button);
    }
	msgBox.fixStandardButtons(buttons);
    if (msgBox.exec() == -1)
        return KeyMessageBox::Cancel;
    return msgBox.standardButton(msgBox.clickedButton());
}