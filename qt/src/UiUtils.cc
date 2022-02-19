/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * UiUtils.cc
 * Copyright (C) 2021-2022 Donn Terry <aesopPlayer@gmail.com>
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

#include <QDialogButtonBox>
#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QShortcut>
#include <QTimer>

#include "MainWindow.hh"
#include "UiUtils.hh"

BlinkWidget::BlinkWidget(int count, const std::function<void()>& on, const std::function<void()>& off, QObject* parent) :
	m_on(on), m_off(off), m_count(count) {
	m_timer = new QTimer(parent);
	QApplication::connect(m_timer, &QTimer::timeout, [this](){
		if (m_count-- %2 == 1) {
			m_off();
		} else {
			m_on();
		}
		if (m_count <= 0) {
			m_timer->stop();
			QApplication::disconnect(m_timer, &QTimer::timeout, nullptr, nullptr);
			delete m_timer;
			delete this;
		}
	});
	m_timer->start(500);
}