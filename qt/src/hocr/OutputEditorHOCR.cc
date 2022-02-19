/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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

#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QFileInfo>
#include <QFontComboBox>
#include <QImage>
#include <QInputDialog>
#include <QShortcut>
#include <QStyledItemDelegate>
#include <QMessageBox>
#include <QPointer>
#include <QStandardItemModel>
#include <QSyntaxHighlighter>
#include <QTimer>
#include <QtSpell.hpp>
#include <QRadioButton>
#include <QButtonGroup>
#include <QTextEdit>
#include <algorithm>
#include <cmath>
#include <cstring>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#undef USE_STD_NAMESPACE

#include "ConfigSettings.hh"
#include "DisplayerToolHOCR.hh"
#include "FileDialogs.hh"
#include "HOCRDocument.hh"
#include "HOCRNormalize.hh"
#include "HOCROdtExporter.hh"
#include "HOCRPdfExporter.hh"
#include "HOCRProofReadWidget.hh"
#include "HOCRTextExporter.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"


class OutputEditorHOCR::HTMLHighlighter : public QSyntaxHighlighter {
public:
	HTMLHighlighter(QTextDocument* document) : QSyntaxHighlighter(document) {
		mFormatMap[NormalState].setForeground(QColor(Qt::black));
		mFormatMap[InTag].setForeground(QColor(75, 75, 255));
		mFormatMap[InAttrKey].setForeground(QColor(75, 200, 75));
		mFormatMap[InAttrValue].setForeground(QColor(255, 75, 75));
		mFormatMap[InAttrValueDblQuote].setForeground(QColor(255, 75, 75));

		mStateMap[NormalState].append({QRegularExpression("<"), InTag, false});
		mStateMap[InTag].append({QRegularExpression(">"), NormalState, true});
		mStateMap[InTag].append({QRegularExpression("\\w+="), InAttrKey, false});
		mStateMap[InAttrKey].append({QRegularExpression("'"), InAttrValue, false});
		mStateMap[InAttrKey].append({QRegularExpression("\""), InAttrValueDblQuote, false});
		mStateMap[InAttrKey].append({QRegularExpression("\\s"), NormalState, false});
		mStateMap[InAttrValue].append({QRegularExpression("'[^']*'"), InTag, true});
		mStateMap[InAttrValueDblQuote].append({QRegularExpression("\"[^\"]*\""), InTag, true});
	}

private:
	enum State { NormalState = -1, InComment, InTag, InAttrKey, InAttrValue, InAttrValueDblQuote };
	struct Rule {
		QRegularExpression pattern;
		State nextState;
		bool addMatched; // add matched length to pos
	};

	QMap<State, QTextCharFormat> mFormatMap;
	QMap<State, QList<Rule>> mStateMap;

	void highlightBlock(const QString& text) override {
		int pos = 0;
		int len = text.length();
		State state = static_cast<State>(previousBlockState());
		while(pos < len) {
			State minState = state;
			int minPos = -1;
			for(const Rule& rule : mStateMap.value(state)) {
				QRegularExpressionMatch match = rule.pattern.match(text, pos);
				if(match.hasMatch() && (minPos < 0 || match.capturedStart() < minPos)) {
					minPos = match.capturedStart() + (rule.addMatched ? match.capturedLength() : 0);
					minState = rule.nextState;
				}
			}
			if(minPos == -1) {
				setFormat(pos, len - pos, mFormatMap[state]);
				pos = len;
			} else {
				setFormat(pos, minPos - pos, mFormatMap[state]);
				pos = minPos;
				state = minState;
			}
		}
		setCurrentBlockState(state);
	}
};

///////////////////////////////////////////////////////////////////////////////

HOCRAttributeEditor::HOCRAttributeEditor(const QString& value, HOCRDocument* doc, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass)
	: QLineEdit(value), m_doc(doc), m_itemIndex(itemIndex), m_attrName(attrName), m_origValue(value), m_attrItemClass(attrItemClass), m_edited(false) {
	setFrame(false);
	connect(m_doc, &HOCRDocument::itemAttributeChanged, this, 
		[this] (const QModelIndex& index, const QString& name, const QString& value) {
			m_edited = false;
			updateValue(index, name, value);
		});
	connect(this, &HOCRAttributeEditor::textChanged, this, &HOCRAttributeEditor::testForPick);
	connect(this, &QLineEdit::returnPressed, this, [this] { validateChanges(true); });
	connect(this, &HOCRAttributeEditor::textEdited, this, [this] { m_edited = true; validateChanges(); });
}

void HOCRAttributeEditor::testForPick() {
	if (!m_edited) {
		validateChanges(true);
	}
}

void HOCRAttributeEditor::focusOutEvent(QFocusEvent* ev) {
	QLineEdit::focusOutEvent(ev);
	validateChanges();
}

void HOCRAttributeEditor::updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value) {
	if(itemIndex == m_itemIndex && name == m_attrName) {
		blockSignals(true);
		setText(value);
		blockSignals(false);
	}
}

void HOCRAttributeEditor::validateChanges(bool force) {
	if(!hasFocus() || force) {
		int pos;
		QString newValue = text();
		if(newValue == m_origValue) {
			return;
		}
		if(validator() && validator()->validate(newValue, pos) != QValidator::Acceptable) {
			setText(m_origValue);
		} else {
			m_doc->editItemAttribute(m_itemIndex, m_attrName, newValue, m_attrItemClass);
			m_origValue = newValue;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

HOCRAttributeCheckbox::HOCRAttributeCheckbox(Qt::CheckState value, HOCRDocument* doc, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass)
	: m_doc(doc), m_itemIndex(itemIndex), m_attrName(attrName), m_attrItemClass(attrItemClass) {
	setCheckState(value);
	connect(m_doc, &HOCRDocument::itemAttributeChanged, this, &HOCRAttributeCheckbox::updateValue);
	connect(this, &HOCRAttributeCheckbox::stateChanged, this, &HOCRAttributeCheckbox::valueChanged);
}

void HOCRAttributeCheckbox::updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value) {
	if(itemIndex == m_itemIndex && name == m_attrName) {
		blockSignals(true);
		setChecked(value == "1");
		blockSignals(false);
	}
}

void HOCRAttributeCheckbox::valueChanged() {
	m_doc->editItemAttribute(m_itemIndex, m_attrName, isChecked() ? "1" : "0", m_attrItemClass);
}

///////////////////////////////////////////////////////////////////////////////


HOCRAttributeLangCombo::HOCRAttributeLangCombo(const QString& value, bool multiple, HOCRDocument* doc, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass)
	: m_doc(doc), m_itemIndex(itemIndex), m_attrName(attrName), m_attrItemClass(attrItemClass) {
	if(multiple) {
		addItem(_("Multiple values"));
		setCurrentIndex(0);
		QStandardItemModel* itemModel = qobject_cast<QStandardItemModel*>(model());
		itemModel->item(0)->setFlags(itemModel->item(0)->flags() & ~Qt::ItemIsEnabled);
	}
	for(const QString& code : QtSpell::Checker::getLanguageList()) {
		QString text = QtSpell::Checker::decodeLanguageCode(code);
		addItem(text, code);
	}
	if(!multiple) {
		setCurrentIndex(findData(value));
	}
	connect(m_doc, &HOCRDocument::itemAttributeChanged, this, &HOCRAttributeLangCombo::updateValue);
	connect(this, qOverload<int>(&HOCRAttributeLangCombo::currentIndexChanged), this, &HOCRAttributeLangCombo::valueChanged);
}

void HOCRAttributeLangCombo::updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value) {
	if(itemIndex == m_itemIndex && name == m_attrName) {
		blockSignals(true);
		setCurrentIndex(findData(value));
		blockSignals(false);
	}
}

void HOCRAttributeLangCombo::valueChanged() {
	m_doc->editItemAttribute(m_itemIndex, m_attrName, currentData().toString(), m_attrItemClass);
}

///////////////////////////////////////////////////////////////////////////////

class HOCRTextDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /* option */, const QModelIndex& /* index */) const {
		return new QLineEdit(parent);
	}
	void setEditorData(QWidget* editor, const QModelIndex& index) const {
		m_currentIndex = index;
		m_currentEditor = static_cast<QLineEdit*>(editor);
		static_cast<QLineEdit*>(editor)->setText(index.model()->data(index, Qt::EditRole).toString());
	}
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
		model->setData(index, static_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
	}
	const QModelIndex& getCurrentIndex() const {
		return m_currentIndex;
	}
	QLineEdit* getCurrentEditor() const {
		return m_currentEditor;
	}

private:
	mutable QModelIndex m_currentIndex;
	mutable QPointer<QLineEdit> m_currentEditor;
};

///////////////////////////////////////////////////////////////////////////////

void OutputEditorHOCR::HOCRBatchProcessor::writeHeader(QIODevice* dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo) const {
	QString header = QString(
	                     "<!DOCTYPE html>\n"
	                     "<html>\n"
	                     "<head>\n"
	                     " <title>%1</title>\n"
	                     " <meta charset=\"utf-8\" /> \n"
	                     " <meta name='ocr-system' content='tesseract %2' />\n"
	                     " <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
	                     "</head><body>\n").arg(QFileInfo(pageInfo.filename).fileName()).arg(tess->Version());
	dev->write(header.toUtf8());
}

void OutputEditorHOCR::HOCRBatchProcessor::writeFooter(QIODevice* dev) const {
	dev->write("</body></html>\n");
}

