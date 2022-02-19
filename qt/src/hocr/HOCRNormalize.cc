/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRNormalizeDialog.cc
 * Copyright (C) 2021-2022 Donn Terry<aesopPlayer@gmail.com>
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

#include "HOCRNormalize.hh"
#include "ConfigSettings.hh"
#include "MainWindow.hh"
#include "Displayer.hh"
#include "OutputEditorHOCR.hh"
#include "Utils.hh"
#include "SubstitutionsManager.hh"

#include <algorithm>
#include <QDebug>
#include <QPushButton>
#include <QFontMetrics>

class PreferenceChoice {
	public:
	PreferenceChoice(const QString& instance) : m_instance(instance) {}

	const QString m_instance = nullptr;
	const QFont* m_preferredFont = nullptr;
	int m_preferredSize = 8;
	SubstitutionsManager *m_subManager;
	const QMap<QString, QString>* m_substitutions;

	bool getNormalizeBBox() {
		QString name = "normalizeBBox_" + m_instance;
	    return ConfigSettings::get<SwitchSetting>(name)->getValue();
	}
	bool getTrimHeight() {
		QString name = "normalizeTrimHeight_" + m_instance;
	    return ConfigSettings::get<SwitchSetting>(name)->getValue();
	}
	bool getNormalizeBase() {
		QString name = "normalizeBase_" + m_instance;
	    return ConfigSettings::get<SwitchSetting>(name)->getValue();
	}
	bool getNormalizeFontSize() {
		QString name = "normalizeFontSize_" + m_instance;
	    return ConfigSettings::get<SwitchSetting>(name)->getValue();
	}
	bool getNormalizeFont() {
		QString name = "normalizeFont_" + m_instance;
	    return ConfigSettings::get<SwitchSetting>(name)->getValue();
	}
	Qt::CheckState getSetBold() {
		QString name = "normalizeSetBold_" + m_instance;
	    return ConfigSettings::get<SwitchSettingTri>(name)->getValue();
	}
	Qt::CheckState getSetItalic() {
		QString name = "normalizeSetItalic_" + m_instance;
	    return ConfigSettings::get<SwitchSettingTri>(name)->getValue();
	}
	const QFont* getFont() const {
	    return m_preferredFont;
	}
	int getFontSize() {
	    return m_preferredSize;
	}
	bool getApplySubst() {
		QString name = "normalizeApplySubst_" + m_instance;
	    return ConfigSettings::get<SwitchSetting>(name)->getValue();
	}
	const QString getTitle() {
		QString name = "normalizeTitle_" + m_instance;
	    return ConfigSettings::get<LineEditSetting>(name)->getValue();
	}

	~PreferenceChoice() {
		delete m_substitutions;
	}
};

