inc_dirs = include_directories('include')
threads_dep = dependency('threads')

libprom = shared_library(
    'prom',
    [
        'src/prom_collector.c',
        'src/prom_collector_registry.c',
        'src/prom_counter.c',
        'src/prom_gauge.c',
        'src/prom_histogram.c',
        'src/prom_histogram_buckets.c',
        'src/prom_linked_list.c',
        'src/prom_map.c',
        'src/prom_metric.c',
        'src/prom_metric_formatter.c',
        'src/prom_metric_sample.c',
        'src/prom_metric_sample_histogram.c',
        'src/prom_process_fds.c',
        'src/prom_process_limits.c',
        'src/prom_process_stat.c',
        'src/prom_procfs.c',
        'src/prom_string_builder.c',
    ],
    c_args: [
        '-std=gnu11',
        '-Werror',
        '-Wall',
        '-Wuninitialized',
        '-Wno-unused-label',
        '-Wno-error=sign-compare',
        '-Wno-error=unused-parameter',
        '-Wno-error=empty-body',
        '-Wno-error=implicit-fallthrough',
    ],
    dependencies: [
        threads_dep,
    ],
    include_directories: [
        inc_dirs,
    ],
    install: true,
    soversion: 1,
)

inc_subdir = 'prom'
install_headers(
    [
        'include/prom.h',
        'include/prom_alloc.h',
        'include/prom_collector.h',
        'include/prom_collector_registry.h',
        'include/prom_counter.h',
        'include/prom_gauge.h',
        'include/prom_histogram.h',
        'include/prom_histogram_buckets.h',
        'include/prom_linked_list.h',
        'include/prom_map.h',
        'include/prom_metric.h',
        'include/prom_metric_sample.h',
        'include/prom_metric_sample_histogram.h',
    ],
    subdir: inc_subdir,
)

pkgcfg = import('pkgconfig')
pkgcfg.generate(
    libprom,
    subdirs: [
        inc_subdir,
    ],
)

libprom_dep = declare_dependency(
    include_directories: [
        inc_dirs,
    ],
    link_with: [
        libprom,
    ],
    dependencies: [
        threads_dep,
    ],
)
