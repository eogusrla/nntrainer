layer_sources = [
  'network_graph.cpp',
]

layer_headers = [
  'network_graph.h'
]

foreach s : layer_sources
  nntrainer_sources += meson.current_source_dir() / s
endforeach

foreach h : layer_headers
  nntrainer_headers += meson.current_source_dir() / h
endforeach
