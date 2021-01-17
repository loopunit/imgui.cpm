add_cpm_module(basis_universal)

add_library(basis_universal STATIC IMPORTED)
target_include_directories(basis_universal INTERFACE ${basis_universal_ROOT}/include)
set_target_properties(basis_universal PROPERTIES IMPORTED_LOCATION ${basis_universal_ROOT}/lib/basis.lib)

####

add_cpm_module(basis_universal FOR_TOOLCHAIN)