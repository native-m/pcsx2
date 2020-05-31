/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSDeviceVK.h"
#include "VulkanMemoryAllocator\vk_mem_alloc.h"

GSDeviceVK::GSDeviceVK()
{
}

bool GSDeviceVK::Create(const std::shared_ptr<GSWnd> &wnd)
{
    if (!GSDevice::Create(wnd))
        return false;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "GSdxVulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "GSdxVulkanEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Retrieve the number of instance extensions
    uint32 extCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL);

    // Retrieve instance extensions
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateInstanceExtensionProperties(NULL, &extCount, exts.data());

    std::vector<const char *> extNames;
    for (const VkExtensionProperties &ext : exts) {
        extNames.push_back(ext.extensionName);
    }

	std::vector<const char *> layerNames;
    VkInstanceCreateInfo instanceInfo = { };

    // enable layers, if needed
    // usually used for debugging
    if (g_enable_layers) {
        uint32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, NULL);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (auto layer : g_vk_layers) {
            for (auto availableLayer : availableLayers) {
                if (std::strncmp(availableLayer.layerName,
                                 layer,
                                 std::strlen(layer)) == 0) {
                    layerNames.push_back(layer);
                    break;
                }
            }
        }
    }

    // Create vulkan instance
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = extCount;
    instanceInfo.ppEnabledExtensionNames = extNames.data();
    instanceInfo.enabledLayerCount = layerNames.size();
    instanceInfo.ppEnabledLayerNames = layerNames.data();

    VkResult ret = vkCreateInstance(&instanceInfo, NULL, &m_vk_instance);

    if (ret != VK_SUCCESS) {
        return false;
    }

    Reset(1, 1);

    return true;
}

bool GSDeviceVK::Reset(int w, int h)
{
    if (!GSDevice::Reset(w, h))
        return false;

    return true;
}

void GSDeviceVK::Destroy()
{
    vkDestroyInstance(m_vk_instance, NULL);
}

GSTexture *GSDeviceVK::CreateSurface(int type, int w, int h, int format)
{
    return new GSTextureVK(type, w, h, format);
}

const char *GSDeviceVK::g_vk_layers[2] = {
//    "VK_LAYER_RENDERDOC_Capture",
    "VK_LAYER_KHRONOS_validation"
};