HOCRNormalize::HOCRNormalizeDialog::HOCRNormalizeDialog(HOCRNormalize* parent) : QDialog(MAIN->getDisplayer()) {
	m_parent = parent;
	ui.setupUi(this);

	// Group 0
	PreferenceChoice *pref = m_parent->m_preferences[0] = new PreferenceChoice("0");

	ADD_SETTING(SwitchSetting("normalizeBBox_0", ui.normalizeBBox_0, false));
	ADD_SETTING(SwitchSetting("normalizeTrimHeight_0", ui.trimHeight_0, false));
	ADD_SETTING(SwitchSetting("normalizeFontSize_0", ui.normalizeFontSize_0, false));
	ADD_SETTING(SwitchSetting("normalizeFont_0", ui.normalizeFont_0, false));
	ADD_SETTING(SwitchSettingTri("normalizeSetBold_0", ui.setBold_0, Qt::PartiallyChecked));
	ADD_SETTING(SwitchSettingTri("normalizeSetItalic_0", ui.setItalic_0, Qt::PartiallyChecked));
	ADD_SETTING(LineEditSetting("normalizeTitle_0", ui.title_0));
	ADD_SETTING(SwitchSetting("normalizeApplySubst_0", ui.applySubst_0, false));

	connect(ui.preferredFont_0, &QFontComboBox::currentFontChanged, this, [this] (const QFont& font) {fontName(0, font, ui.preferredFont_0);} );
	ADD_SETTING(FontComboSetting("normalizePreferredFont_0", ui.preferredFont_0));

	connect(ui.preferredSize_0, qOverload<int>(&QSpinBox::valueChanged), this, [this] (int size) {fontSize(0, size, ui.preferredSize_0);} );
	ADD_SETTING(SpinSetting("normalizePreferredSize_0", ui.preferredSize_0,8));

	pref->m_subManager = new SubstitutionsManager("normalizesubst_0", this);
	connect(ui.openSubst_0, &QPushButton::clicked, this, [this] (bool) {openSubst(0);} );
	connect(pref->m_subManager, &SubstitutionsManager::applySubstitutions, this, [this] (const QMap<QString, QString>& p) { applySubstitutionsToSelected(0, p);} );

	connect(ui.buttonBoxApply_0->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] {apply(0);} );

	// Group 1
	pref = m_parent->m_preferences[1] = new PreferenceChoice("1");

	ADD_SETTING(SwitchSetting("normalizeBBox_1", ui.normalizeBBox_1, false));
	ADD_SETTING(SwitchSetting("normalizeTrimHeight_1", ui.trimHeight_1, false));
	ADD_SETTING(SwitchSetting("normalizeFontSize_1", ui.normalizeFontSize_1, false));
	ADD_SETTING(SwitchSetting("normalizeFont_1", ui.normalizeFont_1, false));
	ADD_SETTING(SwitchSettingTri("normalizeSetBold_1", ui.setBold_1, Qt::PartiallyChecked));
	ADD_SETTING(SwitchSettingTri("normalizeSetItalic_1", ui.setItalic_1, Qt::PartiallyChecked));
	ADD_SETTING(LineEditSetting("normalizeTitle_1", ui.title_1));
	ADD_SETTING(SwitchSetting("normalizeApplySubst_1", ui.applySubst_1, false));

	connect(ui.preferredFont_1, &QFontComboBox::currentFontChanged, this, [this] (const QFont& font) {fontName(1, font, ui.preferredFont_1);} );
	ADD_SETTING(FontComboSetting("normalizePreferredFont_1", ui.preferredFont_1));

	connect(ui.preferredSize_1, qOverload<int>(&QSpinBox::valueChanged), this, [this] (int size) {fontSize(1, size, ui.preferredSize_1);} );
	ADD_SETTING(SpinSetting("normalizePreferredSize_1", ui.preferredSize_1,8));

	pref->m_subManager = new SubstitutionsManager("normalizesubst_1", this);
	connect(ui.openSubst_1, &QPushButton::clicked, this, [this] (bool) {openSubst(1);} );
	connect(pref->m_subManager, &SubstitutionsManager::applySubstitutions, this, [this] (const QMap<QString, QString>& p) { applySubstitutionsToSelected(1, p);} );

	connect(ui.buttonBoxApply_1->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] {apply(1);} );

	// Group 2
	pref = m_parent->m_preferences[2] = new PreferenceChoice("2");

	ADD_SETTING(SwitchSetting("normalizeBBox_2", ui.normalizeBBox_2, false));
	ADD_SETTING(SwitchSetting("normalizeTrimHeight_2", ui.trimHeight_2, false));
	ADD_SETTING(SwitchSetting("normalizeFontSize_2", ui.normalizeFontSize_2, false));
	ADD_SETTING(SwitchSetting("normalizeFont_2", ui.normalizeFont_2, false));
	ADD_SETTING(SwitchSettingTri("normalizeSetBold_2", ui.setBold_2, Qt::PartiallyChecked));
	ADD_SETTING(SwitchSettingTri("normalizeSetItalic_2", ui.setItalic_2, Qt::PartiallyChecked));
	ADD_SETTING(LineEditSetting("normalizeTitle_2", ui.title_2));
	ADD_SETTING(SwitchSetting("normalizeApplySubst_2", ui.applySubst_2, false));

	connect(ui.preferredFont_2, &QFontComboBox::currentFontChanged, this, [this] (const QFont& font) {fontName(2, font, ui.preferredFont_2);} );
	ADD_SETTING(FontComboSetting("normalizePreferredFont_2", ui.preferredFont_2));

	connect(ui.preferredSize_2, qOverload<int>(&QSpinBox::valueChanged), this, [this] (int size) {fontSize(2, size, ui.preferredSize_2);} );
	ADD_SETTING(SpinSetting("normalizePreferredSize_2", ui.preferredSize_2,8));

	pref->m_subManager = new SubstitutionsManager("normalizesubst_2", this);
	connect(ui.openSubst_2, &QPushButton::clicked, this, [this] (bool) {openSubst(2);} );
	connect(pref->m_subManager, &SubstitutionsManager::applySubstitutions, this, [this] (const QMap<QString, QString>& p) { applySubstitutionsToSelected(2, p);} );

	connect(ui.buttonBoxApply_2->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] {apply(2);} );

	// Group 3
	pref = m_parent->m_preferences[3] = new PreferenceChoice("3");

	ADD_SETTING(SwitchSetting("normalizeBBox_3", ui.normalizeBBox_3, false));
	ADD_SETTING(SwitchSetting("normalizeTrimHeight_3", ui.trimHeight_3, false));
	ADD_SETTING(SwitchSetting("normalizeFontSize_3", ui.normalizeFontSize_3, false));
	ADD_SETTING(SwitchSetting("normalizeFont_3", ui.normalizeFont_3, false));
	ADD_SETTING(SwitchSettingTri("normalizeSetBold_3", ui.setBold_3, Qt::PartiallyChecked));
	ADD_SETTING(SwitchSettingTri("normalizeSetItalic_3", ui.setItalic_3, Qt::PartiallyChecked));
	ADD_SETTING(LineEditSetting("normalizeTitle_3", ui.title_3));
	ADD_SETTING(SwitchSetting("normalizeApplySubst_3", ui.applySubst_3, false));

	connect(ui.preferredFont_3, &QFontComboBox::currentFontChanged, this, [this] (const QFont& font) {fontName(3, font, ui.preferredFont_3);} );
	ADD_SETTING(FontComboSetting("normalizePreferredFont_3", ui.preferredFont_3));

	connect(ui.preferredSize_3, qOverload<int>(&QSpinBox::valueChanged), this, [this] (int size) {fontSize(3, size, ui.preferredSize_3);} );
	ADD_SETTING(SpinSetting("normalizePreferredSize_3", ui.preferredSize_3,8));

	pref->m_subManager = new SubstitutionsManager("normalizesubst_3", this);
	connect(ui.openSubst_3, &QPushButton::clicked, this, [this] (bool) {openSubst(3);} );
	connect(pref->m_subManager, &SubstitutionsManager::applySubstitutions, this, [this] (const QMap<QString, QString>& p) { applySubstitutionsToSelected(3, p);} );

	connect(ui.buttonBoxApply_3->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] {apply(3);} );

    // common
	connect(ui.buttonBoxCancel, &QDialogButtonBox::rejected, this, &QDialog::reject);
	ADD_SETTING(VarSetting<int>("normalizePreference", 0));
}

