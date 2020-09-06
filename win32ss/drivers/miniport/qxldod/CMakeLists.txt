include_directories(BEFORE include)

list(APPEND SOURCE
    BaseObject.cpp
    compat.cpp
    driver.cpp
    mspace.c
    QxlDod.cpp)

add_library(qxldod MODULE
    ${SOURCE}
    qxldod.rc)

set_module_type(qxldod kernelmodedriver)
add_importlibs(qxldod videoprt)
add_cd_file(TARGET qxldod DESTINATION reactos/system32/drivers FOR all)
add_driver_inf(QxlDod acpi.inf)

if(NOT MSVC)
    add_compile_flags("-Wno-unused-function")
endif()