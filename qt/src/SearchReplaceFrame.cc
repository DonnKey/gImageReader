/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SearchReplaceFrame.cc
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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

#include <QTimer>
#include <QDebug>

#include "ConfigSettings.hh"
#include "SearchReplaceFrame.hh"
#include "SubstitutionsManager.hh"
#include "UiUtils.hh"

SearchReplaceFrame::SearchReplaceFrame(FocusableMenu* keyParent, QWidget* parent, Qt::WindowFlags f)
	: QFrame(parent, f) {
	ui.setupUi(this);

	m_substitutionsManager = new SubstitutionsManager("substitutionslist", keyParent, this);

	connect(ui.checkBoxMatchCase, &QCheckBox::toggled, this, &SearchReplaceFrame::clearErrorState);
	connect(ui.lineEditSearch, &QLineEdit::textChanged, this, &SearchReplaceFrame::clearErrorState);
	connect(ui.lineEditSearch, &QLineEdit::returnPressed, this, &SearchReplaceFrame::findNext);
	connect(ui.lineEditReplace, &QLineEdit::returnPressed, this, &SearchReplaceFrame::replaceNext);
	connect(ui.toolButtonFindNext, &QToolButton::clicked, this, &SearchReplaceFrame::findNext);
	connect(ui.toolButtonFindPrev, &QToolButton::clicked, this, &SearchReplaceFrame::findPrev);
	connect(ui.toolButtonReplace, &QToolButton::clicked, this, &SearchReplaceFrame::replaceNext);
	connect(ui.toolButtonReplaceAll, &QToolButton::clicked, this, &SearchReplaceFrame::emitReplaceAll);
	connect(ui.toolButtonReplaceSel, &QToolButton::clicked, this, &SearchReplaceFrame::emitReplaceInSelected);

	connect(ui.pushButtonSubstitutions, &QPushButton::clicked, this, [this] {m_substitutionsManager->doShow();} );
	connect(m_substitutionsManager, &SubstitutionsManager::applySubstitutions, this, &SearchReplaceFrame::emitApplySubstitutions);

	ADD_SETTING(SwitchSetting("searchmatchcase", ui.checkBoxMatchCase));
}

void SearchReplaceFrame::setKeyMenu(FocusableMenu* menu) {
	menu->addAction(_("Set &search"), this, [this] {FocusableMenu::showFocusSet(ui.lineEditSearch); });
	menu->addAction(_("Set rep&lace"), this, [this] {FocusableMenu::showFocusSet(ui.lineEditReplace); });
	menu->addAction(_("Find &next"), this, &SearchReplaceFrame::findNext);
	menu->addAction(_("Find &previous"), this, &SearchReplaceFrame::findPrev); 
	menu->addAction(_("&Replace next"), this, 
		[this] {
		// When replacing:
		// Using mouse, you click once to find an item and again to replace and move-to-next.
		// From key shortcuts it loses "focus" without the reFocusTree, and having to enter the
		// key twice is pointless when automating, so simply find and replace with one key.
		reFocusTree();
		findReplace(ui.lineEditSearch->text(), ui.lineEditReplace->text(), ui.checkBoxMatchCase->isChecked(), false, false);
		findReplace(ui.lineEditSearch->text(), ui.lineEditReplace->text(), ui.checkBoxMatchCase->isChecked(), false, true);

	}); 
	menu->addAction(_("Replace &all"), this, &SearchReplaceFrame::emitReplaceAll); 
	menu->addAction(_("Replace &selection"), this, &SearchReplaceFrame::emitReplaceInSelected); 
	menu->addCheckable(_("&Match case"), ui.checkBoxMatchCase); 
	menu->addAction(_("S&ubstitutions"), this, [this] {m_substitutionsManager->doShow(); });
}

void SearchReplaceFrame::clear() {
	ui.lineEditSearch->clear();
	ui.lineEditReplace->clear();
}

void SearchReplaceFrame::clearErrorState() {
	ui.lineEditSearch->setStyleSheet("");
}

void SearchReplaceFrame::setFocus() {
	// Focus goes to the search subwindow if focus is to the freame.
	// But allow KeyMapper to keep focus long enough to handle subsequent keys.
	QTimer::singleShot(1000, [this] {
		ui.lineEditSearch->setFocus();
	});
}

void SearchReplaceFrame::setErrorState() {
	ui.lineEditSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
}

void SearchReplaceFrame::hideSubstitutionsManager() {
	m_substitutionsManager->hide();
}