void OutputEditorHOCR::HOCRBatchProcessor::appendOutput(QIODevice* dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfos, bool /*firstArea*/) const {
	char* text = tess->GetHOCRText(pageInfos.page);
	QDomDocument doc;
	doc.setContent(QString::fromUtf8(text));
	delete[] text;

	QDomElement pageDiv = doc.firstChildElement("div");
	QMap<QString, QString> attrs = HOCRItem::deserializeAttrGroup(pageDiv.attribute("title"));
	// This works because in batch mode the output is created next to the source image
	attrs["image"] = QString("'./%1'").arg(QFileInfo(pageInfos.filename).fileName());
	attrs["ppageno"] = QString::number(pageInfos.page);
	attrs["rot"] = QString::number(pageInfos.angle);
	attrs["res"] = QString::number(pageInfos.resolution);
	attrs["x_tesspsm"] = QString::number(static_cast<int>(pageInfos.mode));
	pageDiv.setAttribute("title", HOCRItem::serializeAttrGroup(attrs));
	dev->write(doc.toByteArray());
}

///////////////////////////////////////////////////////////////////////////////

Q_DECLARE_METATYPE(OutputEditorHOCR::HOCRReadSessionData)

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool) {
	static int reg = qRegisterMetaType<QList<QRect>>("QList<QRect>");
	Q_UNUSED(reg);

	static int reg2 = qRegisterMetaType<HOCRReadSessionData>("HOCRReadSessionData");
	Q_UNUSED(reg2);

	m_tool = tool;
	m_widget = new QWidget;
	ui.setupUi(m_widget);
	m_highlighter = new HTMLHighlighter(ui.plainTextEditOutput->document());

	m_preview = new QGraphicsPixmapItem();
	m_preview->setTransformationMode(Qt::SmoothTransformation);
	m_preview->setZValue(2);
	MAIN->getDisplayer()->scene()->addItem(m_preview);
	m_previewTimer.setSingleShot(true);

	ui.actionOutputReplaceKey->setShortcut(Qt::CTRL | Qt::Key_F);
	ui.actionOutputSaveHOCR->setShortcut(Qt::CTRL | Qt::Key_S);
	ui.actionNavigateNext->setShortcut(Qt::Key_F3);
	ui.actionNavigatePrev->setShortcut(Qt::SHIFT | Qt::Key_F3);

	m_document = new HOCRDocument(ui.treeViewHOCR);
	ui.treeViewHOCR->setModel(m_document);
	ui.treeViewHOCR->setContextMenuPolicy(Qt::CustomContextMenu);
	ui.treeViewHOCR->header()->setStretchLastSection(false);
	ui.treeViewHOCR->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui.treeViewHOCR->setColumnWidth(1, 32);
	ui.treeViewHOCR->setItemDelegateForColumn(0, new HOCRTextDelegate(ui.treeViewHOCR));
	ui.treeViewHOCR->setCursor(Qt::PointingHandCursor);
	ui.treeViewHOCR->installEventFilter(this);


	m_proofReadWidget = new HOCRProofReadWidget(ui.treeViewHOCR, MAIN->getDisplayer());
	m_proofReadWidget->hide();

	MAIN->getDisplayer()->installEventFilter(this);

	ui.comboBoxNavigate->addItem(_("Page"), "ocr_page");
	ui.comboBoxNavigate->addItem(_("Block"), "ocr_carea");
	ui.comboBoxNavigate->addItem(_("Paragraph"), "ocr_par");
	ui.comboBoxNavigate->addItem(_("Line"), "ocr_line");
	ui.comboBoxNavigate->addItem(_("Word"), "ocrx_word");
	ui.comboBoxNavigate->addItem(_("Misspelled word"), "ocrx_word_bad");
	ui.comboBoxNavigate->addItem(_("Low confidence word"), "ocrx_word_lowconf");
	m_blinkTimer = new QTimer(this);

	QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_Delete), m_widget);
	QObject::connect(shortcut, &QShortcut::activated, this, &OutputEditorHOCR::removeCurrentItem);

	ui.actionInsertModeAppend->setData(static_cast<int>(InsertMode::Append));
	ui.actionInsertModeBefore->setData(static_cast<int>(InsertMode::InsertBefore));

	connect(ui.menuInsertMode, &QMenu::triggered, this, &OutputEditorHOCR::setInsertMode);
	connect(ui.checkBoxReplace, &QCheckBox::toggled, this, &OutputEditorHOCR::setReplaceMode);
	connect(ui.toolButtonOpen, &QToolButton::clicked, this, [this] { open(InsertMode::Replace); });
	connect(ui.actionOpenAppend, &QAction::triggered, this, [this] { open(InsertMode::Append); });
	connect(ui.actionOpenInsertBefore, &QAction::triggered, this, [this] { open(InsertMode::InsertBefore); });
	connect(ui.actionOutputSaveHOCR, &QAction::triggered, this, [this] { save(); });
	connect(ui.actionOutputExportODT, &QAction::triggered, this, &OutputEditorHOCR::exportToODT);
	connect(ui.actionOutputExportPDF, &QAction::triggered, this, &OutputEditorHOCR::exportToPDF);
	connect(ui.actionOutputExportText, &QAction::triggered, this, &OutputEditorHOCR::exportToText);
	connect(ui.actionOutputClear, &QAction::triggered, this, &OutputEditorHOCR::clear);
	connect(ui.actionOutputReplace, &QAction::triggered, this, [this] { doReplace(false); });
	connect(ui.actionOutputReplaceKey, &QAction::triggered, this, [this] { doReplace(true); });
	connect(ui.actionToggleWConf, &QAction::triggered, this, &OutputEditorHOCR::toggleWConfColumn);
	connect(ui.actionPreview, &QAction::toggled, this, &OutputEditorHOCR::previewToggled);
	connect(ui.actionProofread, &QAction::toggled, m_proofReadWidget, &HOCRProofReadWidget::setProofreadEnabled);
	connect(&m_previewTimer, &QTimer::timeout, this, &OutputEditorHOCR::updatePreview);
	connect(ui.searchFrame, &SearchReplaceFrame::findReplace, this, &OutputEditorHOCR::findReplace);
	connect(ui.searchFrame, &SearchReplaceFrame::replaceAll, this, &OutputEditorHOCR::replaceAll);
	connect(ui.searchFrame, &SearchReplaceFrame::applySubstitutions, this, &OutputEditorHOCR::applySubstitutions);
	connect(ConfigSettings::get<FontSetting>("customoutputfont"), &FontSetting::changed, this, &OutputEditorHOCR::setFont);
	connect(ConfigSettings::get<SwitchSetting>("systemoutputfont"), &FontSetting::changed, this, &OutputEditorHOCR::setFont);
	connect(ui.treeViewHOCR->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &OutputEditorHOCR::showItemProperties);
	connect(ui.treeViewHOCR, &QTreeView::customContextMenuRequested, this, &OutputEditorHOCR::showTreeWidgetContextMenu);
	connect(this, &OutputEditorHOCR::customContextMenuRequested2, this, &OutputEditorHOCR::showTreeWidgetContextMenu_inner);
	connect(ui.tabWidgetProps, &QTabWidget::currentChanged, this, &OutputEditorHOCR::updateSourceText);
	connect(m_tool, &DisplayerToolHOCR::bboxChanged, this, &OutputEditorHOCR::updateCurrentItemBBox);
	connect(m_tool, &DisplayerToolHOCR::bboxDrawn, this, &OutputEditorHOCR::bboxDrawn);
	connect(m_tool, &DisplayerToolHOCR::positionPicked, this, &OutputEditorHOCR::pickItem);
	connect(m_document, &HOCRDocument::dataChanged, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::rowsInserted, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::rowsRemoved, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::itemAttributeChanged, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::itemAttributeChanged, this, &OutputEditorHOCR::updateSourceText);
	connect(m_document, &HOCRDocument::itemAttributeChanged, this, &OutputEditorHOCR::itemAttributeChanged);
	connect(ui.comboBoxNavigate, qOverload<int>(&QComboBox::currentIndexChanged), this, &OutputEditorHOCR::navigateTargetChanged);
	connect(m_blinkTimer, &QTimer::timeout, this, &OutputEditorHOCR::blinkCombo);
	connect(ui.actionNavigateNext, &QAction::triggered, this, &OutputEditorHOCR::navigateNext);
	connect(ui.actionNavigatePrev, &QAction::triggered, this, &OutputEditorHOCR::navigatePrev);
	connect(ui.actionExpandAll, &QAction::triggered, this, &OutputEditorHOCR::expandItemClass);
	connect(ui.actionCollapseAll, &QAction::triggered, this, &OutputEditorHOCR::collapseItemClass);
	connect(MAIN->getDisplayer(), &Displayer::imageChanged, this, &OutputEditorHOCR::sourceChanged);

	ADD_SETTING(SwitchSetting("replacescans", ui.checkBoxReplace, false));
	ADD_SETTING(ActionSetting("displayconfidence", ui.actionToggleWConf, false));
	ADD_SETTING(ActionSetting("displaypreview", ui.actionPreview, false));

	setFont();

	// Deferred until connect()s are done
	ui.treeViewHOCR->setColumnHidden(1, !ConfigSettings::get<ActionSetting>("displayconfidence")->getValue());
}

OutputEditorHOCR::~OutputEditorHOCR() {
	m_previewTimer.stop();
	delete m_preview;
	delete m_proofReadWidget;
	delete m_widget;
}

void OutputEditorHOCR::setFont() {
	if(ConfigSettings::get<SwitchSetting>("systemoutputfont")->getValue()) {
		ui.plainTextEditOutput->setFont(QFont());
	} else {
		ui.plainTextEditOutput->setFont(ConfigSettings::get<FontSetting>("customoutputfont")->getValue());
	}
}

void OutputEditorHOCR::setInsertMode(QAction* action) {
	m_insertMode = static_cast<InsertMode>(action->data().value<int>());
	ui.toolButtonInsertMode->setIcon(action->icon());
}

void OutputEditorHOCR::setReplaceMode() {
	m_replace = ConfigSettings::get<SwitchSetting>("replacescans")->getValue();
}

