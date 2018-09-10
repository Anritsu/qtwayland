TEMPLATE = subdirs
QT_FOR_CONFIG += waylandcompositor-private

qtConfig(wayland-egl): \
    SUBDIRS += wayland-egl
qtConfig(wayland-brcm): \
    SUBDIRS += brcm-egl
qtConfig(xcomposite-egl): \
    SUBDIRS += xcomposite-egl
qtConfig(xcomposite-glx): \
    SUBDIRS += xcomposite-glx
qtConfig(wayland-drm-egl-server-buffer): \
    SUBDIRS += drm-egl-server
qtConfig(wayland-libhybris-egl-server-buffer): \
    SUBDIRS += libhybris-egl-server

### TODO: make shm-emulation configurable
SUBDIRS += shm-emulation-server

SUBDIRS += hardwarelayer
