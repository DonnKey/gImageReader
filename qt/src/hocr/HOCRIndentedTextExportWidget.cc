/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRIndentedTextExportWidget.cc
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

#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "DisplayerToolHOCR.hh"
#include "OutputEditorHOCR.hh"
#include "HOCRDocument.hh"
#include "HOCRIndentedTextExportWidget.hh"
#include "HOCRIndentedTextExporter.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"


HOCRIndentedTextExportWidget::HOCRIndentedTextExportWidget(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument, const HOCRPage* hocrpage, QWidget* parent)
	: HOCRExporterWidget(parent), m_displayerTool(displayerTool), m_document(hocrdocument), m_previewPage(hocrpage) {
	ui.setupUi(this);

	connect(ui.checkBoxPreview, &QCheckBox::toggled, this, &HOCRIndentedTextExportWidget::updatePreview);
	connect(ui.checkBox_GuideBars, &QCheckBox::toggled, this, &HOCRIndentedTextExportWidget::updatePreview);
	connect(ui.spinBox_OriginX, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRIndentedTextExportWidget::updatePreview);
	connect(ui.spinBox_OriginY, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRIndentedTextExportWidget::updatePreview);
	connect(ui.doubleSpinBox_CellWidth, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &HOCRIndentedTextExportWidget::updatePreview);
	connect(ui.doubleSpinBox_CellHeight, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &HOCRIndentedTextExportWidget::updatePreview);
	connect(ui.pushButton_OriginCompute, &QPushButton::clicked, this, &HOCRIndentedTextExportWidget::computeOrigin);
	connect(ui.pushButton_CellCompute, &QPushButton::clicked, this, &HOCRIndentedTextExportWidget::computeCell);
	connect(ui.comboBox_FontFamily, &QFontComboBox::currentFontChanged, this, &HOCRIndentedTextExportWidget::updatePreview);
	connect(ui.spinBox_FontSize, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRIndentedTextExportWidget::updatePreview);
	connect(ui.spinBox_FontStretch, qOverload<int>(&QSpinBox::valueChanged), this, &HOCRIndentedTextExportWidget::updatePreview);

	ADD_SETTING(SwitchSetting("indentpreview", ui.checkBoxPreview, true));
	ADD_SETTING(SwitchSetting("indentguidebars", ui.checkBox_GuideBars, true));
	ADD_SETTING(SpinSetting("indentoriginX", ui.spinBox_OriginX, 100));
	ADD_SETTING(SpinSetting("indentoriginY", ui.spinBox_OriginY, 150));
	ADD_SETTING(DoubleSpinSetting("indentcellW", ui.doubleSpinBox_CellWidth, 23.0));
	ADD_SETTING(DoubleSpinSetting("indentcellH", ui.doubleSpinBox_CellHeight, 33.0));
	ADD_SETTING(FontComboSetting("indentfontfamily", ui.comboBox_FontFamily, QFont("Monospace")));
	ADD_SETTING(SpinSetting("indentfontsize", ui.spinBox_FontSize, 10));
	ADD_SETTING(SpinSetting("indentfontStretch", ui.spinBox_FontStretch, 100));

	m_preview = new QGraphicsPixmapItem();
	m_preview->setTransformationMode(Qt::SmoothTransformation);
	m_preview->setZValue(3);
	updatePreview();
	MAIN->getDisplayer()->scene()->addItem(m_preview);
}

HOCRIndentedTextExportWidget::~HOCRIndentedTextExportWidget() {
	MAIN->getDisplayer()->scene()->removeItem(m_preview);
	delete m_preview;
	static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor())->showPreview(OutputEditorHOCR::showMode::resume);
}

void HOCRIndentedTextExportWidget::setPreviewPage(const HOCRDocument* hocrdocument, const HOCRPage* hocrpage) {
	m_document = hocrdocument;
	m_previewPage = hocrpage;
	updatePreview();
}

HOCRIndentedTextExporter::ExporterSettings& HOCRIndentedTextExportWidget::getSettings() const {
	HOCRIndentedTextExporter::IndentedTextSettings& settings = m_settings;
	settings.m_originX = ui.spinBox_OriginX->value();
	settings.m_originY = ui.spinBox_OriginY->value();
	settings.m_cellWidth = ui.doubleSpinBox_CellWidth->value();
	settings.m_cellHeight = ui.doubleSpinBox_CellHeight->value();
	settings.m_fontFamily = ui.comboBox_FontFamily->currentFont().family();
	settings.m_fontSize = ui.spinBox_FontSize->value();
	settings.m_fontStretch = ui.spinBox_FontStretch->value()/100.;
	settings.m_guideBars = ui.checkBox_GuideBars->isChecked();

	return settings;
}

