add_cpm_module(basis_universal FOR_TOOLCHAIN)

add_executable(cpm_toolchain::basisu IMPORTED GLOBAL)
set_target_properties(cpm_toolchain::basisu PROPERTIES IMPORTED_LOCATION ${basis_universal_ROOT}/bin/basisu.exe)

add_library(cpm_toolchain::basis_universal STATIC IMPORTED)
target_include_directories(cpm_toolchain::basis_universal INTERFACE ${basis_universal_ROOT}/include)
set_target_properties(cpm_toolchain::basis_universal PROPERTIES IMPORTED_LOCATION ${basis_universal_ROOT}/lib/basis.lib)

####

add_cpm_module(basis_universal)

add_library(cpm_runtime::basis_universal STATIC IMPORTED)
target_include_directories(cpm_runtime::basis_universal INTERFACE ${basis_universal_ROOT}/include)
set_target_properties(cpm_runtime::basis_universal PROPERTIES IMPORTED_LOCATION ${basis_universal_ROOT}/lib/basis.lib)
