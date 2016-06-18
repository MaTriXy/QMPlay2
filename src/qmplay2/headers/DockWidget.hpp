/*
	QMPlay2 is a video and audio player.
	Copyright (C) 2010-2016  Błażej Szczygieł

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published
	by the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DOCKWIDGET_HPP
#define DOCKWIDGET_HPP

#include <QDockWidget>

class DockWidget : public QDockWidget
{
public:
	inline DockWidget() :
		titleBarVisible(true), globalTitleBarVisible(true)
	{}
	inline void setTitleBarVisible(bool v = true)
	{
		setTitleBarWidget((titleBarVisible = v) && globalTitleBarVisible ? NULL : &emptyW);
	}
	inline void setGlobalTitleBarVisible(bool v)
	{
		globalTitleBarVisible = v;
		setTitleBarVisible(titleBarVisible);
	}
private:
	class EmptyW : public QWidget
	{
#if QT_VERSION >= 0x050000 && QT_VERSION < 0x050600
		void showEvent(QShowEvent *);
#endif

		QSize sizeHint() const;
	} emptyW;
	bool titleBarVisible, globalTitleBarVisible;
};

#endif // DOCKWIDGET_HPP
