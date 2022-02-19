/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Ui_MainWindow.cc
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
#include "ui_MainWindow.h"
#include "Ui_MainWindow.hh"
#include "MainWindow.hh"
#include <QDoubleSpinBox>
#include <QMenu>
#include <QWidgetAction>

void UI_MainWindow::setupUi(QMainWindow* mainWindow) {
	Ui_MainWindow::setupUi(mainWindow);

	// Do remaining things which are not possible in designer
	toolBarMain->setContextMenuPolicy(Qt::PreventContextMenu);

	// Remove & from some labels which designer insists in adding
	dockWidgetSources->setWindowTitle(gettext("Sources"));
	dockWidgetOutput->setWindowTitle(gettext("Output"));

	// Hide image controls widget
	widgetImageControls->setVisible(false);

	// Rotate spinbox
	frameRotation = new QFrame(mainWindow);
	frameRotation->setFrameShape(QFrame::StyledPanel);
	frameRotation->setFrameShadow(QFrame::Sunken);
	frameRotation->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	frameRotation->setToolTip(gettext("Rotate page"));

	QHBoxLayout* layoutRotation = new QHBoxLayout(frameRotation);
	layoutRotation->setContentsMargins(1, 1, 1, 1);
	layoutRotation->setSpacing(0);

	actionRotateCurrentPage = new QAction(QIcon(":/icons/rotate_page"), gettext("Rotate current page"), mainWindow);
	actionRotateAllPages = new QAction(QIcon(":/icons/rotate_pages"), gettext("Rotate all pages"), mainWindow);
	actionRotateAuto = new QAction(QIcon(":/icons/rotate_auto"), gettext("Auto rotate when recognizing"), mainWindow);

	menuRotation = new QMenu(mainWindow);
	menuRotation->addAction(actionRotateCurrentPage);
	menuRotation->addAction(actionRotateAllPages);
	menuRotation->addAction(actionRotateAuto);

	toolButtonRotation = new QToolButton(mainWindow);
	toolButtonRotation->setIcon(QIcon(":/icons/rotate_pages"));
	toolButtonRotation->setToolTip(gettext("Select rotation mode"));
	toolButtonRotation->setPopupMode(QToolButton::InstantPopup);
	toolButtonRotation->setAutoRaise(true);
	toolButtonRotation->setMenu(menuRotation);

	layoutRotation->addWidget(toolButtonRotation);

	spinBoxRotation = new QDoubleSpinBox(mainWindow);
	spinBoxRotation->setRange(0.0, 359.9);
	spinBoxRotation->setDecimals(1);
	spinBoxRotation->setSingleStep(0.1);
	spinBoxRotation->setWrapping(true);
	spinBoxRotation->setFrame(false);
	spinBoxRotation->setKeyboardTracking(false);
	spinBoxRotation->setSizePolicy(spinBoxRotation->sizePolicy().horizontalPolicy(), QSizePolicy::MinimumExpanding);
	layoutRotation->addWidget(spinBoxRotation);

	actionRotate = new QWidgetAction(mainWindow);
	actionRotate->setDefaultWidget(frameRotation);

	toolBarMain->insertAction(actionImageControls, actionRotate);

	// Page spinbox
	framePage = new QFrame(mainWindow);
	framePage->setFrameShape(QFrame::StyledPanel);
	framePage->setFrameShadow(QFrame::Sunken);
	framePage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	framePage->setToolTip(gettext("Select page"));

	QHBoxLayout* layoutPage = new QHBoxLayout(framePage);
	layoutPage->setContentsMargins(1, 1, 1, 1);
	layoutPage->setSpacing(0);

	QToolButton* toolButtonPage = new QToolButton(mainWindow);
	toolButtonPage->setIcon(QPixmap(":/icons/page"));
	toolButtonPage->setEnabled(false);
	toolButtonPage->setAutoRaise(true);
	layoutPage->addWidget(toolButtonPage);

	spinBoxPage = new QSpinBox(mainWindow);
	spinBoxPage->setRange(1, 1);
	spinBoxPage->setFrame(false);
	spinBoxPage->setKeyboardTracking(false);
	spinBoxPage->setSizePolicy(spinBoxPage->sizePolicy().horizontalPolicy(), QSizePolicy::MinimumExpanding);
	layoutPage->addWidget(spinBoxPage);

	actionPage = new QWidgetAction(mainWindow);
	actionPage->setDefaultWidget(framePage);

	toolBarMain->insertAction(actionImageControls, actionPage);
	actionPage->setVisible(false);

	QFont smallFont;
	smallFont.setPointSizeF(smallFont.pointSizeF() * 0.9);

	// OCR mode button
	QWidget* ocrModeWidget = new QWidget();
	ocrModeWidget->setLayout(new QVBoxLayout());
	ocrModeWidget->layout()->setContentsMargins(0, 0, 0, 0);
	ocrModeWidget->layout()->setSpacing(0);
	QLabel* outputModeLabel = new QLabel(gettext("OCR mode:"));
	outputModeLabel->setFont(smallFont);
	ocrModeWidget->layout()->addWidget(outputModeLabel);
	comboBoxOCRMode = new QComboBox();
	comboBoxOCRMode->setFont(smallFont);
	comboBoxOCRMode->setFrame(false);
	comboBoxOCRMode->setCurrentIndex(-1);
	ocrModeWidget->layout()->addWidget(comboBoxOCRMode);
	toolBarMain->insertWidget(actionAutodetectLayout, ocrModeWidget);

	actionAutodetectLayout->setVisible(false);

	// Recognize and language button
	toolButtonRecognize = new QToolButton(mainWindow);
	toolButtonRecognize->setIcon(QIcon::fromTheme("insert-text"));
	toolButtonRecognize->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	toolButtonRecognize->setFont(smallFont);
	toolBarMain->insertWidget(actionToggleOutputPane, toolButtonRecognize);

	toolButtonLanguages = new QToolButton(mainWindow);
	toolButtonLanguages->setIcon(QIcon::fromTheme("applications-education-language"));
	toolButtonLanguages->setPopupMode(QToolButton::InstantPopup);
	toolBarMain->insertWidget(actionToggleOutputPane, toolButtonLanguages);

	toolBarMain->insertSeparator(actionToggleOutputPane);

	// Spacer before app menu button
	QWidget* toolBarMainSpacer = new QWidget(toolBarMain);
	toolBarMainSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	toolBarMain->addWidget(toolBarMainSpacer);

	// KeyMap window
	toolButtonEditKeyMap = new QToolButton(mainWindow);
	toolButtonEditKeyMap->setAutoRaise(true);
	toolButtonEditKeyMap->setIcon(QIcon::fromTheme("preferences-desktop-keyboard-symbolic.symbolic"));
	toolButtonEditKeyMap->setPopupMode(QToolButton::InstantPopup);
	toolButtonEditKeyMap->setToolTip("Map keys to actions");
	toolBarMain->addWidget(toolButtonEditKeyMap);

	// App menu
	menuAppMenu = new QMenu(mainWindow);

	actionRedetectLanguages = new QAction(QIcon::fromTheme("view-refresh"), gettext("Redetect Languages"), mainWindow);
	menuAppMenu->addAction(actionRedetectLanguages);

	actionManageLanguages = new QAction(QIcon::fromTheme("applications-education-language"), gettext("Manage Languages"), mainWindow);
	menuAppMenu->addAction(actionManageLanguages);

	actionPreferences = new QAction(QIcon::fromTheme("preferences-system"), gettext("Preferences"), mainWindow);
	menuAppMenu->addAction(actionPreferences);

	menuAppMenu->addSeparator();

	actionHelp = new QAction(QIcon::fromTheme("help-contents"), gettext("Help"), mainWindow);
	menuAppMenu->addAction(actionHelp);

	actionAbout = new QAction(QIcon::fromTheme("help-about"), gettext("About"), mainWindow);
	menuAppMenu->addAction(actionAbout);

	// App menu button
	toolButtonAppMenu = new QToolButton(mainWindow);
	toolButtonAppMenu->setIcon(QIcon::fromTheme("preferences-system"));
	toolButtonAppMenu->setPopupMode(QToolButton::InstantPopup);
	toolButtonAppMenu->setMenu(menuAppMenu);
	toolBarMain->addWidget(toolButtonAppMenu);

	// Sources toolbar
	actionSourceFolder = new QAction(QIcon::fromTheme("folder-open"), gettext("Add folder"), mainWindow);
	actionSourcePaste = new QAction(QIcon::fromTheme("edit-paste"), gettext("Paste"), mainWindow);
	actionSourceScreenshot = new QAction(QIcon::fromTheme("camera-photo"), gettext("Take Screenshot"), mainWindow);

	toolButtonSourceAdd = new QToolButton(mainWindow);
	toolButtonSourceAdd->setIcon(QIcon::fromTheme("document-open"));
	toolButtonSourceAdd->setText(gettext("Add Images"));
	toolButtonSourceAdd->setToolTip(gettext("Add images"));
	toolButtonSourceAdd->setPopupMode(QToolButton::MenuButtonPopup);

	actionSourceRemove = new QAction(QIcon::fromTheme("list-remove"), gettext("Remove Image"), mainWindow);
	actionSourceRemove->setToolTip(gettext("Remove image from list"));
	actionSourceRemove->setEnabled(false);
	actionSourceDelete = new QAction(QIcon::fromTheme("user-trash"), gettext("Delete Image"), mainWindow);
	actionSourceDelete->setToolTip(gettext("Delete image"));
	actionSourceDelete->setEnabled(false);
	actionSourceClear = new QAction(QIcon::fromTheme("edit-clear"), gettext("Clear List"), mainWindow);
	actionSourceClear->setToolTip(gettext("Clear list"));
	actionSourceClear->setEnabled(false);

	toolBarSources = new QToolBar(mainWindow);
	toolBarSources->setToolButtonStyle(Qt::ToolButtonIconOnly);
	toolBarSources->setIconSize(QSize(1, 1) * toolBarSources->style()->pixelMetric(QStyle::PM_SmallIconSize));
	toolBarSources->addWidget(toolButtonSourceAdd);
	toolBarSources->addAction(actionSourceFolder);
	toolBarSources->addAction(actionSourcePaste);
	toolBarSources->addAction(actionSourceScreenshot);
	toolBarSources->addSeparator();
	toolBarSources->addAction(actionSourceRemove);
	toolBarSources->addAction(actionSourceDelete);
	toolBarSources->addAction(actionSourceClear);
	static_cast<QVBoxLayout*>(tabSources->layout())->insertWidget(0, toolBarSources);
}