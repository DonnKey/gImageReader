/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * KeyMapManager.cc
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

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMessageBox>
#include <QStyledItemDelegate>

#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "OutputTextEdit.hh"
#include "KeyMapManager.hh"
#include "Utils.hh"

Qt::Key toKeyCode(QString codeString);
Qt::Key toKeyCode(QString codeString, int &pos);
KeyString* toKeyString(QString codeString);
KeyString* toKeyString(QString codeString, int &pos);
QString fromKeyString(KeyString* keyString);
QString fromKeyCode(Qt::Key keyCode);
const int modifierMask = (Qt::ShiftModifier|Qt::ControlModifier|Qt::KeypadModifier|Qt::AltModifier|Qt::MetaModifier);

typedef QMap<Qt::Key, KeyString*> KeyMap;
KeyMap keyMap;
bool KeyMapManager::m_awaitingFinish;

static const Qt::Key Key_ERROR = Qt::Key(-1);
static const Qt::Key Key_Delay = Qt::Key(-2); // keep at -2
static const Qt::Key Key_Slow = Qt::Key(-3);
static const Qt::Key Key_Mouse = Qt::Key(-4);
static const Qt::Key Key_Note = Qt::Key(-5);
static const Qt::Key Key_Call = Qt::Key(-6);
static const Qt::Key Key_Wait = Qt::Key(-7);
static const Qt::Key Key_Start = Qt::Key(-8); // keep at most negative
static const QMap<QString, Qt::Key> specialNames = {
	{"BS",			Qt::Key_Backspace},
	{"TAB",			Qt::Key_Tab},
	{"ENTER",		Qt::Key_Return},
	{"ESC",			Qt::Key_Escape},
	{"SPACE",		Qt::Key_Space},
	{"LT",			Qt::Key_Less},
	{"BSLASH",		Qt::Key_Backslash},
	{"DEL",			Qt::Key_Delete},
	{"UP",			Qt::Key_Up},
	{"DOWN",		Qt::Key_Down},
	{"LEFT",		Qt::Key_Left},
	{"RIGHT",		Qt::Key_Right},
	{"F1",			Qt::Key_F1},
	{"F2",			Qt::Key_F2},
	{"F3",			Qt::Key_F3},
	{"F4",			Qt::Key_F4},
	{"F5",			Qt::Key_F5},
	{"F6",			Qt::Key_F6},
	{"F7",			Qt::Key_F7},
	{"F8",			Qt::Key_F8},
	{"F9",			Qt::Key_F9},
	{"F10",			Qt::Key_F10},
	{"F11",			Qt::Key_F11},
	{"F12",			Qt::Key_F12},
	{"INSERT",		Qt::Key_Insert},
	{"HOME",		Qt::Key_Home},
	{"END",			Qt::Key_End},
	{"PAGEUP",		Qt::Key_PageUp},
	{"PAGEDOWN",	Qt::Key_PageDown},
	{"PAUSE",		Qt::Key_Pause},
	{"SCROLLLOCK",	Qt::Key_ScrollLock},
	{"ALT",			Qt::Key_Alt},
	{"CTRL",		Qt::Key_Control},
	{"SHIFT",		Qt::Key_Shift},
	{"META",		Qt::Key_Meta},
	{"DELAY",		Key_Delay},
	{"SLOW",		Key_Slow},
	{"MOUSE",		Key_Mouse},
	{"NOTE",		Key_Note},
	{"START",		Key_Start},
	{"CALL",		Key_Call},
	{"WAIT",		Key_Wait},
};

