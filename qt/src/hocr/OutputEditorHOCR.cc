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
#include "HOCRIndentedTextExporter.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "UiUtils.hh"
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

HOCRAttributeEditor::HOCRAttributeEditor(const QString& value, HOCRDocument* doc, const TreeViewHOCR *treeView, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass)
	: QLineEdit(value), m_doc(doc), m_itemIndex(itemIndex), m_attrName(attrName), m_origValue(value), m_attrItemClass(attrItemClass), m_edited(false), m_treeView(treeView) {

	setFrame(false);
	m_note = nullptr;
	if (m_attrName == "title:bbox") {
		m_note = new QLabel(this);
		m_note->setFocusPolicy(Qt::NoFocus);
		m_note->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_note->setStyleSheet("background-color: transparent; color:grey");
		const HOCRItem* item = m_doc->itemAtIndex(itemIndex);
		QString note = QString("(%2x%3)").arg(item->bbox().width()-1).arg(item->bbox().height()-1);
		m_note->setText(note);
		if (m_treeView->selectionModel()->selectedRows().count() > 1) {
			// it's nonsense to change more than one bbox to the same thing
			setReadOnly(true);
			setStyleSheet("background-color: lightGray;");
		}
	}

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

void HOCRAttributeEditor::resizeEvent(QResizeEvent *event) {
	if(m_note != nullptr) {
		m_note->resize(width()/2, height());
		m_note->move(QPoint(width()/2,0));
	}
}

void HOCRAttributeEditor::focusOutEvent(QFocusEvent* ev) {
	QLineEdit::focusOutEvent(ev);
	validateChanges();
}

void HOCRAttributeEditor::updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value) {
	if(itemIndex == m_itemIndex && name == m_attrName) {
		blockSignals(true);
		if (name == "title:bbox") {
			const HOCRItem* item = m_doc->itemAtIndex(itemIndex);
			QString note = QString("(%2x%3)").arg(item->bbox().width()-1).arg(item->bbox().height()-1);
			m_note->setText(note);
		}
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
			QModelIndexList indices = m_treeView->selectionModel()->selectedRows();
			for (QModelIndex i:indices) {
				m_doc->editItemAttribute(i, m_attrName, newValue, m_attrItemClass);
			}
			m_origValue = newValue;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

HOCRAttributeCheckbox::HOCRAttributeCheckbox(Qt::CheckState value, HOCRDocument* doc, const TreeViewHOCR *model, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass)
	: m_doc(doc), m_itemIndex(itemIndex), m_attrName(attrName), m_attrItemClass(attrItemClass), m_treeView(model){
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
	QModelIndexList indices = m_treeView->selectionModel()->selectedRows();
	for (QModelIndex i:indices) {
		m_doc->editItemAttribute(i, m_attrName, isChecked() ? "1" : "0", m_attrItemClass);
	}
}

///////////////////////////////////////////////////////////////////////////////


HOCRAttributeLangCombo::HOCRAttributeLangCombo(const QString& value, bool multiple, HOCRDocument* doc, const TreeViewHOCR* treeView, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass)
	: m_doc(doc), m_itemIndex(itemIndex), m_attrName(attrName), m_attrItemClass(attrItemClass), m_treeView(treeView) {
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
	QModelIndexList indices = m_treeView->selectionModel()->selectedRows();
	for (QModelIndex i:indices) {
		m_doc->editItemAttribute(i, m_attrName, currentData().toString(), m_attrItemClass);
	}
}

///////////////////////////////////////////////////////////////////////////////

class HOCRTextDelegate : public QStyledItemDelegate {
public:
	HOCRTextDelegate(TreeViewHOCR* parent) : m_treeView(parent) {
		QStyledItemDelegate(static_cast<QObject*>(parent));
	}

	QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& /* option */, const QModelIndex& /* index */) const {
		m_currentEditor = new QLineEdit(parent);
		return m_currentEditor;
	}
	void setEditorData(QWidget* editor, const QModelIndex& index) const {
		m_currentIndex = index;
		m_currentEditor = static_cast<QLineEdit*>(editor);
		m_currentEditor->setText(index.model()->data(index, Qt::EditRole).toString());
		
	}
	void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const {
		QString newText = static_cast<QLineEdit*>(editor)->text();
		QString oldText = index.model()->data(index, Qt::EditRole).toString();
		if (newText != oldText) {
			// Don't change things unnecessarily: data modifications cause side effects
			model->setData(index, newText, Qt::EditRole);
		}
	}
	const QModelIndex& getCurrentIndex() const {
		return m_currentIndex;
	}
	QLineEdit* getCurrentEditor() const {
		return m_currentEditor;
	}
	void setText(QString text) {
		m_currentEditor->setText(text);
	}
	void setSelection(int start, int len) {
		m_start = start;
		m_len = len;
		m_currentEditor->setSelection(m_start, m_len);
	}
	void reSetSelection() {
		if (m_currentIndex == m_treeView->currentIndex()) {
			if (m_currentEditor == nullptr) {
				// If the editor was destroyed by the tree (e.g. on defocus), 
				/// the QPointer will be nulled... rebuild.
				if (!m_currentIndex.isValid()) {
					// There's nothing to edit
					return;
				}
				m_treeView->edit(m_currentIndex);
				m_currentEditor->setSelection(m_start, m_len);
			}
			m_currentEditor->setFocus();
		} else {
			m_currentIndex = QModelIndex();
			m_treeView->setFocus();
		}
	}
	const QString text() {
		return m_currentEditor->text();
	}
	const QString selectedText() {
		return m_currentEditor->selectedText();
	}
	int selectionStart() {
		return m_currentEditor->selectionStart();
	}

private:
	TreeViewHOCR* m_treeView;
	mutable QModelIndex m_currentIndex;
	mutable QPointer<QLineEdit> m_currentEditor;
	mutable int m_start=0;
	mutable int m_len=0;
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

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool, FocusableMenu* keyParent) {
	m_keyParent = keyParent;
	static int reg = qRegisterMetaType<QList<QRect>>("QList<QRect>");
	Q_UNUSED(reg);

	static int reg2 = qRegisterMetaType<HOCRReadSessionData>("HOCRReadSessionData");
	Q_UNUSED(reg2);

	m_tool = tool;
	m_widget = new QWidget;
	ui.setupUi(m_widget, keyParent);
	m_highlighter = new HTMLHighlighter(ui.plainTextEditOutput->document());

	m_preview = new QGraphicsPixmapItem();
	m_preview->setTransformationMode(Qt::SmoothTransformation);
	m_preview->setZValue(3);
	MAIN->getDisplayer()->scene()->addItem(m_preview);
	m_previewTimer.setSingleShot(true);

	m_selectedItems = new QGraphicsPixmapItem();
	m_selectedItems->setTransformationMode(Qt::SmoothTransformation);
	m_selectedItems->setZValue(2);
	MAIN->getDisplayer()->scene()->addItem(m_selectedItems);

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

	ui.comboBoxNavigate->addItem(_("Page"), "ocr_page"); // Note: first chars must be unique... '&' doesn't work
	ui.comboBoxNavigate->addItem(_("Block"), "ocr_carea");
	ui.comboBoxNavigate->addItem(_("Section (Paragraph)"), "ocr_par");
	ui.comboBoxNavigate->addItem(_("Line"), "ocr_line");
	ui.comboBoxNavigate->addItem(_("Word"), "ocrx_word");
	ui.comboBoxNavigate->addItem(_("Misspelled word"), "ocrx_word_bad");
	ui.comboBoxNavigate->addItem(_("Low confidence word"), "ocrx_word_lowconf");

	QShortcut* shortcut = new QShortcut(QKeySequence(Qt::Key_Delete), m_widget);
	QObject::connect(shortcut, &QShortcut::activated, this, &OutputEditorHOCR::removeCurrentItem);

	ui.actionInsertModeAppend->setData(static_cast<int>(InsertMode::Append));
	ui.actionInsertModeBefore->setData(static_cast<int>(InsertMode::InsertBefore));

	keyParent->clear();
	keyParent->addAction(_("Select &insert mode  \t\u27a4"),
		[this]() { ui.menuInsertMode->exec(ui.toolButtonInsertMode->mapToGlobal(ui.toolButtonInsertMode->geometry().bottomLeft())); });
	keyParent->addFileDialog(_("&Open hOCR file (replace)"), [this] { return open(InsertMode::Replace); });
	keyParent->addAction(_("   ... &append or insert  \t\u27a4"),
		[this]() { ui.menuOpen->exec(ui.toolButtonOpen->mapToGlobal(ui.toolButtonOpen->geometry().bottomLeft())); });
	ui.menuOutputSaveHOCR = 
	keyParent->addFileDialog(_("&Save as hOCR text"),[this] {return save();});
	ui.menuOutputSaveHOCR->setEnabled(false);

	ui.exportMenu = new FocusableMenu(_("&Export"),keyParent);
	ui.menuOutputExport = keyParent->addMenu(ui.exportMenu);
		ui.menuOutputExport->setEnabled(false);
		ui.exportMenu->addFileDialog(QIcon::fromTheme("text-plain"), _("Export to plain &text"), 
			[this]()->bool {return exportToText();} );
		ui.exportMenu->addFileDialog(QIcon::fromTheme("text-plain"), _("Export to plain text, preserve &whitespace"), 
			[this]()->bool {return exportToIndentedText();} );
		ui.exportMenu->addFileDialog(QIcon::fromTheme("application-pdf"), _("Export to &PDF"),
			[this]()->bool {return exportToPDF();} );
		ui.exportMenu->addFileDialog(QIcon::fromTheme("x-office-document"), _("Export to &ODT"),
			[this]()->bool {return exportToODT();} );
	ui.toolButtonOutputExport->setMenu(ui.exportMenu);

	keyParent->addAction(_("&Clear"), this, &OutputEditorHOCR::clear);

		FocusableMenu* menuFindReplace = new FocusableMenu(_("&Find and Replace"), keyParent);
		ui.searchFrame->setKeyMenu(menuFindReplace);
	ui.menuOutputFind = 
	keyParent->addMenu(menuFindReplace);
		ui.menuOutputFind->setEnabled(false);

		FocusableMenu* menuNavigate = new FocusableMenu(_("&Navigate"), keyParent);
		menuNavigate->addAction(_("Set &Target"), this, [this] { FocusableMenu::showFocusSet(ui.comboBoxNavigate); });
		menuNavigate->addAction(_("&Next"), this, &OutputEditorHOCR::navigateNext);
		menuNavigate->addAction(_("&Previous"), this, &OutputEditorHOCR::navigatePrev);
		menuNavigate->addAction(_("&Expand All"), this, &OutputEditorHOCR::expandItemClass);
		menuNavigate->addAction(_("&Collapse All"), this, &OutputEditorHOCR::collapseItemClass);
		menuNavigate->addAction(_("P&roperties Tab"), this, [this] { FocusableMenu::showFocusSet(ui.tabWidgetProps, 0); });
		menuNavigate->addAction(_("&Source Tab"), this, [this] { FocusableMenu::showFocusSet(ui.tabWidgetProps, 1); });
		menuNavigate->addAction(_("&Current Page"), this, [this] { navigateNextPrev(false, "ocr_page", false); });
	ui.menuOutputNavigate = 
	keyParent->addMenu(menuNavigate);
		ui.menuOutputNavigate->setEnabled(false);
	keyParent->addDialog(_("&Preferences"), [this, keyParent] {doPreferences(keyParent);} );
	keyParent->addAction(_("Sho&w HOCR Context menu"), [this] {
		QRect pos = ui.treeViewHOCR->visualRect(ui.treeViewHOCR->currentIndex());
		emit ui.treeViewHOCR->customContextMenuRequested(pos.center());
	} );

	connect(ui.menuInsertMode, &QMenu::triggered, this, &OutputEditorHOCR::setInsertMode);
	connect(ui.toolButtonOpen, &QToolButton::clicked, this, 
		[this, keyParent] () { FocusableMenu::showFileDialogMenu(keyParent, [this]{return open(InsertMode::Replace);}); });
	connect(ui.actionOpenAppend, &QAction::triggered, this,
		[this, keyParent] () { FocusableMenu::showFileDialogMenu(keyParent, [this]{return open(InsertMode::Append);}); });
	connect(ui.actionOpenInsertBefore, &QAction::triggered, this,
		[this, keyParent] () { FocusableMenu::showFileDialogMenu(keyParent, [this]{return open(InsertMode::InsertBefore);}); });
	connect(ui.actionOutputSaveHOCR, &QAction::triggered, this, [this] { save(); });
	connect(ui.actionOutputClear, &QAction::triggered, this, &OutputEditorHOCR::clear);
	connect(ui.actionOutputReplace, &QAction::triggered, this, [this] { doReplace(false); });
	connect(ui.actionOutputReplaceKey, &QAction::triggered, this, [this] { doReplace(true); });
	connect(ui.actionOutputSettings, &QAction::triggered, this, [this, keyParent] { doPreferences(keyParent); });
	connect(&m_previewTimer, &QTimer::timeout, this, [this] {showPreview(OutputEditorHOCR::showMode::show); }); 
	connect(ui.searchFrame, &SearchReplaceFrame::findReplace, this, &OutputEditorHOCR::findReplace);
	connect(ui.searchFrame, &SearchReplaceFrame::replaceAll, this, &OutputEditorHOCR::replaceAll);
	connect(ui.searchFrame, &SearchReplaceFrame::reFocusTree, this, &OutputEditorHOCR::reFocusTree);
	connect(ui.searchFrame, &SearchReplaceFrame::applySubstitutions, this, &OutputEditorHOCR::applySubstitutions);
	connect(ConfigSettings::get<FontSetting>("customoutputfont"), &FontSetting::changed, this, &OutputEditorHOCR::setFont);
	connect(ConfigSettings::get<SwitchSetting>("systemoutputfont"), &FontSetting::changed, this, &OutputEditorHOCR::setFont);
	// Defer running showItemProperties until the selectionModel updates the row count, because we need to know when
	// the row count is more than one.
	connect(ui.treeViewHOCR->selectionModel(), &QItemSelectionModel::currentRowChanged, this, 
		[this] (const QModelIndex& index,const QModelIndex& prev) { QTimer::singleShot(0, [this, index, prev] {showItemProperties(index,prev);} ); } ); 
	connect(ui.treeViewHOCR->selectionModel(), &QItemSelectionModel::selectionChanged, this, &OutputEditorHOCR::showSelections);
	connect(ui.treeViewHOCR, &QTreeView::customContextMenuRequested, this, &OutputEditorHOCR::showTreeWidgetContextMenu);
	connect(this, &OutputEditorHOCR::customContextMenuRequested2, this, &OutputEditorHOCR::showTreeWidgetContextMenu_inner);
	connect(ui.tabWidgetProps, &QTabWidget::currentChanged, this, &OutputEditorHOCR::updateSourceText);
	connect(m_tool, &DisplayerToolHOCR::bboxChanged, this, &OutputEditorHOCR::updateCurrentItemBBox);
	connect(m_tool, &DisplayerToolHOCR::bboxDrawn, this, &OutputEditorHOCR::bboxDrawn);
	connect(m_tool, &DisplayerToolHOCR::positionPicked, this, &OutputEditorHOCR::pickItem);
	connect(m_document, &HOCRDocument::dataChanged, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::rowsInserted, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::rowsRemoved, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::modelReset, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::itemAttributeChanged, this, &OutputEditorHOCR::setModified);
	connect(m_document, &HOCRDocument::itemAttributeChanged, this, &OutputEditorHOCR::updateSourceText);
	connect(m_document, &HOCRDocument::itemAttributeChanged, this, &OutputEditorHOCR::itemAttributeChanged);
	connect(ui.comboBoxNavigate, qOverload<int>(&QComboBox::currentIndexChanged), this, &OutputEditorHOCR::navigateTargetChanged);
	connect(ui.actionNavigateNext, &QAction::triggered, this, &OutputEditorHOCR::navigateNext);
	connect(ui.actionNavigatePrev, &QAction::triggered, this, &OutputEditorHOCR::navigatePrev);
	connect(ui.actionExpandAll, &QAction::triggered, this, &OutputEditorHOCR::expandItemClass);
	connect(ui.actionCollapseAll, &QAction::triggered, this, &OutputEditorHOCR::collapseItemClass);
	connect(MAIN->getDisplayer(), &Displayer::imageChanged, this, &OutputEditorHOCR::sourceChanged);
	connect(ui.outputDialogUi.checkBox_Preview, &QCheckBox::toggled, this, &OutputEditorHOCR::previewToggled);
	connect(ui.outputDialogUi.checkBox_Overheight, &QCheckBox::toggled, this, &OutputEditorHOCR::previewToggled);
	connect(ui.outputDialogUi.checkBox_NonAscii, &QCheckBox::toggled, this, &OutputEditorHOCR::previewToggled);
	connect(ui.outputDialogUi.checkBox_WConf, &QCheckBox::toggled, this, &OutputEditorHOCR::toggleWConfColumn);
	connect(ui.outputDialogUi.doubleSpinBox_Stretch, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &OutputEditorHOCR::previewToggled);
	connect(ui.menuOutputFind, &QAction::triggered, this, [this]  { doReplace(true); });

	ADD_SETTING(SwitchSetting("replacescans", ui.outputDialogUi.checkBox_ReplaceScan, false));
	ADD_SETTING(SwitchSetting("displayconfidence", ui.outputDialogUi.checkBox_WConf, false));
	ADD_SETTING(SwitchSetting("displaypreview", ui.outputDialogUi.checkBox_Preview, false));
	ADD_SETTING(SwitchSetting("displayoverheight", ui.outputDialogUi.checkBox_Overheight, true));
	ADD_SETTING(SwitchSetting("displaynonascii", ui.outputDialogUi.checkBox_NonAscii, true));
	ADD_SETTING(DoubleSpinSetting("previewfontstretch", ui.outputDialogUi.doubleSpinBox_Stretch, 100.0));

	setFont();

	// Deferred until connect()s are done
	ui.treeViewHOCR->setColumnHidden(1, !ui.outputDialogUi.checkBox_WConf->isChecked());
}

OutputEditorHOCR::~OutputEditorHOCR() {
	m_previewTimer.stop();
	delete m_preview;
	delete m_proofReadWidget;
	delete m_widget;
	delete m_selectedItems;
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

void OutputEditorHOCR::setModified() {
	ui.actionOutputSaveHOCR->setEnabled(m_document->pageCount() > 0);
	ui.toolButtonOutputExport->setEnabled(m_document->pageCount() > 0);
	ui.toolBarNavigate->setEnabled(m_document->pageCount() > 0);
	ui.menuOutputSaveHOCR->setEnabled(m_document->pageCount() > 0);
	ui.menuOutputExport->setEnabled(m_document->pageCount() > 0);
	ui.menuOutputNavigate->setEnabled(m_document->pageCount() > 0);
	ui.menuOutputFind->setEnabled(m_document->pageCount() > 0);
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
	if (!ui.outputDialogUi.checkBox_ReplaceScan->isChecked()) {
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

void OutputEditorHOCR::setAngle(double angle) {
	const HOCRItem* item = m_document->itemAtIndex(ui.treeViewHOCR->currentIndex());
	HOCRPage* page = item->page();
	page->setAngle(angle);
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
					expandCollapseChildren(child, true);
				}
			} else {
				expandCollapseChildren(next, false);
			}
		}
		next = m_document->nextIndex(next);
	} while(next != start);
	if (expand) {
		ui.treeViewHOCR->scrollTo(ui.treeViewHOCR->currentIndex());
	}
}