void HOCRIndentedTextExportWidget::updatePreview() {
	if(!m_preview) {
		return;
	}
	m_preview->setVisible(ui.checkBoxPreview->isChecked());
	if(!m_document || m_document->pageCount() == 0 || !m_previewPage || !ui.checkBoxPreview->isChecked()) {
		return;
	}

	const HOCRPage* page = m_previewPage;
	QRect bbox = page->bbox();
	int pageDpi = page->resolution();

	HOCRIndentedTextExporter::IndentedTextSettings& settings = static_cast<HOCRIndentedTextExporter::IndentedTextSettings&>(getSettings());

	QImage image(bbox.size(), QImage::Format_ARGB32);
	image.fill(QColor(Qt::transparent));
	image.setDotsPerMeterX(pageDpi / 0.0254); // 1 in = 0.0254 m
	image.setDotsPerMeterY(pageDpi / 0.0254);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);

	HOCRQPainterIndentedTextPrinter printer(&painter);
	if(!settings.m_fontFamily.isEmpty()) {
		printer.setFontFamily(settings.m_fontFamily, false, false);
	}
	if(settings.m_fontSize != -1) {
		printer.setFontSize(settings.m_fontSize);
	}

	printer.printPage(page, settings);
	m_preview->setPixmap(QPixmap::fromImage(image));
	m_preview->setPos(-0.5 * bbox.width(), -0.5 * bbox.height());
	static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor())->showPreview(OutputEditorHOCR::showMode::suspend);
}

bool HOCRIndentedTextExportWidget::findOrigin(const HOCRItem *item) {
	if(!item->isEnabled()) {
		return false;
	}
	QString itemClass = item->itemClass();
	if(itemClass == "ocr_line") {
		QRect itemRect = item->bbox();
		ui.spinBox_OriginX->setValue(itemRect.left());
		ui.spinBox_OriginY->setValue(itemRect.top());
		return true;
	} else {
		int childCount = item->children().size();
		for(int i = 0, n = item->children().size(); i < n; ++i) {
			if (findOrigin(item->children()[i])) {
				return true;
			}
		}
	}
	return false;
}

void HOCRIndentedTextExportWidget::computeOrigin() {
	const HOCRPage* page = m_previewPage;
	findOrigin(page);
}

class computeSpaces {
	double m_accumulatedWidth;
	double m_accumulatedCharacters;
	QList<QPair<int,int>> m_textLines;

	void findCells(const HOCRItem *item) {
		if(!item->isEnabled()) {
			return;
		}

		QString itemClass = item->itemClass();

		if(itemClass == "ocr_line") {
			QRect itemRect = item->bbox();
			m_textLines.append(QPair<int,int>(itemRect.top(), itemRect.height()));
		}

		if(itemClass == "ocrx_word") {
			m_accumulatedWidth += item->bbox().width();
			int len = item->text().length();
			// shorten effective string len to allow for narrow margins at bbox ends.
			m_accumulatedCharacters += len - 1.0/len;
		} else {
			for(auto child : item->children()) {
				findCells(child);
			}
		}
	}

public:
	void computeCell(const HOCRPage* page, Ui::IndentedTextExportWidget& ui) {
		m_accumulatedWidth = 0.0;
		m_accumulatedCharacters = 0.0001;
		m_textLines.clear();

		findCells(page);

		// Character width
		double pitch = m_accumulatedWidth/m_accumulatedCharacters;
		if (pitch < 1. || pitch > 200.) {
			// The "last resort" values are just educated guesses.
			pitch = page->resolution()/100. * 8.;
		}
		ui.doubleSpinBox_CellWidth->setValue(pitch);

		// Line spacing
		if (m_textLines.size() > 2) {
			std::sort(m_textLines.begin(), m_textLines.end(), [this] (auto a,auto b) {return a.first < b.first;});

			int sum = 0;
			for (auto entry:m_textLines) {
				sum += entry.second;
			}
			double mean = sum/m_textLines.size();

			sum = 0;
			int counted = 0;
			for (auto entry:m_textLines) {
				if (entry.second > mean*0.75 && entry.second < mean*1.5) {
					sum += entry.second;
					counted++;
				}
			}

			if (counted > 2 && sum > 0) {
				// text height of lines with outliers (tesseract artifacts) discarded:
				mean = sum/counted;
			}

			int currY = m_textLines[0].first;
			int lines = 0;
			for (int i = 1; i<m_textLines.size(); i++) {
				// 1.5: margin for error to avoid glitchy behavior at edges
				while (currY + mean/1.5 < m_textLines[i].first) {
					currY += mean;
					lines++;
				}
				currY = m_textLines[i].first;
			}
			int height = m_textLines[m_textLines.size()-1].first - m_textLines[0].first + mean;

			pitch = height/(lines+1);
		} else if (m_textLines.size() == 2) {
			pitch = m_textLines[1].first - m_textLines[0].first;
		} else if (m_textLines.size() == 1) {
			pitch = m_textLines[0].second;
		} else {
			pitch = 0.;
		}
		if (pitch < 1. || pitch > 200.) {
			pitch = page->resolution()/100. * 11.;
		}

		ui.doubleSpinBox_CellHeight->setValue(pitch);

		m_textLines.clear();
	}
};

void HOCRIndentedTextExportWidget::computeCell() {
	computeSpaces().computeCell(m_previewPage, ui);
}