void OutputEditorHOCR::setModified() {
	ui.actionOutputSaveHOCR->setEnabled(m_document->pageCount() > 0);
	ui.toolButtonOutputExport->setEnabled(m_document->pageCount() > 0);
	ui.toolBarNavigate->setEnabled(m_document->pageCount() > 0);
    if(!m_previewTimer.isActive()) { 
		m_previewTimer.start(100); // Use a timer because setModified is potentially called a large number of times when the HOCR tree changes
		// but don't let auto-repeat keys block all updates.
	}
	m_modified = true;
}

OutputEditorHOCR::ReadSessionData* OutputEditorHOCR::initRead(tesseract::TessBaseAPI& tess) {
	HOCRReadSessionData* data = new HOCRReadSessionData;
	data->beginIndex = data->insertIndex = m_insertMode == InsertMode::Append ? m_document->pageCount() : currentPage();
	data->removeIndex = -1;
	data->pageInfo.mode = tess.GetPageSegMode();
	return data;
}

void OutputEditorHOCR::setupPage(ReadSessionData *d, QString& oldSource, int oldPage) {
	HOCRReadSessionData *data = static_cast<HOCRReadSessionData*>(d);
	if (!m_replace) {
		return;
	}
	int position = positionOf(oldSource, oldPage);
	if (position < 0) {
		data->removeIndex = -1;
		return;
	}
	data->insertIndex = position;
	data->removeIndex = position;
	data->beginIndex = position;
}

void OutputEditorHOCR::read(tesseract::TessBaseAPI& tess, ReadSessionData* data) {
	tess.SetVariable("hocr_font_info", "true");
	char* text = tess.GetHOCRText(data->pageInfo.page);
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	QMetaObject::invokeMethod(this, "addPage", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(text)), Q_ARG(HOCRReadSessionData, *hdata));
	delete[] text;
	++hdata->insertIndex;
}

void OutputEditorHOCR::readError(const QString& errorMsg, ReadSessionData* data) {
	static_cast<HOCRReadSessionData*>(data)->errors.append(QString("%1[%2]: %3").arg(data->pageInfo.filename).arg(data->pageInfo.page).arg(errorMsg));
}

void OutputEditorHOCR::finalizeRead(ReadSessionData* data) {
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	if(!hdata->errors.isEmpty()) {
		QString message = QString(_("The following pages could not be processed:\n%1").arg(hdata->errors.join("\n")));
		QMessageBox::warning(MAIN, _("Recognition errors"), message);
	}
	selectPage(hdata->beginIndex);
	OutputEditor::finalizeRead(data);
}

void OutputEditorHOCR::addPage(const QString& hocrText, HOCRReadSessionData data) {
	QDomDocument doc;
	doc.setContent(hocrText);

	QDomElement pageDiv = doc.firstChildElement("div");
	QMap<QString, QString> attrs = HOCRItem::deserializeAttrGroup(pageDiv.attribute("title"));
	attrs["image"] = QString("'%1'").arg(data.pageInfo.filename);
	attrs["ppageno"] = QString::number(data.pageInfo.page);
	attrs["rot"] = QString::number(data.pageInfo.angle);
	attrs["res"] = QString::number(data.pageInfo.resolution);
	attrs["x_tesspsm"] = QString::number(data.pageInfo.mode);
	pageDiv.setAttribute("title", HOCRItem::serializeAttrGroup(attrs));

	if (data.removeIndex >= 0) {
		removePageByPosition(data.removeIndex);
		data.removeIndex = -1;
	}

	QModelIndex index = m_document->insertPage(data.insertIndex, pageDiv, true);

	expandCollapseChildren(index, true);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;

	selectPage(data.insertIndex);
}

int OutputEditorHOCR::positionOf(const QString& source, int sourcePage) const {
	if (source.isEmpty() || sourcePage < 0) {
		return -1;
	}
	for(int i = 0, n = m_document->pageCount(); i < n; ++i) {
		const HOCRPage* page = m_document->page(i);
		if(page->pageNr() == sourcePage && page->sourceFile() == source) {
			return i;
		}
	}
	return -1;
}

void OutputEditorHOCR::navigateTargetChanged() {
	QString target = ui.comboBoxNavigate->itemData(ui.comboBoxNavigate->currentIndex()).toString();
	bool allowExpandCollapse = !target.startsWith("ocrx_word");
	ui.actionExpandAll->setEnabled(allowExpandCollapse);
	ui.actionCollapseAll->setEnabled(allowExpandCollapse);
}

void OutputEditorHOCR::expandCollapseItemClass(bool expand) {
	QString target = ui.comboBoxNavigate->itemData(ui.comboBoxNavigate->currentIndex()).toString();
	QModelIndex start = m_document->index(0, 0);
	QModelIndex next = start;
	do {
		const HOCRItem* item = m_document->itemAtIndex(next);
		if(item && item->itemClass() == target) {
			if(expand) {
				ui.treeViewHOCR->setExpanded(next, expand);
				for(QModelIndex parent = next.parent(); parent.isValid(); parent = parent.parent()) {
					ui.treeViewHOCR->setExpanded(parent, true);
				}
				for(QModelIndex child = m_document->index(0, 0, next); child.isValid(); child = child.sibling(child.row() + 1, 0)) {
					expandCollapseChildren(child, false);
				}
			} else {
				expandCollapseChildren(next, false);
			}
		}
		next = m_document->nextIndex(next);
	} while(next != start);
	ui.treeViewHOCR->scrollTo(ui.treeViewHOCR->currentIndex());
}

void OutputEditorHOCR::blinkCombo() {
	if (m_blinkCounter == 0) {
		m_blinkCounter = 8;
		m_blinkTimer->start(500);
	}
	if (m_blinkCounter-- % 2 == 1) {
		ui.comboBoxNavigate->setStyleSheet("");
	} else {
		ui.comboBoxNavigate->setStyleSheet("background-color: red");
	}
	if (m_blinkCounter <= 0) {
		m_blinkTimer->stop();
	}
}

void OutputEditorHOCR::navigateNextPrev(bool next) {
	QString target = ui.comboBoxNavigate->itemData(ui.comboBoxNavigate->currentIndex()).toString();
	bool misspelled = false;
	bool lowconf = false;
	if(target == "ocrx_word_bad") {
		target = "ocrx_word";
		misspelled = true;
	} else if(target == "ocrx_word_lowconf") {
		target = "ocrx_word";
		lowconf = true;
	}
	QModelIndex start = ui.treeViewHOCR->currentIndex();
	QModelIndex found = m_document->prevOrNextIndex(next, start, target, misspelled, lowconf);
	if (found == start) {
		blinkCombo();
	}
	ui.treeViewHOCR->setCurrentIndex(found);
}

void OutputEditorHOCR::expandCollapseChildren(const QModelIndex& index, bool expand) const {
	int nChildren = m_document->rowCount(index);
	if(nChildren > 0) {
		ui.treeViewHOCR->setExpanded(index, expand);
		for(int i = 0; i < nChildren; ++i) {
			expandCollapseChildren(m_document->index(i, 0, index), expand);
		}
	}
}


bool OutputEditorHOCR::newPage(const HOCRPage* page) {
	// Change to a *single* new page. See singleSelect in addSource().
	return page && MAIN->getSourceManager()->addSource(page->sourceFile(), true, true) && MAIN->getDisplayer()->setup(&page->pageNr(), &page->resolution(), &page->angle());
}

int OutputEditorHOCR::currentPage() {
	QModelIndexList selected = ui.treeViewHOCR->selectionModel()->selectedIndexes();
	if(selected.isEmpty()) {
		return m_document->pageCount();
	}
	QModelIndex index = selected.front();
	if(!index.isValid()) {
		return m_document->pageCount();
	}
	while(index.parent().isValid()) {
		index = index.parent();
	}
	return index.row();
}

void OutputEditorHOCR::showItemProperties(const QModelIndex& index, const QModelIndex& prev) {
	m_tool->setAction(DisplayerToolHOCR::ACTION_NONE);
	const HOCRItem* prevItem = m_document->itemAtIndex(prev);
	ui.tableWidgetProperties->setRowCount(0);
	ui.plainTextEditOutput->setPlainText("");

	const HOCRItem* currentItem = m_document->itemAtIndex(index);
	if(!currentItem) {
		m_tool->clearSelection();
		return;
	}
	const HOCRPage* page = currentItem->page();

	int row = -1;
	QMap<QString, QString> attrs = currentItem->getAllAttributes();
	for(auto it = attrs.begin(), itEnd = attrs.end(); it != itEnd; ++it) {
		QString attrName = it.key();
		if(attrName == "class" || attrName == "id") {
			continue;
		}
		QStringList parts = attrName.split(":");
		ui.tableWidgetProperties->insertRow(++row);
		QTableWidgetItem* attrNameItem = new QTableWidgetItem(parts.last());
		attrNameItem->setFlags(attrNameItem->flags() & ~Qt::ItemIsEditable);
		ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
		ui.tableWidgetProperties->setCellWidget(row, 1, createAttrWidget(index, attrName, it.value()));
	}

	// ocr_class:attr_key:attr_values
	QMap<QString, QMap<QString, QSet<QString>>> occurrences;
	currentItem->getPropagatableAttributes(occurrences);
	for(auto it = occurrences.begin(), itEnd = occurrences.end(); it != itEnd; ++it) {
		ui.tableWidgetProperties->insertRow(++row);
		QTableWidgetItem* sectionItem = new QTableWidgetItem(it.key());
		sectionItem->setFlags(sectionItem->flags() & ~(Qt::ItemIsEditable | Qt::ItemIsSelectable));
		sectionItem->setBackground(Qt::lightGray);
		QFont sectionFont = sectionItem->font();
		sectionFont.setBold(true);
		sectionItem->setFont(sectionFont);
		ui.tableWidgetProperties->setItem(row, 0, sectionItem);
		ui.tableWidgetProperties->setSpan(row, 0, 1, 2);
		for(auto attrIt = it.value().begin(), attrItEnd = it.value().end(); attrIt != attrItEnd; ++attrIt) {
			const QString& attrName = attrIt.key();
			const QSet<QString>& attrValues = attrIt.value();
			int attrValueCount = attrValues.size();
			ui.tableWidgetProperties->insertRow(++row);
			QStringList parts = attrName.split(":");
			QTableWidgetItem* attrNameItem = new QTableWidgetItem(parts.last());
			attrNameItem->setFlags(attrNameItem->flags() & ~Qt::ItemIsEditable);
			ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
			ui.tableWidgetProperties->setCellWidget(row, 1, createAttrWidget(index, attrName, attrValueCount == 1 ? * (attrValues.begin()) : "", it.key(), attrValueCount > 1));
		}
	}

	ui.plainTextEditOutput->setPlainText(currentItem->toHtml());
	ui.treeViewHOCR->setCurrentIndex(index);

	if(newPage(page)) {
		// Minimum bounding box
		QRect minBBox;
		if(currentItem->itemClass() == "ocr_page") {
			minBBox = currentItem->bbox();
			m_tool->clearSelection();
		} else {
			for(const HOCRItem* child : currentItem->children()) {
				minBBox = minBBox.united(child->bbox());
			}
			m_tool->setSelection(currentItem->bbox(), minBBox);
		}
	}
}

