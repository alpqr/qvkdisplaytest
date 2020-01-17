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
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QVulkanInstance inst;
    qDebug() << "Available instance extensions:" << inst.supportedExtensions();
    if (!inst.supportedExtensions().contains(QByteArrayLiteral("VK_KHR_display")))
        qFatal("VK_KHR_display is not supported");

    inst.setLayers({ QByteArrayLiteral("VK_LAYER_LUNARG_standard_validation") });
    inst.setExtensions({ QByteArrayLiteral("VK_KHR_display") });

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

    for (uint32_t i = 0; i < displayCount; ++i) {
        const VkDisplayPropertiesKHR &disp(displayProps[i]);
        qDebug("Display #%u:\n  display: %p\n  name: %s\n  dimensions: %ux%u\n  resolution: %ux%u",
               i, (void *) disp.display, disp.displayName,
               disp.physicalDimensions.width, disp.physicalDimensions.height,
               disp.physicalResolution.width, disp.physicalResolution.height);

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
            qDebug("  Mode #%u:\n  mode: %p\n  visibleRegion: %ux%u\n  refreshRate: %u",
                   j, (void *) mode.displayMode,
                   mode.parameters.visibleRegion.width, mode.parameters.visibleRegion.height,
                   mode.parameters.refreshRate);
        }
    }

    uint32_t planeCount = 0;
    err = pfnGetPhysicalDeviceDisplayPlanePropertiesKHR(physDev, &planeCount, nullptr);
    if (err != VK_SUCCESS)
        qFatal("Failed to get plane properties: %d", err);

    qDebug("Plane count: %u", planeCount);

    QVarLengthArray<VkDisplayPlanePropertiesKHR, 4> planeProps;
    planeProps.resize(planeCount);
    pfnGetPhysicalDeviceDisplayPlanePropertiesKHR(physDev, &planeCount, planeProps.data());

    return 0;
}
