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

#ifndef FORMATCONTEXT_HPP
#define FORMATCONTEXT_HPP

#include <ChapterInfo.hpp>
#include <StreamInfo.hpp>
#include <TimeStamp.hpp>

#include <QCoreApplication>
#include <QVector>
#include <QMutex>

extern "C"
{
	#include <libavformat/version.h>
}

#if LIBAVFORMAT_VERSION_INT >= 0x382400 // >= 56.36.00
	#define MP3_FAST_SEEK
#endif

struct AVFormatContext;
struct AVDictionary;
struct AVStream;
struct AVPacket;
struct Packet;

class FormatContext
{
	Q_DECLARE_TR_FUNCTIONS(FormatContext)
public:
	FormatContext(QMutex &avcodec_mutex);
	~FormatContext();

	bool metadataChanged() const;

	QList<ChapterInfo> getChapters() const;

	QString name() const;
	QString title() const;
	QList<QMPlay2Tag> tags() const;
	bool getReplayGain(bool album, float &gain_db, float &peak) const;
	double length() const;
	int bitrate() const;
	QByteArray image(bool forceCopy) const;

	bool seek(int pos, bool backward);
	bool read(Packet &encoded, int &idx);
	void pause();
	void abort();

	bool open(const QString &_url);

	void setStreamOffset(double offset);

	bool isLocal, isStreamed, isError;
	StreamsInfo streamsInfo;
	double currPos;
private:
	StreamInfo *getStreamInfo(AVStream *stream) const;
	AVDictionary *getMetadata() const;

	QVector<int> index_map;
	QVector<AVStream *> streams;
	QVector<TimeStamp> streamsTS;
	QVector<double> streamsOffset;
	AVFormatContext *formatCtx;
	AVPacket *packet;

	bool isPaused, isAborted, fixMkvAss;
	mutable bool isMetadataChanged;
	double lastTime, startTime;
#ifndef MP3_FAST_SEEK
	qint64 seekByByteOffset;
#endif
	bool isOneStreamOgg;
	bool forceCopy;

	int lastErr, errFromSeek;
	bool maybeHasFrame;

#if LIBAVFORMAT_VERSION_MAJOR <= 55
	AVDictionary *metadata;
#endif

	QMutex &avcodec_mutex;
};

#endif // FORMATCONTEXT_HPP
