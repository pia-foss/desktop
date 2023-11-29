require_relative 'systemutil'


class LoadedLibChecker
    PIA_INCLUDED_LINUX_LIBRARIES = [
        "kapps_core.so",
        "kapps_net.so",
        "kapps_regions.so",
        "libcrypto.so",
        "libicui18n.so",
        "libQt5Core.so",
        "libQt5DBus.so",
        "libQt5Gui.so",
        "libQt5Network.so",
        "libQt5Qml.so",
        "libQt5QmlModels.so",
        "libQt5QmlWorkerScript.so",
        "libQt5Quick.so",
        "libQt5QuickControls2.so",
        "libQt5QuickShapes.so",
        "libQt5QuickTemplates2.so",
        "libQt5WaylandClient.so",
        "libQt5Widgets.so",
        "libQt5XcbQpa.so",
        "libssl.so",
        "libxcb-composite.so",
        "libxcb-damage.so",
        "libxcb-dpms.so",
        "libxcb-dri2.so",
        "libxcb-dri3.so",
        "libxcb-ewmh.so",
        "libxcb-glx.so",
        "libxcb-icccm.so",
        "libxcb-image.so",
        "libxcb-keysyms.so",
        "libxcb-present.so",
        "libxcb-randr.so",
        "libxcb-record.so",
        "libxcb-render-util.so",
        "libxcb-render.so",
        "libxcb-res.so",
        "libxcb-screensaver.so",
        "libxcb-shape.so",
        "libxcb-shm.so",
        "libxcb-sync.so",
        "libxcb-util.so",
        "libxcb-xf86dri.so",
        "libxcb-xfixes.so",
        "libxcb-xinerama.so",
        "libxcb-xinput.so",
        "libxcb-xkb.so",
        "libxcb-xtest.so",
        "libxcb-xv.so",
        "libxcb-xvmc.so",
        "libxcb.so",
        "pia-clientlib.so",
        "pia-commonlib.so"
    ]
    
    def self.find_incorrect_libs(process_name)
        if !SystemUtil.linux?
            raise "#{SystemUtil.os} not supported"
        end
        incorrectly_loaded_libs = []
        loaded_libs = SystemUtil.loaded_libs process_name
        loaded_libs.each do |line|
            # Find which pia lib is represented in the line (if any)
            pia_lib = PIA_INCLUDED_LINUX_LIBRARIES.select { |lib| line.include? lib }
            # Find if the lib is loaded from pia's install dir
            loads_pia_lib = line.start_with? "/opt/piavpn"
            
            # If we loaded a pia_lib, but not from the install dir, it was incorrectly loaded
            if not pia_lib.empty? and not loads_pia_lib
                incorrectly_loaded_libs.append pia_lib[0]
            end
        end
        incorrectly_loaded_libs
    end

end