QWidget* OutputEditorHOCR::createAttrWidget(const QModelIndex& itemIndex, const QString& attrName, const QString& attrValue, const QString& attrItemClass, bool multiple) {
	static QMap<QString, QString> attrLineEdits = {
		{"title:bbox", "\\d+\\s+\\d+\\s+\\d+\\s+\\d+"},
		{"title:x_fsize", "\\d+"},
		{"title:baseline", "[-+]?\\d+\\.?\\d*\\s[-+]?\\d+\\.?\\d*"}
	};
	auto it = attrLineEdits.find(attrName);
	if(it != attrLineEdits.end()) {
		QLineEdit* lineEdit = new HOCRAttributeEditor(attrValue, m_document, itemIndex, attrName, attrItemClass);
		lineEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(it.value())));
		if(multiple) {
			lineEdit->setPlaceholderText(_("Multiple values"));
		}
		return lineEdit;
	} else if(attrName == "title:x_font") {
		QFontComboBox* combo = new QFontComboBox();
		combo->setCurrentIndex(-1);
		QLineEdit* edit = new HOCRAttributeEditor(attrValue, m_document, itemIndex, attrName, attrItemClass);
		edit->blockSignals(true); // Because the combobox alters the text as soon as setLineEdit is called...
		combo->setLineEdit(edit);
		edit->setText(attrValue);
		edit->blockSignals(false);
		if(multiple) {
			combo->lineEdit()->setPlaceholderText(_("Multiple values"));
		}
		return combo;
	} else if(attrName == "lang") {
		HOCRAttributeLangCombo* combo = new HOCRAttributeLangCombo(attrValue, multiple, m_document, itemIndex, attrName, attrItemClass);
		return combo;
	} else if(attrName == "bold" || attrName == "italic") {
		Qt::CheckState value = multiple ? Qt::PartiallyChecked : attrValue == "1" ? Qt::Checked : Qt::Unchecked;
		return new HOCRAttributeCheckbox(value, m_document, itemIndex, attrName, attrItemClass);
	} else {
		QLineEdit* lineEdit = new QLineEdit(attrValue);
		lineEdit->setFrame(false);
		lineEdit->setReadOnly(true);
		return lineEdit;
	}
}

void OutputEditorHOCR::updateCurrentItemBBox(QRect bbox) {
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
	m_document->editItemAttribute(current, "title:bbox", bboxstr);
}

void OutputEditorHOCR::updateSourceText() {
	if(ui.tabWidgetProps->currentWidget() == ui.plainTextEditOutput) {
		QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
		const HOCRItem* currentItem = m_document->itemAtIndex(current);
		if(currentItem) {
			ui.plainTextEditOutput->setPlainText(currentItem->toHtml());
		}
	}
}

void OutputEditorHOCR::itemAttributeChanged(const QModelIndex& itemIndex, const QString& name, const QString& /*value*/) {
	const HOCRItem* currentItem = m_document->itemAtIndex(itemIndex);
	if(name == "title:bbox" && currentItem) {
		// Minimum bounding box
		QRect minBBox;
		if(currentItem->itemClass() == "ocr_page") {
			minBBox = currentItem->bbox();
		} else {
			for(const HOCRItem* child : currentItem->children()) {
				minBBox = minBBox.united(child->bbox());
			}
		}
		m_tool->setSelection(currentItem->bbox(), minBBox);
	}
}

class GetWordDialog : public QDialog
{
	//Q_OBJECT
public:
	GetWordDialog(QWidget *parent, const QString& name, NewWordMode initialMode) : QDialog(parent) {
		setWindowTitle(name);
		setObjectName("GetWordDialog dialog");
		QVBoxLayout* mainBox = new QVBoxLayout(this);

		m_policyBoxGroup = new QButtonGroup(mainBox);
		QGroupBox* policies = new QGroupBox(_("Place word..."), this);
		QVBoxLayout* policyBoxLayout = new QVBoxLayout();
		if(initialMode == NewWordMode::CurrentLine) {
			QRadioButton* policy1 = new QRadioButton(_("...in selected Textline"),policies);
			m_policyBoxGroup->addButton(policy1,static_cast<int>(NewWordMode::CurrentLine));
			policy1->setToolTip(_("From the one selected in the HOCR tree"));
			policy1->setChecked(true);
			policyBoxLayout->addWidget(policy1);
		}
		QRadioButton* policy2 = new QRadioButton(_("...in nearest Textline"),policies);
		policy2->setToolTip(_("The nearest above the cursor"));
		policy2->setChecked(initialMode == NewWordMode::NearestLine);
		m_policyBoxGroup->addButton(policy2,static_cast<int>(NewWordMode::NearestLine));
		policyBoxLayout->addWidget(policy2);
		QRadioButton* policy3 = new QRadioButton(_("...in new Textline at cursor"),policies);
		policy3->setToolTip(_("A new textline just below the Textline nearest above cursor"));
		policy3->setChecked(initialMode == NewWordMode::NewLine);
		m_policyBoxGroup->addButton(policy3,static_cast<int>(NewWordMode::NewLine));
		policyBoxLayout->addWidget(policy3);

		policies->setLayout(policyBoxLayout);
		mainBox->addWidget(policies);

		QGroupBox* captionBox = new QGroupBox(this);
		QHBoxLayout* caption = new QHBoxLayout();
		captionBox->setContentsMargins(0, 0, 0, 0);
		caption->setContentsMargins(0, 0, 0, 0);
		caption->addWidget(new QLabel(_("Enter word:")));
		caption->addStretch(1);
		QCheckBox* fitCheckBox = new QCheckBox(_("Fit"));
		fitCheckBox->setToolTip(_("Size new Bounding Box to fit text"));
		QString currentTitle;
		int currentIndex;
		HOCRNormalize().currentDefault(currentTitle, currentIndex);
		QCheckBox* nrmCheckBox = new QCheckBox(_("N%1 ").arg(currentIndex));
		nrmCheckBox->setToolTip(_("Apply most recent normalization (%1)").arg(currentTitle));
		caption->addWidget(nrmCheckBox);
		caption->addWidget(fitCheckBox);
		caption->setSpacing(0);
		captionBox->setLayout(caption);
		captionBox->setStyleSheet("border:none; margin 0px; padding 0px");
		mainBox->addWidget(captionBox);

		m_lineEdit = new QLineEdit(this);
		m_lineEdit->setFocus();
		mainBox->addWidget(m_lineEdit);

		QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		mainBox->addWidget(buttons);

		connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &GetWordDialog::done);
		connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &GetWordDialog::cancel);
		connect(fitCheckBox, &QCheckBox::toggled, this, [](){}); // doesn't work without it!
		connect(nrmCheckBox, &QCheckBox::toggled, this, [](){});

		ADD_SETTING(SwitchSetting("fitNewWord", fitCheckBox, true));
		ADD_SETTING(SwitchSetting("normalizeNewWord", nrmCheckBox, true));

		exec();
		setFocus();
	}

	QString getText(NewWordMode& mode) {
		mode = static_cast<NewWordMode>(m_policyBoxGroup->checkedId());
		return m_text;
	}

	private:
	QButtonGroup* m_policyBoxGroup;
	QString m_text;
	QLineEdit *m_lineEdit;

	private slots:
	void done() {
		m_text = m_lineEdit->text();
		close();
	}
	void cancel() {
		m_text = QString();
		close();
	}
};

QDomElement OutputEditorHOCR::newLine(QDomDocument &doc, const QRect& bbox, QMap<QString, QMap<QString, QSet<QString>>>& propAttrs) {
	QDomElement newElement;

	newElement = doc.createElement("span");
	newElement.setAttribute("class", "ocr_line");
	QSet<QString> propLineBaseline = propAttrs["ocrx_line"]["baseline"];
	// Tesseract does as follows:
	// row_height = x_height + ascenders - descenders
	// font_pt_size = row_height * 72 / dpi (72 = pointsPerInch)
	// As a first approximation, assume x_size = bbox.height() and ascenders = descenders = bbox.height() / 4
	QMap<QString, QString> titleAttrs;
	titleAttrs["bbox"] = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
	titleAttrs["x_ascenders"] = QString("%1").arg(0.25 * bbox.height());
	titleAttrs["x_descenders"] = QString("%1").arg(0.25 * bbox.height());
	titleAttrs["x_size"] = QString("%1").arg(bbox.height());
	titleAttrs["baseline"] = propLineBaseline.size() == 1 ? *propLineBaseline.begin() : QString("0 0");
	newElement.setAttribute("title", HOCRItem::serializeAttrGroup(titleAttrs));
	return newElement;
}

