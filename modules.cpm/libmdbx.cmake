add_cpm_module(mdbx FOR_TOOLCHAIN)

add_library(cpm_toolchain::mdbx STATIC IMPORTED)
set_target_properties(cpm_toolchain::mdbx PROPERTIES IMPORTED_LOCATION ${mdbx_ROOT}/lib/mdbx.lib)
target_include_directories(cpm_toolchain::mdbx INTERFACE ${mdbx_ROOT}/include)

####

add_cpm_module(mdbx)
add_library(mdbx_runtime STATIC IMPORTED)
set_target_properties(mdbx_runtime PROPERTIES IMPORTED_LOCATION ${mdbx_ROOT}/lib/mdbx.lib)
target_include_directories(mdbx_runtime INTERFACE ${mdbx_ROOT}/include)
add_library(cpm_runtime::mdbx ALIAS mdbx_runtime)