void HOCRNormalize::HOCRNormalizeDialog::fontName(int index, const QFont& font, QFontComboBox *fontBox ) {
	m_parent->m_preferences[index]->m_preferredFont = new QFont(font);
}

void HOCRNormalize::HOCRNormalizeDialog::fontSize(int index, int size, QSpinBox *editBox) {
	m_parent->m_preferences[index]->m_preferredSize = size;
}

void HOCRNormalize::HOCRNormalizeDialog::openSubst(const int index) {
	for (int i=0; i<4; i++) {
		m_parent->m_preferences[i]->m_subManager->hide();
	}
	m_parent->m_preferences[index]->m_subManager->show();
	m_parent->m_preferences[index]->m_subManager->raise();
}

void HOCRNormalize::HOCRNormalizeDialog::applySubstitutionsToSelected(const int index, const QMap<QString, QString>& substitutions) {
	// For user pressing Apply in subst manager window from Normalize: just substitute, nothing else
	PreferenceChoice* pref = m_parent->m_preferences[index];
	m_parent->normalizeSelection(pref, true);
}

void HOCRNormalize::HOCRNormalizeDialog::apply(const int index) {
	ConfigSettings::get<VarSetting<int>>("normalizePreference")->setValue(index);
	PreferenceChoice* pref = m_parent->m_preferences[index];
	m_parent->normalizeSelection(pref, false);
	close();
}

