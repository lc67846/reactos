include_directories(BEFORE virtio)

list(APPEND SOURCE
    helper.c
    helper.h
    resources.h
    trace.h
    utils.c
    vioscsi.c
    vioscsi.h
    vioscsidt.h
    virtio_pci.c)

add_library(vioscsi MODULE
    ${SOURCE}
    vioscsi.rc)

set_module_type(vioscsi kernelmodedriver)
add_importlibs(vioscsi ntoskrnl hal)
add_cd_file(TARGET vioscsi DESTINATION reactos/system32/drivers NO_CAB FOR all)
