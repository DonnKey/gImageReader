/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRIndentedTextExportWidget.hh
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

#ifndef HOCRINDENTEDTEXTEXPORTWIDGET_HH
#define HOCRINDENTEDTEXTEXPORTWIDGET_HH

#include <QImage>
#include <QVector>

#include "HOCRIndentedTextExporter.hh"
#include "ui_IndentedTextExportWidget.h"

class QGraphicsPixmapItem;

class DisplayerToolHOCR;
class HOCRDocument;
class HOCRPage;


class HOCRIndentedTextExportWidget : public HOCRExporterWidget {
	Q_OBJECT
public:
	HOCRIndentedTextExportWidget(DisplayerToolHOCR* displayerTool, const HOCRDocument* hocrdocument = nullptr, const HOCRPage* hocrpage = nullptr, QWidget* parent = nullptr);
	~HOCRIndentedTextExportWidget();
	void setPreviewPage(const HOCRDocument* hocrdocument, const HOCRPage* hocrpage);
	HOCRIndentedTextExporter::ExporterSettings& getSettings() const override;

	friend class HOCRIndentedTextExportDialog;

private:
	bool findOrigin(const HOCRItem *item);
	void findCells(const HOCRItem *item);

	Ui::IndentedTextExportWidget ui;
	QGraphicsPixmapItem* m_preview = nullptr;
	DisplayerToolHOCR* m_displayerTool;
	const HOCRDocument* m_document = nullptr;
	const HOCRPage* m_previewPage;
	mutable HOCRIndentedTextExporter::IndentedTextSettings m_settings;

private slots:
	void updatePreview();
	void computeOrigin();
	void computeCell();
};

#endif // HOCRINDENTEDTEXTEXPORTWIDGET_HH