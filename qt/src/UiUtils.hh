/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * UiUtils.hh
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

#ifndef UIUTILS_HH
#define UIUTILS_HH

#include <functional>

class BlinkWidget : QObject {
	Q_OBJECT
public:
	BlinkWidget(int count, const std::function<void()>& on, const std::function<void()>& off, QObject*parent = nullptr);
protected:
	~BlinkWidget() {}; // class must be used with 'new'

private:
	QTimer *m_timer;
	int m_count;
	const std::function<void()> m_on;
	const std::function<void()> m_off;
};

#endif