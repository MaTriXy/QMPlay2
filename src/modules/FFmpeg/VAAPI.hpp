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

#include <QCoreApplication>
#include <QVersionNumber>
#include <QVector>
#include <QMutex>
#include <QHash>

#include <memory>

#include <va/va.h>
#include <va/va_vpp.h>

struct AVBufferRef;
struct AVFrame;
class QImage;
class Frame;

class VAAPI
{
    Q_DECLARE_TR_FUNCTIONS(VAAPI)

public:
    VAAPI();
    ~VAAPI();

    bool open();

    void init(int codecId, int width, int height, bool allowFilters);

    bool vaapiCreateSurface(VASurfaceID &surface, int w, int h);

    void maybeInitVPP(int surfaceW, int surfaceH);
    void clearVPP(bool resetAllowFilters = true);

    void applyVideoAdjustment(int brightness, int contrast, int saturation, int hue);

    bool filterVideo(const Frame &frame, VASurfaceID &id, int &field);

    VASurfaceID getVppId();

    void insertFrame(VASurfaceID id, AVFrame *frame);

    bool checkCodec(const char *codecName) const;

private:
    void clearVPPFrames();

public:
    AVBufferRef *m_hwDeviceBufferRef = nullptr;

    QString m_vendor;
    bool m_isMesaRadeon = false;
    QVersionNumber m_driverVersion; // Used for Mesa Radeon workarounds

    bool ok = false;

    int m_fd = -1;
    VADisplay VADisp = nullptr;

    int outW = 0, outH = 0;

    int m_codecId = 0;

    std::unique_ptr<QMutex> m_mutex;

    // Postprocessing
    VAProcDeinterlacingType vpp_deint_type = VAProcDeinterlacingNone;
    bool use_vpp = false;

private:
    int m_version = 0;

    // Postprocessing
    bool m_allowFilters = false;
    VAContextID context_vpp;
    VAConfigID config_vpp;
    VABufferID m_vppDeintBuff;
    VASurfaceID id_vpp;
    QVector<VASurfaceID> m_refs;
    VASurfaceID m_lastVppSurface;
    int m_nBackwardRefs, m_nForwardRefs;
    bool m_hasVppFrame = false;

    QHash<VASurfaceID, Frame> m_vppFrames;
};