static const QMap<Qt::Key, QString> specialKeys = {
	{Qt::Key_Backspace,		"BS"},
	{Qt::Key_Tab,			"Tab"},
	{Qt::Key_Return,		"Enter"},
	{Qt::Key_Escape,		"Esc"},
	{Qt::Key_Space,			"Space"},
	{Qt::Key_Less,			"Lt"},
	{Qt::Key_Backslash,		"BSlash"},
	{Qt::Key_Delete,		"Del"},
	{Qt::Key_Up,			"Up"},
	{Qt::Key_Down,			"Down"},
	{Qt::Key_Left,			"Left"},
	{Qt::Key_Right,			"Right"},
	{Qt::Key_F1,			"F1"},
	{Qt::Key_F2,			"F2"},
	{Qt::Key_F3,			"F3"},
	{Qt::Key_F4,			"F4"},
	{Qt::Key_F5,			"F5"},
	{Qt::Key_F6,			"F6"},
	{Qt::Key_F7,			"F7"},
	{Qt::Key_F8,			"F8"},
	{Qt::Key_F9,			"F9"},
	{Qt::Key_F10,			"F10"},
	{Qt::Key_F11,			"F11"},
	{Qt::Key_F12,			"F12"},
	{Qt::Key_Insert,		"Insert"},
	{Qt::Key_Home,			"Home"},
	{Qt::Key_End,			"End"},
	{Qt::Key_PageUp,		"PageUp"},
	{Qt::Key_PageDown,		"PageDown"},
	{Qt::Key_Pause,			"Pause"},
	{Qt::Key_ScrollLock,	"ScrollLock"},
	{Qt::Key_Alt,			"Alt"},
	{Qt::Key_Control,		"Ctrl"},
	{Qt::Key_Shift,			"Shift"},
	{Qt::Key_Meta,			"Meta"},
	{Key_Delay,				"Delay"},
	{Key_Slow,				"Slow"},
	{Key_Mouse,				"Mouse"},
	{Key_Note,				"Note"},
	{Key_Start,				"Start"},
	{Key_Call,				"Call"},
	{Key_Wait,				"Wait"},
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class KeyStringValidator : public QValidator {
public:
	KeyStringValidator(bool single, QLineEdit* parent = nullptr) : QValidator(parent) {
		m_currentEditor = parent;
		m_single = single;
	}

	QValidator::State validate(QString &str, int& pos) const override {
		int newPos = 0;

		QValidator::State state;
		if (m_single) {
			Qt::Key keyCode = toKeyCode(str, newPos);
			state = keyCode == Key_ERROR || str.length() > newPos ? Intermediate : Acceptable;
		} else {
			KeyString* keyString = toKeyString(str, newPos);
			state = keyString == nullptr ? Intermediate : Acceptable;
		}
		if (state == Intermediate) {
			int edPos = m_currentEditor->cursorPosition();
			if (newPos > edPos) {
				std::swap(newPos, edPos);
			}
			if (edPos == newPos) {
				newPos -= 1;
			}
			setHighlight(newPos, edPos);
		} else {
			setHighlight(0,0);
		}
		static_cast<KeyMapManager*>(parent()->parent()->parent()->parent())->showCloseButton(state == Acceptable);
		return state;
	}

	void setHighlight(int start, int end) const {
		QTextCharFormat fmt;
		fmt.setBackground(Qt::yellow);
		QList<QInputMethodEvent::Attribute> attributes;
		int len = end-start;
		start = start - m_currentEditor->cursorPosition();
		attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, start, len, QVariant(fmt)));
		QInputMethodEvent event(QString(), attributes);
		QCoreApplication::sendEvent(m_currentEditor, &event);
	}

private:
	QLineEdit* m_currentEditor;
	bool m_single;
};

class KeyMapDelegate : public QStyledItemDelegate {
public:
	KeyMapDelegate(QTableWidget* parent, bool single) {
		QStyledItemDelegate(static_cast<QObject*>(parent));
		m_single = single;
	}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /* option */, const QModelIndex& /* index */) const override {
		QLineEdit* editor = new QLineEdit(parent);
		editor->setAttribute(Qt::WA_InputMethodEnabled);
		editor->setValidator(new KeyStringValidator(m_single, editor));
		return editor;
	}

	void setEditorData(QWidget* ed, const QModelIndex& index) const override {
		QLineEdit *editor = static_cast<QLineEdit*>(ed);
		editor->setText(index.model()->data(index, Qt::EditRole).toString());
	}
private:
	mutable bool m_single;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

