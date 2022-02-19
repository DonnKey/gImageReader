/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRBatchExportDialog.cc
 * Copyright (C) 2020-2022 Sandro Mani <manisandro@gmail.com>
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

#include "HOCRBatchExportDialog.hh"
#include "ConfigSettings.hh"
#include "FileTreeModel.hh"
#include "HOCRDocument.hh"
#include "HOCRPdfExporter.hh"
#include "HOCRPdfExportWidget.hh"
#include "HOCROdtExporter.hh"
#include "HOCRTextExporter.hh"
#include "HOCRIndentedTextExporter.hh"
#include "HOCRIndentedTextExportWidget.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "UiUtils.hh"
#include "Utils.hh"

#include <QDirIterator>
#include <QFileDialog>


HOCRBatchExportDialog::HOCRBatchExportDialog(FocusableMenu* keyParent, QWidget* parent)
	: QDialog(parent) {
	ui.setupUi(this);
	setModal(true);
	FocusableMenu::sequenceFocus(this, ui.batchTabWidget);

	ui.comboBoxFormat->addItem(_("PDF"), ExportPdf);
	ui.comboBoxFormat->addItem(_("ODT"), ExportOdt);
	ui.comboBoxFormat->addItem(_("Plain text"), ExportTxt);
	ui.comboBoxFormat->addItem(_("Text - whitespace preserved"), ExportIndentedTxt);
	ui.comboBoxFormat->setCurrentIndex(-1);

	m_sourceTreeModel = new FileTreeModel(this);
	m_outputTreeModel = new FileTreeModel(this);

	ui.treeViewInput->setModel(m_sourceTreeModel);
	ui.treeViewOutput->setModel(m_outputTreeModel);

	ui.treeViewInput->header()->hideSection(1);
	ui.treeViewOutput->header()->hideSection(1);

	ui.progressBar->hide();
	ui.batchTabWidget->setTabEnabled(1,false);

	m_previewTimer.setSingleShot(true);

	connect(ui.toolButtonSourceFolder, &QToolButton::clicked, this,
		[this, keyParent]() { FocusableMenu::showFileDialogMenu(keyParent, [this]{setSourceFolder(); return true;}); });
	connect(ui.comboBoxFormat, qOverload<int>(&QComboBox::currentIndexChanged), this, &HOCRBatchExportDialog::setExportFormat);
	connect(ui.spinBoxExportLevel, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRBatchExportDialog::updateOutputTree);
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &HOCRBatchExportDialog::apply);
	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &HOCRBatchExportDialog::reject);
	connect(&m_previewTimer, &QTimer::timeout, this, &HOCRBatchExportDialog::updateExportPreview);

	ADD_SETTING(ComboSetting("batchexportformat", ui.comboBoxFormat));
	ADD_SETTING(LineEditSetting("batchexportsourcefolder", ui.lineEditSourceFolder));

	fillSourceFolder();
}

void HOCRBatchExportDialog::setSourceFolder() {
	QString initialFolder = ConfigSettings::get<LineEditSetting>("batchexportsourcefolder")->getValue();
	if (initialFolder.isEmpty()) {
		initialFolder = Utils::documentsFolder();
	}
	QString dir = QFileDialog::getExistingDirectory(MAIN->getDialogHost(), _("Select folder..."), initialFolder);
	if(dir.isEmpty()) {
		return;
	}
	ui.lineEditSourceFolder->setText(dir);
	fillSourceFolder();
}

void HOCRBatchExportDialog::fillSourceFolder() {
	QString dir = ConfigSettings::get<LineEditSetting>("batchexportsourcefolder")->getValue();
	if (dir.isEmpty()) {
		return;
	}
	m_sourceTreeModel->clear();

	QDirIterator it(dir, QStringList() << "*.html", QDir::Files, QDirIterator::Subdirectories);
	while(it.hasNext()) {
		QString filename = it.next();
		m_sourceTreeModel->insertFile(filename, nullptr);
	}
	ui.treeViewInput->expandAll();
	updateOutputTree();
}

void HOCRBatchExportDialog::setExportFormat() {
	delete m_exporterWidget;
	m_exporterWidget = nullptr;

	ExportMode mode = static_cast<ExportMode>(ui.comboBoxFormat->currentData().toInt());
	OutputEditorHOCR* editor = static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor());
	if(mode == ExportPdf) {
		m_exporterWidget = new HOCRPdfExportWidget(editor->getTool());
		ui.tabOptions->layout()->addWidget(m_exporterWidget);
		ui.batchTabWidget->setTabEnabled(1,true);
	} else if(mode == ExportIndentedTxt) {
		m_exporterWidget = new HOCRIndentedTextExportWidget(editor->getTool());
		ui.tabOptions->layout()->addWidget(m_exporterWidget);
		ui.batchTabWidget->setTabEnabled(1,true);
	} else {
		ui.batchTabWidget->setTabEnabled(1,false);
	}
	updateOutputTree();
}