void OutputEditorHOCR::blinkCombo() {
	new BlinkWidget(8,
		[this]{ui.comboBoxNavigate->setStyleSheet("background-color: red"); },
		[this]{ui.comboBoxNavigate->setStyleSheet("");}, this );
}

void OutputEditorHOCR::navigateNextPrev(bool next, const QString &t, bool advance) {
	QString target = t;
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
	if(!advance && start.isValid()) {
		const HOCRItem* item = m_document->itemAtIndex(start);
		if(item && item->itemClass() == target) {
			return;
		}
	}
	QModelIndex found = m_document->prevOrNextIndex(next, start, target, misspelled, lowconf);
	if (found == start) {
		blinkCombo();
	}
	ui.treeViewHOCR->setCurrentIndex(found);
	ui.treeViewHOCR->scrollTo(found, QAbstractItemView::PositionAtCenter);
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

bool OutputEditorHOCR::isFullyExpanded(const QModelIndex& index) const {
	if (m_document->itemAtIndex(index)->itemClass() == "ocr_line") {
		return ui.treeViewHOCR->isExpanded(index);
	}
	else if (!ui.treeViewHOCR->isExpanded(index)) {
		return false;
	}
	int nChildren = m_document->rowCount(index);
	if(nChildren > 0) {
		for(int i = 0; i < nChildren; ++i) {
			if(!isFullyExpanded(m_document->index(i, 0, index))) {
				return false;
			}
		}
	};
	return true;
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
		MAIN->showCurrentPage("");
		return;
	}
	const HOCRPage* page = currentItem->page();
	QModelIndex pageIndex = m_document->indexAtItem(page);
	MAIN->showCurrentPage(pageIndex.model()->data(pageIndex, Qt::EditRole).toString());

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
		QLineEdit* lineEdit = new HOCRAttributeEditor(attrValue, m_document, ui.treeViewHOCR, itemIndex, attrName, attrItemClass);
		lineEdit->setValidator(new QRegExpValidator(QRegExp(it.value())));
		if(multiple) {
			lineEdit->setPlaceholderText(_("Multiple values"));
		}
		return lineEdit;
	} else if(attrName == "title:x_font") {
		QFontComboBox* combo = new QFontComboBox();
		combo->setCurrentIndex(-1);
		QLineEdit* edit = new HOCRAttributeEditor(attrValue, m_document, ui.treeViewHOCR, itemIndex, attrName, attrItemClass);
		edit->blockSignals(true); // Because the combobox alters the text as soon as setLineEdit is called...
		combo->setLineEdit(edit);
		edit->setText(attrValue);
		edit->blockSignals(false);
		if(multiple) {
			combo->lineEdit()->setPlaceholderText(_("Multiple values"));
		}
		return combo;
	} else if(attrName == "lang") {
		HOCRAttributeLangCombo* combo = new HOCRAttributeLangCombo(attrValue, multiple, m_document, ui.treeViewHOCR, itemIndex, attrName, attrItemClass);
		return combo;
	} else if(attrName == "bold" || attrName == "italic") {
		Qt::CheckState value = multiple ? Qt::PartiallyChecked : attrValue == "1" ? Qt::Checked : Qt::Unchecked;
		return new HOCRAttributeCheckbox(value, m_document, ui.treeViewHOCR, itemIndex, attrName, attrItemClass);
	} else {
		QLineEdit* lineEdit = new QLineEdit(attrValue);
		lineEdit->setFrame(false);
		lineEdit->setReadOnly(true);
		return lineEdit;
	}
}

