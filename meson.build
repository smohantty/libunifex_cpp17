project('cpp20_app', 'cpp',
  version : '0.1.0',
  default_options : [
    'warning_level=3',
    'cpp_std=c++20',
    'buildtype=release'
  ]
)

# Compiler configuration
cpp = meson.get_compiler('cpp')

# Check for required compiler features
if not cpp.has_header('filesystem')
  error('C++17 filesystem header is required')
endif

# Add libunifex as a subproject
libunifex_subproject = subproject('libunifex')
libunifex_dep = libunifex_subproject.get_variable('unifex_dep')

# Include directories
inc_dir = include_directories('include')

# Subdirectories
subdir('src')