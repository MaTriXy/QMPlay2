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

#include <PLS.hpp>

#include <Functions.hpp>
#include <Reader.hpp>
#include <Writer.hpp>

inline void prepareList(Playlist::Entries &list, int idx)
{
	if (idx <= list.size() - 1)
		return;
	for (int i = list.size(); i <= idx; i++)
		list += Playlist::Entry();
}

/**/

Playlist::Entries PLS::read()
{
	Reader *reader = ioCtrl.rawPtr<Reader>();
	Entries list;

	QString playlistPath = Functions::filePath(reader->getUrl());
	if (playlistPath.startsWith("file://"))
		playlistPath.remove(0, 7);
	else
		playlistPath.clear();

	const QList<QByteArray> playlistLines = readLines();
	for (int i = 0; i < playlistLines.count(); ++i)
	{
		const QByteArray &line = playlistLines[i];
		if (line.isEmpty())
			continue;
		const int idx = line.indexOf('=');
		if (idx < 0)
			continue;

		int number_idx = -1;
		for (int i = 0; i < line.length(); ++i)
		{
			if (line[i] == '=')
				break;
			if (line[i] >= '0' && line[i] <= '9')
			{
				number_idx = i;
				break;
			}
		}
		if (number_idx == -1)
			continue;

		const QByteArray key = line.left(number_idx);
		const QByteArray value = line.mid(idx+1);
		const int entry_idx = line.mid(number_idx, idx - number_idx).toInt() - 1;

		prepareList(list, entry_idx);
		if (entry_idx < 0 || entry_idx > list.size() - 1)
			continue;

		if (key == "File")
			list[entry_idx].url = Functions::Url(value, playlistPath);
		else if (key == "Title")
			(list[entry_idx].name = value).replace('\001', '\n');
		else if (key == "Length" && list[entry_idx].length == -1.0)
			list[entry_idx].length = value.toInt();
		else if (key == "QMPlay_length")
			list[entry_idx].length = value.toDouble();
		else if (key == "QMPlay_sel")
			list[entry_idx].selected = value.toInt();
		else if (key == "QMPlay_queue")
			list[entry_idx].queue = value.toInt();
		else if (key == "QMPlay_GID")
			list[entry_idx].GID = value.toInt();
		else if (key == "QMPlay_parent")
			list[entry_idx].parent = value.toInt();
	}

	return list;
}
bool PLS::write(const Entries &list)
{
	Writer *writer = ioCtrl.rawPtr<Writer>();
	writer->write(QString("[playlist]\r\nNumberOfEntries=" + QString::number(list.size()) + "\r\n").toUtf8());
	for (int i = 0; i < list.size(); i++)
	{
		const Playlist::Entry &entry = list[i];
		const QString idx = QString::number(i+1);
		QString url = entry.url;
		const bool isFile = url.startsWith("file://");
		if (isFile)
		{
			url.remove(0, 7);
#ifdef Q_OS_WIN
			url.replace("/", "\\");
#endif
		}
		if (!url.isEmpty())
			writer->write(QString("File" + idx + "=" + url + "\r\n").toUtf8());
		if (!entry.name.isEmpty())
			writer->write(QString("Title" + idx + "=" + QString(entry.name).replace('\n', '\001') + "\r\n").toUtf8());
		if (entry.length >= 0.0)
		{
			writer->write(QString("Length" + idx + "=" + QString::number((qint32)(entry.length + 0.5)) + "\r\n").toUtf8());
			writer->write(QString("QMPlay_length" + idx + "=" + QString::number(entry.length, 'g', 13) + "\r\n").toUtf8());
		}
		if (entry.selected)
			writer->write(QString("QMPlay_sel" + idx + "=" + QString::number(entry.selected) + "\r\n").toUtf8());
		if (entry.queue)
			writer->write(QString("QMPlay_queue" + idx + "=" + QString::number(entry.queue) + "\r\n").toUtf8());
		if (entry.GID)
			writer->write(QString("QMPlay_GID" + idx + "=" + QString::number(entry.GID) + "\r\n").toUtf8());
		if (entry.parent)
			writer->write(QString("QMPlay_parent" + idx + "=" + QString::number(entry.parent) + "\r\n").toUtf8());
	}
	return true;
}

PLS::~PLS()
{}