KeyMapManager::KeyMapManager(QWidget* parent)
	: QDialog(parent) {
	setModal(true);
	setWindowTitle(_("KeyMap"));

	QAction* openAction = new QAction(QIcon::fromTheme("document-open"), _("Open"), this);
	openAction->setToolTip(_("Open"));

	QAction* openAppendAction = new QAction(QIcon(":/icons/open-append"), _("Open Append"), this);
	openAppendAction->setToolTip(_("Open (append)"));

	QAction* saveAction = new QAction(QIcon::fromTheme("document-save"), _("Save"), this);
	saveAction->setToolTip(_("Save"));

	QAction* clearAction = new QAction(QIcon::fromTheme("edit-clear"), _("Clear"), this);
	clearAction->setToolTip(_("Clear"));

	QAction* addAction = new QAction(QIcon::fromTheme("list-add"), _("Add"), this);
	addAction->setToolTip(_("Add"));

	m_removeAction = new QAction(QIcon::fromTheme("list-remove"), _("Remove"), this);
	m_removeAction->setToolTip(_("Remove"));
	m_removeAction->setEnabled(false);

	QAction* sortAction = new QAction(QIcon::fromTheme("view-sort-ascending"), _("Sort"), this);
	sortAction->setToolTip(_("Sort"));

	QLabel* help = new QLabel(this);
	QIcon ic = QIcon::fromTheme("help-hint");
	help->setPixmap(ic.pixmap(16,16));
	help->setToolTip(_("<html><head/><body><p>Map a single key to a key sequence to be used when the single key is typed.  "
	"It may contain data and multiple actions."
	"<br>Keys can be printable characters or special keys. "
	"<br>&lt;C-A&gt; for Ctrl-A or &lt;K-Del&gt; for keypad <em>Del</em> (S,C,A,M,K). "
	"<br>&lt;Delay&gt; is 500ms delay. "
	"<br>&lt;Slow&gt; for debugging. "
	"<br>&lt;Start&gt; for once on startup. Hold &lt;Alt&gt; to skip."
	"<br>&lt;Mouse&gt; await one mouse click/drag. "
	"<br>&lt;Wait&gt; Busy wait (certain tasks only). "
	"<br>&lt;Note&gt; ignore rest of string."
	"</p></body></html>"));

	QWidget* spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QToolBar* toolbar = new QToolBar(this);
	toolbar->setIconSize(QSize(1, 1) * toolbar->style()->pixelMetric(QStyle::PM_SmallIconSize));
	toolbar->addAction(openAction);
	toolbar->addAction(openAppendAction);
	toolbar->addAction(saveAction);
	toolbar->addAction(clearAction);
	toolbar->addSeparator();
	toolbar->addAction(addAction);
	toolbar->addAction(m_removeAction);
	toolbar->addAction(sortAction);
	toolbar->addWidget(spacer);
	toolbar->addWidget(help);

	m_tableWidget = new QTableWidget(0, 2, this);
	m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_tableWidget->setEditTriggers(QAbstractItemView::CurrentChanged);
	m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	m_tableWidget->horizontalHeader()->setVisible(true);
	m_tableWidget->verticalHeader()->setVisible(false);
	m_tableWidget->setHorizontalHeaderLabels(QStringList() << _("Key") << _("Action"));

	m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);

	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setMargin(4);
	layout->addWidget(toolbar);
	layout->addWidget(m_tableWidget);
	layout->addWidget(m_buttonBox);

	setLayout(layout);
	setFixedWidth(800);

	connect(openAction, &QAction::triggered, this, [this] () {openList(false);});
	connect(openAppendAction, &QAction::triggered, this, [this] () {openList(true);});
	connect(saveAction, &QAction::triggered, this, &KeyMapManager::saveList);
	connect(clearAction, &QAction::triggered, this, &KeyMapManager::clearList);
	connect(addAction, &QAction::triggered, this, &KeyMapManager::addRow);
	connect(sortAction, &QAction::triggered, this, &KeyMapManager::sortTable);
	connect(m_buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &KeyMapManager::hide);
	connect(m_removeAction, &QAction::triggered, this, &KeyMapManager::removeRows);
	connect(m_tableWidget->selectionModel(), &QItemSelectionModel::selectionChanged, this, &KeyMapManager::onTableSelectionChanged);
	connect(m_tableWidget, &QTableWidget::cellChanged, this, &KeyMapManager::tableCellChanged);

	m_tableWidget->setItemDelegateForColumn(0, new KeyMapDelegate(m_tableWidget, true));
	m_tableWidget->setItemDelegateForColumn(1, new KeyMapDelegate(m_tableWidget, false));

	ADD_SETTING(TableSetting("keymap", m_tableWidget));
	refreshKeyMap();

	QApplication::instance()->installEventFilter(this);

	m_currentKeys = nullptr;

	QTimer::singleShot(1000, [this] {
		startupSending();
	});
}