void OutputEditorHOCR::bboxDrawn(const QRect& bbox, int action) {
	QDomDocument doc;
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* currentItem = m_document->itemAtIndex(current);
	if(!currentItem) {
		return;
	}
	QModelIndex index = QModelIndex();

	QMap<QString, QMap<QString, QSet<QString>>> propAttrs;
	currentItem->getPropagatableAttributes(propAttrs);
	QDomElement newElement;
	int newPos = -1; // at end

	if(action == DisplayerToolHOCR::ACTION_DRAW_GRAPHIC_RECT) {
		newElement = doc.createElement("div");
		newElement.setAttribute("class", "ocr_graphic");
		newElement.setAttribute("title", QString("bbox %1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom()));
	} else if(action == DisplayerToolHOCR::ACTION_DRAW_CAREA_RECT) {
		newElement = doc.createElement("div");
		newElement.setAttribute("class", "ocr_carea");
		newElement.setAttribute("title", QString("bbox %1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom()));
	} else if(action == DisplayerToolHOCR::ACTION_DRAW_PAR_RECT) {
		newElement = doc.createElement("p");
		newElement.setAttribute("class", "ocr_par");
		newElement.setAttribute("title", QString("bbox %1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom()));
	} else if(action == DisplayerToolHOCR::ACTION_DRAW_LINE_RECT) {
		newElement = newLine(doc, bbox, propAttrs);
	} else if(action == DisplayerToolHOCR::ACTION_DRAW_WORD_RECT) {
		NewWordMode mode = NewWordMode::CurrentLine;
		QModelIndex foundLine = pickLine(bbox.topLeft());
		const HOCRItem* foundItem = m_document->itemAtIndex(foundLine);
		
		if (bbox.height() == 0) {
			if(!foundItem) {
				return;
			}
			QRectF bigBox = foundItem->bbox();
			// expand top, bottom 10%; left 30%, right 100%
			bigBox = QRect(std::max(0.0, bigBox.x()-bigBox.width()*0.3), std::max(0.0, bigBox.y()-bigBox.height()*0.1), bigBox.width()*2.3, bigBox.height()*1.2);
			if (bigBox.contains(bbox.topLeft())) {
				mode = NewWordMode::NearestLine;
			} else {
				mode = NewWordMode::NewLine;
			}
		}

		QString text = GetWordDialog(m_widget, _("Add Word"), mode).getText(mode);
		if(text.isEmpty()) {
			m_tool->clearSelection();
			return;
		}

		if (mode == NewWordMode::NearestLine) {
			current = foundLine;
			currentItem = foundItem;
			propAttrs.clear();
			currentItem->getPropagatableAttributes(propAttrs);
		}

		if (mode == NewWordMode::NewLine) {
			current = foundLine;
			QModelIndex parent = current.parent();
			const HOCRItem* parentItem = m_document->itemAtIndex(parent);
			int newRow = current.row();
			int top = bbox.top();
			QVector<HOCRItem *>children = parentItem->children();
			while(newRow > 0) {
				if (top > children[newRow]->bbox().top()) {
					break;
				}
				newRow--;
			}
			newRow += 1;
			while(newRow < children.size()) {
				if (top < children[newRow]->bbox().top()) {
					break;
				}
				newRow++;
			}
			if (newRow >= children.size()) {
				newRow = -1; // at end
			}
			if (parentItem->bbox().top() >= bbox.top()) {
				newRow = 0;
			}
			QRect newBBox = bbox;
			newBBox.setHeight(0);
			propAttrs.clear();
			parentItem->getPropagatableAttributes(propAttrs);
			QDomElement line = newLine(doc, newBBox, propAttrs);
			current = m_document->addItem(current.parent(), line, newRow);
			currentItem = m_document->itemAtIndex(current);
			propAttrs.clear();
			currentItem->getPropagatableAttributes(propAttrs);
		}

		newElement = doc.createElement("span");
		newElement.setAttribute("class", "ocrx_word");
		QMap<QString, QSet<QString>> propWord = propAttrs["ocrx_word"];

		newElement.setAttribute("lang", propWord["lang"].size() == 1 ? *propWord["lang"].begin() : m_document->defaultLanguage());
		QMap<QString, QString> titleAttrs;
		titleAttrs["x_wconf"] = "100";
		titleAttrs["x_font"] = propWord["title:x_font"].size() == 1 ?
		                       *propWord["title:x_font"].begin() : QFont().family();
		titleAttrs["x_fsize"] = propWord["title:x_fsize"].size() == 1 ?
		                        *propWord["title:x_fsize"].begin() : 
								bbox.height() == 0 ? "8" :
								QString("%1").arg(qRound(bbox.height() * 72. / currentItem->page()->resolution()));
		if (propWord["bold"].size() == 1) { newElement.setAttribute("bold", *propWord["bold"].begin()); }
		if (propWord["italic"].size() == 1) { newElement.setAttribute("italic", *propWord["italic"].begin()); }

		int x2 = bbox.right();
		int y2 = bbox.bottom();
		if (QSettings().value("fitNewWord").toBool()) {
			int maxFontSize = -1;
			for(QString i:propWord["title:x_fsize"]) {
				maxFontSize = std::max(i.toInt(),maxFontSize);
			}
			if (maxFontSize > 0) {
				titleAttrs["x_fsize"] = QString("%1").arg(maxFontSize);
			}

			for (int i = 0; i < currentItem->children().size(); i++) {
				if (currentItem->children().at(i)->bbox().left() > bbox.left()) {
					newPos = i;
					break;
				}
			}
		} 

		titleAttrs["bbox"] = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(x2).arg(y2);
		newElement.setAttribute("title", HOCRItem::serializeAttrGroup(titleAttrs));
		newElement.appendChild(doc.createTextNode(text));
		if (QSettings().value("normalizeNewWord").toBool() || QSettings().value("fitNewWord").toBool() || bbox.height() == 0) {
			index = m_document->addItem(current, newElement, newPos);
			const HOCRItem* item = m_document->itemAtIndex(index);

			if (QSettings().value("normalizeNewWord").toBool()) {
				HOCRNormalize().normalizeSingle(m_document, item);
			}

			// Must occur after normalization
			QFont font;
			if(!item->fontFamily().isEmpty()) {
				font.setFamily(item->fontFamily());
			}
			font.setBold(item->fontBold());
			font.setItalic(item->fontItalic());
			font.setPointSizeF(item->fontSize());
			QFontMetricsF fm(font);
			// These report in 1/96 inch pixels (afaict), even though we're mostly thinking in points.
			int len = qRound((fm.horizontalAdvance(text)) * currentItem->page()->resolution()/96.);
			int hei = qRound((fm.capHeight()) * currentItem->page()->resolution()/96.);
			int x1 = bbox.left();
			int y1 = bbox.top();
			if (bbox.height() == 0 && QSettings().value("fitNewWord").toBool()) {
				x1 -= len/2;
				y1 -= hei/2;
				x2 = x1 + len;
				y2 = y1 + hei;
				if (currentItem->bbox().height() > 0) {
					y1 = std::max(currentItem->bbox().center().y() - hei/2, currentItem->bbox().top());
					y2 = std::min(y1+hei, currentItem->bbox().bottom());
				}
			} else if (bbox.height() == 0) {
				x2 = x1 + len;
				y2 = y1 + hei;
			} else {
				x2 = std::min(x1+len,bbox.right());
				y2 = std::min(y1+hei,bbox.bottom());
			}

			QString bboxstr = QString("%1 %2 %3 %4").arg(x1).arg(y1).arg(x2).arg(y2);
			m_document->editItemAttribute(index, "title:bbox", bboxstr);
		}
	} else {
		return;
	}
	if (!index.isValid()) {
		index = m_document->addItem(current, newElement, newPos);
	}
	if(index.isValid()) {
		ui.treeViewHOCR->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	}
}

void OutputEditorHOCR::addWordAtCursor() {
	QPoint p = MAIN->getDisplayer()->mapFromGlobal(QCursor::pos());
	QPointF q = MAIN->getDisplayer()->mapToScene(p);
	QPointF qOld = q;
	QRectF bounds = MAIN->getDisplayer()->getSceneBoundingRect();
	q.rx() = std::min(std::max(bounds.x(), q.x()), bounds.x() + bounds.width());
	q.ry() = std::min(std::max(bounds.y(), q.y()), bounds.y() + bounds.height());
	if (q != qOld) {
		// If clamping changed things, it's off-screen and ignore it
		return;
	}
	QRectF rf = QRectF(q, q).normalized();
	QRect r = rf.translated(-bounds.toRect().topLeft()).toRect();
	bboxDrawn(r, static_cast<int>(DisplayerToolHOCR::ACTION_DRAW_WORD_RECT));
}

void OutputEditorHOCR::showTreeWidgetContextMenu(const QPoint& point) {
	QModelIndex idx = ui.treeViewHOCR->currentIndex();
	QRect rect = ui.treeViewHOCR->visualRect(idx);
	if (rect.isValid()) {
		m_contextMenuLocation = QPoint(point.x(),rect.bottom()+1);
	} else {
		m_contextMenuLocation = point;
	}
	showTreeWidgetContextMenu_inner(m_contextMenuLocation);
}

void OutputEditorHOCR::showTreeWidgetContextMenu_inner(const QPoint& point) {
	QModelIndexList indices = ui.treeViewHOCR->selectionModel()->selectedRows();
	int nIndices = indices.size();
	if(nIndices == 0) {
		return;
	}
	if(nIndices > 1) {
		// Check if merging or swapping is allowed (items are valid siblings)
		const HOCRItem* firstItem = m_document->itemAtIndex(indices.first());
		if(!firstItem) {
			return;
		}
		QSet<QString> classes;
		classes.insert(firstItem->itemClass());
		QVector<int> rows = {indices.first().row()};
		for(int i = 1; i < nIndices; ++i) {
			const HOCRItem* item = m_document->itemAtIndex(indices[i]);
			if(!item || item->parent() != firstItem->parent()) {
				return;
			}
			classes.insert(item->itemClass());
			rows.append(indices[i].row());
		}

		QMenu menu;
		std::sort(rows.begin(), rows.end());
		bool consecutive = (rows.last() - rows.first()) == nIndices - 1;
		bool graphics = firstItem->itemClass() == "ocr_graphic";
		bool pages = firstItem->itemClass() == "ocr_page";
		bool sameClass = classes.size() == 1;

		QAction* actionMerge = nullptr;
		QAction* actionSplit = nullptr;
		QAction* actionSwap = nullptr;
		QAction* actionNormalize = nullptr;
		if(consecutive && !graphics && !pages && sameClass) { // Merging allowed
			actionMerge = menu.addAction(_("Merge"));
			if(firstItem->itemClass() != "ocr_carea") {
				actionSplit = menu.addAction(_("Split from parent"));
			}
		}
		actionSwap = menu.addAction(_("Swap two"));
		if(nIndices != 2) { // Swapping allowed
			actionSwap->setEnabled(false);
		}
		actionNormalize = menu.addAction(_("Normalize all selected"));

		QAction* clickedAction = menu.exec(ui.treeViewHOCR->mapToGlobal(point));
		if(!clickedAction) {
			return;
		}
		ui.treeViewHOCR->selectionModel()->blockSignals(true);
		QModelIndex newIndex;
		if(clickedAction == actionMerge) {
			newIndex = m_document->mergeItems(indices.first().parent(), rows.first(), rows.last());
		} else if(clickedAction == actionSplit) {
			newIndex = m_document->splitItem(indices.first().parent(), rows.first(), rows.last());
			expandCollapseChildren(newIndex, true);
		} else if(clickedAction == actionSwap) {
			newIndex = m_document->swapItems(indices.first().parent(), rows.first(), rows.last());
		} else if(clickedAction == actionNormalize) {
			QList<HOCRItem*> items;
			for (auto i:indices) {
				items.append(m_document->mutableItemAtIndex(i));
			}
			HOCRNormalize().normalizeTree(m_document, &items);
			newIndex = indices.last();
		}
		if(newIndex.isValid()) {
			ui.treeViewHOCR->selectionModel()->blockSignals(true);
			ui.treeViewHOCR->selectionModel()->clear();
			ui.treeViewHOCR->selectionModel()->blockSignals(false);
			ui.treeViewHOCR->selectionModel()->setCurrentIndex(newIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		}
		ui.treeViewHOCR->selectionModel()->blockSignals(false);
		// Nothing else is allowed with multiple items selected
		return;
	}

	QModelIndex index = indices.front();
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(!item) {
		return;
	}

	QMenu menu;
	m_contextMenu = &menu;
	QAction* actionAddGraphic = nullptr;
	QAction* actionAddCArea = nullptr;
	QAction* actionAddPar = nullptr;
	QAction* actionAddLine = nullptr;
	QAction* actionAddWord = nullptr;
	QAction* actionNormalize = nullptr;
	QAction* actionSplit = nullptr;
	QAction* actionRemove = nullptr;
	QAction* actionExpand = nullptr;
	QAction* actionCollapse = nullptr;
	QAction* actionMoveUp = nullptr;
	QAction* actionMoveDown = nullptr;
	QAction* nonActionMultiple = nullptr;

	nonActionMultiple = menu.addAction(_("Multiple Selection Menu"));
	nonActionMultiple->setEnabled(false);
	menu.addSeparator();

	QString itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		actionAddGraphic = menu.addAction(_("Add graphic region"));
		actionAddCArea = menu.addAction(_("Add text block"));
	} else if(itemClass == "ocr_carea") {
		actionAddPar = menu.addAction(_("Add paragraph"));
	} else if(itemClass == "ocr_par") {
		actionAddLine = menu.addAction(_("Add line"));
	} else if(itemClass == "ocr_line") {
		actionAddWord = menu.addAction(_("Add word"));
	} else if(itemClass == "ocrx_word") {
		m_document->addSpellingActions(&menu, index);
	}
	if(!menu.actions().isEmpty()) {
		menu.addSeparator();
	}
	actionNormalize = menu.addAction(_("Normalize"));
	if(itemClass == "ocr_par" || itemClass == "ocr_line" || itemClass == "ocrx_word") {
		actionSplit = menu.addAction(_("Split from parent"));
	}
	actionRemove = menu.addAction(_("Remove"));
	actionRemove->setShortcut(QKeySequence(Qt::Key_Delete));
	if(m_document->rowCount(index) > 0) {
		actionExpand = menu.addAction(_("Expand all"));
		actionCollapse = menu.addAction(_("Collapse all"));
	}
	m_contextIndexUp = nullptr;
	if(index.row() > 0) {
		actionMoveUp = menu.addAction(_("Move Up (U)"));
		m_contextIndexUp = &index;
	}
	m_contextIndexDown = nullptr;
	if(index.row() < index.model()->rowCount(index.parent()) - 1) {
		actionMoveDown = menu.addAction(_("Move Down (D)"));
		m_contextIndexDown = &index;
	}
	menu.installEventFilter(this);

	QAction* clickedAction = menu.exec(ui.treeViewHOCR->mapToGlobal(point));
	if(!clickedAction) {
		return;
	}
	if(clickedAction == actionAddGraphic) {
		m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_GRAPHIC_RECT);
	} else if(clickedAction == actionAddCArea) {
		m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_CAREA_RECT);
	} else if(clickedAction == actionAddPar) {
		m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_PAR_RECT);
	} else if(clickedAction == actionAddLine) {
		m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_LINE_RECT);
	} else if(clickedAction == actionAddWord) {
		m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_WORD_RECT);
	} else if(clickedAction == actionSplit) {
		QModelIndex newIndex = m_document->splitItem(index.parent(), index.row(), index.row());
		ui.treeViewHOCR->selectionModel()->setCurrentIndex(newIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		expandCollapseChildren(newIndex, true);
	} else if(clickedAction == actionNormalize) {
		QList<HOCRItem*>items = QList<HOCRItem*>({m_document->mutableItemAtIndex(index)});
		HOCRNormalize().normalizeTree(m_document, &items);
		showItemProperties(index);
	} else if(clickedAction == actionRemove) {
		m_document->removeItem(ui.treeViewHOCR->selectionModel()->currentIndex());
	} else if(clickedAction == actionExpand) {
		expandCollapseChildren(index, true);
	} else if(clickedAction == actionCollapse) {
		expandCollapseChildren(index, false);
	} else if(clickedAction == actionMoveUp) {
		moveUpDown(index,-1);
	} else if(clickedAction == actionMoveDown) {
		moveUpDown(index,+1);
	}
	menu.setAttribute(Qt::WA_DeleteOnClose, true);
}

void OutputEditorHOCR::moveUpDown(const QModelIndex& index, int by) {
	QModelIndex newIndex = m_document->index(index.row()+by, 0, index.parent());
	bool newExpanded = ui.treeViewHOCR->isExpanded(index);
	bool oldExpanded = ui.treeViewHOCR->isExpanded(newIndex);
	m_document->swapItems(index.parent(), index.row(), index.row()+by);
	newIndex = m_document->index(index.row()+by, 0, index.parent());
	ui.treeViewHOCR->selectionModel()->setCurrentIndex(newIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	expandCollapseChildren(newIndex, newExpanded);
	QModelIndex index2 = m_document->index(index.row(), 0, index.parent()); // The old index doesn't work for this (some pointer changed?)
	expandCollapseChildren(index2, oldExpanded);
	ui.treeViewHOCR->scrollTo(by>0?index2:newIndex, QAbstractItemView::PositionAtCenter);

	// Allow easy repeats: rebuild the menu (it might be different)
	m_contextMenu->close();
	QTimer::singleShot(0, [this] {
		// The scroll isn't finished until we get here.
		QModelIndex idx = ui.treeViewHOCR->selectionModel()->currentIndex();
		QRect rect = ui.treeViewHOCR->visualRect(idx);
		if (rect.isValid()) {
			m_contextMenuLocation = QPoint(m_contextMenuLocation.x(),rect.bottom()+1);
		}
		customContextMenuRequested2(m_contextMenuLocation);} ); // not recursive!
}

bool OutputEditorHOCR::eventFilter(QObject* obj, QEvent* ev) {
	// This is where we coordinate focus and keystrokes between the displayer
	// view and the tree view of the scanned image.
	Displayer* displayer = MAIN->getDisplayer();
	if (obj == displayer) {
		if(ev->type() == QEvent::Enter) {
			if (displayer->focusProxy() != nullptr) {
				displayer->setFocus();
			} else {
				ui.treeViewHOCR->setFocus();
			}
		}
		return false;
	}
	if (obj == ui.treeViewHOCR) {
		if(ev->type() == QEvent::Enter) {
			ui.treeViewHOCR->setFocus();
			return true;
		}
		return false;
	}
	// Self
	// To be deleted in future version...
	if(ev->type() == QEvent::KeyPress) {
		QKeyEvent* kev = static_cast<QKeyEvent*>(ev);
		if (kev->key() == Qt::Key_U) {
			if (m_contextIndexUp != nullptr) {
				moveUpDown(*m_contextIndexUp,-1);
			}
			return true;
		}
		if (kev->key() == Qt::Key_D) {
			if (m_contextIndexDown != nullptr) {
				moveUpDown(*m_contextIndexDown,+1);
			}
			return true;
		}
	}
	return false;
}

void OutputEditorHOCR::keyPressEvent(QKeyEvent *event) {
	ui.treeViewHOCR->keyPressEvent(event);
}

void OutputEditorHOCR::pickItem(const QPoint& point) {
	int pageNr;
	QString filename = MAIN->getDisplayer()->getCurrentImage(pageNr);
	QModelIndex pageIndex = m_document->searchPage(filename, pageNr);
	const HOCRItem* pageItem = m_document->itemAtIndex(pageIndex);
	if(!pageItem) {
		return;
	}
	const HOCRPage* page = pageItem->page();
	// Transform point into coordinate space used when page was OCRed from canvas position
	double alpha = (page->angle() - MAIN->getDisplayer()->getCurrentAngle()) / 180. * M_PI;
	double scale = double(page->resolution()) / double(MAIN->getDisplayer()->getCurrentResolution());
	QPoint newPoint( scale * (point.x() * std::cos(alpha) - point.y() * std::sin(alpha)) + 0.5 * page->bbox().width(),
	                 scale * (point.x() * std::sin(alpha) + point.y() * std::cos(alpha)) + 0.5 * page->bbox().height());
	QModelIndex index = m_document->searchAtCanvasPos(pageIndex, newPoint, page->resolution()/10);
	if (!index.isValid()) {
		MAIN->getDisplayer()->setFocus();
		return;
	}
	ui.treeViewHOCR->setCurrentIndex(index);
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(item->itemClass() == "ocrx_word") {
		ui.treeViewHOCR->setCurrentIndex(index);
	} else {
		MAIN->getDisplayer()->setFocus();
	}
}

QModelIndex OutputEditorHOCR::pickLine(const QPoint& point) {
	int pageNr;
	QString filename = MAIN->getDisplayer()->getCurrentImage(pageNr);
	QModelIndex pageIndex = m_document->searchPage(filename, pageNr);
	const HOCRItem* pageItem = m_document->itemAtIndex(pageIndex);
	if(!pageItem) {
		return QModelIndex();
	}
	const HOCRPage* page = pageItem->page();
	// Transform point into coordinate space used when page was OCRed
	double alpha = (page->angle() - MAIN->getDisplayer()->getCurrentAngle()) / 180. * M_PI;
	double scale = double(page->resolution()) / double(MAIN->getDisplayer()->getCurrentResolution());
	QPoint newPoint( scale * (point.x() * std::cos(alpha) - point.y() * std::sin(alpha)),
	                 scale * (point.x() * std::sin(alpha) + point.y() * std::cos(alpha)) );
    return m_document->lineAboveCanvasPos(pageIndex, newPoint);
}

void OutputEditorHOCR::toggleWConfColumn() {
	ui.treeViewHOCR->setColumnHidden(1, !ConfigSettings::get<ActionSetting>("displayconfidence")->getValue());
}

bool OutputEditorHOCR::open(InsertMode mode, QStringList files) {
	if(mode == InsertMode::Replace && !clear(false)) {
		return false;
	}
	if(files.isEmpty()) {
		files = FileDialogs::openDialog(_("Open hOCR File"), "", "outputdir", QString("%1 (*.html)").arg(_("hOCR HTML Files")), true);
	}
	if(files.isEmpty()) {
		return false;
	}
	int pos = mode == InsertMode::InsertBefore ? currentPage() : m_document->pageCount();
	QStringList failed;
	QStringList invalid;
	int added = 0;
	for(const QString& filename : files) {
		QFile file(filename);
		if(!file.open(QIODevice::ReadOnly)) {
			failed.append(filename);
			continue;
		}
		QDomDocument doc;
		doc.setContent(&file);
		QDomElement div = doc.firstChildElement("html").firstChildElement("body").firstChildElement("div");
		if(div.isNull() || div.attribute("class") != "ocr_page") {
			invalid.append(filename);
			continue;
		}
		while(!div.isNull()) {
			// Need to query next before adding page since the element is reparented
			QDomElement nextDiv = div.nextSiblingElement("div");
			m_document->insertPage(pos++, div, false, QFileInfo(filename).absolutePath());
			div = nextDiv;
			++added;
		}
	}
	if(added > 0) {
		m_modified = mode != InsertMode::Replace;
		if(mode == InsertMode::Replace && m_filebasename.isEmpty()) {
			QFileInfo finfo(files.front());
			m_filebasename = finfo.absoluteDir().absoluteFilePath(finfo.completeBaseName());
		}
		MAIN->setOutputPaneVisible(true);
	}
	QStringList errorMsg;
	if(!failed.isEmpty()) {
		errorMsg.append(_("The following files could not be opened:\n%1").arg(failed.join("\n")));
	}
	if(!invalid.isEmpty()) {
		errorMsg.append(_("The following files are not valid hOCR HTML:\n%1").arg(invalid.join("\n")));
	}
	if(!errorMsg.isEmpty()) {
		QMessageBox::critical(MAIN, _("Unable to open files"), errorMsg.join("\n\n"));
	}
	return added > 0;
}

bool OutputEditorHOCR::selectPage(int nr) {
	if(!m_document || nr >= m_document->pageCount() || nr < 0) {
		return false;
	}
	QModelIndex index = m_document->indexAtItem(m_document->page(nr));
	if(index.isValid()) {
		ui.treeViewHOCR->setCurrentIndex(index);
	}
	return index.isValid();
}

bool OutputEditorHOCR::save(const QString& filename) {
	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	QString outname = filename;
	if(outname.isEmpty()) {
		QString suggestion = m_filebasename;
		if(suggestion.isEmpty()) {
			QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
			if(!sources.isEmpty()) {
				QFileInfo finfo(sources.first()->path);
				suggestion = finfo.absoluteDir().absoluteFilePath(finfo.completeBaseName());
			} else {
				suggestion = _("output");
			}
		}
		outname = FileDialogs::saveDialog(_("Save hOCR Output..."), suggestion + ".html", "outputdir", QString("%1 (*.html)").arg(_("hOCR HTML Files")));
		if(outname.isEmpty()) {
			return false;
		}
	}
	QFile file(outname);
	if(!file.open(QIODevice::WriteOnly)) {
		QMessageBox::critical(MAIN, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	QByteArray current = setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "C");
	tesseract::TessBaseAPI tess;
	setlocale(LC_ALL, current.constData());
	QString header = QString(
	                     "<!DOCTYPE html>\n"
	                     "<html>\n"
	                     "<head>\n"
	                     " <title>%1</title>\n"
	                     " <meta charset=\"utf-8\" /> \n"
	                     " <meta name='ocr-system' content='tesseract %2' />\n"
	                     " <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
	                     "</head>\n").arg(QFileInfo(outname).fileName()).arg(tess.Version());
	file.write(header.toUtf8());
	m_document->convertSourcePaths(QFileInfo(outname).absolutePath(), false);
	file.write(m_document->toHTML().toUtf8());
	m_document->convertSourcePaths(QFileInfo(outname).absolutePath(), true);
	file.write("</html>\n");
	m_modified = false;
	QFileInfo finfo(outname);
	m_filebasename = finfo.absoluteDir().absoluteFilePath(finfo.completeBaseName());
	return true;
}

bool OutputEditorHOCR::crashSave(const QString& filename) const {
	QFile file(filename);
	if(file.open(QIODevice::WriteOnly)) {
		QString header = QString(
		                     "<!DOCTYPE html>\n"
		                     "<html>\n"
		                     "<head>\n"
		                     " <title>%1</title>\n"
		                     " <meta charset=\"utf-8\" /> \n"
		                     " <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
		                     "</head>\n").arg(QFileInfo(filename).fileName());
		file.write(header.toUtf8());
		m_document->convertSourcePaths(QFileInfo(filename).absolutePath(), false);
		file.write(m_document->toHTML().toUtf8());
		m_document->convertSourcePaths(QFileInfo(filename).absolutePath(), true);
		file.write("</html>\n");
		return true;
	}
	return false;
}

bool OutputEditorHOCR::exportToODT() {
	QString suggestion = m_filebasename;
	if(suggestion.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
	}

	QString outname = FileDialogs::saveDialog(_("Save ODT Output..."), suggestion + ".odt", "outputdir", QString("%1 (*.odt)").arg(_("OpenDocument Text Documents")));
	if(outname.isEmpty()) {
		return false;
	}

	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	MAIN->getDisplayer()->setBlockAutoscale(true);
	bool success = HOCROdtExporter().run(m_document, outname);
	MAIN->getDisplayer()->setBlockAutoscale(false);
	return success;
}

bool OutputEditorHOCR::exportToPDF() {
	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* item = m_document->itemAtIndex(current);
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	bool success = false;
	if(!newPage(page)) {
		return false;
	}
	HOCRPdfExportDialog dialog(m_tool, m_document, page, MAIN);
	if(dialog.exec() != QDialog::Accepted) {
		return false;
	}
	HOCRPdfExporter::PDFSettings settings = dialog.getPdfSettings();

	QString suggestion = m_filebasename;
	if(suggestion.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
	}

	QString outname;
	while(true) {
		outname = FileDialogs::saveDialog(_("Save PDF Output..."), suggestion + ".pdf", "outputdir", QString("%1 (*.pdf)").arg(_("PDF Files")));
		if(outname.isEmpty()) {
			break;
		}
		if(m_document->referencesSource(outname)) {
			QMessageBox::warning(MAIN, _("Invalid Output"), _("Cannot overwrite a file which is a source image of this document."));
			continue;
		}
		break;
	}
	if(outname.isEmpty()) {
		return false;
	}

	MAIN->getDisplayer()->setBlockAutoscale(true);
	success = HOCRPdfExporter().run(m_document, outname, &settings);
	MAIN->getDisplayer()->setBlockAutoscale(false);
	newPage(item->page());
	return success;
}

bool OutputEditorHOCR::exportToText() {
	QString suggestion = m_filebasename;
	if(suggestion.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
	}

	QString outname = FileDialogs::saveDialog(_("Save Text Output..."), suggestion + ".txt", "outputdir", QString("%1 (*.txt)").arg(_("Text Files")));
	if(outname.isEmpty()) {
		return false;
	}

	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	MAIN->getDisplayer()->setBlockAutoscale(true);
	bool success = HOCRTextExporter().run(m_document, outname);
	MAIN->getDisplayer()->setBlockAutoscale(false);
	return success;
}

bool OutputEditorHOCR::clear(bool hide) {
	m_previewTimer.stop();
	ui.actionPreview->setChecked(false);
	if(!m_widget->isVisible()) {
		return true;
	}
	if(m_modified) {
		int response = QMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		if(response == QMessageBox::Save) {
			if(!save()) {
				return false;
			}
		} else if(response != QMessageBox::Discard) {
			return false;
		}
	}
	m_proofReadWidget->clear();
	m_document->clear();
	ui.tableWidgetProperties->setRowCount(0);
	ui.plainTextEditOutput->clear();
	m_tool->clearSelection();
	m_modified = false;
	m_filebasename.clear();
	if(hide) {
		MAIN->setOutputPaneVisible(false);
	}
	return true;
}

void OutputEditorHOCR::setLanguage(const Config::Lang& lang) {
	m_document->setDefaultLanguage(lang.code);
}

void OutputEditorHOCR::onVisibilityChanged(bool /*visible*/) {
	ui.searchFrame->hideSubstitutionsManager();
}

bool OutputEditorHOCR::findReplaceInItem(const QModelIndex& index, const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace, bool& currentSelectionMatchesSearch) {
	// Check that the item is a word
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(!item || item->itemClass() != "ocrx_word") {
		return false;
	}
	Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
	HOCRTextDelegate* delegate = static_cast<HOCRTextDelegate*>(ui.treeViewHOCR->itemDelegateForColumn(0));
	// If the item is already in edit mode, continue searching inside the text
	if(delegate->getCurrentIndex() == index && delegate->getCurrentEditor()) {
		QLineEdit* editor = delegate->getCurrentEditor();
		bool matchesSearch = editor->selectedText().compare(searchstr, cs) == 0;
		int selStart = editor->selectionStart();
		if(matchesSearch && replace) {
			QString oldText = editor->text();
			editor->setText(oldText.left(selStart) + replacestr + oldText.mid(selStart + searchstr.length()));
			editor->setSelection(selStart, replacestr.length());
			return true;
		}
		bool matchesReplace = editor->selectedText().compare(replacestr, cs) == 0;
		int pos = -1;
		if(backwards) {
			pos = selStart - 1;
			pos = pos < 0 ? -1 : editor->text().lastIndexOf(searchstr, pos, cs);
		} else {
			pos = matchesSearch ? selStart + searchstr.length() : matchesReplace ? selStart + replacestr.length() : selStart;
			pos = editor->text().indexOf(searchstr, pos, cs);
		}
		if(pos != -1) {
			editor->setSelection(pos, searchstr.length());
			return true;
		}
		currentSelectionMatchesSearch = matchesSearch;
		return false;
	}
	// Otherwise, if item contains text, set it in edit mode
	int pos = backwards ? item->text().lastIndexOf(searchstr, -1, cs) : item->text().indexOf(searchstr, 0, cs);
	if(pos != -1) {
		ui.treeViewHOCR->setCurrentIndex(index);
		ui.treeViewHOCR->edit(index);
		Q_ASSERT(delegate->getCurrentIndex() == index && delegate->getCurrentEditor());
		delegate->getCurrentEditor()->setSelection(pos, searchstr.length());
		return true;
	}
	return false;
}

void OutputEditorHOCR::findReplace(const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace) {
	ui.searchFrame->clearErrorState();
	QModelIndex current = ui.treeViewHOCR->currentIndex();
	if(!current.isValid()) {
		current = m_document->index(backwards ? (m_document->rowCount() - 1) : 0, 0);
	}
	QModelIndex neww = current;
	bool currentSelectionMatchesSearch = false;
	while(!findReplaceInItem(neww, searchstr, replacestr, matchCase, backwards, replace, currentSelectionMatchesSearch)) {
		neww = backwards ? m_document->prevIndex(neww) : m_document->nextIndex(neww);
		if(!neww.isValid() || neww == current) {
			// Break endless loop
			if(!currentSelectionMatchesSearch) {
				ui.searchFrame->setErrorState();
			}
			return;
		}
	}
}

void OutputEditorHOCR::replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	QModelIndex start = m_document->index(0, 0);
	QModelIndex curr = start;
	Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
	int count = 0;
	do {
		const HOCRItem* item = m_document->itemAtIndex(curr);
		if(item && item->itemClass() == "ocrx_word") {
			if(item->text().contains(searchstr, cs)) {
				++count;
				m_document->setData(curr, item->text().replace(searchstr, replacestr, cs), Qt::EditRole);
			}
		}
		curr = m_document->nextIndex(curr);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	} while(curr != start);
	if(count == 0) {
		ui.searchFrame->setErrorState();
	}
	MAIN->popState();
}

void OutputEditorHOCR::applySubstitutions(const QMap<QString, QString>& substitutions, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	QModelIndex start = m_document->index(0, 0);
	Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
	for(auto it = substitutions.begin(), itEnd = substitutions.end(); it != itEnd; ++it) {
		QString search = it.key();
		QString replace = it.value();
		QModelIndex curr = start;
		do {
			const HOCRItem* item = m_document->itemAtIndex(curr);
			if(item && item->itemClass() == "ocrx_word") {
				m_document->setData(curr, item->text().replace(search, replace, cs), Qt::EditRole);
			}
			curr = m_document->nextIndex(curr);
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		} while(curr != start);
	}
	MAIN->popState();
}

void OutputEditorHOCR::removeCurrentItem() {
	m_document->removeItem(ui.treeViewHOCR->selectionModel()->currentIndex());
}

void OutputEditorHOCR::removePageByPosition(int position) {
	const HOCRPage* page = m_document->page(position);
	m_document->HOCRDocument::removeItem(m_document->indexAtItem(page));
}

void OutputEditorHOCR::sourceChanged() {
	int page;
	QString path = MAIN->getDisplayer()->getCurrentImage(page);
	// Check if source is in document tree
	QModelIndex pageIndex = m_document->searchPage(path, page);
	if(!pageIndex.isValid()) {
		ui.treeViewHOCR->setCurrentIndex(QModelIndex());
	} else {
		QModelIndex curIndex = ui.treeViewHOCR->currentIndex();
		while(curIndex != pageIndex && curIndex.parent().isValid()) {
			curIndex = curIndex.parent();
		}
		if(curIndex != pageIndex) {
			ui.treeViewHOCR->setCurrentIndex(pageIndex);
		}
	}
	updatePreview();
}

void OutputEditorHOCR::previewToggled() {
	bool active = ConfigSettings::get<SwitchSetting>("displaypreview")->getValue();
	QModelIndex index = ui.treeViewHOCR->currentIndex();
	if(active && !index.isValid() && m_document->pageCount() > 0) {
		ui.treeViewHOCR->setCurrentIndex(m_document->indexAtItem(m_document->page(0)));
	} else {
		updatePreview();
	}
}

void OutputEditorHOCR::updatePreview() {
	const HOCRItem* item = m_document->itemAtIndex(ui.treeViewHOCR->currentIndex());
	if(!ui.actionPreview->isChecked() || !item) {
		m_preview->setVisible(false);
		return;
	}

	const HOCRPage* page = item->page();
	const QRect& bbox = page->bbox();
	int pageDpi = page->resolution();

	QImage image(bbox.size(), QImage::Format_ARGB32);
	image.fill(QColor(255, 255, 255, 127));
	image.setDotsPerMeterX(pageDpi / 0.0254); // 1 in = 0.0254 m
	image.setDotsPerMeterY(pageDpi / 0.0254);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);

	drawPreview(painter, page);

	m_preview->setPixmap(QPixmap::fromImage(image));
	m_preview->setPos(-0.5 * bbox.width(), -0.5 * bbox.height());
	m_preview->setVisible(true);
}

void OutputEditorHOCR::drawPreview(QPainter& painter, const HOCRItem* item) {
	if(!item->isEnabled()) {
		return;
	}
	QString itemClass = item->itemClass();
	if(itemClass == "ocr_line") {
		QPair<double, double> baseline = item->baseLine();
		double textangle = item->textangle();
		const QRect& lineRect = item->bbox();
		for(HOCRItem* wordItem : item->children()) {
			if(!wordItem->isEnabled()) {
				continue;
			}
			const QRect& wordRect = wordItem->bbox();
			QFont font;
			if(!wordItem->fontFamily().isEmpty()) {
				font.setFamily(wordItem->fontFamily());
			}
			font.setBold(wordItem->fontBold());
			font.setItalic(wordItem->fontItalic());
			font.setPointSizeF(wordItem->fontSize());
			painter.setFont(font);
			// See https://github.com/kba/hocr-spec/issues/15
			// double y is the y coordinate of the mid-point along the x axis on baseline for line first*x+second (mx+b)
			if(std::abs(textangle) == 0) {
				// Using the lineRect.bottom() for the whole line gives much more readable results on horizontal text.
				double y = lineRect.bottom() + (wordRect.center().x() - lineRect.x()) * baseline.first + baseline.second;
				painter.drawText(wordRect.x(), y, wordItem->text());
			}
			else {
				// ... but wordRect.bottom is more accurate; for nonzero angles we'll choose that.
				// The only actual value seen here has been 90.
				double y = wordRect.bottom() + (wordRect.center().x() - lineRect.x()) * baseline.first + baseline.second;
				painter.save();
				painter.translate(wordRect.x()+(wordRect.right()-wordRect.left())*std::sin(textangle/180.0 * M_PI), y);
				painter.rotate(-textangle);
				painter.drawText(0, 0, wordItem->text());
				painter.restore();
			}
		}
	} else if(itemClass == "ocr_graphic") {
		painter.drawImage(item->bbox(), m_tool->getSelection(item->bbox()));
	} else {
		for(HOCRItem* childItem : item->children()) {
			drawPreview(painter, childItem);;
		}
	}
}

void OutputEditorHOCR::doReplace(bool force) {
	if(!force) {
		ui.searchFrame->clear();
		force = !ui.searchFrame->isVisible();
	}
	ui.searchFrame->setVisible(force);
	ui.searchFrame->setFocus();
	ui.actionOutputReplace->setChecked(force); 
}