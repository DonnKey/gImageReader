/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRIndentedTextExporter.hh
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

#ifndef HOCRINDENTEDTEXTEXPORTER_HH
#define HOCRINDENTEDTEXTEXPORTER_HH

#include "common.hh"
#include "HOCRExporter.hh"

#include <QString>
#include <QFontDatabase>

class QTextStream;
class DisplayerToolHOCR;
class HOCRItem;
class HOCRPage;
class HOCRIndentedTextExportWidget;

class HOCRIndentedTextExporter : public HOCRExporter {
	Q_OBJECT
public:
	struct IndentedTextSettings : ExporterSettings {
		int m_originX;
		int m_originY;
		double m_cellWidth;
		double m_cellHeight;
		QString m_fontFamily;
		int m_fontSize;
		double m_fontStretch;
		bool m_guideBars;
	};

	bool run(const HOCRDocument* hocrdocument, const QString& outname, const ExporterSettings* settings = nullptr) override;
};

class HOCRIndentedTextExportDialog : public QDialog {
	Q_OBJECT
public:
	HOCRIndentedTextExportDialog(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument, const HOCRPage* hocrpage, QWidget* parent = nullptr);
	HOCRIndentedTextExporter::IndentedTextSettings& getIndentedTextSettings() const;

private:
	HOCRIndentedTextExportWidget* m_widget = nullptr;
};

class HOCRIndentedTextPrinter : public QObject {
	Q_OBJECT
public:
	virtual void setFontFamily(const QString& family, bool bold, bool italic) = 0;
	virtual void setFontSize(double pointSize) = 0;
	virtual void drawText(double x, double y, const QString& text) = 0;
	virtual void drawBlank(double x, double y, const QString& text) = 0;
	virtual void drawBar(const QRect& box) = 0;
	virtual void printPage(const HOCRItem* item, const HOCRIndentedTextExporter::IndentedTextSettings& pdfSettings) = 0;

protected:
	const HOCRIndentedTextExporter::IndentedTextSettings* m_settings;

private:
	void printChildren(const HOCRItem* item);
	QString buildLine(const HOCRItem* line, double& nextY);
	double m_currentY;
};

class HOCRQPainterIndentedTextPrinter : public HOCRIndentedTextPrinter {
public:
	HOCRQPainterIndentedTextPrinter(QPainter* painter);
	void setFontFamily(const QString& family, bool bold, bool italic) override;
	void setFontSize(double pointSize) override;
	void drawText(double x, double y, const QString& text) override;
	void drawBlank(double x, double y, const QString& text) override;
	void drawBar(const QRect& box) override;
	void printPage(const HOCRItem* item, const HOCRIndentedTextExporter::IndentedTextSettings& pdfSettings) override;

private:
	QFontDatabase m_fontDatabase;
	QPainter* m_painter;
	QFont m_curFont;
};

class HOCRQPrinterIndentedTextPrinter : public HOCRIndentedTextPrinter {
public:
	HOCRQPrinterIndentedTextPrinter(QTextStream* stream);
	void setFontFamily(const QString& family, bool bold, bool italic) override {};
	void setFontSize(double pointSize) override {};
	void drawText(double x, double y, const QString& text) override;
	void drawBlank(double x, double y, const QString& text) override;
	void drawBar(const QRect& box) override {};
	void printPage(const HOCRItem* item, const HOCRIndentedTextExporter::IndentedTextSettings& pdfSettings) override;

private:
	QTextStream* m_stream;
};

#endif // HOCRINDENTEDTEXTEXPORTER_HH
