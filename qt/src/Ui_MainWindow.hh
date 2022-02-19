/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Ui_MainWindow.hh
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

#ifndef UI_MAINWINDOW_HH
#define UI_MAINWINDOW_HH

#include "common.hh"
#include "ui_MainWindow.h"
#include <QDoubleSpinBox>
#include <QMenu>
#include <QWidgetAction>

class FocusableMenu;

class UI_MainWindow : public Ui_MainWindow {
public:
	QAction* actionAbout;
	QAction* actionHelp;
	QAction* actionRedetectLanguages;
	QAction* actionManageLanguages;
	QAction* actionRotateCurrentPage;
	QAction* actionRotateAllPages;
	QAction* actionRotateAuto;
	QAction* actionSourceClear;
	QAction* actionSourceDelete;
	QAction* actionSourcePaste;
	QAction* actionSourceFolder;
	QAction* actionSourceRemove;
	QAction* actionSourceScreenshot;
	QComboBox* comboBoxOCRMode;
	QDoubleSpinBox* spinBoxRotation;
	QSpinBox* spinBoxPage;
	QFrame* frameRotation;
	QFrame* framePage;
	FocusableMenu* menuAppMenu;
	QMenu* menuRotation;
	QToolBar* toolBarSources;
	QToolButton* toolButtonRotation;
	QToolButton* toolButtonRecognize;
	QToolButton* toolButtonLanguages;
	QToolButton* toolButtonAppMenu;
	QToolButton* toolButtonSourceAdd;
	QToolButton* toolButtonEditKeyMap;
	QWidgetAction* actionRotate;
	QWidgetAction* actionPage;
	FocusableMenu* menuSourcesShortcut;
	FocusableMenu* menuOutputShortcut;
	FocusableMenu* menuTopLevelShortcut;
	FocusableMenu* menuBatchExportShortcut;
	FocusableMenu* menuPreferences;
	QToolButton* toolButtonShortcutMenu;
	QAction* controlsMenuAction;
	QAction* autodetectMenuAction;
	QAction* pageMenuAction;
	QAction* startScanAction;
	QDialog* batchExportDialog;

	void setupUi(QMainWindow* mainWindow);
};

#endif // UI_MAINWINDOW_HH
