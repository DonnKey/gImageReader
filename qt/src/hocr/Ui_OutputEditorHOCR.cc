/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Ui_OutputEditorHOCR.cc
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
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QToolButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "Ui_OutputEditorHOCR.hh"
#include "TreeViewHOCR.hh"
#include "MainWindow.hh"
#include "UiUtils.hh"
#include "ui_OutputSettingsDialog.h"

void UI_OutputEditorHOCR::setupUi(QWidget* widget, FocusableMenu* keyParent) {
	widget->setLayout(new QVBoxLayout());
	widget->layout()->setContentsMargins(0, 0, 0, 0);
	widget->layout()->setSpacing(0);

	// Output toolbar
	actionInsertModeAppend = new QAction(QIcon(":/icons/ins_hocr_append"), gettext("&Append new output after last page"), widget);
	actionInsertModeBefore = new QAction(QIcon(":/icons/ins_hocr_before"), gettext("&Insert new output before current page"), widget);

	menuInsertMode = new QMenu(widget);
	menuInsertMode->addAction(actionInsertModeAppend);
	menuInsertMode->addAction(actionInsertModeBefore);


	toolButtonInsertMode = new QToolButton(widget);
	toolButtonInsertMode->setIcon(QIcon(":/icons/ins_hocr_append"));
	toolButtonInsertMode->setToolTip(gettext("Select insert mode"));
	toolButtonInsertMode->setPopupMode(QToolButton::InstantPopup);
	toolButtonInsertMode->setMenu(menuInsertMode);

	actionOpenAppend = new QAction(gettext("&Append document after last page"), widget);
	actionOpenInsertBefore = new QAction(gettext("&Insert document before current page"), widget);

	menuOpen = new QMenu(widget);
	menuOpen->addAction(actionOpenAppend);
	menuOpen->addAction(actionOpenInsertBefore);

	toolButtonOpen = new QToolButton(widget);
	toolButtonOpen->setIcon(QIcon::fromTheme("document-open"));
	toolButtonOpen->setText(gettext("Open hOCR file"));
	toolButtonOpen->setToolTip(gettext("Open hOCR file (replace)"));
	toolButtonOpen->setPopupMode(QToolButton::MenuButtonPopup);
	toolButtonOpen->setMenu(menuOpen);

	actionOutputSaveHOCR = new QAction(QIcon::fromTheme("document-save-as"), gettext("Save as hOCR text"), widget);
	actionOutputSaveHOCR->setToolTip(gettext("Save as hOCR text"));
	actionOutputSaveHOCR->setEnabled(false);
	toolButtonOutputExport = new QToolButton(widget);
	toolButtonOutputExport->setIcon(QIcon::fromTheme("document-export"));
	toolButtonOutputExport->setText(gettext("Export"));
	toolButtonOutputExport->setToolTip(gettext("Export"));
	toolButtonOutputExport->setEnabled(false);
	toolButtonOutputExport->setPopupMode(QToolButton::InstantPopup);
	actionOutputClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear output"), widget);
	actionOutputClear->setToolTip(gettext("Clear output"));
	actionOutputReplace = new QAction(QIcon::fromTheme("edit-find-replace"), gettext("Find and Replace"), widget);
	actionOutputReplace->setToolTip(gettext("Find and replace"));
	actionOutputReplace->setCheckable(true);
	actionOutputReplaceKey = new QAction(widget);

	actionOutputSettings = new QAction(QIcon::fromTheme("preferences-system"), gettext("Output Window Preferences"));
	outputDialog = new QDialog(MAIN);
	outputDialogUi.setupUi(outputDialog);
	outputDialog->setModal(true);
	FocusableMenu::sequenceFocus(outputDialog, outputDialogUi.checkBox_Preview);

	toolBarOutput = new QToolBar(widget);
	toolBarOutput->setToolButtonStyle(Qt::ToolButtonIconOnly);
	toolBarOutput->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
	toolBarOutput->addWidget(toolButtonInsertMode);
	toolBarOutput->addSeparator();
	toolBarOutput->addWidget(toolButtonOpen);
	toolBarOutput->addAction(actionOutputSaveHOCR);
	toolBarOutput->addWidget(toolButtonOutputExport);
	toolBarOutput->addAction(actionOutputClear);
	toolBarOutput->addSeparator();
	toolBarOutput->addAction(actionOutputReplace);

	QWidget* spacer = new QWidget(toolBarOutput);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	toolBarOutput->addWidget(spacer);

	toolBarOutput->addAction(actionOutputSettings);
	
	widget->addAction(actionOutputReplaceKey); // to some harmless parent

	widget->layout()->addWidget(toolBarOutput);

	searchFrame = new SearchReplaceFrame(keyParent, widget);
	searchFrame->setVisible(false);
	widget->layout()->addWidget(searchFrame);

	splitter = new QSplitter(Qt::Vertical, widget);
	widget->layout()->addWidget(splitter);

	QWidget* treeContainer = new QWidget(widget);
	treeContainer->setLayout(new QVBoxLayout());
	treeContainer->layout()->setSpacing(0);
	treeContainer->layout()->setContentsMargins(0, 0, 0, 0);
	splitter->addWidget(treeContainer);

	treeViewHOCR = new TreeViewHOCR(widget);
	treeViewHOCR->setHeaderHidden(true);
	treeViewHOCR->setSelectionMode(QTreeWidget::ExtendedSelection);
	treeContainer->layout()->addWidget(treeViewHOCR);

	actionNavigateNext = new QAction(QIcon::fromTheme("go-down"), gettext("Next (F3)"), widget);
	actionNavigatePrev = new QAction(QIcon::fromTheme("go-up"), gettext("Previous (Shift+F3)"), widget);
	comboBoxNavigate = new QComboBox();
	comboBoxNavigate->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	comboBoxNavigate->setMinimumContentsLength(10);
	actionExpandAll = new QAction(QIcon(":/icons/expand"), gettext("Expand all"), widget);
	actionCollapseAll = new QAction(QIcon(":/icons/collapse"), gettext("Collapse all"), widget);

	toolBarNavigate = new QToolBar(widget);
	toolBarNavigate->setToolButtonStyle(Qt::ToolButtonIconOnly);
	toolBarNavigate->setIconSize(QSize(1, 1) * toolBarOutput->style()->pixelMetric(QStyle::PM_SmallIconSize));
	toolBarNavigate->addWidget(comboBoxNavigate);
	toolBarNavigate->addSeparator();
	toolBarNavigate->addAction(actionNavigateNext);
	toolBarNavigate->addAction(actionNavigatePrev);
	toolBarNavigate->addSeparator();
	toolBarNavigate->addAction(actionExpandAll);
	toolBarNavigate->addAction(actionCollapseAll);
	treeContainer->layout()->addWidget(toolBarNavigate);

	tabWidgetProps = new QTabWidget(widget);

	tableWidgetProperties = new QTableWidget(widget);
	tableWidgetProperties->setColumnCount(2);
	tableWidgetProperties->horizontalHeader()->setVisible(false);
	tableWidgetProperties->verticalHeader()->setVisible(false);
	tableWidgetProperties->horizontalHeader()->setStretchLastSection(true);
	tabWidgetProps->addTab(tableWidgetProperties, gettext("P&roperties"));

	plainTextEditOutput = new OutputTextEdit(widget);
	plainTextEditOutput->setReadOnly(true);
	tabWidgetProps->addTab(plainTextEditOutput, gettext("&Source"));


	splitter->addWidget(tabWidgetProps);
}