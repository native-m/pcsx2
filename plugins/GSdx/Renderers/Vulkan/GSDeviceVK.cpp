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
    : m_vk_instance{nullptr}
    , m_vk_physical_device{nullptr}
    , m_vk_device{nullptr}
    , m_vk_graphics_queue{nullptr}
    , m_surface{0}
    , m_swapchain{VK_NULL_HANDLE}
    , m_queue_fams{}
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
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);

    // Retrieve instance extensions
    std::vector<VkExtensionProperties> exts(ext_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, exts.data());

    std::vector<const char *> ext_names;
    for (const auto &ext : exts) {
        ext_names.push_back(ext.extensionName);
    }

    std::vector<const char *> layer_names;

    // enable layers, if needed
    // usually used for debugging
    if (g_enable_layers) {
        uint32 layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (auto layer : g_vk_layers) {
            for (const auto &available_layer : available_layers) {
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
    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = ext_count;
    instance_info.ppEnabledExtensionNames = ext_names.data();
    instance_info.enabledLayerCount = layer_names.size();
    instance_info.ppEnabledLayerNames = layer_names.data();

    VkResult ret = vkCreateInstance(&instance_info, nullptr, &m_vk_instance);

    if (ret != VK_SUCCESS) {
        return false;
    }

    // Retrieve all devices
    uint32 num_devices;
    vkEnumeratePhysicalDevices(m_vk_instance, &num_devices, nullptr);

    std::vector<VkPhysicalDevice> devices(num_devices);
    vkEnumeratePhysicalDevices(m_vk_instance, &num_devices, devices.data());

    std::string device_id = theApp.GetConfigS("AdapterVulkan");

    // Find and select the device
    // if the "default" option was choosen, then we will automatically find the suitable device
    if (device_id == "default" && m_vk_physical_device == nullptr) {
        m_vk_physical_device = FindSuitableDevice(devices);
        if (m_vk_physical_device == nullptr) {
            return false;
        }
    } else {
        for (const auto &device : devices) {
            VkPhysicalDeviceProperties device_properties;
            vkGetPhysicalDeviceProperties(device, &device_properties);

            if (GSAdapter(device_properties.vendorID, device_properties.deviceID, 0, 0) == device_id) {
                m_vk_physical_device = device;
            }
        }
    }

    CreateSurface(wnd);

    m_queue_fams = QueryQueueFamily(m_vk_physical_device, m_surface);
    ASSERT(m_queue_fams.has_graphics == true && m_queue_fams.has_present == true);

    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    std::set<uint32> unique_queue_families = {
        m_queue_fams.graphics_fam,
        m_queue_fams.present_fam};

    const float queue_priority = 1.0f;
    for (auto queue_fam : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_info = {};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = queue_fam;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_infos.push_back(queue_info);
    }

    uint32 dev_ext_count = 0;
    vkEnumerateDeviceExtensionProperties(m_vk_physical_device, nullptr, &dev_ext_count, nullptr);

    std::vector<VkExtensionProperties> dev_ext_props(dev_ext_count);
    std::vector<const char *> dev_ext_names;
    vkEnumerateDeviceExtensionProperties(m_vk_physical_device, nullptr, &dev_ext_count, dev_ext_props.data());

    for (const auto &ext_prop : dev_ext_props) {
        dev_ext_names.push_back(ext_prop.extensionName);
    }

    VkDeviceCreateInfo device_info = {};
    VkPhysicalDeviceFeatures device_features = {};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.pQueueCreateInfos = queue_infos.data();
    device_info.queueCreateInfoCount = queue_infos.size();
    device_info.enabledLayerCount = layer_names.size();
    device_info.ppEnabledLayerNames = layer_names.data();
    device_info.enabledExtensionCount = dev_ext_count;
    device_info.ppEnabledExtensionNames = dev_ext_names.data();
    device_info.pEnabledFeatures = &device_features;

    if (vkCreateDevice(m_vk_physical_device, &device_info, nullptr, &m_vk_device) != VK_SUCCESS) {
        return false;
    }

    vkGetDeviceQueue(m_vk_device, m_queue_fams.graphics_fam, 0, &m_vk_graphics_queue);
    vkGetDeviceQueue(m_vk_device, m_queue_fams.present_fam, 0, &m_vk_present_queue);

	CreateSwapchain(1, 1);

    // Phew..
    Reset(1, 1);

    return true;
}

bool GSDeviceVK::Reset(int w, int h)
{
    if (!GSDevice::Reset(w, h))
        return false;

    if (m_swapchain) {
        DestroySwapchain();
        CreateSwapchain(w, h);

		m_backbuffer = new GSTextureVK(GSTextureVK::Backbuffer, w, h, m_swapchain_fmt);
    }

    return true;
}

// stolen from:
// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
VkPhysicalDevice GSDeviceVK::FindSuitableDevice(const std::vector<VkPhysicalDevice> &devices)
{
    std::multimap<int64_t, VkPhysicalDevice> device_candidates;

    for (const auto &device : devices) {
        VkPhysicalDeviceProperties device_properties = {};
        VkPhysicalDeviceMemoryProperties device_memory = {};
        int64_t score = 0;

        vkGetPhysicalDeviceProperties(device, &device_properties);
        vkGetPhysicalDeviceMemoryProperties(device, &device_memory);

        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += 1000;

        score += static_cast<int64_t>(device_properties.limits.maxImageDimension2D);

        // Get local VRAM size
        for (const auto &heap : device_memory.memoryHeaps) {
            if ((heap.flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 1) {
                score += heap.size / 1000000; // in megabyte
            }
        }

        device_candidates.insert(std::make_pair(score, device));
    }

    if (device_candidates.rbegin()->first > 0) {
        return device_candidates.rbegin()->second;
    }

    return nullptr;
}

GSDeviceVK::QueueFamilies GSDeviceVK::QueryQueueFamily(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    uint32_t queue_family_count = 0;
    std::vector<VkQueueFamilyProperties> queue_families;
    QueueFamilies fams = {};

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    queue_families.resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    uint32_t i = 0;
    for (const auto &queue_family : queue_families) {
        VkBool32 present_support = VK_FALSE;

        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

        if (present_support) {
            fams.present_fam = i;
            fams.has_present = true;
        }

        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            fams.graphics_fam = i;
            fams.has_graphics = true;
        }

        i++;
    }

    return fams;
}

void GSDeviceVK::CreateSurface(const std::shared_ptr<GSWnd> &wnd)
{
    VkWin32SurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_info.hwnd = static_cast<HWND>(wnd->GetHandle());
    surface_info.hinstance = theApp.GetModuleHandle();

    if (vkCreateWin32SurfaceKHR(m_vk_instance, &surface_info, nullptr, &m_surface)) {
        throw GSDXRecoverableError();
    }
}

void GSDeviceVK::CreateSwapchain(int w, int h)
{
    const uint32 queue_fam_indicies[] = {m_queue_fams.graphics_fam, m_queue_fams.present_fam};
    VkSwapchainCreateInfoKHR swapchain_info = {};
    uint32 swapchain_image_count;

    // Setup swapchain create info
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = m_surface;
    swapchain_info.minImageCount = 2;
    swapchain_info.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_info.imageExtent.width = 1;
    swapchain_info.imageExtent.height = 1;
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (m_queue_fams.graphics_fam != m_queue_fams.present_fam) {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_info.queueFamilyIndexCount = 2;
        swapchain_info.pQueueFamilyIndices = queue_fam_indicies;
    } else {
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_info.queueFamilyIndexCount = 0;
        swapchain_info.pQueueFamilyIndices = nullptr;
    }

    swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;

    // Create
    if (vkCreateSwapchainKHR(m_vk_device, &swapchain_info, nullptr, &m_swapchain) != VK_SUCCESS) {
        throw GSDXRecoverableError();
    }

    // Query swapchain images
    vkGetSwapchainImagesKHR(m_vk_device, m_swapchain, &swapchain_image_count, nullptr);
    m_swapchain_images.resize(swapchain_image_count);
    vkGetSwapchainImagesKHR(m_vk_device, m_swapchain, &swapchain_image_count, m_swapchain_images.data());

    m_swapchain_fmt = swapchain_info.imageFormat;
    m_swapchain_extent = swapchain_info.imageExtent;

    // Setup swapchain image view
    m_swapchain_image_views.resize(swapchain_image_count);

    for (uint32 i = 0; i < swapchain_image_count; i++) {
        VkImageViewCreateInfo image_view_info = {};
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.image = m_swapchain_images[i];
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = m_swapchain_fmt;
        image_view_info.components = {VK_COMPONENT_SWIZZLE_IDENTITY};
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_vk_device, &image_view_info, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS) {
            throw GSDXRecoverableError();
        }
    }
}

void GSDeviceVK::DestroySwapchain()
{
    for (auto image_view : m_swapchain_image_views) {
        vkDestroyImageView(m_vk_device, image_view, nullptr);
    }

    m_swapchain_image_views.clear();

    if (m_swapchain) {
        vkDestroySwapchainKHR(m_vk_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

GSTexture *GSDeviceVK::CreateSurface(int type, int w, int h, int format)
{
    return new GSTextureVK(type, w, h, format);
}

void GSDeviceVK::Destroy()
{
    DestroySwapchain();
    vkDestroySurfaceKHR(m_vk_instance, m_surface, nullptr);
    vkDestroyDevice(m_vk_device, nullptr);
    vkDestroyInstance(m_vk_instance, nullptr);
}

const char *GSDeviceVK::g_vk_layers[1] = {
    //    "VK_LAYER_RENDERDOC_Capture",
    "VK_LAYER_KHRONOS_validation"};
