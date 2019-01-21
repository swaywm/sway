systemd_dep = dependency('systemd')

if systemd_dep.found()
  systemd_units = files(
      'sway-session.target',
  )
  user_units_dir = systemd_dep.get_pkgconfig_variable('systemduserunitdir')
  install_data(systemd_units, install_dir: user_units_dir)

  sway_systemd_scripts = files(
      '10-systemd',
  )

  install_data(sway_systemd_scripts, install_dir: sysconfdir + '/sway/config.d')
endif
