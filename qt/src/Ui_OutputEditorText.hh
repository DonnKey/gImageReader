/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Ui_OutputEditorText.hh
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

#ifndef UI_OUTPUTEDITORTEXT_HH
#define UI_OUTPUTEDITORTEXT_HH

#include "common.hh"
#include "OutputTextEdit.hh"
#include "SearchReplaceFrame.hh"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

class UI_OutputEditorText {
public:
	QAction* actionOutputModeAppend;
	QAction* actionOutputModeCursor;
	QAction* actionOutputModeReplace;
	QAction* actionOutputClear;
	QAction* actionOutputRedo;
	QAction* actionOutputReplace;
	QAction* actionOutputSave;
	QAction* actionOutputPostprocTitle1;
	QAction* actionOutputPostprocKeepEndMark;
	QAction* actionOutputPostprocKeepQuote;
	QAction* actionOutputPostprocTitle2;
	QAction* actionOutputPostprocJoinHyphen;
	QAction* actionOutputPostprocCollapseSpaces;
	QAction* actionOutputPostprocKeepParagraphs;
	QAction* actionOutputPostprocTitle3;
	QAction* actionOutputPostprocDrawWhitespace;
	QAction* actionOutputUndo;
	QMenu* menuOutputMode;
	QMenu* menuOutputPostproc;
	QTabWidget* tabWidget;
	QToolBar* toolBarOutput;
	QToolButton* toolButtonOpen;
	QToolButton* toolButtonOutputMode;
	QToolButton* toolButtonOutputPostproc;
	QToolButton* toolButtonAddTab;

	SearchReplaceFrame* searchFrame;

	void setupUi(QWidget* widget);
};

#endif // UI_OUTPUTEDITORTEXT_HH
