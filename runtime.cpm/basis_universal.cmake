add_cpm_module(basis_universal)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/basis_universal_cpm_runtime_build)

add_library(basis_universal_cpm STATIC IMPORTED)
target_include_directories(basis_universal_cpm INTERFACE ${basis_universal_ROOT}/include)
set_target_properties(basis_universal_cpm PROPERTIES IMPORTED_LOCATION ${basis_universal_ROOT}/lib/basis.lib)