void KeyMapManager::showCloseButton(bool show) {
	m_buttonBox->setEnabled(show);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void KeyMapManager::sendOnePress() {
	// Be sure to target the current widget!
	QWidget* target = static_cast<QWidget*>(QApplication::focusWidget());
	if (target==nullptr) {
		QTimer::singleShot(m_interval, [this] {
			sendOnePress();
		});
		return;
	}
	Qt::Key keyCode = m_currentKeys->at(m_currentPosition);
	switch((int)keyCode) {
	case Key_Slow:
		m_interval = 1000;
		m_currentPosition += 1;
		keyCode = m_currentKeys->at(m_currentPosition);
		break;
	case Key_Delay:
		m_currentPosition += 1;
		QTimer::singleShot(500, [this] {
			sendOnePress();
		});
		return;
	case Key_Mouse:
		m_mousePosted = false;
		QTimer::singleShot(500, [this] {
			awaitMouseUp();
		});
		return;
	case Key_Wait:
		QTimer::singleShot(500, [this] {
			awaitWaitable();
		});
		return;
	case Key_Call:
		keyCode = m_currentKeys->at(m_currentPosition+1);
		m_currentPosition += 2;
		m_keyStack.push(KeyStackEntry(m_currentKeys, m_currentPosition));
		startSending(keyCode);
		return;
	case Key_Note:
		// Just ignore everything else
		return;
	}
	Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers(keyCode & modifierMask);
	keyCode = Qt::Key(keyCode & ~modifierMask); 

	if (modifiers && Qt::AltModifier != 0) {
		// Sending a character with the alt modifier directly doesn't work if a
		// menu needs to be brought up. Give it time to bring up the menu.
		QTimer::singleShot(m_interval/4, [this] {
			sendAlt();
		});
		return;
	}

	QTimer::singleShot(m_interval, [this] {
		sendOneCharacter();
	});
}

void KeyMapManager::sendAlt() {
	// Be sure to target the current widget!
	QWidget* target = static_cast<QWidget*>(QApplication::focusWidget());

	QKeyEvent * eva = new KeyEvent (QEvent::KeyPress, Qt::Key_Alt, Qt::AltModifier, QString(""));
	qApp->postEvent(target,(QEvent *)eva);

	QTimer::singleShot(m_interval, [this] {
		sendOneCharacter();
	});
}

void KeyMapManager::sendOneCharacter() {
	// Be sure to target the current widget!
	QWidget* target = static_cast<QWidget*>(QApplication::focusWidget());
	Qt::Key keyCode = m_currentKeys->at(m_currentPosition);
	Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers(keyCode & modifierMask);
	keyCode = Qt::Key(keyCode & ~modifierMask); 

	QKeyEvent * ev1 = new KeyEvent (QEvent::KeyPress, keyCode, modifiers, QString(QChar(keyCode).unicode()));
	qApp->postEvent(target,(QEvent *)ev1);

	QTimer::singleShot(m_interval, [this] {
		sendOneRelease();
	});
}

void KeyMapManager::sendOneRelease() {
	QWidget* target = static_cast<QWidget*>(QApplication::focusWidget());
	if (target==nullptr) {
		QTimer::singleShot(m_interval, [this] {
			sendOneRelease();
		});
		return;
	}

	Qt::Key keyCode = m_currentKeys->at(m_currentPosition);
	Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers(keyCode & modifierMask);
	keyCode = Qt::Key(keyCode & ~modifierMask); 

	QKeyEvent * ev2 = new KeyEvent(QEvent::KeyRelease, keyCode, modifiers, QString(QChar(keyCode).unicode()));
	qApp->postEvent(target,(QEvent *)ev2);
	
	m_posted = false;
	QTimer::singleShot(m_interval, [this] {
		awaitOneRelease();
	});
	return;
}

bool KeyMapManager::advance() {
 	m_currentPosition++;
 	while (m_currentPosition >= m_currentKeys->length() || m_currentKeys->at(m_currentPosition) == Key_Note) {
 		if (m_keyStack.size() == 0) {
 			// Done!
 			m_currentKeys = nullptr;
			return false;
 		}
 		m_currentPosition = m_keyStack.top().m_pos;
 		m_currentKeys = m_keyStack.top().m_string;
 		m_keyStack.pop();
 	}
	return true;
}

void KeyMapManager::awaitOneRelease() {
	if (!m_posted) {
		QTimer::singleShot(m_interval, [this] {
			awaitOneRelease();
		});
		return;
	}
	if (!advance()) {
		return;
	}

	QTimer::singleShot(m_interval, [this] {
		sendOnePress();
	});
}

void KeyMapManager::awaitMouseUp() {
	if (!m_mousePosted) {
		QTimer::singleShot(500, [this] {
			awaitMouseUp();
		});
		return;
	}

	if (!advance()) {
		return;
	}

	QTimer::singleShot(m_interval, [this] {
		sendOnePress();
	});
}

void KeyMapManager::awaitWaitable() {
	if (m_awaitingFinish) {
		QTimer::singleShot(500, [this] {
			awaitWaitable();
		});
		return;
	}

	if (!advance()) {
		return;
	}

	QTimer::singleShot(m_interval, [this] {
		sendOnePress();
	});
}

void KeyMapManager::waitableStarted() {
	m_awaitingFinish = true;
}

void KeyMapManager::waitableDone() {
	m_awaitingFinish = false;
}

bool KeyMapManager::eventFilter(QObject* target, QEvent* ev) {
	if(ev->type() == QEvent::MouseButtonRelease) {
		m_mousePosted = true;
		return false;
	}
	if(ev->type() == QEvent::KeyRelease) {
		if (dynamic_cast<KeyEvent*>(ev) != nullptr) {
			m_posted = true;
			return false;
		}
	}
	if(ev->type() != QEvent::KeyPress) {
		return false;
	}
	if (dynamic_cast<KeyEvent*>(ev) != nullptr) {
		// Skip this if it's our own "typing"!
		return false;
	}
	if (m_currentKeys != nullptr) {
		// Ignore real keys while "typing"
		return true;
	}
	
	// A real key to expand
	QKeyEvent* kev = static_cast<QKeyEvent*>(ev);

	if (kev->timestamp() == m_lastTimestamp) {
		// but don't stutter
		return false;
	}
	m_lastTimestamp = kev->timestamp();
	// Note, Qt won't deliver K-S-<digit> as distinct from K-<digit>
	// (Ctrl and alt work as expected.)
	// That might be worked around by looking at the global modifiers.
	return startSending(Qt::Key(kev->key() | kev->modifiers()));
}

bool KeyMapManager::startSending(Qt::Key keyCode) {
	KeyMap::const_iterator found = keyMap.constFind(keyCode);
	if (found == keyMap.constEnd()) {
		return false;
	} 

	m_interval = 50;
	m_currentKeys = found.value();
	m_currentPosition = 0;
	sendOnePress();
	return true;
}

void KeyMapManager::startupSending() {
	// queryKeyboardModifiers isn't ready during initialization
	if (QApplication::queryKeyboardModifiers() == Qt::NoModifier) {
		startSending(Key_Start);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void KeyMapManager::openList(bool append) {
	QString dir = !m_currentFile.isEmpty() ? QFileInfo(m_currentFile).absolutePath() : "";
	QStringList files;
	if (append) {
		files = FileDialogs::openDialog(_("Open KeyMap (append)"), dir, "auxdir", QString("%1 (*.txt)").arg(_("KeyMap List (append)")), false, this);
	} else {
		if(!clearList()) {
			return;
		}
		files = FileDialogs::openDialog(_("Open KeyMap"), dir, "auxdir", QString("%1 (*.txt)").arg(_("KeyMap List")), false, this);
	}

	if(!files.isEmpty()) {
		QString filename = files.front();
		QFile file(filename);
		if(!file.open(QIODevice::ReadOnly)) {
			QMessageBox::critical(this, _("Error Reading File"), _("Unable to read '%1'.").arg(filename));
			return;
		}
		m_currentFile = filename;

		bool errors = false;
		m_tableWidget->blockSignals(true);
		while(!file.atEnd()) {
			QString line = MAIN->getConfig()->useUtf8() ? QString::fromUtf8(file.readLine()) : QString::fromLocal8Bit(file.readLine());
			line.chop(1);
			if(line.isEmpty()) {
				continue;
			}
			QList<QString> fields = line.split('\t');
			if(fields.size() < 2) {
				errors = true;
				continue;
			}
			int row = m_tableWidget->rowCount();
			m_tableWidget->insertRow(row);
			QTableWidgetItem* i1 = new QTableWidgetItem(fields[0]);
			QTableWidgetItem* i2 = new QTableWidgetItem(fields[1]);
			i1->setFlags(i1->flags() | Qt::ItemIsEditable | Qt::ItemIsEnabled);
			i2->setFlags(i2->flags() | Qt::ItemIsEditable | Qt::ItemIsEnabled);
			m_tableWidget->setItem(row, 0, i1);
			m_tableWidget->setItem(row, 1, i2);
		}
		m_tableWidget->blockSignals(false);
		if(errors) {
			QMessageBox::warning(this, _("Errors Occurred Reading File"), _("Some entries of the key map could not be read."));
		}
	}
}

bool KeyMapManager::saveList() {
	QString filename = FileDialogs::saveDialog(_("Save KeyMap"), m_currentFile, "auxdir", QString("%1 (*.txt)").arg(_("KeyMap")), false, this);
	QString ext = QFileInfo(filename).completeSuffix();
	if (ext.isEmpty()) {
		filename += "txt";
	}
	if(filename.isEmpty()) {
		return false;
	}
	QFile file(filename);
	if(!file.open(QIODevice::WriteOnly)) {
		QMessageBox::critical(this, _("Error Saving File"), _("Unable to write to '%1'.").arg(filename));
		return false;
	}
	m_currentFile = filename;
	for(int row = 0, nRows = m_tableWidget->rowCount(); row < nRows; ++row) {
		QString line = QString("%1\t%2\n").arg(m_tableWidget->item(row, 0)->text()).arg(m_tableWidget->item(row, 1)->text());
		file.write(MAIN->getConfig()->useUtf8() ? line.toUtf8() : line.toLocal8Bit());
	}
	return true;
}

bool KeyMapManager::clearList() {
	if(m_tableWidget->rowCount() > 0) {
		int response = QMessageBox::question(this, _("Save List?"), _("Do you want to save the current list?"), QMessageBox::Save, QMessageBox::Discard, QMessageBox::Cancel);
		if(response == QMessageBox::Save) {
			if(!saveList()) {
				return false;
			}
		} else if(response != QMessageBox::Discard) {
			return false;
		}
		m_tableWidget->setRowCount(0);
	}
	return true;
}

void KeyMapManager::addRow() {
	int row = m_tableWidget->rowCount();
	m_tableWidget->insertRow(row);
	m_tableWidget->setItem(row, 0, new QTableWidgetItem());
	m_tableWidget->setItem(row, 1, new QTableWidgetItem());
	QTableWidgetItem* item = m_tableWidget->item(row,0);
	m_tableWidget->editItem(item);
	m_tableWidget->setCurrentItem(item);
}

void KeyMapManager::removeRows() {
	m_tableWidget->blockSignals(true);
	for(const QModelIndex& index : m_tableWidget->selectionModel()->selectedRows()) {
		m_tableWidget->removeRow(index.row());
	}
	m_tableWidget->blockSignals(false);
	emit m_tableWidget->cellChanged(9999,9999);
}

void KeyMapManager::sortTable() {
	m_tableWidget->blockSignals(true);
	m_tableWidget->sortByColumn(0, Qt::AscendingOrder);
	m_tableWidget->blockSignals(false);
	emit m_tableWidget->cellChanged(9999,9999);
}

void KeyMapManager::onTableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected) {
	m_removeAction->setEnabled(!selected.isEmpty());
}

void KeyMapManager::showEvent(QShowEvent *event) {
	// Bug workaround: forces "clean" edit state - without it clicking on highlighted items ignored
	m_tableWidget->clearSelection();
	if (m_tableWidget->rowCount() > 0) {
		QTableWidgetItem* item = m_tableWidget->item(0,1);
		m_tableWidget->setCurrentItem(item);
		item = m_tableWidget->item(0,0);
		m_tableWidget->setCurrentItem(item);
	}
	QDialog::showEvent(event);
}

void KeyMapManager::hideEvent(QHideEvent *event) {
	refreshKeyMap();
	QDialog::hideEvent(event);
}

void KeyMapManager::refreshKeyMap() const {
	keyMap.clear();
	for(int row = 0, nRows = m_tableWidget->rowCount(); row < nRows; ++row) {
		keyMap.insert(toKeyCode(m_tableWidget->item(row, 0)->text()), toKeyString(m_tableWidget->item(row, 1)->text()));
	}
}

void KeyMapManager::tableCellChanged(int row, int column) {
	if (column > 100) {
		// see remove and sort
		return;
	}
	QTableWidgetItem* item = m_tableWidget->item(row,column);
	QString str = item->text();

	// Save normalized form
	m_tableWidget->blockSignals(true);
	item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsEnabled);
	if (column == 0) {
		Qt::Key keyCode = toKeyCode(str);
		item->setText(fromKeyCode(keyCode));
	} else if (column == 1) {
		KeyString* keyString = toKeyString(str);
		item->setText(fromKeyString(keyString));
	}
	m_tableWidget->blockSignals(false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Qt::Key parseOneKeyCode(QString codeString, /*inout*/ int& pointer) {
	int keyCode = Key_ERROR;
	int last = codeString.length() - 1;

	if (last < 0) {
		return Qt::Key(keyCode);
	}
	if (codeString.at(pointer) != '<') {
		keyCode = Qt::Key(codeString.at(pointer).unicode());
		pointer += 1;
		return Qt::Key(keyCode);
	}

	int modFlags = 0;
	pointer += 1;
	while (true) {
		if (last <= pointer) {
			break;
		}
		if (codeString.at(pointer+1) != '-') {
			break;
		}		
		// invariant: pointing at unknown-hyphen
		switch (codeString.at(pointer).unicode()) {
		case 'C':
		case 'c':
			modFlags |= Qt::ControlModifier;
			break;
		case 'S':
		case 's':
			modFlags |= Qt::ShiftModifier;
			break;
		case 'A':
		case 'a':
			modFlags |= Qt::AltModifier;
			break;
		case 'M':
		case 'm':
			modFlags |= Qt::MetaModifier;
			break;
		case 'K':
		case 'k':
			modFlags |= Qt::KeypadModifier;
			break;
        default: 
			// not a legal letter-hyphen
			return Qt::Key(keyCode);
		}
		pointer += 2;
	}

	// Be sure 2 or more characters remain
	if (pointer >= last) {
		return Qt::Key(keyCode);
	}

	// pointing at unknown-unknown
	if (codeString.at(pointer) != '\\' && codeString.at(pointer+1) == '>') {
		// pointing at unknown (not\), greater: take one character
		QKeySequence seq = QKeySequence(codeString.at(pointer));
		keyCode = seq[0];
		pointer += 2;
	} else if (codeString.at(pointer) == '\\') {
		// pointing at \ something
		if (last < pointer + 2 || codeString.at(pointer+2) != '>') {
			return Qt::Key(keyCode);
		}
		// pointing at \ someting >: take 'something'
		QKeySequence seq = QKeySequence(codeString.at(pointer+1));
		keyCode = seq[0];
		pointer += 3;
	} else {
		// pointing at unknown !greater (and at least 2 chars remain): s.b. special word
		int left = pointer;
		pointer += 2;
		if (pointer > last) {
			return Qt::Key(keyCode);
		}
		while (codeString.at(pointer).isLetterOrNumber()) {
			pointer++;
			if (pointer > last) {
				return Qt::Key(keyCode);
			}
		}
		if (codeString.at(pointer) != '>') {
			return Qt::Key(keyCode);
		}

		QString keyName = codeString.mid(left, pointer-left).toUpper();
		QMap<QString,Qt::Key>::const_iterator found = specialNames.constFind(keyName);
		if (found == specialNames.constEnd()) {
			return Qt::Key(keyCode);
		}
		keyCode = found.value();
		pointer += 1;
	}
	keyCode |= modFlags;
	return Qt::Key(keyCode);
}

Qt::Key toKeyCode(QString codeString, int& pointer) {
	return parseOneKeyCode(codeString, pointer);
}

Qt::Key toKeyCode(QString codeString) {
	int pointer = 0;
	return parseOneKeyCode(codeString, pointer);
}

QString fromKeyCode(Qt::Key keyCode) {
	QString result;
	result.reserve(20);

	if (keyCode == Key_ERROR) {
		return "";
	}
	if (keyCode >= Key_Start && keyCode <= Key_Delay) {
		return "<" + specialKeys[keyCode] + ">";
	}

	int baseCode = keyCode & ~modifierMask;
	QMap<Qt::Key,QString>::const_iterator found = specialKeys.constFind(Qt::Key(baseCode));
	QString keyName;
	if (found != specialKeys.constEnd()) {
		keyName = found.value();
	} else {
		keyName = QKeySequence(baseCode).toString();
	}
	if (keyName.length() == 1 && ((keyCode & modifierMask) == 0)) {
		return QString(QChar(keyCode));
	}
	result = "<";
	if (keyCode & Qt::ShiftModifier) {
		result += "S-";
	}
	if (keyCode & Qt::ControlModifier) {
		result += "C-";
	}
	if (keyCode & Qt::KeypadModifier) {
		result += "K-";
	}
	if (keyCode & Qt::AltModifier) {
		result += "A-";
	}
	if (keyCode & Qt::MetaModifier) {
		result += "M-";
	}
	result += keyName + ">";
	return result;
}

KeyString* toKeyString(QString codeString) {
	int pos;
	return toKeyString(codeString, pos);
}

KeyString* toKeyString(QString codeString, int& pos) {
	int stringPointer = 0;
	KeyString* keyString = new KeyString();
	keyString->reserve(20);
	while (stringPointer < codeString.length()) {
		pos = stringPointer;
		Qt::Key key = parseOneKeyCode(codeString, stringPointer);
		if (key == Key_ERROR) {
			delete keyString;
			return nullptr;
		}
		keyString->append(key);
		if (key == Key_Note) {
			while (stringPointer < codeString.length()) {
				keyString->append(Qt::Key(codeString.at(stringPointer++).unicode()));
			}
			return keyString;
		}
	}
	return keyString;
}

QString fromKeyString(KeyString* keyString) {
	QString codeString;
	codeString.reserve(100);
	for (int stringPointer=0; stringPointer < keyString->length(); stringPointer++) {
		Qt::Key key = keyString->at(stringPointer);
		codeString.append(fromKeyCode(key));
		if (key == Key_Note) {
			stringPointer += 1;
			while (stringPointer < keyString->length()) {
				codeString.append(keyString->at(stringPointer++));
			}
			return codeString;
		}
	}
	return codeString;
}