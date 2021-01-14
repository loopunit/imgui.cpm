add_cpm_module(python FOR_TOOLCHAIN)

set(Python3_ROOT_DIR ${python_ROOT})
find_package(Python3 COMPONENTS Interpreter Development)
