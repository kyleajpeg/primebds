# Plugin/host RTTI is not the Endstone player conversion contract. Guard all
# command and event paths, including future additions, against this regression.
file(GLOB_RECURSE sources "${SOURCE_ROOT}/src/*.cpp" "${SOURCE_ROOT}/include/*.h")
foreach(source IN LISTS sources)
    file(READ "${source}" content)
    if(content MATCHES "dynamic_cast[ \t\r\n]*<[ \t\r\n]*(const[ \t]+)?endstone::Player")
        message(FATAL_ERROR "Use Endstone asPlayer(), not cross-module player RTTI: ${source}")
    endif()
endforeach()
message(STATUS "No cross-module player RTTI casts remain")
