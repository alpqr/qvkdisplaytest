#include <QGuiApplication>
#include <QVulkanInstance>
#include <QtGui/private/qrhi_p.h>
#include <QtGui/private/qrhivulkan_p.h>

PFN_vkGetPhysicalDeviceDisplayPropertiesKHR pfnGetPhysicalDeviceDisplayPropertiesKHR;
PFN_vkGetDisplayModePropertiesKHR pfnGetDisplayModePropertiesKHR;
PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR pfnGetPhysicalDeviceDisplayPlanePropertiesKHR;

PFN_vkGetDisplayPlaneSupportedDisplaysKHR pfnGetDisplayPlaneSupportedDisplaysKHR;
PFN_vkGetDisplayPlaneCapabilitiesKHR pfnGetDisplayPlaneCapabilitiesKHR;

PFN_vkCreateDisplayPlaneSurfaceKHR pfnCreateDisplayPlaneSurfaceKHR;

PFN_vkDestroySurfaceKHR pfnDestroySurfaceKHR;

void resolveDisplayFuncs(QVulkanInstance *inst)
{
    pfnGetPhysicalDeviceDisplayPropertiesKHR = (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)
        inst->getInstanceProcAddr("vkGetPhysicalDeviceDisplayPropertiesKHR");
    pfnGetDisplayModePropertiesKHR = (PFN_vkGetDisplayModePropertiesKHR)
        inst->getInstanceProcAddr("vkGetDisplayModePropertiesKHR");
    pfnGetPhysicalDeviceDisplayPlanePropertiesKHR = (PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR)
        inst->getInstanceProcAddr("vkGetPhysicalDeviceDisplayPlanePropertiesKHR");

    pfnGetDisplayPlaneSupportedDisplaysKHR = (PFN_vkGetDisplayPlaneSupportedDisplaysKHR)
        inst->getInstanceProcAddr("vkGetDisplayPlaneSupportedDisplaysKHR");
    pfnGetDisplayPlaneCapabilitiesKHR = (PFN_vkGetDisplayPlaneCapabilitiesKHR)
        inst->getInstanceProcAddr("vkGetDisplayPlaneCapabilitiesKHR");

    pfnCreateDisplayPlaneSurfaceKHR = (PFN_vkCreateDisplayPlaneSurfaceKHR)
        inst->getInstanceProcAddr("vkCreateDisplayPlaneSurfaceKHR");

    pfnDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)
        inst->getInstanceProcAddr("vkDestroySurfaceKHR");
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QVulkanInstance inst;
    qDebug() << "Available instance extensions:" << inst.supportedExtensions();
    if (!inst.supportedExtensions().contains(QByteArrayLiteral("VK_KHR_display")))
        qFatal("VK_KHR_display is not supported");

    // enable validation, if available
    inst.setLayers({ QByteArrayLiteral("VK_LAYER_LUNARG_standard_validation") });

    // for now assume the platform plugin will do this for us
    // inst.setExtensions({ QByteArrayLiteral("VK_KHR_display") });

    if (!inst.create())
        qFatal("Failed to create Vulkan instance: %d", inst.errorCode());

    qDebug() << "Enabled instance extensions:" << inst.extensions();

    QRhiVulkanInitParams rhiInitParams;
    rhiInitParams.inst = &inst;
    QScopedPointer<QRhi> rhi(QRhi::create(QRhi::Vulkan, &rhiInitParams));
    if (!rhi)
        qFatal("Failed to create a Vulkan-based QRhi");

    resolveDisplayFuncs(&inst);

    const QRhiVulkanNativeHandles *rhiNativeHandles = static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles());
    VkPhysicalDevice physDev = rhiNativeHandles->physDev;

    uint32_t displayCount = 0;
    VkResult err = pfnGetPhysicalDeviceDisplayPropertiesKHR(physDev, &displayCount, nullptr);
    if (err != VK_SUCCESS)
        qFatal("Failed to get display properties: %d", err);

    qDebug("Display count: %u", displayCount);

    QVarLengthArray<VkDisplayPropertiesKHR, 4> displayProps;
    displayProps.resize(displayCount);
    pfnGetPhysicalDeviceDisplayPropertiesKHR(physDev, &displayCount, displayProps.data());

    VkDisplayKHR display = VK_NULL_HANDLE;
    VkDisplayModeKHR displayMode = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;

    for (uint32_t i = 0; i < displayCount; ++i) {
        const VkDisplayPropertiesKHR &disp(displayProps[i]);
        qDebug("Display #%u:\n  display: %p\n  name: %s\n  dimensions: %ux%u\n  resolution: %ux%u",
               i, (void *) disp.display, disp.displayName,
               disp.physicalDimensions.width, disp.physicalDimensions.height,
               disp.physicalResolution.width, disp.physicalResolution.height);

        // Just pick the first display and the first mode.
        if (i == 0)
            display = disp.display;

        uint32_t modeCount = 0;
        if (pfnGetDisplayModePropertiesKHR(physDev, disp.display, &modeCount, nullptr) != VK_SUCCESS) {
            qWarning("Failed to get modes for display");
            continue;
        }
        QVarLengthArray<VkDisplayModePropertiesKHR, 16> modeProps;
        modeProps.resize(modeCount);
        pfnGetDisplayModePropertiesKHR(physDev, disp.display, &modeCount, modeProps.data());
        for (uint32_t j = 0; j < modeCount; ++j) {
            const VkDisplayModePropertiesKHR &mode(modeProps[j]);
            qDebug("  Mode #%u:\n    mode: %p\n    visibleRegion: %ux%u\n    refreshRate: %u",
                   j, (void *) mode.displayMode,
                   mode.parameters.visibleRegion.width, mode.parameters.visibleRegion.height,
                   mode.parameters.refreshRate);
            if (j == 0) {
                displayMode = mode.displayMode;
                width = mode.parameters.visibleRegion.width;
                height = mode.parameters.visibleRegion.height;
            }
        }
    }

    if (display == VK_NULL_HANDLE || displayMode == VK_NULL_HANDLE)
        qFatal("Failed to choose display and mode");

    uint32_t planeCount = 0;
    err = pfnGetPhysicalDeviceDisplayPlanePropertiesKHR(physDev, &planeCount, nullptr);
    if (err != VK_SUCCESS)
        qFatal("Failed to get plane properties: %d", err);

    qDebug("Plane count: %u", planeCount);

    QVarLengthArray<VkDisplayPlanePropertiesKHR, 4> planeProps;
    planeProps.resize(planeCount);
    pfnGetPhysicalDeviceDisplayPlanePropertiesKHR(physDev, &planeCount, planeProps.data());

    uint32_t planeIndex = UINT_MAX;
    for (uint32_t i = 0; i < planeCount; ++i) {
        uint32_t supportedDisplayCount = 0;
        err = pfnGetDisplayPlaneSupportedDisplaysKHR(physDev, i, &supportedDisplayCount, nullptr);
        if (err != VK_SUCCESS)
            qFatal("Failed to query supported displays for plane: %d", err);

        QVarLengthArray<VkDisplayKHR, 4> supportedDisplays;
        supportedDisplays.resize(supportedDisplayCount);
        pfnGetDisplayPlaneSupportedDisplaysKHR(physDev, i, &supportedDisplayCount, supportedDisplays.data());
        qDebug("Plane #%u supports %u displays, currently bound to display %p",
               i, supportedDisplayCount, (void *) planeProps[i].currentDisplay);

        VkDisplayPlaneCapabilitiesKHR caps;
        err = pfnGetDisplayPlaneCapabilitiesKHR(physDev, displayMode, i, &caps);
        if (err != VK_SUCCESS)
            qFatal("Failed to query plane capabilities: %d", err);

        qDebug("  supportedAlpha: %d (1=no, 2=global, 4=per pixel, 8=per pixel premul)\n"
               "  minSrc=%d, %d %ux%u\n"
               "  maxSrc=%d, %d %ux%u\n"
               "  minDst=%d, %d %ux%u\n"
               "  maxDst=%d, %d %ux%u\n",
               int(caps.supportedAlpha),
               caps.minSrcPosition.x, caps.minSrcPosition.y, caps.minSrcExtent.width, caps.minSrcExtent.height,
               caps.maxSrcPosition.x, caps.maxSrcPosition.y, caps.maxSrcExtent.width, caps.maxSrcExtent.height,
               caps.minDstPosition.x, caps.minDstPosition.y, caps.minDstExtent.width, caps.minDstExtent.height,
               caps.maxDstPosition.x, caps.maxDstPosition.y, caps.maxDstExtent.width, caps.maxDstExtent.height);

        // if the plane is not in use and supports our chosen display, use that plane
        if (supportedDisplays.contains(display)
            && (planeProps[i].currentDisplay == VK_NULL_HANDLE || planeProps[i].currentDisplay == display))
        {
            planeIndex = i;
        }
    }

    if (planeIndex == UINT_MAX)
        qFatal("Failed to find a suitable plane");

    qDebug("Using plane #%u", planeIndex);

    VkDisplaySurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.displayMode = displayMode;
    surfaceCreateInfo.planeIndex = planeIndex;
    surfaceCreateInfo.planeStackIndex = planeProps[planeIndex].currentStackIndex;
    surfaceCreateInfo.transform = VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR;
    surfaceCreateInfo.globalAlpha = 1.0f;
    surfaceCreateInfo.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    surfaceCreateInfo.imageExtent = { width, height };

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    err = pfnCreateDisplayPlaneSurfaceKHR(inst.vkInstance(), &surfaceCreateInfo, nullptr, &surface);
    if (err != VK_SUCCESS || surface == VK_NULL_HANDLE)
        qFatal("Failed to create surface: %d", err);

    qDebug("Created surface %p", (void *) surface);

    pfnDestroySurfaceKHR(inst.vkInstance(), surface, nullptr);
    return 0;
}
