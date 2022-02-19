/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Ui_OutputEditorText.cc
 * Copyright (C) 2013-2020 Sandro Mani <manisandro@gmail.com>
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

#include "common.hh"
#include "OutputTextEdit.hh"
#include "SearchReplaceFrame.hh"
#include "Ui_OutputEditorText.hh"
#include "ui_OutputPostprocDialog.h"
#include "MainWindow.hh"
#include "UiUtils.hh"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

void UI_OutputEditorText::setupUi(QWidget* widget, FocusableMenu* keyParent) {
	widget->setLayout(new QVBoxLayout());
	widget->layout()->setContentsMargins(0, 0, 0, 0);
	widget->layout()->setSpacing(0);

	// Open
	toolButtonOpen = new QToolButton(widget);
	toolButtonOpen->setIcon(QIcon::fromTheme("document-open"));
	toolButtonOpen->setToolTip(gettext("Open"));
	toolButtonOpen->setPopupMode(QToolButton::MenuButtonPopup);

	// Output insert mode
	actionOutputModeAppend = new QAction(QIcon(":/icons/ins_append"), gettext("Append to current text"), widget);
	actionOutputModeCursor = new QAction(QIcon(":/icons/ins_cursor"), gettext("Insert at cursor"), widget);
	actionOutputModeReplace = new QAction(QIcon(":/icons/ins_replace"), gettext("Replace current text"), widget);

	menuOutputMode = new QMenu(widget);
	menuOutputMode->addAction(actionOutputModeAppend);
	menuOutputMode->addAction(actionOutputModeCursor);
	menuOutputMode->addAction(actionOutputModeReplace);

	postprocDialog = new QDialog(MAIN);
	postprocDialog->setModal(true);
	postprocDialogUi.setupUi(postprocDialog);
	FocusableMenu::sequenceFocus(postprocDialog, postprocDialogUi.checkBox_KeepEndMark);

	// Output toolbar
	toolButtonOutputMode = new QToolButton(widget);
	toolButtonOutputMode->setIcon(QIcon(":/icons/ins_append"));
	toolButtonOutputMode->setToolTip(gettext("Select insert mode"));
	toolButtonOutputMode->setPopupMode(QToolButton::InstantPopup);
	toolButtonOutputMode->setMenu(menuOutputMode);

	toolButtonOutputPostproc = new QToolButton(widget);
	toolButtonOutputPostproc->setIcon(QIcon(":/icons/stripcrlf"));
	toolButtonOutputPostproc->setText(gettext("Strip Line Breaks"));
	toolButtonOutputPostproc->setToolTip(gettext("Strip line breaks on selected text"));
	toolButtonOutputPostproc->setPopupMode(QToolButton::MenuButtonPopup);

	actionOutputReplace = new QAction(QIcon::fromTheme("edit-find-replace"), gettext("Find and Replace"), widget);
	actionOutputReplace->setToolTip(gettext("Find and replace"));
	actionOutputReplace->setCheckable(true);
	actionOutputUndo = new QAction(QIcon::fromTheme("edit-undo"), gettext("Undo"), widget);
	actionOutputUndo->setToolTip(gettext("Undo"));
	actionOutputUndo->setEnabled(false);
	actionOutputRedo = new QAction(QIcon::fromTheme("edit-redo"), gettext("Redo"), widget);
	actionOutputRedo->setToolTip(gettext("Redo"));
	actionOutputRedo->setEnabled(false);
	actionOutputSave = new QAction(QIcon::fromTheme("document-save-as"), gettext("Save Output"), widget);
	actionOutputSave->setToolTip(gettext("Save output"));
	actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear Output"), widget);
	actionOutputClear->setToolTip(gettext("Clear output"));

	toolBarOutput = new QToolBar(widget);
	toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
	toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
	toolBarOutput->addWidget(toolButtonOpen);
	toolBarOutput->addAction(actionOutputSave);
	toolBarOutput->addWidget(toolButtonOutputMode);
	toolBarOutput->addWidget(toolButtonOutputPostproc);
	toolBarOutput->addAction(actionOutputReplace);
	toolBarOutput->addAction(actionOutputUndo);
	toolBarOutput->addAction(actionOutputRedo);
	toolBarOutput->addAction(actionOutputClear);

	widget->layout()->addWidget(toolBarOutput);

	searchFrame = new SearchReplaceFrame(keyParent, widget);
	searchFrame->setVisible(false);
	widget->layout()->addWidget(searchFrame);

	toolButtonAddTab = new QToolButton();
	toolButtonAddTab->setIcon(QIcon::fromTheme("list-add"));
	toolButtonAddTab->setToolTip(gettext("Add tab"));
	toolButtonAddTab->setAutoRaise(true);

	tabWidget = new QTabWidget(widget);
	tabWidget->setTabsClosable(true);
	tabWidget->setCornerWidget(toolButtonAddTab);
	widget->layout()->addWidget(tabWidget);
}