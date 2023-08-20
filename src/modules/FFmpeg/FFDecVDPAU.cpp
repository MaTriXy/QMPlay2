/*
    QMPlay2 is a video and audio player.
    Copyright (C) 2010-2023  Błażej Szczygieł

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

#include <FFDecVDPAU.hpp>
#include <StreamInfo.hpp>
#include <Functions.hpp>
#include <FFCommon.hpp>
#include <VDPAU.hpp>
#include <VDPAUOpenGL.hpp>
#include <GPUInstance.hpp>

extern "C"
{
    #include <libavformat/avformat.h>
    #include <libavutil/pixdesc.h>
    #include <libavutil/hwcontext.h>
    #include <libavutil/hwcontext_vdpau.h>
}

#include <QDebug>

using namespace std;

static inline void YUVjToYUV(AVPixelFormat &pixFmt)
{
    // FFmpeg VDPAU implementation doesn't support YUVJ
    if (pixFmt == AV_PIX_FMT_YUVJ420P)
        pixFmt = AV_PIX_FMT_YUV420P;
}

static AVPixelFormat vdpauGetFormat(AVCodecContext *codecCtx, const AVPixelFormat *pixFmt)
{
    while (*pixFmt != AV_PIX_FMT_NONE)
    {
        if (*pixFmt == AV_PIX_FMT_VDPAU)
        {
            YUVjToYUV(codecCtx->sw_pix_fmt);
            return *pixFmt;
        }
        ++pixFmt;
    }
    return AV_PIX_FMT_NONE;
}

/**/

FFDecVDPAU::FFDecVDPAU(Module &module)
{
    SetModule(module);
}
FFDecVDPAU::~FFDecVDPAU()
{
    if (m_vdpau.use_count() == 1)
        destroyDecoder();
}

bool FFDecVDPAU::set()
{
    bool ret = true;

    m_deintMethod = sets().getInt("VDPAUDeintMethod");
    m_nrEnabled = sets().getBool("VDPAUNoiseReductionEnabled");
    m_nrLevel = sets().getDouble("VDPAUNoiseReductionLvl");

    if (m_vdpau)
        m_vdpau->setVideoMixerDeintNr(m_deintMethod, m_nrEnabled, m_nrLevel);

    return (sets().getBool("DecoderVDPAUEnabled") && ret);
}

QString FFDecVDPAU::name() const
{
    return "FFmpeg/" VDPAUWriterName;
}

shared_ptr<VideoFilter> FFDecVDPAU::hwAccelFilter() const
{
    return m_vdpau;
}

int FFDecVDPAU::decodeVideo(const Packet &encodedPacket, Frame &decoded, AVPixelFormat &newPixFmt, bool flush, unsigned hurryUp)
{
    if (m_vdpau->hasError())
    {
        m_hasCriticalError = true;
        return -1;
    }

    int ret = FFDecHWAccel::decodeVideo(encodedPacket, decoded, newPixFmt, flush, hurryUp);
    if (m_hasHWDecContext && ret > -1)
    {
        if (!decoded.isEmpty())
            m_vdpau->maybeCreateVideoMixer(codec_ctx->coded_width, codec_ctx->coded_height, decoded);
    }

    return ret;
}

bool FFDecVDPAU::open(StreamInfo &streamInfo)
{
    if (streamInfo.params->codec_type != AVMEDIA_TYPE_VIDEO || !hasHWAccel("vdpau"))
        return false;

    if (Functions::isX11EGL() && QMPlay2Core.renderer() == QMPlay2CoreClass::Renderer::OpenGL)
        return false;

    const AVPixelFormat pix_fmt = streamInfo.pixelFormat();
    if (pix_fmt != AV_PIX_FMT_YUV420P && pix_fmt != AV_PIX_FMT_YUVJ420P)
        return false;

    AVCodec *codec = init(streamInfo);
    if (!codec)
        return false;

    auto vdpauOpenGL = QMPlay2Core.gpuInstance()->getHWDecContext<VDPAUOpenGL>();
    if (vdpauOpenGL)
    {
        m_vdpau = vdpauOpenGL->getVDPAU();
        Q_ASSERT(m_vdpau);
    }

    AVBufferRef *hwDeviceBufferRef = nullptr;
    if (!m_vdpau)
    {
#ifdef FIND_HWACCEL_DRIVERS_PATH
        FFCommon::setDriversPath("vdpau", "VDPAU_DRIVER_PATH");
#endif

        if (av_hwdevice_ctx_create(&hwDeviceBufferRef, AV_HWDEVICE_TYPE_VDPAU, nullptr, nullptr, 0) != 0)
            return false;

        m_vdpau = make_shared<VDPAU>(hwDeviceBufferRef);
        if (!m_vdpau->init())
            return false;

        m_vdpau->registerPreemptionCallback(preemptionCallback, m_vdpau.get());
    }
    else
    {
        m_vdpau->clearBuffer();
        hwDeviceBufferRef = av_buffer_ref(m_vdpau->m_hwDeviceBufferRef);
    }

    if (!m_vdpau->checkCodec(streamInfo.codec_name.constData()))
        return false;

    m_vdpau->setVideoMixerDeintNr(m_deintMethod, m_nrEnabled, m_nrLevel);

    YUVjToYUV(codec_ctx->pix_fmt);
    codec_ctx->hw_device_ctx = hwDeviceBufferRef;
    codec_ctx->get_format = vdpauGetFormat;
    codec_ctx->thread_count = 1;
    codec_ctx->extra_hw_frames = 4;
    if (!openCodec(codec))
        return false;

    vdpauOpenGL = make_shared<VDPAUOpenGL>(m_vdpau);
    if (!QMPlay2Core.gpuInstance()->setHWDecContextForVideoOutput(vdpauOpenGL))
        return false;

    m_hasHWDecContext = true;

    m_timeBase = streamInfo.time_base;
    return true;
}

void FFDecVDPAU::preemptionCallback(uint32_t device, void *context)
{
    Q_UNUSED(device)
    Q_UNUSED(context)
    // IMPLEMENT ME: When VDPAU is preempted (VT switch) everything must be recreated,
    // but it is only possible after switching VT to X11.
    QMPlay2Core.logError("VDPAU :: Preemption");
}
