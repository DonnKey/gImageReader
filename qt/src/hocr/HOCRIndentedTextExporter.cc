/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRIndentedTextExporter.cc
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
#include "HOCRDocument.hh"
#include "HOCRIndentedTextExporter.hh"
#include "HOCRIndentedTextExportWidget.hh"
#include "MainWindow.hh"
#include "UiUtils.hh"
#include "Displayer.hh"

#include <QDesktopServices>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <QUrl>

bool HOCRIndentedTextExporter::run(const HOCRDocument* hocrdocument, const QString& outname, const ExporterSettings* settings) {
	QFile outputFile(outname);
	if(!outputFile.open(QIODevice::WriteOnly)) {
		QMessageBox::warning(MAIN, _("Export failed"), _("The text export failed: unable to write output file."));
		return false;
	}

	QString output;
	QTextStream* outputStream = new QTextStream(&output, QIODevice::WriteOnly);
	HOCRQPrinterIndentedTextPrinter printer(outputStream);

	for(int i = 0, n = hocrdocument->pageCount(); i < n; ++i) {
		const HOCRPage* page = hocrdocument->page(i);
		if(!page->isEnabled()) {
			continue;
		}
		printer.printPage(page, static_cast<const HOCRIndentedTextExporter::IndentedTextSettings&>(*settings));
	}
	outputFile.write(MAIN->getConfig()->useUtf8() ? output.toUtf8() : output.toLocal8Bit());
	outputFile.close();

	bool openAfterExport = ConfigSettings::get<SwitchSetting>("openafterexport")->getValue();
	if(openAfterExport) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(outname));
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////

