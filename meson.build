project('cpp17_app', 'cpp',
  version : '0.1.0',
  default_options : [
    'warning_level=3',
    'cpp_std=c++17',
    'buildtype=release'
  ]
)

# Compiler configuration
cpp = meson.get_compiler('cpp')

# Check for required compiler features
if not cpp.has_header('filesystem')
  error('C++17 filesystem header is required')
endif

# Import CMake module for subproject support
cmake = import('cmake')

# Configure CMake options for libunifex subproject
opt_var = cmake.subproject_options()
opt_var.add_cmake_defines({
  'BUILD_TESTING': false,
  'UNIFEX_BUILD_EXAMPLES': false,
  'UNIFEX_USE_SYSTEM_GTEST': false
})

# Add libunifex as a CMake subproject
libunifex_subproject = cmake.subproject('libunifex', options: opt_var)
libunifex_dep = libunifex_subproject.dependency('unifex')

# Include directories
inc_dir = include_directories('include')

# Subdirectories
subdir('src')