void HOCRNormalize::normalizeTree(HOCRDocument* hocrdocument, QList<HOCRItem*>* items) {
	m_doc = hocrdocument;
	m_items = items;
	m_dialog = new HOCRNormalizeDialog(this);

	m_dialog->exec();
}

void HOCRNormalize::normalizeSingle(HOCRDocument* hocrdocument, const HOCRItem* item) {
	m_doc = hocrdocument;
	m_dialog = new HOCRNormalizeDialog(this);
	int currentChoice = ConfigSettings::get<VarSetting<int>>("normalizePreference")->getValue();
	// Don't substitute here... this is for add-word.
	m_preferences[currentChoice]->m_substitutions = nullptr; 
	normalizeItem(item, m_preferences[currentChoice], false);
}

void HOCRNormalize::currentDefault(QString& title, int& number) {
	m_dialog = new HOCRNormalizeDialog(this);
	number = ConfigSettings::get<VarSetting<int>>("normalizePreference")->getValue();
	title = m_preferences[number]->getTitle();
	number++; // one-based to user
}

void HOCRNormalize::normalizeSelection(PreferenceChoice *pref, bool substituteOnly) {
	if (pref->getApplySubst() || substituteOnly) {
		pref->m_substitutions = pref->m_subManager->getSubstitutions();
	} else {
		pref->m_substitutions = nullptr;
	}
	bool success = Utils::busyTask([this, pref, substituteOnly] {
		for (HOCRItem* item:*m_items) {
			normalizeItem(item, pref, substituteOnly);
		}
		return true;
	}, _("Normalizing ..."));
}

void HOCRNormalize::normalizeItem(const HOCRItem* item, PreferenceChoice *pref, bool substituteOnly) {
	QString itemClass = item->itemClass();
	const QMap<QString,QString> attr = item->getAttributes();

	QModelIndex index = m_doc->indexAtItem(item);
	if(itemClass == "ocrx_word") {
		if (pref->m_substitutions != nullptr) {
			Qt::CaseSensitivity cs = Qt::CaseSensitive;
			for(auto it = pref->m_substitutions->begin(), itEnd = pref->m_substitutions->end(); it != itEnd; ++it) {
				QString search = it.key();
				QString replace = it.value();
				m_doc->setData(index, item->text().replace(search, replace, cs), Qt::EditRole);
			}
		}
		if (substituteOnly) {
			return;
		}
		if(pref->getNormalizeFont()) {
			m_doc->editItemAttribute(index, "title:x_font", pref->getFont()->family(), itemClass);
		}
		if(pref->getNormalizeFontSize()) {
			int oldSize = item->fontSize();
			int newSize = pref->getFontSize();
			m_doc->editItemAttribute(index, "title:x_fsize", QString::number(newSize), itemClass);

			double scale = static_cast<double>(newSize)/static_cast<double>(oldSize);
			if(scale != 1.0 && pref->getNormalizeBBox()) {
				QRect bbox = item->bbox();
				int w = bbox.width();
				int h = bbox.height();
				int deltaw = w*scale - w;
				int deltah = h*scale - h;
				// scale leaving midpoint of left edge fixed
				QString newBox = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()-deltah/2).arg(bbox.right()+deltaw).arg(bbox.bottom()+deltah/2);
				m_doc->editItemAttribute(index, "title:bbox", newBox, itemClass);
			}
		}
		if(pref->getSetBold() != Qt::PartiallyChecked) {
			m_doc->editItemAttribute(index, "bold", pref->getSetBold() == Qt::Checked ? "1" : "0", itemClass);
		}
		if(pref->getSetItalic() != Qt::PartiallyChecked) {
			m_doc->editItemAttribute(index, "italic", pref->getSetItalic() == Qt::Checked ? "1" : "0", itemClass);
		}
		if(pref->getTrimHeight() && item->isOverheight(true)) {
			m_doc->fitToFont(index);
		}
		return;
	} 

	for(int i = 0, n = item->children().size(); i < n; ++i) {
		normalizeItem(item->children()[i], pref, substituteOnly);
	}
}