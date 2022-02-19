/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Ui_OutputEditorHOCR.hh
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

#ifndef UI_OUTPUTEDITORHOCR_HH
#define UI_OUTPUTEDITORHOCR_HH

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

#include "TreeViewHOCR.hh"
#include "MainWindow.hh"
#include "ui_OutputSettingsDialog.h"

class FocusableMenu;

class UI_OutputEditorHOCR {
public:
	QMenu* menuInsertMode;
	QMenu* menuOpen;
	FocusableMenu* exportMenu;
	QToolButton* toolButtonInsertMode;
	QToolButton* toolButtonOutputExport;
	QToolButton* toolButtonOpen;
	QAction* actionInsertModeAppend;
	QAction* actionInsertModeBefore;
	QAction* actionOpenAppend;
	QAction* actionOpenInsertBefore;
	QAction* actionOutputClear;
	QAction* actionOutputSaveHOCR;
	QAction* actionOutputExportText;
	QAction* actionOutputExportIndentedText;
	QAction* actionOutputExportPDF;
	QAction* actionOutputExportODT;
	QAction* actionOutputReplace;
	QAction* actionOutputReplaceKey;
	QAction* actionOutputSettings;
	QAction* actionNavigateNext;
	QAction* actionNavigatePrev;
	QAction* actionExpandAll;
	QAction* actionCollapseAll;
	QAction* menuOutputSaveHOCR;
	QAction* menuOutputExport;
	QAction* menuOutputNavigate;
	QAction* menuOutputFind;
	QComboBox* comboBoxNavigate;

	QToolBar* toolBarOutput;
	QToolBar* toolBarNavigate;
	QTabWidget* tabWidgetProps;

	QSplitter* splitter;
	TreeViewHOCR* treeViewHOCR;
	QTableWidget* tableWidgetProperties;
	OutputTextEdit* plainTextEditOutput;
	SearchReplaceFrame* searchFrame;

	QDialog* outputDialog;
	Ui::OutputSettingsDialog outputDialogUi;

	void setupUi(QWidget* widget, FocusableMenu* keyParent);
};

#endif // UI_OUTPUTEDITORHOCR_HH