void OutputEditorHOCR::updateCurrentItemBBox(QRect bbox, bool affectsChildren) {
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	if (affectsChildren) {
		const HOCRItem* currentItem = m_document->itemAtIndex(current);
		QRect oldBbox = currentItem->bbox();
		QPoint moved = bbox.topLeft() - oldBbox.topLeft();
		// Assume rigid motion here.
		m_document->xlateItem(current, moved.y(), moved.x());
	} else {
		// See QRect documentation for off-by-one issue with lower right corner.
		QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.left()+bbox.width()).arg(bbox.top()+bbox.height());
		m_document->editItemAttribute(current, "title:bbox", bboxstr);
	}
	// Move ProofReadWidget correspndingly; note: widget content order won't change because that's in hOCR order, not graphical order.
	emit MAIN->getDisplayer()->imageChanged();
	m_proofReadWidget->updateWidget(true);
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
		m_proofReadWidget->updateWidget(true);
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
			QRadioButton* policy1 = new QRadioButton(_("...in &selected Textline"),policies);
			m_policyBoxGroup->addButton(policy1,static_cast<int>(NewWordMode::CurrentLine));
			policy1->setToolTip(_("From the one selected in the HOCR tree"));
			policy1->setFocusPolicy(Qt::NoFocus);
			policy1->setChecked(true);
			policyBoxLayout->addWidget(policy1);
		}
		QRadioButton* policy2 = new QRadioButton(_("...in nearest &Textline"),policies);
		policy2->setToolTip(_("The nearest above the cursor"));
		policy2->setFocusPolicy(Qt::NoFocus);
		policy2->setChecked(initialMode == NewWordMode::NearestLine);
		m_policyBoxGroup->addButton(policy2,static_cast<int>(NewWordMode::NearestLine));
		policyBoxLayout->addWidget(policy2);
		QRadioButton* policy3 = new QRadioButton(_("...in new Textline at &cursor"),policies);
		policy3->setToolTip(_("A new textline just below the Textline nearest above cursor"));
		policy3->setFocusPolicy(Qt::NoFocus);
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
		QCheckBox* fitCheckBox = new QCheckBox(_("&Fit"));
		fitCheckBox->setToolTip(_("Size new Bounding Box to fit text"));
		fitCheckBox->setFocusPolicy(Qt::NoFocus);
		QString currentTitle;
		int currentIndex;
		HOCRNormalize().currentDefault(currentTitle, currentIndex);
		QCheckBox* nrmCheckBox = new QCheckBox(_("&N%1 ").arg(currentIndex));
		nrmCheckBox->setToolTip(_("Apply most recent normalization (%1)").arg(currentTitle));
		nrmCheckBox->setFocusPolicy(Qt::NoFocus);
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
			// CTRL-W: take location from current mouse
			foundLine = pickLine(bbox.topLeft());
			foundItem = m_document->itemAtIndex(foundLine);
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
		ui.treeViewHOCR->scrollTo(index, QAbstractItemView::PositionAtCenter);
		m_proofReadWidget->updateWidget(true);
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

