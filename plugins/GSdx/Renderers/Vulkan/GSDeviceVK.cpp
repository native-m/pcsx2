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

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "GSdxVulkan";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "GSdxVulkanEngine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	// Retrieve the number of instance extensions
	uint32 ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);

	// Retrieve instance extensions
    std::vector<VkExtensionProperties> exts(ext_count);
    vkEnumerateInstanceExtensionProperties(NULL, &ext_count, exts.data());

	std::vector<const char *> ext_names;
	for (const VkExtensionProperties &ext : exts) {
        ext_names.push_back(ext.extensionName);
	}

	std::vector<const char *> layer_names;
	VkInstanceCreateInfo instance_info = { };

	// enable layers, if needed
	// usually used for debugging
	if (g_enable_layers) {
		uint32 layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);

		std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

		for (auto layer : g_vk_layers) {
			for (auto available_layer : available_layers) {
                if (std::strncmp(available_layer.layerName,
								 layer,
								 std::strlen(layer)) == 0) {
                    layer_names.push_back(layer);
					break;
				}
			}
		}
	}

	// Create vulkan instance
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pApplicationInfo = &app_info;
	instance_info.enabledExtensionCount = ext_count;
	instance_info.ppEnabledExtensionNames = ext_names.data();
    instance_info.enabledLayerCount = layer_names.size();
	instance_info.ppEnabledLayerNames = layer_names.data();

	VkResult ret = vkCreateInstance(&instance_info, NULL, &m_vk_instance);

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