HOCRIndentedTextExportDialog::HOCRIndentedTextExportDialog(DisplayerToolHOCR* displayerTool, 
	const HOCRDocument* hocrdocument, const HOCRPage* hocrpage, QWidget* parent)
	: QDialog(parent) {
	setModal(true);
	setLayout(new QVBoxLayout);
	m_widget = new HOCRIndentedTextExportWidget(displayerTool, hocrdocument, hocrpage);
	FocusableMenu::sequenceFocus(this, m_widget->ui.checkBoxPreview);
	layout()->addWidget(m_widget);

	QDialogButtonBox* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout()->addWidget(bbox);
	connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

HOCRIndentedTextExporter::IndentedTextSettings& HOCRIndentedTextExportDialog::getIndentedTextSettings() const {
	return static_cast<HOCRIndentedTextExporter::IndentedTextSettings&>(m_widget->getSettings());
}

/////////////////////////////////////////////////////////////////////////////////////

void HOCRIndentedTextPrinter::printPage(const HOCRItem* item, const HOCRIndentedTextExporter::IndentedTextSettings& settings) {
	m_settings = &settings;
	m_currentY = m_settings->m_originY;
	printChildren(item);
}

void HOCRIndentedTextPrinter::printChildren(const HOCRItem* item) {
	if(!item->isEnabled()) {
		return;
	}

	QString itemClass = item->itemClass();
	QRect itemRect = item->bbox();
	int childCount = item->children().size();
	if(itemClass == "ocr_line") {
		double newY;
		QString text = HOCRIndentedTextPrinter::buildLine(item, newY);
		// 2.0: allow lots of margin
		while (newY - m_currentY >= m_settings->m_cellHeight/2.0) {
			drawBlank(0, m_currentY, ">");
			m_currentY += m_settings->m_cellHeight;
		}
		drawText(m_settings->m_originX, m_currentY, text);
		m_currentY += m_settings->m_cellHeight;
	} else {
		for(int i = 0, n = item->children().size(); i < n; ++i) {
			printChildren(item->children()[i]);
		}
	}
}

QString HOCRIndentedTextPrinter::buildLine(const HOCRItem* line, double& nextY) {
	double prevX = m_settings->m_originX;
	double pitchX = m_settings->m_cellWidth;
	QString buffer;
	buffer.reserve(500);

	for(int i = 0, n = line->children().size(); i < n; ++i) {
		HOCRItem* item = line->children()[i];
		QString text = item->text();

		int startChar = (item->bbox().left()-m_settings->m_originX)/pitchX;
		int padding = std::max(0, startChar - buffer.length());
		if (padding == 0) {
			if (item->bbox().left() - prevX >= pitchX) {
				padding = 1;
			}
			if (padding == 0 && buffer.size()>0) {
				QChar right = buffer[buffer.size()-1];
				if (right.isLetterOrNumber() && text.size()>0) {
					QChar left = text[0];
					if (left.isLetterOrNumber()) {
						padding = 1;
					}
				}
			}
		}
		prevX = item->bbox().right();
		buffer.resize(buffer.length() + padding, ' ');
		buffer.append(text);

		nextY = item->bbox().top();
	}
	return buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HOCRQPainterIndentedTextPrinter::HOCRQPainterIndentedTextPrinter(QPainter* painter) : m_painter(painter) {}

void HOCRQPainterIndentedTextPrinter::printPage(const HOCRItem* item, const HOCRIndentedTextExporter::IndentedTextSettings& settings) {
    m_settings = &settings; 
	const int barHeight = 3;
	if (m_settings->m_guideBars) {
		for (double pos = m_settings->m_originY; pos<item->bbox().height(); pos += 2*barHeight * m_settings->m_cellHeight) {
			drawBar(QRect(0,pos,item->bbox().width(), barHeight * m_settings->m_cellHeight));
		}
	}
	HOCRIndentedTextPrinter::printPage(item, settings);
}

void HOCRQPainterIndentedTextPrinter::setFontFamily(const QString& family, bool bold, bool italic) {
	float curSize = m_curFont.pointSize();
	if(m_fontDatabase.hasFamily(family)) {
		m_curFont.setFamily(family);
	}
	m_curFont.setPointSize(curSize);
	m_curFont.setBold(bold);
	m_curFont.setItalic(italic);
	m_painter->setFont(m_curFont);
}

void HOCRQPainterIndentedTextPrinter::setFontSize(double pointSize) {
	if(pointSize != m_curFont.pointSize()) {
		m_curFont.setPointSize(pointSize);
		m_painter->setFont(m_curFont);
	}
}

void HOCRQPainterIndentedTextPrinter::drawText(double x, double y, const QString& text) {
	m_painter->save();
	m_painter->scale(m_settings->m_fontStretch, 1.0);
	m_painter->drawText(QRect(x,y,10000,10000), text);
	m_painter->restore();
}

void HOCRQPainterIndentedTextPrinter::drawBlank(double x, double y, const QString& text) {
	if (!m_settings->m_guideBars) {
		return;
	}
	m_painter->save();
	m_painter->scale(m_settings->m_fontStretch, 1.0);
	m_painter->drawText(QRect(x,y,10000,10000), text);
	m_painter->restore();
}

void HOCRQPainterIndentedTextPrinter::drawBar(const QRect& box) {
	m_painter->save();
	QPen pen = m_painter->pen();
	pen.setWidth(2);
	m_painter->setPen(pen);
	m_painter->setBrush(QColor(0,0,32,32));
	m_painter->fillRect(box, m_painter->brush());
	m_painter->restore();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

HOCRQPrinterIndentedTextPrinter::HOCRQPrinterIndentedTextPrinter(QTextStream* stream) : m_stream(stream) {}

void HOCRQPrinterIndentedTextPrinter::printPage(const HOCRItem* item, const HOCRIndentedTextExporter::IndentedTextSettings& settings) {
	HOCRIndentedTextPrinter::printPage(item, settings);
	*m_stream << "\f";
}

void HOCRQPrinterIndentedTextPrinter::drawText(double x, double y, const QString& text) {
	*m_stream << text << "\n";
}

void HOCRQPrinterIndentedTextPrinter::drawBlank(double x, double y, const QString& text) {
	*m_stream << "\n";
}