void OutputEditorHOCR::bulkOperation(/*inout*/ QModelIndex &index, const std::function <void ()>& op) {
	const HOCRItem* oldItem = m_document->itemAtIndex(index);
	const HOCRPage* pageItem = oldItem->page();
	bool oldExpanded = ui.treeViewHOCR->isExpanded(index);
	bool oldFullyExpanded = isFullyExpanded(m_document->indexAtItem(pageItem));

	op();
	index = m_document->indexAtItem(oldItem);

	if (oldFullyExpanded) {
		expandCollapseChildren(m_document->indexAtItem(pageItem), true);
	} else {
		expandCollapseChildren(index, oldExpanded);
	}
	ui.treeViewHOCR->setCurrentIndex(index);
	ui.treeViewHOCR->scrollTo(index, QAbstractItemView::PositionAtCenter);
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
		menu.setFocus();
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
			actionMerge = menu.addAction(_("&Merge"));
			if(firstItem->itemClass() != "ocr_carea") {
				actionSplit = menu.addAction(_("&Split from parent"));
			}
		}
		actionSwap = menu.addAction(_("S&wap two"));
		if(nIndices != 2) { // Swapping allowed
			actionSwap->setEnabled(false);
		}
		actionNormalize = menu.addAction(_("&Normalize all selected"));

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
			newIndex = indices.last();
			bulkOperation(newIndex, [this, &items]() {
				HOCRNormalize().normalizeTree(m_document, &items, m_keyParent);
			});
		}
		if(newIndex.isValid()) {
			ui.treeViewHOCR->selectionModel()->blockSignals(true);
			ui.treeViewHOCR->selectionModel()->clear();
			ui.treeViewHOCR->selectionModel()->blockSignals(false);
			ui.treeViewHOCR->selectionModel()->setCurrentIndex(newIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			ui.treeViewHOCR->scrollTo(newIndex, QAbstractItemView::PositionAtCenter);
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
	menu.setToolTipsVisible(true);
	menu.setFocus();
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
	QAction* actionFit = nullptr;
	QAction* actionSortX = nullptr;
	QAction* actionSortY = nullptr;
	QAction* actionFlatten = nullptr;
	QAction* actionClean = nullptr;
	QAction* nonActionMultiple = nullptr;

	nonActionMultiple = menu.addAction(_("Multiple Selection Menu"));
	nonActionMultiple->setEnabled(false);
	menu.addSeparator();

	QString itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		actionAddGraphic = menu.addAction(_("Add &graphic region"));
		actionAddCArea = menu.addAction(_("Add &text block"));
	} else if(itemClass == "ocr_carea") {
		actionAddPar = menu.addAction(_("Add &paragraph"));
	} else if(itemClass == "ocr_par") {
		actionAddLine = menu.addAction(_("Add &line"));
	} else if(itemClass == "ocr_line") {
		actionAddWord = menu.addAction(_("Add &word"));
	} else if(itemClass == "ocrx_word") {
		m_document->addSpellingActions(&menu, index);
	}
	if(!menu.actions().isEmpty()) {
		menu.addSeparator();
	}
	actionNormalize = menu.addAction(_("&Normalize"));
	if(itemClass == "ocrx_word" && item->isOverheight()) {
		actionFit = menu.addAction(_("Trim &height"));
		actionFit->setToolTip(_("Heuristic trim overheight word to font size"));
	}
	if(itemClass == "ocr_par" || itemClass == "ocr_line" || itemClass == "ocrx_word") {
		actionSplit = menu.addAction(_("&Split from parent"));
	}
	actionRemove = menu.addAction(_("&Remove"));
	actionRemove->setShortcut(QKeySequence(Qt::Key_Delete));
	if(m_document->rowCount(index) > 0) {
		actionExpand = menu.addAction(_("&Expand item"));
		actionCollapse = menu.addAction(_("&Collapse item"));
	}
	if(index.row() > 0) {
		actionMoveUp = menu.addAction(_("Move &Up"));
	}
	if(index.row() < index.model()->rowCount(index.parent()) - 1) {
		actionMoveDown = menu.addAction(_("Move &Down"));
	}
	if((itemClass == "ocr_page" || itemClass == "ocr_carea" || itemClass == "ocr_par")
		&& item->children().size() > 1) {
		actionSortY = menu.addAction(_("Sort immediate children on &Y position"));
	}
	if(itemClass == "ocr_line") {
		actionSortX = menu.addAction(_("Sort immediate children on &X position"));
	}
	if(itemClass == "ocr_page" || itemClass == "ocr_carea") {
		actionFlatten = menu.addAction(_("&Flatten"));
	}
	if(itemClass == "ocr_page" || itemClass == "ocr_carea" || itemClass == "ocr_par") {
		actionClean = menu.addAction(_("&Clean empty items"));
	}

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
		ui.treeViewHOCR->scrollTo(newIndex, QAbstractItemView::PositionAtCenter);
	} else if(clickedAction == actionNormalize) {
		bulkOperation(index, [this, index]() {
			QList<HOCRItem*>items = QList<HOCRItem*>({m_document->mutableItemAtIndex(index)});
			HOCRNormalize().normalizeTree(m_document, &items, m_keyParent);
			});
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
	} else if(clickedAction == actionFit) {
		m_document->fitToFont(index);
	} else if(clickedAction == actionSortX) {
		bool oldExpanded = ui.treeViewHOCR->isExpanded(index);
		m_document->sortOnX(index);
		expandCollapseChildren(index, oldExpanded);
	} else if(clickedAction == actionSortY) {
		bool oldExpanded = ui.treeViewHOCR->isExpanded(index);
		m_document->sortOnY(index);
		expandCollapseChildren(index, oldExpanded);
	} else if(clickedAction == actionFlatten) {
		bulkOperation(index, [this, index]() {m_document->flatten(index);});
	} else if(clickedAction == actionClean) {
		bulkOperation(index, [this, index]() {m_document->cleanEmptyItems(index);});
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
	ui.treeViewHOCR->scrollTo(index2, QAbstractItemView::PositionAtCenter);

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
	if(ev->type() == QEvent::FocusIn) {
		// Particularly for the first click after startup!
		QFocusEvent* fe = static_cast<QFocusEvent*>(ev);
		if (fe->reason() == Qt::MouseFocusReason) {
			m_proofReadWidget->showWidget(true);
		}
		return false;
	}
	if(ev->type() == QEvent::Enter) {
		// N.B. Enter events frequently occur when the mouse is in a window and something on the keyboard
		// causes the enter -- without the mouse being moved!
		if (obj == displayer) {
			// Don't steal focus except from tree view.
			QWidget* widget = qApp->focusWidget();
			if(widget == nullptr) {
				widget = ui.treeViewHOCR;
			}
			while (widget!=nullptr) {
				if (widget == ui.treeViewHOCR) {
					break;
				}
				widget = widget->parentWidget();
			}

			if (widget == ui.treeViewHOCR) {
				m_proofReadWidget->showWidget(true);
				displayer->setFocus(); // note: proxy likely from proofReadWidget
				return true;
			}
		}
		else if (obj == ui.treeViewHOCR) {
			// Don't steal focus except from displayer
			HOCRTextDelegate* delegate = static_cast<HOCRTextDelegate*>(ui.treeViewHOCR->itemDelegateForColumn(0));
			QWidget* widget = qApp->focusWidget();
			if(widget == nullptr) {
				widget = displayer;
			}
			while (widget!=nullptr) {
				if (widget == displayer) {
					break;
				}
				widget = widget->parentWidget();
			}
			if (widget == displayer) {
				m_proofReadWidget->showWidget(false);
				delegate->reSetSelection();
				return true;
			}
		}
	}
	return false;
}

