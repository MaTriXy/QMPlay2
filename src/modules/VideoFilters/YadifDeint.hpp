/*
    QMPlay2 is a video and audio player.
    Copyright (C) 2010-2025  Błażej Szczygieł

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

#pragma once

#include <VideoFilter.hpp>

#include <QThreadPool>

class YadifDeint final : public VideoFilter
{
public:
    YadifDeint(bool m_doubler, bool m_spatialCheck);

    bool filter(QQueue<Frame> &framesQueue) override;

    bool processParams(bool *paramsCorrected) override;
private:
    const bool m_doubler;
    const bool m_spatialCheck;
    QThreadPool m_threadsPool;
};

#define YadifDeintName "Yadif"
#define YadifNoSpatialDeintName YadifDeintName " (no spatial check)"
#define Yadif2xDeintName YadifDeintName " 2x"
#define Yadif2xNoSpatialDeintName Yadif2xDeintName " (no spatial check)"

#define YadifDescr "Yet Another DeInterlacong Filter"
