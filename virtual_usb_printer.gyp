{
  'target_defaults': {
    'variables': {
      'deps': [
        'libbrillo-<(libbase_ver)',
        'libchrome-<(libbase_ver)',
      ],
    },
  },
  'targets': [
    {
      'target_name': 'virtual-usb-printer',
      'type': 'executable',
      'sources': [
        'cups_constants.cc',
        'device_descriptors.cc',
        'ipp_util.cc',
        'load_config.cc',
        'op_commands.cc',
        'server.cc',
        'smart_buffer.cc',
        'usbip.cc',
        'usb_printer.cc',
        'value_util.cc',
        'virtual_usb_printer.cc',
      ],
    },
  ],
  'conditions': [
    ['USE_test == 1', {
      'targets': [
        {
          'target_name': 'ipp-util-testrunner',
          'type': 'executable',
          'dependencies': [
            '../../platform2/common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'cups_constants.cc',
            'ipp_util.cc',
            'ipp_test.cc',
            'smart_buffer.cc',
            'value_util.cc',
          ],
        },
        {
          'target_name': 'smart-buffer-testrunner',
          'type': 'executable',
          'dependencies': [
            '../../platform2/common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'smart_buffer.cc',
            'smart_buffer_test.cc',
            'value_util.cc',
          ],
        },
        {
          'target_name': 'load-config-testrunner',
          'type': 'executable',
          'dependencies': [
            '../../platform2/common-mk/testrunner.gyp:testrunner',
          ],
          'variables': {
            'deps': [
              'libchrome-test-<(libbase_ver)',
            ],
          },
          'sources': [
            'load_config.cc',
            'load_config_test.cc',
            'device_descriptors.cc',
            'value_util.cc',
          ],
        },
      ],
    }],
  ],
}