void OutputEditorHOCR::keyPressEvent(QKeyEvent *event) {
	ui.treeViewHOCR->keyPressEvent(event);
}

static const QModelIndex emptyIndex = QModelIndex();
void OutputEditorHOCR::pickItem(const QPoint& point, QMouseEvent* event) {
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
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(item->itemClass() == "ocrx_word") {
		QItemSelectionModel* sel = ui.treeViewHOCR->selectionModel();
		QModelIndex origIndex = index;
		QModelIndex parentIndex = index.parent();
		QModelIndex oldParent = emptyIndex;

		while (parentIndex != emptyIndex) {
			if (sel->isSelected(parentIndex)) {
				index = parentIndex;
				parentIndex = parentIndex.parent();
				break;
			}
			parentIndex = parentIndex.parent();
		}
		if (parentIndex == emptyIndex || parentIndex.parent() == emptyIndex) {
			// It's useless to select the whole page here.
			oldParent = index;
			index = origIndex;
			parentIndex = index.parent();
		}

		if((event->modifiers() & Qt::ControlModifier) != 0) {
			if((event->modifiers() & Qt::ShiftModifier) != 0) {
				sel->select(index, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
				sel->select(oldParent, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
				sel->select(parentIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
				deselectChildren(sel, parentIndex);
				index = parentIndex;
			} else if(origIndex == index) {
				sel->select(index, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
			} else {
				// "explode" instead of toggle since we don't have any other way to remember it.
				sel->select(index, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
				const HOCRItem* parentItem = m_document->itemAtIndex(index);
				for (HOCRItem* childItem: parentItem->children()) {
					sel->select(m_document->indexAtItem(childItem), QItemSelectionModel::Select | QItemSelectionModel::Rows);
				}
			} 
			ui.treeViewHOCR->scrollTo(index, QAbstractItemView::PositionAtCenter);
		} else {
			if((event->modifiers() & Qt::ShiftModifier) != 0) {
				ui.treeViewHOCR->setCurrentIndex(parentIndex);
			} else {
				ui.treeViewHOCR->setCurrentIndex(origIndex);
				if (event->button() == Qt::RightButton) {
					QRect pos = ui.treeViewHOCR->visualRect(origIndex);
					emit ui.treeViewHOCR->customContextMenuRequested(pos.center());
				}
			}
		}
	} 
	ui.treeViewHOCR->scrollTo(ui.treeViewHOCR->currentIndex(), QAbstractItemView::PositionAtCenter);
	MAIN->getDisplayer()->setFocus();
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

void OutputEditorHOCR::deselectChildren(QItemSelectionModel *model, QModelIndex& index) {
	const HOCRItem* parentItem = m_document->itemAtIndex(index);
	for (HOCRItem* childItem: parentItem->children()) {
		QModelIndex childIndex =m_document->indexAtItem(childItem);
		model->select(childIndex, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
		deselectChildren(model, childIndex);
	}
}

void OutputEditorHOCR::toggleWConfColumn() {
	ui.treeViewHOCR->setColumnHidden(1, !ui.outputDialogUi.checkBox_WConf->isChecked());
}

bool OutputEditorHOCR::open(InsertMode mode, QStringList files) {
	if(mode == InsertMode::Replace && !clear(false)) {
		return false;
	}
	QString modeName;
	switch(mode) {
	case InsertMode::Replace:
		modeName = _("Replace");
		break;
	case InsertMode::Append:
		modeName = _("Append");
		break;
	case InsertMode::InsertBefore:
		modeName = _("Insert before");
		break;
	}
	if(files.isEmpty()) {
		QString suggestion = ConfigSettings::get<VarSetting<QString>>("lasthocrsave")->getValue();
		files = FileDialogs::openDialog(_("Open hOCR File (%1)").arg(modeName), suggestion, "outputdir", QString("%1 (*.html)").arg(_("hOCR HTML Files")), true, MAIN->getDialogHost());
	}
	if(files.isEmpty()) {
		return false;
	}
	int pos = mode == InsertMode::InsertBefore ? currentPage() : m_document->pageCount();
	QStringList failed;
	QStringList invalid;
	QStringList added;
	int currentPage = -1;
	for(const QString& filename : files) {
		QFile file(filename);
		if(!file.open(QIODevice::ReadOnly)) {
			failed.append(filename);
			continue;
		}
		QFileInfo info(file.fileName());
		m_baseNames.append(info.fileName());
		QDomDocument doc;
		doc.setContent(&file);
		QDomElement head = doc.firstChildElement("html").firstChildElement("head");
		QDomNodeList metadata = head.elementsByTagName("meta");
		for (int i=0; i<metadata.count(); i++) {
			QDomElement data = metadata.at(i).toElement();
			if(data.hasAttribute("name")) {
				if (data.attribute("name") == "ocr-current-page" && data.hasAttribute("content")) {
					QString pageString = data.attribute("content");
					// our page numbers are 1-base, but hOCR is 0 base (page 0 is "cover")
					currentPage = pageString.toInt() + pos - 1;
					break;
				}
			}
		}

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
			added.append(filename);
		}
	}
	MAIN->ui.dockWidgetOutput->setWindowTitle(m_baseNames.join(" "));
	if(added.size() > 0) {
		m_modified = mode != InsertMode::Replace;
		if(mode == InsertMode::Replace && m_filebasename.isEmpty()) {
			QFileInfo finfo(added.front());
			m_filebasename = finfo.absoluteDir().absoluteFilePath(finfo.completeBaseName());
		}
		MAIN->setOutputPaneVisible(true);
		ConfigSettings::get<VarSetting<QString>>("lasthocrsave")->setValue(added.front());
		if (currentPage >= 0) {
			selectPage(currentPage);
		}
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
	return added.size() > 0;
}

bool OutputEditorHOCR::selectPage(int nr) {
	if(!m_document || nr >= m_document->pageCount() || nr < 0) {
		return false;
	}
	QModelIndex index = m_document->indexAtItem(m_document->page(nr));
	if(index.isValid()) {
		ui.treeViewHOCR->setCurrentIndex(index);
		ui.treeViewHOCR->scrollTo(index, QAbstractItemView::PositionAtCenter);
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
		outname = FileDialogs::saveDialog(_("Save hOCR Output..."), suggestion + ".html", "outputdir", QString("%1 (*.html)").arg(_("hOCR HTML Files")),
			false, MAIN->getDialogHost());
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
	QModelIndex currentIndex = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* item = m_document->itemAtIndex(currentIndex);
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	const int pageNr = page->pageNr();
	QString header = QString(
	                     "<!DOCTYPE html>\n"
	                     "<html>\n"
	                     "<head>\n"
	                     " <title>%1</title>\n"
	                     " <meta charset=\"utf-8\" /> \n"
	                     " <meta name='ocr-system' content='tesseract %2' />\n"
	                     " <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
	                     " <meta name='ocr-current-page' content='%3'/>\n"
	                     "</head>\n").arg(QFileInfo(outname).fileName()).arg(tess.Version()).arg(QString::number(pageNr-1));
	file.write(header.toUtf8());
	m_document->convertSourcePaths(QFileInfo(outname).absolutePath(), false);
	file.write(m_document->toHTML().toUtf8());
	m_document->convertSourcePaths(QFileInfo(outname).absolutePath(), true);
	file.write("</html>\n");
	m_modified = false;
	QFileInfo finfo(outname);
	m_filebasename = finfo.absoluteDir().absoluteFilePath(finfo.completeBaseName());
	m_baseNames.clear();
	m_baseNames.append(finfo.fileName());
	MAIN->ui.dockWidgetOutput->setWindowTitle(finfo.fileName());
	ConfigSettings::get<VarSetting<QString>>("lasthocrsave")->setValue(finfo.absoluteFilePath());
	return true;
}

bool OutputEditorHOCR::crashSave(const QString& filename) const {
	QFile file(filename);
	QByteArray current = setlocale(LC_ALL, NULL);
	setlocale(LC_ALL, "C");
	tesseract::TessBaseAPI tess;
	setlocale(LC_ALL, current.constData());
	QModelIndex currentIndex = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* item = m_document->itemAtIndex(currentIndex);
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	const int pageNr = page->pageNr();
	if(file.open(QIODevice::WriteOnly)) {
		QString header = QString(
		                     "<!DOCTYPE html>\n"
		                     "<html>\n"
		                     "<head>\n"
		                     " <title>%1</title>\n"
		                     " <meta charset=\"utf-8\" /> \n"
							 " <meta name='ocr-system' content='tesseract %2' />\n"
		                     " <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
							 " <meta name='ocr-current-page' content='%3'/>\n"
							 "</head>\n").arg(QFileInfo(filename).fileName()).arg(tess.Version()).arg(QString::number(pageNr-1));
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

	QString outname = FileDialogs::saveDialog(_("Save ODT Output..."), suggestion + ".odt", "outputdir", QString("%1 (*.odt)").arg(_("OpenDocument Text Documents")),
			false, MAIN->getDialogHost());
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
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* item = m_document->itemAtIndex(current);
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	bool success = false;
	if(!newPage(page)) {
		return false;
	}

	ui.treeViewHOCR->selectionModel()->clear();
	HOCRPdfExportDialog dialog(m_tool, m_document, page, MAIN);
	FocusableMenu menuPdfShortcuts(ui.exportMenu);
	menuPdfShortcuts.useButtons();
	menuPdfShortcuts.mapButtonBoxDefault();
	if (menuPdfShortcuts.execWithMenu(&dialog) != QDialog::Accepted) {
		ui.treeViewHOCR->setCurrentIndex(current);
		ui.treeViewHOCR->scrollTo(current, QAbstractItemView::PositionAtCenter);
		return false;
	}
	HOCRPdfExporter::PDFSettings& settings = dialog.getPdfSettings();

	QString suggestion = m_filebasename;
	if(suggestion.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
	}

	QString outname;
	while(true) {
		outname = FileDialogs::saveDialog(_("Save PDF Output..."), suggestion + ".pdf", "outputdir", QString("%1 (*.pdf)").arg(_("PDF Files")),
			false, MAIN->getDialogHost());
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
		ui.treeViewHOCR->setCurrentIndex(current);
		ui.treeViewHOCR->scrollTo(current, QAbstractItemView::PositionAtCenter);
		return false;
	}

	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	MAIN->getDisplayer()->setBlockAutoscale(true);
	success = HOCRPdfExporter().run(m_document, outname, &settings);
	MAIN->getDisplayer()->setBlockAutoscale(false);
	newPage(item->page());
	ui.treeViewHOCR->setCurrentIndex(current);
	ui.treeViewHOCR->scrollTo(current, QAbstractItemView::PositionAtCenter);
	return success;
}

bool OutputEditorHOCR::exportToText() {
	QString suggestion = m_filebasename;
	if(suggestion.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
	}

	QString outname = FileDialogs::saveDialog(_("Save Text Output..."), suggestion + ".txt", "outputdir", QString("%1 (*.txt)").arg(_("Text Files")),
			false, MAIN->getDialogHost());
	if(outname.isEmpty()) {
		return false;
	}

	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	MAIN->getDisplayer()->setBlockAutoscale(true);
	bool success = HOCRTextExporter().run(m_document, outname);
	MAIN->getDisplayer()->setBlockAutoscale(false);
	return success;
}

bool OutputEditorHOCR::exportToIndentedText() {
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* item = m_document->itemAtIndex(current);
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	bool success = false;
	if(!newPage(page)) {
		return false;
	}

	ui.treeViewHOCR->selectionModel()->clear();
	HOCRIndentedTextExportDialog dialog(m_tool, m_document, page, MAIN);
	FocusableMenu menuIndentedShortcuts(ui.exportMenu);
	menuIndentedShortcuts.useButtons();
	menuIndentedShortcuts.mapButtonBoxDefault();
	if (menuIndentedShortcuts.execWithMenu(&dialog) != QDialog::Accepted) {
		ui.treeViewHOCR->setCurrentIndex(current);
		ui.treeViewHOCR->scrollTo(current, QAbstractItemView::PositionAtCenter);
		return false;
	}
	HOCRIndentedTextExporter::IndentedTextSettings& settings = dialog.getIndentedTextSettings();

	QString suggestion = m_filebasename;
	if(suggestion.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
	}

	QString outname = FileDialogs::saveDialog(_("Save Indented Text Output..."), suggestion + ".txt", "outputdir", QString("%1 (*.txt)").arg(_("Text Files")),
			false, MAIN->getDialogHost());
	if(outname.isEmpty()) {
		ui.treeViewHOCR->setCurrentIndex(current);
		ui.treeViewHOCR->scrollTo(current, QAbstractItemView::PositionAtCenter);
		return false;
	}

	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	MAIN->getDisplayer()->setBlockAutoscale(true);
	success = HOCRIndentedTextExporter().run(m_document, outname, &settings);
	MAIN->getDisplayer()->setBlockAutoscale(false);
	ui.treeViewHOCR->setCurrentIndex(current);
	ui.treeViewHOCR->scrollTo(current, QAbstractItemView::PositionAtCenter);
	return success;
}

bool OutputEditorHOCR::clear(bool hide) {
	m_previewTimer.stop();
	if(!m_widget->isVisible()) {
		return true;
	}
	if(m_modified) {
		int response = KeyMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), KeyMessageBox::Save | KeyMessageBox::Discard | KeyMessageBox::Cancel);
		if(response == KeyMessageBox::Save) {
			if(!save()) {
				return false;
			}
		} else if(response != KeyMessageBox::Discard) {
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
	m_baseNames.clear();
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
		bool matchesSearch = delegate->selectedText().compare(searchstr, cs) == 0;
		int selStart = delegate->selectionStart();
		if(matchesSearch && replace) {
			QString oldText = delegate->text();
			delegate->setText(oldText.left(selStart) + replacestr + oldText.mid(selStart + searchstr.length()));
			delegate->setSelection(selStart, replacestr.length());
			return true;
		}
		bool matchesReplace = delegate->selectedText().compare(replacestr, cs) == 0;
		int pos = -1;
		if(backwards) {
			pos = selStart - 1;
			pos = pos < 0 ? -1 : delegate->text().lastIndexOf(searchstr, pos, cs);
		} else {
			pos = matchesSearch ? selStart + searchstr.length() : matchesReplace ? selStart + replacestr.length() : selStart;
			pos = delegate->text().indexOf(searchstr, pos, cs);
		}
		if(pos != -1) {
			delegate->setSelection(pos, searchstr.length());
			return true;
		}
		currentSelectionMatchesSearch = matchesSearch;
		return false;
	}
	// Otherwise, if item contains text, set it in edit mode
	int pos = backwards ? item->text().lastIndexOf(searchstr, -1, cs) : item->text().indexOf(searchstr, 0, cs);
	if(pos != -1) {
		ui.treeViewHOCR->setCurrentIndex(index);
		ui.treeViewHOCR->scrollTo(index, QAbstractItemView::PositionAtCenter);
		ui.treeViewHOCR->edit(index);
		delegate->setSelection(pos, searchstr.length());
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

void OutputEditorHOCR::reFocusTree() {
	HOCRTextDelegate* delegate = static_cast<HOCRTextDelegate*>(ui.treeViewHOCR->itemDelegateForColumn(0));
	delegate->reSetSelection();
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
			ui.treeViewHOCR->scrollTo(pageIndex, QAbstractItemView::PositionAtCenter);
		}
	}
	showPreview(OutputEditorHOCR::showMode::show);
	if (MAIN->getDisplayer()->underMouse()) {
		m_proofReadWidget->showWidget(true);
	}
}

void OutputEditorHOCR::previewToggled() {
	showPreview(OutputEditorHOCR::showMode::show);
}

void OutputEditorHOCR::showPreview(OutputEditorHOCR::showMode mode) {
	const HOCRItem* item = m_document->itemAtIndex(ui.treeViewHOCR->currentIndex());
	bool inv = false;
	bool suppressed = false;
	switch(mode) {
	case invert:
		inv = true;
		break;
	case show:
		break;
	case suspend:
		m_suspended = true;
		break;
	case resume:
		m_suspended = false;
		break;
	}
	if(item != nullptr && !m_suspended && (ui.outputDialogUi.checkBox_Preview->isChecked()^inv)) {
		updatePreview();
		m_preview->show();
		if (MAIN->getDisplayer()->underMouse()) {
			m_proofReadWidget->showWidget(true);
		}
	} else {
		m_preview->setVisible(false);
		m_proofReadWidget->showWidget(false);
	}
}

void OutputEditorHOCR::updatePreview() {
	const HOCRItem* item = m_document->itemAtIndex(ui.treeViewHOCR->currentIndex());
	if(!item) {
		m_preview->setVisible(false);
		return;
	}

	const HOCRPage* page = item->page();
	const QRect& bbox = page->bbox();
	m_pageDpi = page->resolution();

	QImage image(bbox.size(), QImage::Format_ARGB32);
	image.fill(QColor(255, 255, 255, 63));
	image.setDotsPerMeterX(m_pageDpi / 0.0254); // 1 in = 0.0254 m
	image.setDotsPerMeterY(m_pageDpi / 0.0254);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);

	drawPreview(painter, page);

	m_preview->setPixmap(QPixmap::fromImage(image));
	m_preview->setPos(-0.5 * bbox.width(), -0.5 * bbox.height());
	m_preview->setVisible(true);
	m_proofReadWidget->showWidget(true);
}

static const QString *useDefaultFlag = new QString();
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
			QFontMetrics fm(font);
			painter.setFont(font);

			if (ui.outputDialogUi.checkBox_Overheight->isChecked() && wordItem->isOverheight()) {
				painter.save();
				painter.setBrush(QColor(255, 255, 63, 128)); // yellowish
				painter.drawRect(wordItem->bbox());
				painter.restore();

			}

			QString displayText = wordItem->text();
			const QString* shadowText = wordItem->shadowText();
			bool usingShadowText = false;
			if (!ui.outputDialogUi.checkBox_NonAscii->isChecked()) {
				// nothing
			} else if (shadowText == nullptr) {
				bool isAscii = true;
				for (const QChar ch: displayText) {
					if(ch >= 0x7f) {
						isAscii = false;
						break;
					}
				}

				if (isAscii) {
				    wordItem->setShadowText(useDefaultFlag);
				} else {
					QString* newText = new QString(displayText);
					newText->replace(QChar('<'), QString("&lt;"), Qt::CaseSensitive);
					QRegularExpression re = QRegularExpression("([^\\x{0000}-\\x{007f}]+)");
					newText->replace(re,"<span style=\"background:#a0ffff00; color:magenta\">\\1</span>");
					wordItem->setShadowText(newText);
					displayText = *newText;
					usingShadowText = true;
				}
			} 
			else if (shadowText != useDefaultFlag) {
				displayText = *shadowText;
				usingShadowText = true;
			}

			painter.save();
			QTextDocument doc;
			if (usingShadowText) {
				doc.setHtml(displayText);
			} else {
				// If it's the default string, it might have bare '<'s in it.
				doc.setPlainText(displayText);
			} 
			doc.setDocumentMargin(0);
			doc.setDefaultFont(font);
			doc.setTextWidth(100000); // for side-effect

			double x, y;
			if(textangle == 0) {
				// See https://github.com/kba/hocr-spec/issues/15
				// double y is the y coordinate of the mid-point along the x axis on baseline for line first*x+second (mx+b)
				// Using the lineRect.bottom() for the whole line gives much more readable results on horizontal text.
				x = wordRect.x();
				y = lineRect.bottom() + (wordRect.center().x() - lineRect.x()) * baseline.first + baseline.second
				    + (-doc.size().height()+fm.descent())*m_pageDpi/96.;
			} else {
				// ... but wordRect.bottom is more accurate; for nonzero angles we'll choose that.
				// The only actual value seen here has been 90.
				x = wordRect.x()+(wordRect.right()-wordRect.left())*std::sin(textangle/180.0 * M_PI)
				    + (-doc.size().height()+fm.descent())*m_pageDpi/96.;
				y = wordRect.bottom() + (wordRect.center().x() - lineRect.x()) * baseline.first + baseline.second;
			}

			painter.translate(x,y);
			painter.rotate(-textangle);
			painter.scale((m_pageDpi/96.)*(ui.outputDialogUi.doubleSpinBox_Stretch->value()/100.), m_pageDpi/96.);
			doc.drawContents(&painter);
			painter.restore();
		}
	} else if(itemClass == "ocr_graphic") {
		painter.drawImage(item->bbox(), m_tool->getSelection(item->bbox()));
	} else {
		for(HOCRItem* childItem : item->children()) {
			drawPreview(painter, childItem);;
		}
	}
}

void OutputEditorHOCR::showSelections(const QItemSelection& selected, const QItemSelection& deselected) {
	QItemSelectionModel* model = ui.treeViewHOCR->selectionModel();
	QModelIndexList selections = model->selectedRows();
	if(!selections.empty()) {
		const HOCRItem* item = m_document->itemAtIndex(selections.first());
		const HOCRPage* page = item->page();
		const QRect& bbox = page->bbox();
		int pageDpi = page->resolution();

		QImage image(bbox.size(), QImage::Format_ARGB32);
		image.fill(QColor(255, 255, 255, 63));
		image.setDotsPerMeterX(pageDpi / 0.0254); // 1 in = 0.0254 m
		image.setDotsPerMeterY(pageDpi / 0.0254);
		QPainter painter(&image);
		painter.setRenderHint(QPainter::Antialiasing, false);

		QColor c = QPalette().highlight().color();
		painter.setBrush(QColor(c.red(), c.green(), c.blue(), 31));

		for(QModelIndex sel : selections) {
			if (sel != model->currentIndex()) {
				const HOCRItem* item = m_document->itemAtIndex(sel);
				painter.drawRect(item->bbox());
			}
		}
		m_selectedItems->setPixmap(QPixmap::fromImage(image));
		m_selectedItems->setPos(-0.5 * bbox.width(), -0.5 * bbox.height());
		m_selectedItems->setVisible(true);
	}
}

void OutputEditorHOCR::doPreferences(FocusableMenu *keyParent) {
	FocusableMenu menu(keyParent);
	menu.useButtons();
	menu.mapButtonBoxDefault();
	menu.execWithMenu(ui.outputDialog);
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

QRectF OutputEditorHOCR::getWidgetGeometry() {
	return MAIN->getDisplayer()->mapToScene(m_proofReadWidget->geometry()).boundingRect();
}