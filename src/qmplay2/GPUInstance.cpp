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

#include <GPUInstance.hpp>
#include <Settings.hpp>

#ifdef USE_OPENGL
#   include <opengl/OpenGLInstance.hpp>
#   include <QOffscreenSurface>
#   include <QOpenGLFunctions>
#endif
#ifdef USE_VULKAN
#   include <vulkan/VulkanInstance.hpp>
#endif
#include <VideoWriter.hpp>

#include <QDebug>

using namespace std;

shared_ptr<GPUInstance> GPUInstance::create()
{
    auto &sets = QMPlay2Core.getSettings();
    auto renderer = sets.getString("Renderer");

#if defined(USE_VULKAN)
    if (renderer == "vulkan") try
    {
        auto vkInstance = QmVk::Instance::create(true);
#   ifdef USE_OPENGL
        if (!vkInstance->isPhysicalDeviceGpu() && !sets.getBool("Vulkan/UserApplied"))
        {
            QOffscreenSurface surface;
            QOpenGLContext glCtx;

            surface.create();
            if (!glCtx.create() || !glCtx.makeCurrent(&surface))
                return vkInstance;

            const auto currentRenderer = QByteArray(reinterpret_cast<const char *>(glCtx.functions()->glGetString(GL_RENDERER))).toLower();
            for (auto &&swRendererName : {"llvmpipe", "software", "swrast", "softpipe"})
            {
                if (currentRenderer.contains(swRendererName))
                    return vkInstance;
            }

            qInfo() << "Using OpenGL instead of Vulkan software rasterizer";
            vkInstance.reset();
            renderer = "opengl";
        }
#   endif
        if (vkInstance)
            return vkInstance;
    }
    catch (const vk::SystemError &e)
    {
        qWarning() << "Vulkan is unable to work with QMPlay2 on this platform:" << e.what();
#   ifdef USE_OPENGL
        // Fallback to OpenGL
        renderer = "opengl";
#   endif
    }
#elif defined (USE_OPENGL)
    if (renderer == "vulkan")
    {
        // Use OpenGL if Vulkan is not available
        renderer = "opengl";
    }
#endif

#ifdef USE_OPENGL
    if (renderer == "opengl")
    {
        auto glInstance = make_shared<OpenGLInstance>();
        if (!glInstance->init())
        {
            qWarning() << "OpenGL is unable to work with QMPlay2 on this platform";
            return nullptr;
        }
        return glInstance;
    }
#endif

    return nullptr;
}

void GPUInstance::prepareDestroy()
{
}

bool GPUInstance::checkFiltersSupported() const
{
    return false;
}

shared_ptr<HWDecContext> GPUInstance::getHWDecContext() const
{
    if (m_videoWriter)
        return m_videoWriter->hwDecContext();
    return nullptr;
}

bool GPUInstance::setHWDecContextForVideoOutput(const shared_ptr<HWDecContext> &hwDecContext)
{
    return createOrGetVideoOutput()->setHWDecContext(hwDecContext);
}

void GPUInstance::clearVideoOutput()
{
    m_videoWriter = nullptr;
}
void GPUInstance::resetVideoOutput()
{
    delete m_videoWriter;
    clearVideoOutput();
}