void HOCRBatchExportDialog::updateOutputTree() {
	m_previewTimer.stop();
	m_outputTreeModel->clear();

	m_outputMap.clear();

	int exportLevel = ui.spinBoxExportLevel->value();

	QString dir = ui.lineEditSourceFolder->text();
	if(dir.isEmpty()) {
		return;
	}

	QString exportSuffix;
	ExportMode mode = static_cast<ExportMode>(ui.comboBoxFormat->currentData().toInt());
	switch(mode) {
	case ExportPdf:
		exportSuffix = ".pdf";
		break;
	case ExportOdt:
		exportSuffix = ".pdf";
		break;
	case ExportTxt:
	case ExportIndentedTxt:
		exportSuffix = ".txt";
		break;
	}

	QStringList filenames;
	QDirIterator it(dir, QStringList() << "*.html", QDir::Files, QDirIterator::Subdirectories);
	int deepestlevel = 0;
	while(it.hasNext()) {
		QString filename = it.next();
		filenames.append(filename);
		deepestlevel = std::max(int(QDir::cleanPath(QDir(dir).relativeFilePath(filename)).count('/')), deepestlevel);
	}
	int groupAboveDepth = std::max(0, deepestlevel - exportLevel);
	for (const QString& filename : filenames) {
		int level = QDir::cleanPath(QDir(dir).relativeFilePath(filename)).count('/');
		QFileInfo finfo(filename);
		QString output = finfo.dir().absoluteFilePath(finfo.completeBaseName()) + exportSuffix;
		for (int i = level; i >= groupAboveDepth; --i) {
			output = QFileInfo(output).path() + exportSuffix;
		}
		m_outputMap[output].append(filename);
	}
	for(const QString& output : m_outputMap.keys()) {
		m_outputTreeModel->insertFile(output, nullptr);
	}
	ui.treeViewOutput->expandAll();

	if(m_exporterWidget) {
		m_previewTimer.start(250);
	}
}

void HOCRBatchExportDialog::apply() {
	if (m_outputMap.isEmpty()) {
		blinkFiles();
		return;
	}

	m_previewTimer.stop();

	ui.progressBar->setRange(0, m_outputMap.size());
	ui.progressBar->show();

	HOCRExporter* exporter = nullptr;
	HOCRExporter::ExporterSettings* settings;

	ExportMode mode = static_cast<ExportMode>(ui.comboBoxFormat->currentData().toInt());
	switch(mode) {
	case ExportPdf:
		exporter = new HOCRPdfExporter();
		settings = &m_exporterWidget->getSettings();
		break;
	case ExportOdt:
		exporter = new HOCROdtExporter();
		break;
	case ExportTxt:
		exporter = new HOCRTextExporter();
		break;
	case ExportIndentedTxt:
		exporter = new HOCRIndentedTextExporter();
		settings = &m_exporterWidget->getSettings();
		break;
	}

	OutputEditorHOCR* editor = static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor());

	for(auto it = m_outputMap.begin(), itEnd = m_outputMap.end(); it != itEnd; ++it) {
		if (!editor->open(OutputEditorHOCR::InsertMode::Replace, it.value())) {
			break;
		}
		exporter->run(editor->getDocument(), it.key(), settings);
		ui.progressBar->setValue(ui.progressBar->value() + 1);
	}

	delete exporter;

	ui.progressBar->hide();
	close();
}

void HOCRBatchExportDialog::updateExportPreview() {
	OutputEditorHOCR* editor = dynamic_cast<OutputEditorHOCR*>(MAIN->getOutputEditor());
	if (m_exporterWidget == nullptr) {
		// Nothing
	} else if (editor != nullptr && !m_outputMap.isEmpty()) {
		if (!editor->open(OutputEditorHOCR::InsertMode::Replace, m_outputMap.first())) {
			return;
		}
		HOCRDocument* document = editor->getDocument();
		editor->selectPage(0);
		m_exporterWidget->setPreviewPage(document->pageCount() > 0 ? document : nullptr, document->pageCount() > 0 ? document->page(0) : nullptr);
	} else {
		m_exporterWidget->setPreviewPage(nullptr, nullptr);
	}
}

void HOCRBatchExportDialog::blinkFiles() {
	new BlinkWidget(12,
		[this]{ui.toolButtonSourceFolder->setStyleSheet("background-color: red"); },
		[this]{ui.toolButtonSourceFolder->setStyleSheet("");}, this );
}