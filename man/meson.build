man_sources = [
    'cclip.1.in',
    'cclipd.1.in'
]

foreach man_source: man_sources
    name = man_source.split('.')[0]
    section = man_source.split('.')[1]

    out_man = '@0@.@1@'.format(name, section)

    conf_data = configuration_data()
    conf_data.set('VERSION', meson.project_version())

    configure_file(
        input: man_source,
        output: out_man,
        configuration: conf_data,
        install: true,
        install_dir: join_paths(get_option('mandir'), 'man@0@'.format(section))
    )
endforeach

