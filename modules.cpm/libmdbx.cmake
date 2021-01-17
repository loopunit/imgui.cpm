add_cpm_module(mdbx FOR_TOOLCHAIN)

add_cpm_module(mdbx)
add_library(mdbx STATIC IMPORTED)
set_target_properties(mdbx PROPERTIES IMPORTED_LOCATION ${mdbx_ROOT}/lib/mdbx.lib)
target_include_directories(mdbx INTERFACE ${mdbx_ROOT}/include)
