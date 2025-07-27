# Source files
sources = [
  'main.cpp'
]

# Create main executable
executable('cpp17_app',
  sources,
  dependencies : [libunifex_dep],
  include_directories : [inc_dir],
  install : true,
  cpp_args : ['-std=c++17']
)

# Create mixed types demonstration executable
executable('mixed_types_demo',
  'mixed_types_demo.cpp',
  dependencies : [libunifex_dep],
  include_directories : [inc_dir],
  install : true,
  cpp_args : ['-std=c++17']
)

# Create task dependency graph (DAG) demonstration executable
executable('task_dag_demo',
  'task_dag_demo.cpp',
  dependencies : [libunifex_dep],
  include_directories : [inc_dir],
  install : true,
  cpp_args : ['-std=c++17']
)